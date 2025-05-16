#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uart_interface, LOG_LEVEL_DBG);

#include <zephyr/drivers/uart.h>
#include <zephyr/sys/util_macro.h>

#include "stm32f7xx_remote_io.h"
#include "settings.h"


/* Type definition */
typedef struct Listener
{
    struct Listener *next;
    uart_listen_callback_t cb;
    void *user_data;
} listener_t;

/* Function Prototypes */
void uart_process_rx(void *parameters);

/* UART Device Objects */
// change and append the UART device names according to your board
static const struct device *const uart_dev[UART_MAX] = {
    DEVICE_DT_GET(DT_NODELABEL(usart2)),
    DEVICE_DT_GET(DT_NODELABEL(uart5)),
};

// head of the listener linked list
static listener_t *headListener[UART_MAX] = { NULL };

// mutex lock
static K_MUTEX_DEFINE(uartLock);

utils_ring_buffer_t rxBuffer[UART_MAX]; // ring buffer for UART RX
static uart_context_t uartContext[UART_MAX]; // UART context

// create a thread for processing RX data
K_KERNEL_THREAD_DEFINE(uart_process_rx_task, CONFIG_REMOTEIO_MINIMAL_STACK_SIZE * 2,
                       uart_process_rx, NULL, NULL, NULL,
                       CONFIG_REMOTEIO_SERVICE_PRIORITY + 1, 0, 500);

// semaphore for new line detected
K_SEM_DEFINE(rxNewLineSem, 0, 10);

static void uart_callback(const struct device *dev, void *user_data)
{
    // start processing interrupts in isr
    if (!uart_irq_update(dev))
    {
        return;
    }
    // check if RX buffer has a char
    if (!uart_irq_rx_ready(dev))
    {
        return;
    }

    // get uart context
    uart_context_t *uartCtx = (uart_context_t *)user_data;
    // read the data from rx buffer until the fifo is empty
    char c;
    while (uart_fifo_read(dev, &c, 1) == 1)
    {
        // check if the character is a noise
        if (c == 0 && (uartCtx->events & UART_EVENT_START_RCV) == 0)
            continue;
        else // set the start receive event
            uartCtx->events |= UART_EVENT_START_RCV;

        // check if the character is a new line
        if (c == '\r' || c == '\n')
        {
            // set the new line event
            uartCtx->events |= UART_EVENT_RX_NEW_LINE;
            // reset the start receive event to stop pushing data into buffer
            uartCtx->events &= ~UART_EVENT_START_RCV;
            k_sem_give(&rxNewLineSem);
        }
        // append the character to the rx buffer
        utils_append_to_buffer(uartCtx->rx_buffer, &c, 1);
    }
}

// initialize UART
void uart_init()
{
    // wait for settings to be loaded
    k_event_wait(&settingsLoadedEvent, SETTINGS_LOADED_EVENT, false, K_FOREVER);

    int ret;
    struct uart_config uart_cfg;
    // configure UART devices
    for (uint8_t i = 0; i < UART_MAX; i++)
    {
        // get current uart config
        if ((ret = uart_config_get(uart_dev[i], &uart_cfg)) < 0)
        {
            LOG_ERR("Failed to get UART config for UART%d: %d\n", i, ret);
            return;
        }
        // set UART parameters according to settings
        uart_cfg.baudrate = settings.uart[i].baudrate;
        uart_cfg.data_bits = settings.uart[i].data_bits;
        uart_cfg.stop_bits = settings.uart[i].stop_bits;
        uart_cfg.parity = settings.uart[i].parity;
        uart_cfg.flow_ctrl = settings.uart[i].flow_control;
        // set UART config
        if ((ret = uart_configure(uart_dev[i], &uart_cfg)) < 0)
        {
            LOG_ERR("Failed to configure UART%d: %d\n", i, ret);
            return;
        }
        // set uart irq and callback to receive data
        if ((ret =uart_irq_callback_user_data_set(uart_dev[i], uart_callback, &uartContext[i])) < 0)
        {
            LOG_ERR("Failed to set UART callback for UART%d: %d\n", i, ret);
            return;
        }
        // enable UART RX interrupt
        uart_irq_rx_enable(uart_dev[i]);

        // intialize uart context
        uartContext[i].rx_buffer = &rxBuffer[i];
        uartContext[i].events = 0;
    }
}

