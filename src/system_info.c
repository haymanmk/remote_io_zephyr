#include <app_version.h>
#include "stm32f7xx_remote_io.h"
#include "system_info.h"
#include "uart.h"

void system_info_print(void *user_data, system_info_callback_fn_t cb)
{
    // print system info
    cb(user_data, "Firmware: %s\r\n", APP_VERSION_EXTENDED_STRING);
    cb(user_data, "Commands:\r\n");
    cb(user_data, "  Read: R<service_id> <param1> <param2> ... <paramN>\r\n");
    cb(user_data, "  Write: W<service_id> <param1> <param2> ... <paramN>\r\n\r\n");
    cb(user_data, "System Info:\r\n");
    cb(user_data, "  Digital Inputs: %d\r\n", DIGITAL_INPUT_MAX);
    cb(user_data, "  Digital Outputs: %d\r\n", DIGITAL_OUTPUT_MAX);
    cb(user_data, "  PWM WS28XX Channels: %d\r\n", PWM_WS28XX_LED_MAX-1);
    cb(user_data, "  UART Channels: %d\r\n", UART_MAX-1);
}