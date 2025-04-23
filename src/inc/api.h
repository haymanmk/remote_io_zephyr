#ifndef __API_H
#define __API_H

#include "stm32f7xx_remote_io.h"

#define MAX_INT_DIGITS 10 // please take into consideration the maximum number of digits for int32_t

#define API_RX_BUFFER_SIZE 128 // 1 ~ 254
#define API_TX_BUFFER_SIZE 128 // 1 ~ 254
#define API_ERROR_BUFFER_SIZE API_TX_BUFFER_SIZE // 1 ~ 254

// Service ID
#define SERVICE_ID_STATUS 1
#define SERVICE_ID_SYSTEM_INFO 2
#define SERVICE_ID_INPUT 3
#define SERVICE_ID_OUTPUT 4
#define SERVICE_ID_SUBSCRIBE_INPUT 5
#define SERVICE_ID_UNSUBSCRIBE_INPUT 6
#define SERVICE_ID_SERIAL 7
#define SERVICE_ID_PWM_WS28XX_LED 8
#define SERIVCE_ID_ANALOG_INPUT 9
#define SERVICE_ID_ANALOG_OUTPUT 10

// Setting ID
#define SETTING_ID_IP_ADDRESS 101
#define SETTING_ID_TCP_PORT 102
#define SETTING_ID_NETMASK 103
#define SETTING_ID_GATEWAY 104
#define SETTING_ID_MAC_ADDRESS 105
#define SETTING_ID_BAUD_RATE 106
#define SETTING_ID_DATA_BITS 107
#define SETTING_ID_PARITY 108
#define SETTING_ID_STOP_BITS 109
#define SETTING_ID_FLOW_CONTROL 110
#define SETTING_ID_NUMBER_OF_LEDS 111

enum {
    TOKEN_TYPE_PARAM = 1,
    TOKEN_TYPE_LENGTH,
};

enum {
    PARAM_TYPE_INT32 = 1,
    PARAM_TYPE_FLOAT,
    PARAM_TYPE_ANY,
};

/* Macros */
// remove all the blank spaces when processing the command
#define API_REMOVE_BLANK_SPACES() \
    do { \
        while (rxBufferHead != rxBufferTail && rxBuffer[rxBufferTail] == ' ') { \
            api_increment_rx_buffer_tail_or_wait(); \
        } \
    } while (0)

// wait until rx buffer is not empty
#define API_WAIT_UNTIL_BUFFER_NOT_EMPTY() \
    do { \
        while (api_is_rx_buffer_empty() == STATUS_OK) { \
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY); \
        } \
    } while (0)

// default response to client
#define API_DEFAULT_RESPONSE() \
    do { \
        api_printf("%c%d OK\r\n", commandLine.type, commandLine.id); \
    } while (0)


/* Type definition */
typedef struct Token {
    uint8_t type;
    uint8_t value_type;
    union
    {
        /* data */
        int32_t i32;
        float f;
        void * any;
    };
    struct Token *next;
} token_t;

typedef struct {
    char type; // 'R' for read, 'W' for write
    uint16_t id; // command id
    uint16_t variant; // command variant
    token_t *token; // parameters
} command_line_t;

/* Function prototypes */
void api_init();
io_status_t api_append_to_rx_ring_buffer(char *data, BaseType_t len);
io_status_t api_append_to_tx_ring_buffer(char *data, BaseType_t len);
io_status_t api_append_to_tx_ring_buffer_until_term(char *data, char terminator);
io_status_t api_increment_rx_buffer_head();
io_status_t api_increment_rx_buffer_tail();
void api_increment_rx_buffer_tail_or_wait();
io_status_t api_is_rx_buffer_empty();
io_status_t api_is_rx_buffer_full();
io_status_t api_increment_tx_buffer_head();
io_status_t api_increment_tx_buffer_tail();
char api_pop_tx_buffer();
io_status_t api_is_tx_buffer_empty();
io_status_t api_is_tx_buffer_full();
io_status_t api_printf(const char *format_string, ...);

#endif