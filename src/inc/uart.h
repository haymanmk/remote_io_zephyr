#ifndef __UART_H
#define __UART_H

#define UART_RX_BUFFER_SIZE 64 
#define UART_TX_BUFFER_SIZE 64

// control flags for UART handle
#define UART_CTL_RECV_NONZERO (1 << 0)

/* Type definition */

// UART
// Please do not modify the enum values manually,
// it is related to the counting of the maximum number of UARTs
// and the index in UART handle array.
typedef enum {
    UART_1,
    UART_2,
    UART_MAX,
} uart_index_t;

typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
    uint8_t rx_head;
    uint8_t rx_tail;
    uint8_t is_new_line;
    uint8_t control_flags;
} uart_handle_t;

typedef struct UARTSettings
{
    uint32_t baudrate;
    uint32_t data_bits;
    uint32_t stop_bits;
    uint32_t parity;
    uint32_t flow_control; // under development
} uart_settings_t;

/* Macros */
// get uart handle from HAL uart handle
#define GET_UART_HANDLE(huart) \
    (huart == uartHandles[UART_1].huart) ? &uartHandles[UART_1] : \
    (huart == uartHandles[UART_2].huart) ? &uartHandles[UART_2] : NULL

// get uart handle from is_new_line flag
#define GET_UART_HANDLE_FROM_NEW_LINE() \
    (uartHandles[UART_1].is_new_line) ? &uartHandles[UART_1] : \
    (uartHandles[UART_2].is_new_line) ? &uartHandles[UART_2] : NULL

// get uart index from is_new_line flag
#define GET_UART_INDEX_FROM_NEW_LINE() \
    (uartHandles[UART_1].is_new_line) ? UART_1 : \
    (uartHandles[UART_2].is_new_line) ? UART_2 : UART_MAX

/* Function prototypes */
void uart_init();
void uart_msp_init(uart_index_t uart_index, UART_HandleTypeDef *huart, uart_settings_t *uart_settings);
int uart_printf(uart_index_t uart_index, const uint8_t *data, uint8_t len);

#endif