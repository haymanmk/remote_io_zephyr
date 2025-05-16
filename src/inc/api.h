#ifndef __API_H
#define __API_H

#include "utils.h"

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
#define API_REMOVE_BLANK_SPACES(SERV) \
    do { \
        while (((SERV)->rx_buffer->head != (SERV)->rx_buffer->tail) \
                && (SERV)->rx_buffer->buffer[(SERV)->rx_buffer->tail] == ' ') { \
            api_increment_rx_buffer_tail_or_wait((SERV)->rx_buffer, (SERV)->event); \
        } \
    } while (0)

// wait until rx buffer is not empty
#define API_WAIT_UNTIL_BUFFER_NOT_EMPTY(SERV, EVENT_POINTER) \
    do { \
        while (utils_is_buffer_empty((SERV)->rx_buffer) == STATUS_OK) { \
            k_event_wait((EVENT_POINTER), (SERV)->event, true, K_FOREVER); \
        } \
    } while (0)

// default response to client
#define API_DEFAULT_RESPONSE(SERV, CMD_TYPE, CMD_ID) \
    do { \
        (SERV)->response_cb((SERV)->user_data, "%c%d OK\r\n", (CMD_TYPE), (CMD_ID)); \
    } while (0)


/* Type definition */
typedef void (*api_response_callback_t)(void *user_data, ...);

typedef struct APIServiceContext {
    struct UtilsRingBuffer *rx_buffer; // rx ring buffer
    uint32_t event; // event for receiving new data
    api_response_callback_t response_cb; // callback function for response
    void *user_data; // user data for callback function
} api_service_context_t;

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
    token_t *last_token; // last token, the end of the linked list
} command_line_t;

/* Function prototypes */
void api_init();
void api_task(void *p1, void *p2, void *p3);
void api_increment_rx_buffer_tail_or_wait(utils_ring_buffer_t *ring_buf, uint32_t event);

#endif