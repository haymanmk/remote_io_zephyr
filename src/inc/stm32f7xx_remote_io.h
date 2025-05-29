#ifndef __STM32F7XX_REMOTE_IO_H
#define __STM32F7XX_REMOTE_IO_H

#define FW_VERSION "1.0.0"

typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_FAIL = 2,
} io_status_t;

enum Channel {
    CHANNEL_1 = 1,
};


#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>

#include "main.h"
#include "error_code.h"
#include "utils.h"

#define MAX_INT_DIGITS 10 // please take into consideration the maximum number of digits for int32_t
#define DIGITAL_INPUT_MAX 16 // maximum number of digital inputs
#define DIGITAL_OUTPUT_MAX 16 // maximum number of digital outputs
#define PWM_WS28XX_LED_MAX 1

/* Exported functions */

#endif