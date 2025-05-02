#include <zephyr/kernel.h>
#include "stm32f7xx_remote_io.h"
#include "ethernet_if.h"

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
void api_task(void *p1, void *p2, void *p3);
token_t* api_create_token();
void api_free_tokens(token_t* token);
void api_reset_command_line();
io_status_t api_process_data();
void api_execute_command();
void api_error(uint16_t error_code);

// API task handle
K_THREAD_DEFINE(apiTaskHandle,
                CONFIG_REMOTEIO_MINIMAL_STACK_SIZE * 2,
                api_task,
                NULL,
                NULL,
                NULL,
                tskIDLE_PRIORITY + 1,
                0,
                0);

// event for receiving new data
K_EVENT_DEFINE(newDataEvent);

extern TaskHandle_t processTxTaskHandle;

// ring buffer for received data
// static char rxBuffer[API_RX_BUFFER_SIZE];
// static uint8_t rxBufferHead = 0;
// static uint8_t rxBufferTail = 0;

// ring buffer for sending data
// static char txBuffer[API_TX_BUFFER_SIZE];
// static uint8_t txBufferHead = 0;
// static uint8_t txBufferTail = 0;

static command_line_t commandLine = {0}; // store command line data
static char anyTypeBuffer[UART_TX_BUFFER_SIZE] = {'\0'}; // store data for ANY type
token_t* lastToken = NULL;               // store the last token

extern ws_color_t ws28xx_pwm_color[NUMBER_OF_LEDS];

SemaphoreHandle_t apiAppendTxSemaphoreHandle = NULL;

// initialize API task
void api_init()
{
    // create semacphore for appending data to tx buffer
    apiAppendTxSemaphoreHandle = xSemaphoreCreateBinary();

    // give semaphore
    xSemaphoreGive(apiAppendTxSemaphoreHandle);

    // initialize api task
    xTaskCreate(api_task, "API Task", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY+1, &apiTaskHandle);
}

void api_task(void *p1, void *p2, void *p3)
{
    for (;;)
    {
        // check if rx buffer is empty
        if (api_is_rx_buffer_empty() == STATUS_OK)
        {
            // wait for new data
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }

        if (api_process_data() == STATUS_OK)
        {
            // execute the command
            api_execute_command();
        }
        else
        {
            while (rxBufferHead != rxBufferTail)
            {
                char chr = rxBuffer[rxBufferTail];
                // increment rx buffer tail
                api_increment_rx_buffer_tail();
                if (chr == '\n' || chr == '\r')
                {
                    break;
                }
            }
        }

        // free linked list of tokens
        api_free_tokens(commandLine.token);
        // clear the command line
        api_reset_command_line();
    }
}

// Append received data to buffer
io_status_t api_append_to_rx_ring_buffer(char *data, BaseType_t len)
{
    // append data to buffer
    for (uint8_t i = 0; i < len; i++)
    {
        // check if buffer is full
        if (api_is_rx_buffer_full() == STATUS_OK)
            return STATUS_FAIL;

        // append data to buffer
        rxBuffer[rxBufferHead] = data[i];
        api_increment_rx_buffer_head();
    }

    return STATUS_OK;
}

// Append data to tx buffer with specified length
io_status_t api_append_to_tx_ring_buffer(char *data, BaseType_t len)
{
    if (processTxTaskHandle == NULL)
    {
        return STATUS_FAIL;
    }

    // check if semaphore is created
    if (apiAppendTxSemaphoreHandle == NULL)
    {
        return STATUS_FAIL;
    }

    // take semaphore
    if (xSemaphoreTake(apiAppendTxSemaphoreHandle, portMAX_DELAY) != pdTRUE)
    {
        return STATUS_FAIL;
    }

    // append data to buffer
    for (uint8_t i = 0; i < len; i++)
    {
        // check if buffer is full
        // if (api_is_tx_buffer_full() == STATUS_OK)
        // {
        //     // give semaphore
        //     xSemaphoreGive(apiAppendTxSemaphoreHandle);
        //     return STATUS_FAIL;
        // }

        // append data to buffer
        txBuffer[txBufferHead] = data[i];
        api_increment_tx_buffer_head();

        // notify tx task that there are new data to be sent
        xTaskNotifyGive(processTxTaskHandle);
    }

    // give semaphore
    xSemaphoreGive(apiAppendTxSemaphoreHandle);
    return STATUS_OK;
}

