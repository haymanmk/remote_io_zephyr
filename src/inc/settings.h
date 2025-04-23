#ifndef __SETTINGS_H
#define __SETTINGS_H

#include "stm32f7xx_remote_io.h"

// flags for instructing restoring function to restore settings
#define SETTINGS_RESTORE_DEFAULTS (1 << 0)

// Note: please modify the settings version
// whenever there is a change in the settings structure.
#define SETTINGS_VERSION 2

// type of settings
typedef struct EthernetSettings
{
    uint8_t ip_address_0;
    uint8_t ip_address_1;
    uint8_t ip_address_2;
    uint8_t ip_address_3;
    uint8_t netmask_0;
    uint8_t netmask_1;
    uint8_t netmask_2;
    uint8_t netmask_3;
    uint8_t gateway_0;
    uint8_t gateway_1;
    uint8_t gateway_2;
    uint8_t gateway_3;
    uint8_t mac_address_0;
    uint8_t mac_address_1;
    uint8_t mac_address_2;
    uint8_t mac_address_3;
    uint8_t mac_address_4;
    uint8_t mac_address_5;
    uint8_t tcp_port;
} ethernet_settings_t;

typedef struct UARTSettings
{
    uint32_t baudrate;
    uint32_t data_bits;
    uint32_t stop_bits;
    uint32_t parity;
    uint32_t flow_control; // under development
} uart_settings_t;

typedef struct PWMWS288XX_Settings
{
    uint8_t number_of_leds;
} pwmws288xx_settings_t;

typedef struct
{
    uint8_t settings_version;
    // settings for ethernet
    uint8_t ip_address_0;
    uint8_t ip_address_1;
    uint8_t ip_address_2;
    uint8_t ip_address_3;
    uint8_t netmask_0;
    uint8_t netmask_1;
    uint8_t netmask_2;
    uint8_t netmask_3;
    uint8_t gateway_0;
    uint8_t gateway_1;
    uint8_t gateway_2;
    uint8_t gateway_3;
    uint8_t mac_address_0;
    uint8_t mac_address_1;
    uint8_t mac_address_2;
    uint8_t mac_address_3;
    uint8_t mac_address_4;
    uint8_t mac_address_5;
    uint8_t tcp_port;
    // settings for uart channels
    uart_settings_t uart[UART_MAX];
    // settings for pwmws288xx at channel 1
    pwmws288xx_settings_t pwmws288xx_1;
} settings_t;

extern settings_t settings;

/* Export functions */
void settings_init();
io_status_t settings_save();

#endif