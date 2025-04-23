#include "stm32f7xx_remote_io.h"

void system_info_print()
{
    // print firmware version
    api_printf("Firmware: %s\r\n", FW_VERSION);
    api_printf("\r\n");

    // print help
    api_printf("Commands:\r\n");
    api_printf("  Read: R<service_id> <param1> <param2> ... <paramN>\r\n");
    api_printf("  Write: W<service_id> <param1> <param2> ... <paramN>\r\n");
    api_printf("\r\n");

    // print system info
    api_printf("System Info:\r\n");
    // number of digital inputs
    api_printf("  Digital Inputs: %d\r\n", DIGITAL_INPUT_MAX-1);
    // number of digital outputs
    api_printf("  Digital Outputs: %d\r\n", DIGITAL_OUTPUT_MAX-1);
    // number of PWM WS28XX LED channels
    api_printf("  PWM WS28XX Channels: %d\r\n", PWM_WS28XX_LED_MAX-1);
    // number of UART channels
    api_printf("  UART Channels: %d\r\n", UART_MAX-1);
}