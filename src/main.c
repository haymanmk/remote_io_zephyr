#include "ethernet_if.h"
#include "digital_input.h"
#include "digital_output.h"
#include "flash.h"
#include "settings.h"
#include "uart.h"
#ifndef CONFIG_REMOTEIO_USE_MY_WS28XX
    #include "ws28xx_led.h"
#else
    #include "ws28xx_pwm.h"
#endif

int main(void) {
    // initialize flash memory
    flash_init();

    // initialize settings
    settings_init();

    // initialize digital input
    digital_input_init();

    // initialize digital output
    digital_output_init();

    // initialize UART
    uart_init();

    // initialize WS28XX GPIO
    ws28xx_led_init();

    tcp_server_init();

    return 0;
}