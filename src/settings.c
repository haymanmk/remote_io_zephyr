#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(settings, LOG_LEVEL_DBG);

#include <zephyr/net/net_if.h>

#include "stm32f7xx_remote_io.h"
#include "flash.h"
#include "settings.h"

// create a event to signal when the settings are loaded
K_EVENT_DEFINE(settingsLoadedEvent);

// listen to the event when flash area is ready
extern struct k_event flashAreaReadyEvent;

settings_t settings;

const settings_t defaults = {
    .settings_version = SETTINGS_VERSION,
    .ip_address_0 = 192,
    .ip_address_1 = 168,
    .ip_address_2 = 1,
    .ip_address_3 = 10,
    .netmask_0 = 255,
    .netmask_1 = 255,
    .netmask_2 = 255,
    .netmask_3 = 0,
    .gateway_0 = 192,
    .gateway_1 = 168,
    .gateway_2 = 0,
    .gateway_3 = 1,
    .mac_address_0 = 0x00,
    .mac_address_1 = 0x05,
    .mac_address_2 = 0x4F,
    .mac_address_3 = 0x01,
    .mac_address_4 = 0x02,
    .mac_address_5 = 0x03,
    .tcp_port = 0, // this value will be added to 8500 as the final tcp port, i.e. 8500 + tcp_port
    .uart = {
        {
            .baudrate = 115200,
            .data_bits = UART_WORDLENGTH_8B,
            .stop_bits = UART_STOPBITS_1,
            .parity = UART_PARITY_NONE,
            .flow_control = 0,
        },
        {
            .baudrate = 9600,
            .data_bits = UART_WORDLENGTH_8B,
            .stop_bits = UART_STOPBITS_1,
            .parity = UART_PARITY_NONE,
            .flow_control = 0,
        },
    },
    .pwmws288xx_1 = {
        .number_of_leds = 25,
    },
};

/* Private functions */
void settings_restore(uint8_t restore_flag);
io_status_t settings_save();
io_status_t settings_load();

void settings_restore(uint8_t restore_flag)
{
    // get default network interface
    struct net_if *iface = net_if_get_default();
    if (iface == NULL)
    {
        LOG_ERR("No default network interface found");
        return;
    }
    // get default network interface link address
    struct net_linkaddr *link_addr = net_if_get_link_addr(iface);

    // restore default settings
    settings = defaults;

    // restore MAC address
    settings.mac_address_0 = link_addr->addr[0];
    settings.mac_address_1 = link_addr->addr[1];
    settings.mac_address_2 = link_addr->addr[2];
    settings.mac_address_3 = link_addr->addr[3];
    settings.mac_address_4 = link_addr->addr[4];
    settings.mac_address_5 = link_addr->addr[5];

    settings_save();
}

void settings_init()
{
    // wait for flash area to be ready
    k_event_wait(&flashAreaReadyEvent, FLASH_AREA_READY_EVENT, false, K_FOREVER);

    // load settings from flash
    if (settings_load() != STATUS_OK)
    {
        // if failed to load settings, restore defaults
        settings_restore(SETTINGS_RESTORE_DEFAULTS);
    }

    // signal that the settings are loaded
    k_event_post(&settingsLoadedEvent, SETTINGS_LOADED_EVENT);
}

io_status_t settings_save()
{
    return flash_write_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t *)&settings, sizeof(settings_t));
}

io_status_t settings_load()
{
    if (flash_read_data_with_checksum(FLASH_SECTOR_SETTINGS, (uint8_t *)&settings, sizeof(settings_t)) != STATUS_OK)
    {
        return STATUS_ERROR;
    }

    if (settings.settings_version != SETTINGS_VERSION)
    {
        return STATUS_ERROR;
    }

    return STATUS_OK;
}