// Append data to buffer until terminator is met
io_status_t api_append_to_tx_ring_buffer_until_term(char *data, char terminator)
{
    if (processTxTaskHandle == NULL)
    {
        return STATUS_FAIL;
    }

    // check if semaphore is created
    if (apiAppendTxSemaphoreHandle == NULL)
    {
        return STATUS_FAIL;
    }

    // take semaphore
    if (xSemaphoreTake(apiAppendTxSemaphoreHandle, portMAX_DELAY) != pdTRUE)
    {
        return STATUS_FAIL;
    }

    // append data to buffer
    uint8_t i = 0;
    char chr = data[i];
    for (; chr != terminator; chr = data[++i])
    {
        // check if buffer is full
        while (api_is_tx_buffer_full() == STATUS_OK)
        {
            // wait until buffer is not full
            vTaskDelay(1);
        }

        // append data to buffer
        txBuffer[txBufferHead] = chr;
        api_increment_tx_buffer_head();

        // notify tx task that there are new data to be sent
        xTaskNotifyGive(processTxTaskHandle);
    }

    // give semaphore
    xSemaphoreGive(apiAppendTxSemaphoreHandle);

    return STATUS_OK;
}

// increment rx buffer head
io_status_t api_increment_rx_buffer_head()
{
    UTILS_INCREMENT_BUFFER_HEAD(rxBufferHead, rxBufferTail, API_RX_BUFFER_SIZE);
}

// increment rx buffer tail
io_status_t api_increment_rx_buffer_tail()
{
    UTILS_INCREMENT_BUFFER_TAIL(rxBufferTail, rxBufferHead, API_RX_BUFFER_SIZE);
}

