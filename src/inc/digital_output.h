#ifndef __DIGITAL_OUTPUT_H
#define __DIGITAL_OUTPUT_H

#include "stm32f7xx_remote_io.h"

/* Type definition */
typedef struct DigitalOutput
{
    uint16_t pin;
    GPIO_TypeDef* port;
    bool state;
} digital_output_t;

/* Macros */
#define CREATE_DIGITAL_OUTPUT_INSTANCE(index) \
    static digital_output_t digitalOutput##index = { \
        .pin = DIGITAL_OUTPUT_##index##_PIN, \
        .port = DIGITAL_OUTPUT_##index##_PORT, \
        .state = 0, \
    }

#define DIGITAL_OUTPUT_INSTANCE(index) digitalOutput##index

/* Public functions */
void digital_output_init();
bool digital_output_read(uint8_t index);
uint32_t digital_output_read_all();
void digital_output_write(uint8_t index, bool state);
void digital_output_write_multiple(uint32_t data, uint8_t start_index, uint8_t length);

#endif