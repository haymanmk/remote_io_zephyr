#ifndef __DIGITAL_OUTPUT_H
#define __DIGITAL_OUTPUT_H

#include "stm32f7xx_remote_io.h"

/* Type definition */


/* Public functions */
void digital_output_init();
int digital_output_read(uint8_t index);
uint32_t digital_output_read_all();
int digital_output_write(uint8_t index, bool state);
int digital_output_write_multiple(uint32_t data, uint8_t start_index, uint8_t length);

#endif