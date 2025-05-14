#ifndef __FLASH_H
#define __FLASH_H

#include <zephyr/storage/flash_map.h>
#include "stm32f7xx_remote_io.h"

extern const struct flash_area *storage_0;

// please refer to the reference manual for the flash sector
#define FLASH_SECTOR_SETTINGS   storage_0

// flash area ready event
#define FLASH_AREA_READY_EVENT  (1 << 0)

/* Type definition */


/* Public functions */
io_status_t flash_init(void);
io_status_t flash_write_data_with_checksum(const struct flash_area *sector, uint8_t *data, size_t length);
io_status_t flash_read_data_with_checksum(const struct flash_area *sector, uint8_t *data, size_t length);

#endif