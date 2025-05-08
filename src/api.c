#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(api, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include "stm32f7xx_remote_io.h"
#include "ethernet_if.h"
#include "system_info.h"
#include "api.h"
#include "uart.h"
#include "ws28xx_pwm.h"

#define PARAM_STR_MAX_LENGTH    MAX_INT_DIGITS+2 // 1 for sign, 1 for null terminator

// run through the linked list of tokens and copy the data to the buffer
#define API_COPY_TOKEN_DATA_TO_BUFFER(token, buffer, length) \
    do { \
        token_t *t = token; \
        for (uint8_t i = 0; i < length; i++) { \
            if (t == NULL) { \
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER; \
                break; \
            } \
            buffer[i] = t->i32; \
            t = t->next; \
        } \
    } while (0); \
    if (error_code) break


// function prototypes
token_t* api_create_token();
void api_free_tokens(token_t* token);
void api_reset_command_line();
io_status_t api_process_data(ethernet_if_socket_service_t *service, command_line_t *command_line);
void api_execute_command(ethernet_if_socket_service_t *service, command_line_t *command_line);
void api_error(ethernet_if_socket_service_t *service, uint16_t error_code);

// event for receiving new data
K_EVENT_DEFINE(apiNewDataEvent);

static char anyTypeBuffer[UART_TX_BUFFER_SIZE] = {'\0'}; // store data for ANY type

extern ws_color_t ws28xx_pwm_color[NUMBER_OF_LEDS];


void api_task(void *p1, void *p2, void *p3)
{
    // conver parameters
    // p1 -> service
    ethernet_if_socket_service_t *service = (ethernet_if_socket_service_t *)p1;
    // unused parameters
    (void)p2;
    (void)p3;

    command_line_t command_line = {0}; // store command line data

    for (;;)
    {
        // check if rx buffer is empty
        if (utils_is_buffer_empty(service->rx_buffer) == STATUS_OK)
        {
            // wait for new data event
            k_event_wait(&apiNewDataEvent, service->event, true, K_FOREVER);
        }

        if (api_process_data(service, &command_line) == STATUS_OK)
        {
            // execute the command
            api_execute_command(service, &command_line);
            LOG_DBG("Command executed: %c%d.%d\n", command_line.type, command_line.id, command_line.variant);
        }
        else
        {
            while (service->rx_buffer->head != service->rx_buffer->tail)
            {
                char chr = service->rx_buffer->buffer[service->rx_buffer->tail];
                // increment rx buffer tail
                utils_increment_buffer_tail(service->rx_buffer);
                if (chr == '\n' || chr == '\r')
                {
                    break;
                }
            }
        }

        // free linked list of tokens
        api_free_tokens(command_line.token);
        // clear the command line
        api_reset_command_line(&command_line);
    }
}


// increment rx buffer tail or wait for new data
void api_increment_rx_buffer_tail_or_wait(utils_ring_buffer_t *ring_buf, uint32_t event)
{
    uint8_t nextTail = (ring_buf->tail + 1) % ring_buf->size; // next tail index
    if (nextTail == ring_buf->head)
    {
        // wait for new data
        k_event_wait(&apiNewDataEvent, event, true, K_FOREVER);
    }
    // increment rx buffer tail
    utils_increment_buffer_tail(ring_buf);
}

// functions for tokenizing data
// create token
token_t* api_create_token()
{
    // create token
    token_t* token = (token_t*)malloc(sizeof(token_t));
    if (token == NULL)
    {
        return NULL;
    }

    // initialize token
    token->type = 0;
    token->value_type = 0;
    token->any = NULL;
    token->next = NULL;

    return token;
}

// free linked list of tokens
void api_free_tokens(token_t* token)
{
    token_t* next = NULL;
    while (token != NULL)
    {
        next = token->next;
        token->next = NULL;
        free(token);
        token = next;
    }
}

// reset command line
void api_reset_command_line(command_line_t *command_line)
{
    memset(command_line, 0, sizeof(command_line_t));
}