// increment rx buffer tail or wait for new data
void api_increment_rx_buffer_tail_or_wait()
{
    uint8_t nextTail = (rxBufferTail + 1) % API_RX_BUFFER_SIZE;
    if (nextTail == rxBufferHead)
    {
        // wait for new data
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    // increment rx buffer tail
    rxBufferTail = nextTail;
}

// check if rx buffer is empty
io_status_t api_is_rx_buffer_empty()
{
    return (rxBufferHead == rxBufferTail) ? STATUS_OK : STATUS_FAIL;
}

// check if rx buffer is full
io_status_t api_is_rx_buffer_full()
{
    return (((rxBufferHead + 1) % API_RX_BUFFER_SIZE) == rxBufferTail) ? STATUS_OK : STATUS_FAIL;
}

// increment tx buffer head
io_status_t api_increment_tx_buffer_head()
{
    UTILS_INCREMENT_BUFFER_HEAD(txBufferHead, txBufferTail, API_TX_BUFFER_SIZE);
}

// increment tx buffer tail
io_status_t api_increment_tx_buffer_tail()
{
    UTILS_INCREMENT_BUFFER_TAIL(txBufferTail, txBufferHead, API_TX_BUFFER_SIZE);
}

// check if tx buffer is empty
io_status_t api_is_tx_buffer_empty()
{
    return (txBufferHead == txBufferTail) ? STATUS_OK : STATUS_FAIL;
}

// check if tx buffer is full
io_status_t api_is_tx_buffer_full()
{
    return (((txBufferHead + 1) % API_TX_BUFFER_SIZE) == txBufferTail) ? STATUS_OK : STATUS_FAIL;
}

// pop data from tx buffer
char api_pop_tx_buffer()
{
    char chr = txBuffer[txBufferTail];
    if (api_increment_tx_buffer_tail() != STATUS_OK)
    {
        return '\0';
    }
    return chr;
}

// functions for tokenizing data
// create token
token_t* api_create_token()
{
    // create token
    token_t* token = (token_t*)pvPortMalloc(sizeof(token_t));
    if (token == NULL)
    {
        api_error(API_ERROR_CODE_FAIL_ALLOCATE_MEMORY_FOR_TOKEN);
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
        vPortFree(token);
        token = next;
    }
}

// reset command line
void api_reset_command_line()
{
    memset(&commandLine, 0, sizeof(command_line_t));
    lastToken = NULL;
}

// parse received data
io_status_t api_process_data()
{
    char chr = rxBuffer[rxBufferTail];          // current character
    char param_str[PARAM_STR_MAX_LENGTH] = {'\0'};
    bool isVariant = false;                     // check if there is a variant for the command
    bool isLengthRequired = false;              // check if length is required for the command


    //// [Commnad Type]: lexing the command type //////////////////////////////////
    // check if command type is valid
    switch (chr)
    {
    case 'W': case 'w': // write data
        commandLine.type = 'W';
        break;
    case 'R': case 'r': // read data
        commandLine.type = 'R';
        break;

    case '\n': case '\r': // end of command
        return STATUS_FAIL;

    default:
        api_error(API_ERROR_CODE_INVALID_COMMAND_TYPE);
        return STATUS_FAIL;
    }

    // increment rx buffer tail
    api_increment_rx_buffer_tail_or_wait();


    //// [ID].[Variant]: lexing the command id and variant ////////////////////////
    while (rxBufferHead != rxBufferTail)
    {
        chr = rxBuffer[rxBufferTail];

        if (chr >= '0' && chr <= '9')
        {
            if (isVariant)
            {
                commandLine.variant = commandLine.variant * 10 + (chr - '0');
            }
            else
            {
                commandLine.id = commandLine.id * 10 + (chr - '0');
            }
        }
        else if ((chr == '.') && (commandLine.id != 0))
        {
            if (!isVariant) isVariant = true;
            else
            {
                api_error(API_ERROR_CODE_INVALID_COMMAND_VARIANT);
                return STATUS_FAIL;
            }
        }
        else if ((chr == ' ') && (commandLine.id != 0))
        {
            if (isVariant && commandLine.variant == 0) isVariant = false;
            //remove all the blank spaces when processing the command
            API_REMOVE_BLANK_SPACES();
            // proceed to next parsing step
            break;
        }
        else if ((chr == '\n' || chr == '\r') && commandLine.id != 0)
        {
            // end of command line
            return STATUS_OK;
        }
        else
        {
            api_error(API_ERROR_CODE_INVALID_COMMAND_ID);
            return STATUS_FAIL;
        }

        // increment rx buffer tail
        api_increment_rx_buffer_tail_or_wait();
    }


    //// [Param]: lexing the parameters //////////////////////////////////////////
    token_t* token = api_create_token();

    // check if length parameter is required for the command
    switch (commandLine.id)
    {
    case SERVICE_ID_SERIAL:
        isLengthRequired = true;
        break;
    }

    // check if first character is a digit or a sign
    chr = rxBuffer[rxBufferTail];
    if (!isdigit(chr) && chr != '-')
    {
        api_error(API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
        api_free_tokens(token);
        return STATUS_FAIL;
    }
    else
    {
        if (token == NULL)
        {
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
        commandLine.token = token;
        lastToken = token;
    }

    uint8_t param_index = 0;
    bool isDecimal = false;
    bool isAParam = false;
    bool isError = false;

    while (rxBufferHead != rxBufferTail)
    {
        chr = rxBuffer[rxBufferTail];

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
                    api_error(API_ERROR_CODE_TOO_MANNY_DIGITS);
                }
                else
                {
                    api_error(API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
                }
                isError = true;
                break;
            }
        }
        // ANY type, push any data into the parameter
        else if (token->value_type == PARAM_TYPE_ANY)
        {
            if (param_index < lastToken->i32)
            {
                anyTypeBuffer[param_index++] = chr;
            }
            
            // check if it is the end of the parameter based on the length
            if (param_index >= lastToken->i32)
            {
                // check if next character is a `\r` or `\n`
                uint8_t nextTail = (rxBufferTail+1) % API_RX_BUFFER_SIZE;
                if (nextTail != rxBufferHead)
                {
                    char chr = rxBuffer[nextTail];
                    if (chr == '\n' || chr == '\r')
                    {
                        // end of parameter
                        isAParam = true;
                    }
                    else
                    {
                        api_error(API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
                        isError = true;
                        break;
                    }
                }
                else // wait for new data
                {
                    API_WAIT_UNTIL_BUFFER_NOT_EMPTY();
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
            API_WAIT_UNTIL_BUFFER_NOT_EMPTY();

            // increment rx buffer tail
            api_increment_rx_buffer_tail_or_wait();
            // remove all the blank spaces when processing the command
            API_REMOVE_BLANK_SPACES();
            chr = rxBuffer[rxBufferTail];
            if ((chr == '\n' || chr == '\r') && (token->type != TOKEN_TYPE_LENGTH))
            {
                // increment rx buffer tail
                api_increment_rx_buffer_tail();
                // end of command line
                return STATUS_OK;
            }

            // update the last token
            lastToken = token;

            // not the end of the command line. Create a new token.
            token = api_create_token();
            if (token == NULL)
            {
                api_error(API_ERROR_CODE_FAIL_ALLOCATE_MEMORY_FOR_TOKEN);
                isError = true;
                break;
            }

            // add new token to the linked list
            lastToken->next = token;

            // check if the type of the parameter is supposed to be any
            if (lastToken != NULL && lastToken->type == TOKEN_TYPE_LENGTH)
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
            api_increment_rx_buffer_tail_or_wait();
        }

    } // while loop

    // free linked list of tokens if there is an error
    if (isError)
    {
        return STATUS_FAIL;
    }

    // should not reach here
    api_error(API_ERROR_CODE_INVALID_COMMAND_PARAMETER);
    return STATUS_FAIL;
}

// execute the command
void api_execute_command()
{
    uint16_t error_code = 0;

    // execute the command
    switch (commandLine.id)
    {
    case SERVICE_ID_STATUS:
        // execute status command
        if (commandLine.type == 'R')
        {
            // send default response
            API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_SYSTEM_INFO:
        // execute system info command
        if (commandLine.type == 'R')
        {
            // print system info
            system_info_print();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_SERIAL:
        // execute serial command
        if (commandLine.type == 'W')
        {
            // search for the message token and ingore the length token
            token_t* token = commandLine.token;
            uint8_t length = 0;
            while (token != NULL)
            {
                if (token->type == TOKEN_TYPE_LENGTH)
                {
                    length = token->i32;
                }
                else if (token->value_type == PARAM_TYPE_ANY)
                {
                    break;
                }
                token = token->next;
            }
            // check if the token is found
            if (token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
            }
            else
            {
                // ensure the index of uart is valid
                if (commandLine.variant >= UART_MAX)
                {
                    error_code = API_ERROR_CODE_INVALID_COMMAND_VARIANT;
                    break;
                }
                // send the message to the serial port
                uart_printf((uart_index_t)(commandLine.variant), (uint8_t*)token->any, length);
                // clear param buffer
                memset(anyTypeBuffer, '\0', sizeof(anyTypeBuffer));
                // reply with default response
                API_DEFAULT_RESPONSE();
            }
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_INPUT:
        // execute input command
        if (commandLine.type == 'R')
        {
            // check if parameter is valid
            if (commandLine.token == NULL ||
                commandLine.token->value_type != PARAM_TYPE_INT32 ||
                commandLine.token->i32 == 0 ||
                commandLine.token->i32 >= DIGITAL_INPUT_MAX)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            // if the parameter equals to -1, read all the digital inputs
            if (commandLine.token->i32 == -1)
            {
                // read all the digital inputs
                uint32_t state = digital_input_read_all();
                // send the state to the client, format: "R<Service ID> <State>"
                api_printf("R%d %d\r\n", SERVICE_ID_INPUT, state);
            }
            // read the state of specified digital input
            else
            {
                bool state = digital_input_read((uint8_t)(commandLine.token->i32));
                // send the state to the client, format: "R<Service ID> <Input Index> <State>"
                api_printf("R%d %d %d\r\n", SERVICE_ID_INPUT, commandLine.token->i32, state);
            }
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_SUBSCRIBE_INPUT:
        // execute subscribe input command
        if (commandLine.type == 'W')
        {
            // check if token is valid
            if (commandLine.token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            token_t* token = commandLine.token;
            while (token != NULL)
            {
                // check if parameter is valid
                if (token->value_type != PARAM_TYPE_INT32 ||
                token->i32 == 0 ||
                token->i32 >= DIGITAL_INPUT_MAX)
                {
                    error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                    break;
                }
                // subscribe to the digital input
                digital_input_subscribe(token->i32);
                token = token->next;
            }
            API_DEFAULT_RESPONSE();
        }
        else if (commandLine.type == 'R')
        {
            // print header
            api_printf("R%d ", SERVICE_ID_SUBSCRIBE_INPUT);
            // print current subscribed digital inputs
            digital_input_print_subscribed_inputs();
            // print new line
            api_printf("\r\n");
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_UNSUBSCRIBE_INPUT:
        // execute unsubscribe input command
        // check if token is valid
        if (commandLine.token == NULL)
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
            break;
        }

        if (commandLine.type == 'W')
        {
            token_t* token = commandLine.token;
            while (token != NULL)
            {
                // check if parameter is valid
                if (token->value_type != PARAM_TYPE_INT32 ||
                token->i32 == 0 ||
                token->i32 >= DIGITAL_INPUT_MAX)
                {
                    error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                    break;
                }
                // unsubscribe to the digital input
                digital_input_unsubscribe(token->i32);
                token = token->next;
            }
            API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SERVICE_ID_OUTPUT:
    {
        // check if token is valid
        if (commandLine.token == NULL)
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
            break;
        }

        token_t* token = commandLine.token;

        // get output index
        int16_t output_index = token->i32;

        // execute output command
        if (commandLine.type == 'W')
        {
            // handle different variants of the command
            switch (commandLine.variant)
            {
                case 1: // write to multiple outputs
                {
                    // get the write value
                    if (token == NULL)
                    {
                        error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                        break;
                    }
                    uint32_t data = token->i32;

                    // get the start index
                    token = token->next;
                    if (token == NULL)
                    {
                        error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                        break;
                    }
                    uint8_t start_index = (uint8_t)token->i32;

                    // get the length
                    token = token->next;
                    if (token == NULL)
                    {
                        error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                        break;
                    }
                    uint8_t length = (uint8_t)token->i32;

                    // write to multiple digital outputs
                    digital_output_write_multiple(data, start_index, length);

                    API_DEFAULT_RESPONSE();
                    break;
                }
                default: // write to single output
                {
                    // check if parameter is valid
                    if (output_index == 0 || output_index >= DIGITAL_OUTPUT_MAX)
                    {
                        error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                        break;
                    }

                    // get the write value
                    token = token->next;
                    if (token == NULL)
                    {
                        error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                        break;
                    }
                    bool state = (token->i32 > 0) ? true : false;

                    // write to the digital output
                    digital_output_write(output_index, state);

                    API_DEFAULT_RESPONSE();
                    break;
                }
            }
        }
        else if (commandLine.type == 'R')
        {
            if (output_index == -1)
            {
                // read all the digital outputs
                uint32_t state = digital_output_read_all();
                // send the state to the client, format: "R<Service ID> <State>"
                api_printf("R%d %d\r\n", SERVICE_ID_OUTPUT, state);
            }
            else
            {
                // read the state of specified digital output
                bool state = digital_output_read((uint8_t)output_index);
                // send the state to the client, format: "R<Service ID> <Output Index> <State>"
                api_printf("R%d %d %d\r\n", SERVICE_ID_OUTPUT, output_index, state);
            }
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    }
    case SERVICE_ID_PWM_WS28XX_LED:
    {
        // check if token is valid
        if (commandLine.token == NULL)
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
            break;
        }

        token_t* token = commandLine.token;

        // get the LED index
        uint16_t led_index = token->i32;

        // check if parameter is valid
        if (led_index >= NUMBER_OF_LEDS)
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
            break;
        }

        // execute WS28XX LED command
        if (commandLine.type == 'W')
        {
            // get the color of the LED
            token = token->next;
            if (token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            // get the color of the LED
            uint8_t r = (uint8_t)token->i32;
            token = token->next;
            if (token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            uint8_t g = (uint8_t)token->i32;
            token = token->next;
            if (token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            uint8_t b = (uint8_t)token->i32;

            // set the color of the LED
            if (ws28xx_pwm_set_color(r, g, b, led_index) != HAL_OK)
            {
                error_code = API_ERROR_CODE_SET_LED_COLOR_FAILED;
            }

            // update the LED
            if (ws28xx_pwm_update() != HAL_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_LED_FAILED;
            }
            else API_DEFAULT_RESPONSE();
        }
        else if (commandLine.type == 'R')
        {
            // read the color of the LED
            ws_color_t color = ws28xx_pwm_color[led_index];
            // send the color to the client, format: "R<Service ID> <LED Index> <R> <G> <B>"
            api_printf("R%d %d %d %d %d\r\n", SERVICE_ID_PWM_WS28XX_LED, led_index, color.r, color.g, color.b);
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    }
    case SETTING_ID_IP_ADDRESS:
        if (commandLine.type == 'R')
        {
            // send the IP address to the client, format: "R<Service ID> <IP Addr 1> <IP Addr 2> <IP Addr 3> <IP Addr 4>"
            // e.g. "R101 172 16 0 10"
            api_printf("R%d %d %d %d %d\r\n", SETTING_ID_IP_ADDRESS,
                        settings.ip_address_0, settings.ip_address_1,
                        settings.ip_address_2, settings.ip_address_3);
        }
        else if (commandLine.type == 'W')
        {
            uint8_t ip_address[4] = {0};
            token_t* token = commandLine.token;

            // copy IP address from token to buffer
            API_COPY_TOKEN_DATA_TO_BUFFER(token, ip_address, 4);
            
            // update the IP address in the settings
            settings.ip_address_0 = ip_address[0];
            settings.ip_address_1 = ip_address[1];
            settings.ip_address_2 = ip_address[2];
            settings.ip_address_3 = ip_address[3];

            // save IP address in flash
            if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_IP_FAILED;
            }
            else API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SETTING_ID_NETMASK:
        if (commandLine.type == 'R')
        {
            // send the netmask to the client, format: "R<Service ID> <Netmask 1> <Netmask 2> <Netmask 3> <Netmask 4>"
            // e.g. "R102 255 240 0 0"
            api_printf("R%d %d %d %d %d\r\n", SETTING_ID_NETMASK,
                        settings.netmask_0, settings.netmask_1,
                        settings.netmask_2, settings.netmask_3);
        }
        else if (commandLine.type == 'W')
        {
            uint8_t netmask[4] = {0};
            token_t* token = commandLine.token;

            // copy netmask from token to buffer
            API_COPY_TOKEN_DATA_TO_BUFFER(token, netmask, 4);

            // update the netmask in the settings
            settings.netmask_0 = netmask[0];
            settings.netmask_1 = netmask[1];
            settings.netmask_2 = netmask[2];
            settings.netmask_3 = netmask[3];

            // save netmask in flash
            if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_NETMASK_FAILED;
            }
            else API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SETTING_ID_GATEWAY:
        if (commandLine.type == 'R')
        {
            // send the gateway to the client, format: "R<Service ID> <Gateway 1> <Gateway 2> <Gateway 3> <Gateway 4>"
            // e.g. "R103 172 16 0 1"
            api_printf("R%d %d %d %d %d\r\n", SETTING_ID_GATEWAY,
                        settings.gateway_0, settings.gateway_1,
                        settings.gateway_2, settings.gateway_3);
        }
        else if (commandLine.type == 'W')
        {
            uint8_t gateway[4] = {0};
            token_t* token = commandLine.token;

            // copy gateway from token to buffer
            API_COPY_TOKEN_DATA_TO_BUFFER(token, gateway, 4);

            // update the gateway in the settings
            settings.gateway_0 = gateway[0];
            settings.gateway_1 = gateway[1];
            settings.gateway_2 = gateway[2];
            settings.gateway_3 = gateway[3];

            // save gateway in flash
            if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_GATEWAY_FAILED;
            }
            else API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SETTING_ID_MAC_ADDRESS:
        if (commandLine.type == 'R')
        {
            // send the MAC address to the client, format: "R<Service ID> <MAC Addr 1> <MAC Addr 2> <MAC Addr 3> <MAC Addr 4> <MAC Addr 5> <MAC Addr 6>"
            // e.g. "R104 0 0 0 0 0 0"
            api_printf("R%d %d %d %d %d %d %d\r\n", SETTING_ID_MAC_ADDRESS,
                        settings.mac_address_0, settings.mac_address_1,
                        settings.mac_address_2, settings.mac_address_3,
                        settings.mac_address_4, settings.mac_address_5);
        }
        else if (commandLine.type == 'W')
        {
            uint8_t mac_address[6] = {0};
            token_t* token = commandLine.token;

            // copy MAC address from token to buffer
            API_COPY_TOKEN_DATA_TO_BUFFER(token, mac_address, 6);

            // update the MAC address in the settings
            settings.mac_address_0 = mac_address[0];
            settings.mac_address_1 = mac_address[1];
            settings.mac_address_2 = mac_address[2];
            settings.mac_address_3 = mac_address[3];
            settings.mac_address_4 = mac_address[4];
            settings.mac_address_5 = mac_address[5];

            // save MAC address in flash
            if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_MAC_ADDRESS_FAILED;
            }
            else API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SETTING_ID_TCP_PORT:
        if (commandLine.type == 'R')
        {
            // send the Ethernet port to the client, format: "R<Service ID> <Port>"
            // e.g. "R105 80"
            api_printf("R%d %d\r\n", SETTING_ID_TCP_PORT, settings.tcp_port);
        }
        else if (commandLine.type == 'W')
        {
            // check if token is valid
            if (commandLine.token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            token_t* token = commandLine.token;

            // write the Ethernet port
            settings.tcp_port = (uint16_t)(token->i32);

            // write the Ethernet port
            if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_IP_FAILED; /////////////////////////////////////////////
            }
            else API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    case SETTING_ID_BAUD_RATE:
        // assert if variant is valid
        if (commandLine.variant >= UART_MAX)
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_VARIANT;
            break;
        }

        if (commandLine.type == 'R')
        {
            // send the baud rate to the client, format: "R<Service ID> <Baud Rate>"
            // e.g. "R106.x 115200"
            api_printf("R%d.%d %d\r\n", SETTING_ID_BAUD_RATE, commandLine.variant,
                        settings.uart[commandLine.variant].baudrate);
        }
        else if (commandLine.type == 'W')
        {
            // check if token is valid
            if (commandLine.token == NULL)
            {
                error_code = API_ERROR_CODE_INVALID_COMMAND_PARAMETER;
                break;
            }

            token_t* token = commandLine.token;

            // write the baud rate
            settings.uart[commandLine.variant].baudrate = (uint32_t)(token->i32);

            // write the baud rate
            if (flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t*)&settings, sizeof(settings_t)) != STATUS_OK)
            {
                error_code = API_ERROR_CODE_UPDATE_IP_FAILED; /////////////////////////////////////////////
            }
            else API_DEFAULT_RESPONSE();
        }
        else
        {
            error_code = API_ERROR_CODE_INVALID_COMMAND_TYPE;
        }
        break;
    default:
    {
        // debug print the command
        vLoggingPrintf("Command: %c.%d \r\n", commandLine.type, commandLine.id);
        api_printf("%c%d", commandLine.type, commandLine.id);
        if (commandLine.variant > 0)
        {
            vLoggingPrintf("Variant: %d \r\n", commandLine.variant);
            api_printf(".%d", commandLine.variant);
        }
        api_printf("\r\n");

        // print the parameters
        token_t* token = commandLine.token;
        while (token != NULL)
        {
            if (token->type == TOKEN_TYPE_LENGTH)
            {
                vLoggingPrintf("Length: %d \r\n", token->i32);
            }
            else
            {
                if (token->value_type == PARAM_TYPE_INT32)
                {
                    vLoggingPrintf("Param: %d \r\n", token->i32);
                }
                else if (token->value_type == PARAM_TYPE_FLOAT)
                {
                    vLoggingPrintf("Param: %f \r\n", token->f);
                }
                else // PARAM_TYPE_ANY
                {
                    vLoggingPrintf("Param: %s \r\n", (char*)token->any);
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
        api_error(error_code);
    }
}

io_status_t api_printf(const char *format_string, ...)
{
    char buffer[API_TX_BUFFER_SIZE] = {'\0'};
    va_list args;
    va_start(args, format_string);
    vsnprintf(buffer, API_TX_BUFFER_SIZE, format_string, args);
    va_end(args);

    // append data to tx ring buffer
    if (api_append_to_tx_ring_buffer_until_term(buffer, '\0') != STATUS_OK)
    {
        // error handling
        vLoggingPrintf("Error: Append to TX buffer failed\n");
    }

    return STATUS_OK;
}

void api_error(uint16_t error_code)
{
    // print error code
    api_printf(ERROR_CODE_FORMAT, error_code);
    vLoggingPrintf("Error: %d\n", error_code);
}