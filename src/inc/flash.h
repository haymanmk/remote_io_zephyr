#ifndef __FLASH_H
#define __FLASH_H

#include "stm32f7xx_remote_io.h"

// please refer to the reference manual for the flash sector
#define FLASH_SECTOR_SETTINGS FLASH_SECTOR_11
#define FLASH_SECTOR_SETTINGS_BASE_ADDRESS 0x081C0000UL
#define FLASH_SECTOR_SETTINGS_END_ADDRESS 0x081FFFFFUL

/* Public functions */
io_status_t flash_write_data_with_checksum(uint32_t sector, uint8_t *data, size_t length);
io_status_t flash_read_data_with_checksum(uint32_t sector, uint8_t *data, size_t length);

#endif