// parse received data
io_status_t api_process_data(ethernet_if_socket_service_t *service, command_line_t *command_line)
{
    utils_ring_buffer_t *rx_buf = service->rx_buffer; // rx buffer
    char *rxBuffer = rx_buf->buffer; // buffer pointer
    uint8_t rxBufferSize = rx_buf->size; // buffer size
    char chr = rxBuffer[rx_buf->tail];          // current character
    char param_str[PARAM_STR_MAX_LENGTH] = {'\0'};
    bool isVariant = false;                     // check if there is a variant for the command
    bool isLengthRequired = false;              // check if length is required for the command


    //// [Commnad Type]: lexing the command type //////////////////////////////////
    // check if command type is valid
    switch (chr)
    {
    case 'W': case 'w': // write data
        command_line->type = 'W';
        break;
    case 'R': case 'r': // read data
        command_line->type = 'R';
        break;

    case '\n': case '\r': // end of command
        return STATUS_FAIL;

    default:
        api_error(service, API_ERROR_CODE_INVALID_COMMAND_TYPE);
        return STATUS_FAIL;
    }

    // increment rx buffer tail
    api_increment_rx_buffer_tail_or_wait(rx_buf, service->event);


    //// [ID].[Variant]: lexing the command id and variant ////////////////////////
    while (rx_buf->head != rx_buf->tail)
    {
        chr = rxBuffer[rx_buf->tail];

        if (chr >= '0' && chr <= '9')
        {
            if (isVariant)
            {
                command_line->variant = command_line->variant * 10 + (chr - '0');
            }
            else
            {
                command_line->id = command_line->id * 10 + (chr - '0');
            }
        }
        else if ((chr == '.') && (command_line->id != 0))
        {
            if (!isVariant) isVariant = true;
            else
            {
                api_error(service, API_ERROR_CODE_INVALID_COMMAND_VARIANT);
                return STATUS_FAIL;
            }
        }
        else if ((chr == ' ') && (command_line->id != 0))
        {
            if (isVariant && command_line->variant == 0) isVariant = false;
            //remove all the blank spaces when processing the command
            API_REMOVE_BLANK_SPACES(service);
            // proceed to next parsing step
            break;
        }
        else if ((chr == '\n' || chr == '\r') && command_line->id != 0)
        {
            // end of command line
            return STATUS_OK;
        }
        else
        {
            api_error(service, API_ERROR_CODE_INVALID_COMMAND_ID);
            return STATUS_FAIL;
        }

        // increment rx buffer tail
        api_increment_rx_buffer_tail_or_wait(rx_buf, service->event);
    }


    //// [Param]: lexing the parameters //////////////////////////////////////////
    token_t* token = api_create_token();

    // check if length parameter is required for the command
    switch (command_line->id)
    {
    case SERVICE_ID_SERIAL:
        isLengthRequired = true;
        break;
    }

    // check if first character is a digit or a sign
    chr = rxBuffer[rx_buf->tail];
    if (!isdigit(chr) && chr != '-')
    {
        api_error(service, API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
        api_free_tokens(token);
        return STATUS_FAIL;
    }
    else
    {
        if (token == NULL)
        {
            api_error(service, API_ERROR_CODE_FAIL_ALLOCATE_MEMORY_FOR_TOKEN);
            return STATUS_FAIL;
        }

        // check if length is required for the command
        if (isLengthRequired)
        {
            token->type = TOKEN_TYPE_LENGTH;
        }
        else
        {
            token->type = TOKEN_TYPE_PARAM;
        }

        token->value_type = PARAM_TYPE_INT32;

        // add token to the linked list
        command_line->token = token;
        command_line->last_token = token;
    }

    uint8_t param_index = 0;
    bool isDecimal = false;
    bool isAParam = false;
    bool isError = false;

    while (rx_buf->head != rx_buf->tail)
    {
        chr = rxBuffer[rx_buf->tail];

        // process data based on the value type of the parameter
        // type of number
        if (token->value_type == PARAM_TYPE_INT32)
        {
            if (chr == '-' && param_index == 0)
            {
                param_str[param_index++] = chr;
            }
            else if (chr >= '0' && chr <= '9' && param_index < PARAM_STR_MAX_LENGTH)
            {
                param_str[param_index++] = chr;
            }
            else if ((chr == ' ') && (param_index > 0))
            {
                // end of parameter
                isAParam = true;
            }
            else if ((chr == '\r' || chr == '\n') && (token->type != TOKEN_TYPE_LENGTH) && (param_index > 0))
            {
                // end of parameter
                isAParam = true;
            }
            else if (chr == '.' && !isDecimal && param_index < PARAM_STR_MAX_LENGTH)
            {
                param_str[param_index++] = chr;
                isDecimal = true;
            }
            else
            {
                if (param_index >= PARAM_STR_MAX_LENGTH)
                {
                    api_error(service, API_ERROR_CODE_TOO_MANNY_DIGITS);
                }
                else
                {
                    api_error(service, API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
                }
                isError = true;
                break;
            }
        }
        // ANY type, push any data into the parameter
        else if (token->value_type == PARAM_TYPE_ANY)
        {
            if (param_index < command_line->last_token->i32)
            {
                anyTypeBuffer[param_index++] = chr;
            }
            
            // check if it is the end of the parameter based on the length
            if (param_index >= command_line->last_token->i32)
            {
                // check if next character is a `\r` or `\n`
                uint8_t nextTail = (rx_buf->tail+1) % rxBufferSize;
                if (nextTail != rx_buf->head)
                {
                    char chr = rxBuffer[nextTail];
                    if (chr == '\n' || chr == '\r')
                    {
                        // end of parameter
                        isAParam = true;
                    }
                    else
                    {
                        api_error(service, API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
                        isError = true;
                        break;
                    }
                }
                else // wait for new data
                {
                    API_WAIT_UNTIL_BUFFER_NOT_EMPTY(service, &apiNewDataEvent);
                }
            }
        }

        // check if it is the end of the parameter
        if (isAParam)
        {
            // check if the parameter is a float
            if (isDecimal)
            {
                token->value_type = PARAM_TYPE_FLOAT;
                token->f = atof(param_str);
            }
            else if (token->value_type == PARAM_TYPE_INT32)
            {
                token->i32 = atoi(param_str);
            }
            else
            {
                token->any = &anyTypeBuffer;
            }

            // reset parameters
            memset(param_str, '\0', sizeof(param_str));
            param_index = 0;
            isDecimal = false;
            isAParam = false;

            // check if it is the end of the command line
            // wait until rx buffer is not empty
            API_WAIT_UNTIL_BUFFER_NOT_EMPTY(service, &apiNewDataEvent);

            // increment rx buffer tail
            api_increment_rx_buffer_tail_or_wait(rx_buf, service->event);
            // remove all the blank spaces when processing the command
            API_REMOVE_BLANK_SPACES(service);
            chr = rxBuffer[rx_buf->tail];
            if ((chr == '\n' || chr == '\r') && (token->type != TOKEN_TYPE_LENGTH))
            {
                // increment rx buffer tail
                utils_increment_buffer_tail(rx_buf);
                // end of command line
                return STATUS_OK;
            }

            // update the last token
            command_line->last_token = token;

            // not the end of the command line. Create a new token.
            token = api_create_token();
            if (token == NULL)
            {
                api_error(service, API_ERROR_CODE_FAIL_ALLOCATE_MEMORY_FOR_TOKEN);
                isError = true;
                break;
            }

            // add new token to the linked list
            command_line->last_token->next = token;

            // check if the type of the parameter is supposed to be any
            if (command_line->last_token != NULL && command_line->last_token->type == TOKEN_TYPE_LENGTH)
            {
                token->value_type = PARAM_TYPE_ANY;
            }
            else
            {
                token->value_type = PARAM_TYPE_INT32;
            }
        }
        else
        {
            // increment rx buffer tail
            api_increment_rx_buffer_tail_or_wait(rx_buf, service->event);
        }

    } // while loop

    // free linked list of tokens if there is an error
    if (isError)
    {
        return STATUS_FAIL;
    }

    // should not reach here
    api_error(service, API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
    return STATUS_FAIL;
}

// execute the command
void api_execute_command(ethernet_if_socket_service_t *service, command_line_t *command_line)
{
    uint16_t error_code = 0;

    // execute the command
    switch (command_line->id)
    {
    case SERVICE_ID_STATUS:
        // execute status command
        if (command_line->type == 'R')
        {
            // send default response
            API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_SYSTEM_INFO:
        // execute system info command
        if (command_line->type == 'R')
        {
            // get system info
            system_info_print(service);
        }   
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    // case SERVICE_ID_SERIAL:
    //     // execute serial command
    //     if (command_line->type == 'W')
    //     {
    //         // search for the message token and ingore the length token
    //         token_t* token = command_line->token;
    //         uint8_t length = 0;
    //         while (token != NULL)
    //         {
    //             if (token->type == TOKEN_TYPE_LENGTH)
    //             {
    //                 length = token->i32;
    //             }
    //             else if (token->value_type == PARAM_TYPE_ANY)
    //             {
    //                 break;
    //             }
    //             token = token->next;
    //         }
    //         // check if the token is found
    //         if (token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //         }
    //         else
    //         {
    //             // ensure the index of uart is valid
    //             if (command_line->variant >= UART_MAX)
    //             {
    //                 error_code = API_ERROR_CODE_INVALID_COMMAND_VARIANT;
    //                 break;
    //             }
    //             // send the message to the serial port
    //             uart_printf((uart_index_t)(command_line->variant), (uint8_t*)token->any, length);
    //             // clear param buffer
    //             memset(anyTypeBuffer, '\0', sizeof(anyTypeBuffer));
    //             // reply with default response
    //             API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //         }
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    case SERVICE_ID_INPUT:
        // execute input command
        if (command_line->type == 'R')
        {
            // check if parameter is valid
            if (command_line->token == NULL ||
                command_line->token->value_type != PARAM_TYPE_INT32 ||
                command_line->token->i32 == 0 ||
                command_line->token->i32 > DIGITAL_INPUT_MAX)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            // if the parameter equals to -1, read all the digital inputs
            if (command_line->token->i32 == -1)
            {
                // read all the digital inputs
                uint32_t state = digital_input_read_all();
                // send the state to the client, format: "R<Service ID> <State>"
                ethernet_if_send(service, "R%d %d\r\n", SERVICE_ID_INPUT, state);
            }
            // read the state of specified digital input
            else
            {
                bool state = digital_input_read((uint8_t)(command_line->token->i32));
                // send the state to the client, format: "R<Service ID> <Input Index> <State>"
                ethernet_if_send(service, "R%d %d %d\r\n", SERVICE_ID_INPUT, command_line->token->i32, state);
            }
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    // case SERVICE_ID_SUBSCRIBE_INPUT:
    //     // execute subscribe input command
    //     if (command_line->type == 'W')
    //     {
    //         // check if token is valid
    //         if (command_line->token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //             break;
    //         }

    //         token_t* token = command_line->token;
    //         while (token != NULL)
    //         {
    //             // check if parameter is valid
    //             if (token->value_type != PARAM_TYPE_INT32 ||
    //             token->i32 == 0 ||
    //             token->i32 > DIGITAL_INPUT_MAX)
    //             {
    //                 error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                 break;
    //             }
    //             // subscribe to the digital input
    //             digital_input_subscribe(token->i32);
    //             token = token->next;
    //         }
    //         API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else if (command_line->type == 'R')
    //     {
    //         // print header
    //         ethernet_if_send(service, "R%d ", SERVICE_ID_SUBSCRIBE_INPUT);
    //         // print current subscribed digital inputs
    //         digital_input_print_subscribed_inputs();
    //         // print new line
    //         ethernet_if_send(service, "\r\n");
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SERVICE_ID_UNSUBSCRIBE_INPUT:
    //     // execute unsubscribe input command
    //     // check if token is valid
    //     if (command_line->token == NULL)
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //         break;
    //     }

    //     if (command_line->type == 'W')
    //     {
    //         token_t* token = command_line->token;
    //         while (token != NULL)
    //         {
    //             // check if parameter is valid
    //             if (token->value_type != PARAM_TYPE_INT32 ||
    //             token->i32 == 0 ||
    //             token->i32 > DIGITAL_INPUT_MAX)
    //             {
    //                 error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                 break;
    //             }
    //             // unsubscribe to the digital input
    //             digital_input_unsubscribe(token->i32);
    //             token = token->next;
    //         }
    //         API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SERVICE_ID_OUTPUT:
    // {
    //     // check if token is valid
    //     if (command_line->token == NULL)
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //         break;
    //     }

    //     token_t* token = command_line->token;

    //     // get output index
    //     int16_t output_index = token->i32;

    //     // execute output command
    //     if (command_line->type == 'W')
    //     {
    //         // handle different variants of the command
    //         switch (command_line->variant)
    //         {
    //             case 1: // write to multiple outputs
    //             {
    //                 // get the write value
    //                 if (token == NULL)
    //                 {
    //                     error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                     break;
    //                 }
    //                 uint32_t data = token->i32;

    //                 // get the start index
    //                 token = token->next;
    //                 if (token == NULL)
    //                 {
    //                     error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                     break;
    //                 }
    //                 uint8_t start_index = (uint8_t)token->i32;

    //                 // get the length
    //                 token = token->next;
    //                 if (token == NULL)
    //                 {
    //                     error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                     break;
    //                 }
    //                 uint8_t length = (uint8_t)token->i32;

    //                 // write to multiple digital outputs
    //                 digital_output_write_multiple(data, start_index, length);

    //                 API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //                 break;
    //             }
    //             default: // write to single output
    //             {
    //                 // check if parameter is valid
    //                 if (output_index == 0 || output_index >= DIGITAL_OUTPUT_MAX)
    //                 {
    //                     error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                     break;
    //                 }

    //                 // get the write value
    //                 token = token->next;
    //                 if (token == NULL)
    //                 {
    //                     error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //                     break;
    //                 }
    //                 bool state = (token->i32 > 0) ? true : false;

    //                 // write to the digital output
    //                 digital_output_write(output_index, state);

    //                 API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //                 break;
    //             }
    //         }
    //     }
    //     else if (command_line->type == 'R')
    //     {
    //         if (output_index == -1)
    //         {
    //             // read all the digital outputs
    //             uint32_t state = digital_output_read_all();
    //             // send the state to the client, format: "R<Service ID> <State>"
    //             ethernet_if_send(service, "R%d %d\r\n", SERVICE_ID_OUTPUT, state);
    //         }
    //         else
    //         {
    //             // read the state of specified digital output
    //             bool state = digital_output_read((uint8_t)output_index);
    //             // send the state to the client, format: "R<Service ID> <Output Index> <State>"
    //             ethernet_if_send(service, "R%d %d %d\r\n", SERVICE_ID_OUTPUT, output_index, state);
    //         }
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // }
    // case SERVICE_ID_PWM_WS28XX_LED:
    // {
    //     // check if token is valid
    //     if (command_line->token == NULL)
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //         break;
    //     }

    //     token_t* token = command_line->token;

    //     // get the LED index
    //     uint16_t led_index = token->i32;

    //     // check if parameter is valid
    //     if (led_index >= NUMBER_OF_LEDS)
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //         break;
    //     }

    //     // execute WS28XX LED command
    //     if (command_line->type == 'W')
    //     {
    //         // get the color of the LED
    //         token = token->next;
    //         if (token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //             break;
    //         }

    //         // get the color of the LED
    //         uint8_t r = (uint8_t)token->i32;
    //         token = token->next;
    //         if (token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //             break;
    //         }

    //         uint8_t g = (uint8_t)token->i32;
    //         token = token->next;
    //         if (token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //             break;
    //         }

    //         uint8_t b = (uint8_t)token->i32;

    //         // set the color of the LED
    //         if (ws28xx_pwm_set_color(r, g, b, led_index) != HAL_OK)
    //         {
    //             error_code = API_ERROR_CODE_SET_LED_COLOR_FAILED;
    //         }

    //         // update the LED
    //         if (ws28xx_pwm_update() != HAL_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_LED_FAILED;
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else if (command_line->type == 'R')
    //     {
    //         // read the color of the LED
    //         ws_color_t color = ws28xx_pwm_color[led_index];
    //         // send the color to the client, format: "R<Service ID> <LED Index> <R> <G> <B>"
    //         ethernet_if_send(service, "R%d %d %d %d %d\r\n", SERVICE_ID_PWM_WS28XX_LED, led_index, color.r, color.g, color.b);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // }
    // case SETTING_ID_IP_ADDRESS:
    //     if (command_line->type == 'R')
    //     {
    //         // send the IP address to the client, format: "R<Service ID> <IP Addr 1> <IP Addr 2> <IP Addr 3> <IP Addr 4>"
    //         // e.g. "R101 172 16 0 10"
    //         ethernet_if_send(service, "R%d %d %d %d %d\r\n", SETTING_ID_IP_ADDRESS,
    //                     settings.ip_address_0, settings.ip_address_1,
    //                     settings.ip_address_2, settings.ip_address_3);
    //     }
    //     else if (command_line->type == 'W')
    //     {
    //         uint8_t ip_address[4] = {0};
    //         token_t* token = command_line->token;

    //         // copy IP address from token to buffer
    //         API_COPY_TOKEN_DATA_TO_BUFFER(token, ip_address, 4);
            
    //         // update the IP address in the settings
    //         settings.ip_address_0 = ip_address[0];
    //         settings.ip_address_1 = ip_address[1];
    //         settings.ip_address_2 = ip_address[2];
    //         settings.ip_address_3 = ip_address[3];

    //         // save IP address in flash
    //         if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_IP_FAILED;
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SETTING_ID_NETMASK:
    //     if (command_line->type == 'R')
    //     {
    //         // send the netmask to the client, format: "R<Service ID> <Netmask 1> <Netmask 2> <Netmask 3> <Netmask 4>"
    //         // e.g. "R102 255 240 0 0"
    //         ethernet_if_send(service, "R%d %d %d %d %d\r\n", SETTING_ID_NETMASK,
    //                     settings.netmask_0, settings.netmask_1,
    //                     settings.netmask_2, settings.netmask_3);
    //     }
    //     else if (command_line->type == 'W')
    //     {
    //         uint8_t netmask[4] = {0};
    //         token_t* token = command_line->token;

    //         // copy netmask from token to buffer
    //         API_COPY_TOKEN_DATA_TO_BUFFER(token, netmask, 4);

    //         // update the netmask in the settings
    //         settings.netmask_0 = netmask[0];
    //         settings.netmask_1 = netmask[1];
    //         settings.netmask_2 = netmask[2];
    //         settings.netmask_3 = netmask[3];

    //         // save netmask in flash
    //         if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_NETMASK_FAILED;
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SETTING_ID_GATEWAY:
    //     if (command_line->type == 'R')
    //     {
    //         // send the gateway to the client, format: "R<Service ID> <Gateway 1> <Gateway 2> <Gateway 3> <Gateway 4>"
    //         // e.g. "R103 172 16 0 1"
    //         ethernet_if_send(service, "R%d %d %d %d %d\r\n", SETTING_ID_GATEWAY,
    //                     settings.gateway_0, settings.gateway_1,
    //                     settings.gateway_2, settings.gateway_3);
    //     }
    //     else if (command_line->type == 'W')
    //     {
    //         uint8_t gateway[4] = {0};
    //         token_t* token = command_line->token;

    //         // copy gateway from token to buffer
    //         API_COPY_TOKEN_DATA_TO_BUFFER(token, gateway, 4);

    //         // update the gateway in the settings
    //         settings.gateway_0 = gateway[0];
    //         settings.gateway_1 = gateway[1];
    //         settings.gateway_2 = gateway[2];
    //         settings.gateway_3 = gateway[3];

    //         // save gateway in flash
    //         if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_GATEWAY_FAILED;
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SETTING_ID_MAC_ADDRESS:
    //     if (command_line->type == 'R')
    //     {
    //         // send the MAC address to the client, format: "R<Service ID> <MAC Addr 1> <MAC Addr 2> <MAC Addr 3> <MAC Addr 4> <MAC Addr 5> <MAC Addr 6>"
    //         // e.g. "R104 0 0 0 0 0 0"
    //         ethernet_if_send(service, "R%d %d %d %d %d %d %d\r\n", SETTING_ID_MAC_ADDRESS,
    //                     settings.mac_address_0, settings.mac_address_1,
    //                     settings.mac_address_2, settings.mac_address_3,
    //                     settings.mac_address_4, settings.mac_address_5);
    //     }
    //     else if (command_line->type == 'W')
    //     {
    //         uint8_t mac_address[6] = {0};
    //         token_t* token = command_line->token;

    //         // copy MAC address from token to buffer
    //         API_COPY_TOKEN_DATA_TO_BUFFER(token, mac_address, 6);

    //         // update the MAC address in the settings
    //         settings.mac_address_0 = mac_address[0];
    //         settings.mac_address_1 = mac_address[1];
    //         settings.mac_address_2 = mac_address[2];
    //         settings.mac_address_3 = mac_address[3];
    //         settings.mac_address_4 = mac_address[4];
    //         settings.mac_address_5 = mac_address[5];

    //         // save MAC address in flash
    //         if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_MAC_ADDRESS_FAILED;
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SETTING_ID_TCP_PORT:
    //     if (command_line->type == 'R')
    //     {
    //         // send the Ethernet port to the client, format: "R<Service ID> <Port>"
    //         // e.g. "R105 80"
    //         ethernet_if_send(service, "R%d %d\r\n", SETTING_ID_TCP_PORT, settings.tcp_port);
    //     }
    //     else if (command_line->type == 'W')
    //     {
    //         // check if token is valid
    //         if (command_line->token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //             break;
    //         }

    //         token_t* token = command_line->token;

    //         // write the Ethernet port
    //         settings.tcp_port = (uint16_t)(token->i32);

    //         // write the Ethernet port
    //         if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_IP_FAILED; /////////////////////////////////////////////
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    // case SETTING_ID_BAUD_RATE:
    //     // assert if variant is valid
    //     if (command_line->variant >= UART_MAX)
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_VARIANT;
    //         break;
    //     }

    //     if (command_line->type == 'R')
    //     {
    //         // send the baud rate to the client, format: "R<Service ID> <Baud Rate>"
    //         // e.g. "R106.x 115200"
    //         ethernet_if_send(service, "R%d.%d %d\r\n", SETTING_ID_BAUD_RATE, command_line->variant,
    //                     settings.uart[command_line->variant].baudrate);
    //     }
    //     else if (command_line->type == 'W')
    //     {
    //         // check if token is valid
    //         if (command_line->token == NULL)
    //         {
    //             error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
    //             break;
    //         }

    //         token_t* token = command_line->token;

    //         // write the baud rate
    //         settings.uart[command_line->variant].baudrate = (uint32_t)(token->i32);

    //         // write the baud rate
    //         if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
    //         {
    //             error_code = API_ERROR_CODE_UPDATE_IP_FAILED; /////////////////////////////////////////////
    //         }
    //         else API_DEFAULT_RESPONSE(service, command_line->type, command_line->id);
    //     }
    //     else
    //     {
    //         error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
    //     }
    //     break;
    default:
    {
        // debug print the command
        LOG_DBG("Command: %c.%d \r\n", command_line->type, command_line->id);
        ethernet_if_send(service, "%c%d", command_line->type, command_line->id);
        if (command_line->variant > 0)
        {
            LOG_DBG("Variant: %d \r\n", command_line->variant);
            ethernet_if_send(service, ".%d", command_line->variant);
        }
        ethernet_if_send(service, "\r\n");

        // print the parameters
        token_t* token = command_line->token;
        while (token != NULL)
        {
            if (token->type == TOKEN_TYPE_LENGTH)
            {
                LOG_DBG("Length: %d \r\n", token->i32);
            }
            else
            {
                if (token->value_type == PARAM_TYPE_INT32)
                {
                    LOG_DBG("Param: %d \r\n", token->i32);
                }
                else if (token->value_type == PARAM_TYPE_FLOAT)
                {
                    LOG_DBG("Param: %f \r\n", (double)token->f);
                }
                else // PARAM_TYPE_ANY
                {
                    LOG_DBG("Param: %s \r\n", (char*)token->any);
                    // clear the buffer
                    memset(anyTypeBuffer, '\0', sizeof(anyTypeBuffer));
                }
            }
            token = token->next;
        }

        break;
    }
    }

    // check if there is an error
    if (error_code != 0)
    {
        api_error(service, error_code);
    }
}

void api_error(ethernet_if_socket_service_t *service, uint16_t error_code)
{
    // print error code
    ethernet_if_send(service, ERROR_CODE_FORMAT, error_code);
    LOG_ERR("Error: %d\n", error_code);
}