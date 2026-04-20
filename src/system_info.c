#include <app_version.h>
#include <zephyr/kernel.h>
#include "stm32f7xx_remote_io.h"
#include "system_info.h"
#include "uart.h"

static system_status_t system_status = SYSTEM_STATUS_CHECKING_FOR_UPDATE; // default status
// Implement semaphore for system status
K_SEM_DEFINE(system_status_sem, 1, 1);

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

system_status_t system_info_get_status(void) {
    k_sem_take(&system_status_sem, K_FOREVER);
    system_status_t status = system_status;
    k_sem_give(&system_status_sem);
    return status;
}

void system_info_set_status(system_status_t status) {
    k_sem_take(&system_status_sem, K_FOREVER);
    system_status = status;
    k_sem_give(&system_status_sem);
}

void system_info_status_code_to_string(system_status_t status, char *buffer, size_t buffer_size) {
    const char *status_str;

    switch (status) {
        case SYSTEM_STATUS_OK:
            status_str = "OK";
            break;
        case SYSTEM_STATUS_ERROR:
            status_str = "ERROR";
            break;
        case SYSTEM_STATUS_CHECKING_FOR_UPDATE:
            status_str = "CHECKING_FOR_UPDATE";
            break;
        case SYSTEM_STATUS_UPDATE_AVAILABLE:
            status_str = "UPDATE_AVAILABLE";
            break;
        case SYSTEM_STATUS_UPDATING:
            status_str = "UPDATING";
            break;
        case SYSTEM_STATUS_MENDER_DOWNLOADING:
            status_str = "MENDER_DOWNLOADING";
            break;
        case SYSTEM_STATUS_MENDER_INSTALLING:
            status_str = "MENDER_INSTALLING";
            break;
        case SYSTEM_STATUS_MENDER_REBOOTING:
            status_str = "MENDER_REBOOTING";
            break;
        default:
            status_str = "UNKNOWN";
    }

    snprintf(buffer, buffer_size, "%s", status_str);
}