void uart_process_rx(void *parameters)
{
    uart_context_t uartCtx;

    for (;;)
    {
        // wait for new line event
        k_sem_take(&rxNewLineSem, K_FOREVER);

        // lock the mutex in case the listeners are being modified
        k_mutex_lock(&uartLock, K_FOREVER);

        // iterate over all uart contexts
        for (uint8_t i = 0; i < UART_MAX; i++)
        {
            uartCtx = uartContext[i];
            // check if there is a new line in the rx buffer
            if ((uartCtx.events & UART_EVENT_RX_NEW_LINE) == 0) continue;

            // reset the new line event
            uartCtx.events &= ~UART_EVENT_RX_NEW_LINE;

            // process the rx buffer
            // tx buffer
            char txBuffer[UART_TX_BUFFER_SIZE] = {'\0'};
            uint8_t txBuffIndex = 0;
            char c;
            while (utils_pop_from_buffer(uartCtx.rx_buffer, &c) == STATUS_OK)
            {
                // check if the character is a new line
                if (c == '\r' || c == '\n')
                {
                    if (txBuffIndex == 0) continue; // skip empty lines
                    
                    // execute the callback function
                    listener_t *current = headListener[i];
                    while (current != NULL)
                    {
                        // check if the callback is not NULL
                        if (current->cb == NULL)
                        {
                            LOG_ERR("Callback is NULL\n");
                            break;
                        }
                        current->cb(current->user_data, txBuffer, txBuffIndex, i);
                        current = current->next;
                    }
                    // reset the tx buffer
                    txBuffIndex = 0;
                }
                else
                {
                    if ((txBuffIndex+1) <= UART_TX_BUFFER_SIZE)
                        txBuffer[txBuffIndex++] = c;
                }
            }
        }

        // unlock the mutex
        k_mutex_unlock(&uartLock);
    }
}

int uart_printf(uart_index_t uart_index, const uint8_t *data, uint8_t len)
{
    // assert if uart index is valid
    if (uart_index >= UART_MAX)
    {
        return STATUS_ERROR;
    }

    for (uint8_t i = 0; i < len; i++)
    {
        uart_poll_out(uart_dev[uart_index], data[i]);
    }

    LOG_DBG("UART%d: %s\n", uart_index, data);

    return STATUS_OK;
}

int uart_listener_callback_set(uart_index_t uart_index, uart_listen_callback_t callback, void *user_data)
{
    // assert if uart index is valid
    if (uart_index >= UART_MAX)
    {
        return -1;
    }

    // check if the listener is already registered
    listener_t *current = headListener[uart_index];
    while (current != NULL)
    {
        if (current->cb == callback && current->user_data == user_data)
        {
            // listener is already registered
            return 0;
        }
        current = current->next;
    }

    // add the callback to the list of listeners
    listener_t *new_listener = (listener_t *)malloc(sizeof(listener_t));
    if (new_listener == NULL)
    {
        LOG_ERR("Failed to allocate memory for listener\n");
        return -1;
    }
    new_listener->cb = callback;
    new_listener->user_data = user_data;
    new_listener->next = NULL;
    // add the listener to the head of the list
    utils_append_node((utils_node_t *)new_listener, (utils_node_t *)&headListener[uart_index]);

    return 0;
}

int uart_listener_callback_remove(uart_index_t uart_index, uart_listen_callback_t callback, void *user_data)
{
    // assert if uart index is valid
    if (uart_index >= UART_MAX)
    {
        return -1;
    }

    // lock the mutex
    k_mutex_lock(&uartLock, K_FOREVER);
    int ret = -1;
    // if callback is not NULL, remove the callback from the list of listeners.
    // if callback is NULL, remove all listeners.
    listener_t *current = headListener[uart_index];
    listener_t *prev = NULL;
    while (current != NULL)
    {
        if ((callback == NULL || current->cb == callback)
            && current->user_data == user_data)
        {
            if (prev == NULL)
            {
                headListener[uart_index] = current->next;
            }
            else
            {
                prev->next = current->next;
            }
            free(current);
            ret = 0;
            goto exit;
        }
        prev = current;
        current = current->next;
    }

exit:
    // unlock the mutex
    k_mutex_unlock(&uartLock);
    return ret;
}

/**
 * @brief Remove a user listener from the UART listener list
 *        according to the user data.
 * * @param uart_index: UART index
 * * @param user_data: user data
 * @return 0 on success, -1 on failure
 */
int uart_user_listener_remove(uart_index_t uart_index, void *user_data)
{
    // assert if uart index is valid
    if (uart_index >= UART_MAX)
    {
        return -1;
    }

    for (uint8_t i = 0; i < UART_MAX; i++)
    {
        if (headListener[i] == NULL)
        {
            continue;
        }

        if (uart_listener_callback_remove(i, NULL, user_data) != 0)
        {
            LOG_ERR("Failed to remove user listener for UART%d\n", i);
            return -1;
        }
        LOG_INF("Removed user listener for UART%d\n", i);
    }

    return 0;
}