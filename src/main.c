#include <app_version.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

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
    // print current version
    LOG_INF("RemoteIO STM32F7xx - Version: %s", APP_VERSION_EXTENDED_STRING);

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