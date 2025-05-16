#ifndef __UART_H
#define __UART_H

#define UART_RX_BUFFER_SIZE 64 
#define UART_TX_BUFFER_SIZE 64

// Context events
#define UART_EVENT_START_RCV (1 << 0) // UART receive start event
#define UART_EVENT_RX_NEW_LINE (1 << 1) // RX new line event

/* Type definition */
typedef void (*uart_listen_callback_t)(void *user_data, void *data, uint8_t len, uint8_t uart_index);

typedef struct UartSettings {
    uint32_t baudrate; // baudrate
    uint8_t data_bits; // data bits
    uint8_t stop_bits; // stop bits
    uint8_t parity; // parity
    uint8_t flow_control; // flow control
} uart_settings_t;

// UART
// Please do not modify the enum values manually,
// it is related to the counting of the maximum number of UARTs
// and the index in UART handle array.
typedef enum {
    UART_1,
    UART_2,
    UART_MAX,
} uart_index_t;

typedef struct UartContext {
    utils_ring_buffer_t *rx_buffer; // RX buffer
    volatile uint8_t events; // events set in bit-wise
} uart_context_t;

/* Macros */

/* Function prototypes */
void uart_init();
int uart_printf(uart_index_t uart_index, const uint8_t *data, uint8_t len);
int uart_listener_callback_set(uart_index_t uart_index, uart_listen_callback_t callback, void *user_data);
int uart_listener_callback_remove(uart_index_t uart_index, uart_listen_callback_t callback, void *user_data);
int uart_user_listener_remove(uart_index_t uart_index, void *user_data);

#endif