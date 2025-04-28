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
#include "cpu_map.h"
#include "utils.h"
#include "freertos.h"
#include "flash.h"
#include "settings.h"
#include "ethernet_if.h"
#include "ws28xx_pwm.h"
#include "uart.h"
#include "digital_input.h"
#include "digital_output.h"
#include "system_info.h"
#include "api.h"
#include "uart.h"


/* Exported functions */

#endif