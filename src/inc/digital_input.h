#ifndef __DIGITAL_INPUT_H
#define __DIGITAL_INPUT_H

#include "stm32f7xx_remote_io.h"

/* Type definition */
typedef struct DigitalInput
{
    uint16_t pin;
    GPIO_TypeDef* port;
    bool state;
} digital_input_t;

typedef void (*digital_input_callback_fn_t)(void *user_data, ...);

/* Macros */
#define CREATE_DIGITAL_INPUT_INSTANCE(index) \
    static digital_input_t digitalInput##index = { \
        .pin = DIGITAL_INPUT_##index##_PIN, \
        .port = DIGITAL_INPUT_##index##_PORT, \
        .state = 0, \
    }

#define READ_DIGITAL_INPUT_STATE(index) HAL_GPIO_ReadPin(digitalInput##index.port, digitalInput##index.pin)

#define DIGITAL_INPUT_INSTANCE(index) digitalInput##index

/* public functions */
io_status_t digital_input_init();
bool digital_input_read(uint8_t index);
uint32_t digital_input_read_all();
void digital_input_subscribe(void *user_data, uint8_t index, digital_input_callback_fn_t callback);
void digital_input_unsubscribe(void *user_data, uint8_t index);
void digital_input_unsubscribe_all(void *user_data);
void digital_input_print_subscribed_inputs(void *user_data, digital_input_callback_fn_t callback);

#endif