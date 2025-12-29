#include <app_version.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
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

/**
 * @brief Fetal error handler
 * @param reason Reason for the fatal error
 * @param esf    Exception context. May be NULL.
 * @return This function does not return
 */
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf) {
    LOG_ERR("Fatal error occurred! reason: %u, esf: %p", reason, esf);
    LOG_ERR("System will reboot now.");
    LOG_PANIC();
    sys_reboot(SYS_REBOOT_WARM);
    while (1) {
        /* Wait for reboot */
    }
}