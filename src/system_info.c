#include "stm32f7xx_remote_io.h"
#include "system_info.h"
#include "uart.h"

void system_info_print(ethernet_if_socket_service_t *service)
{
    // print system info directly to the socket service to minimize stack usage
    ethernet_if_send(service, "Firmware: %s\r\n\r\n", FW_VERSION);
    ethernet_if_send(service, "Commands:\r\n");
    ethernet_if_send(service, "  Read: R<service_id> <param1> <param2> ... <paramN>\r\n");
    ethernet_if_send(service, "  Write: W<service_id> <param1> <param2> ... <paramN>\r\n\r\n");
    ethernet_if_send(service, "System Info:\r\n");
    ethernet_if_send(service, "  Digital Inputs: %d\r\n", DIGITAL_INPUT_MAX);
    ethernet_if_send(service, "  Digital Outputs: %d\r\n", DIGITAL_OUTPUT_MAX);
    ethernet_if_send(service, "  PWM WS28XX Channels: %d\r\n", PWM_WS28XX_LED_MAX-1);
    ethernet_if_send(service, "  UART Channels: %d\r\n", UART_MAX-1);
}