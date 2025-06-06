#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/util_macro.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(flash_storage, LOG_LEVEL_DBG);

#include "stm32f7xx_remote_io.h"
#include "flash.h"

#define NUM_FLASH_AREAS 1 // Number of flash areas to bring up

/**
 * @brief The label for the flash area can be found in the device tree.
 */
// storage partition id
#define STORAGE_PARTITION_0_ID FIXED_PARTITION_ID(storage_partition_0)
// #define STORAGE_PARTITION_1_ID FIXED_PARTITION_ID(storage_partition_1)

/* Private function prototypes */
io_status_t flash_erase_sector(const struct flash_area *sector);

/* Variables */
// create a event to signal when the flash area is ready
K_EVENT_DEFINE(flashAreaReadyEvent);

// flash device reference - storage partition
const struct flash_area *storage_0 = NULL;
// const struct flash_area *storage_1 = NULL;

// used by listify
#define __BRING_UP_FLASH_AREA(id, _) \
    if (flash_area_open(STORAGE_PARTITION_##id##_ID, &storage_##id) != 0) \
    { \
        LOG_ERR("Failed to open flash area %d", id); \
        return STATUS_ERROR; \
    } \
    if (!flash_area_device_is_ready(storage_##id)) \
    { \
        LOG_ERR("Flash area %d device is not ready", id); \
        flash_area_close(storage_##id); \
        return STATUS_ERROR; \
    }
#define BRING_UP_FLASH_AREA(N) \
    do { \
        LISTIFY(N, __BRING_UP_FLASH_AREA, ( )) \
    } while (0)

// initialize the flash device
io_status_t flash_init(void)
{
    // // get the flash area
    // if (flash_area_open(STORAGE_PARTITION_ID, &storage_partition) != 0)
    // {
    //     LOG_ERR("Failed to open flash area");
    //     return STATUS_ERROR; // failed to open flash area
    // }

    // // check if the device is ready
    // if (!flash_area_device_is_ready(storage_partition))
    // {
    //     LOG_ERR("Flash area device is not ready");
    //     flash_area_close(storage_partition);
    //     return STATUS_ERROR; // flash area device is not ready
    // }
    BRING_UP_FLASH_AREA(NUM_FLASH_AREAS);

    // signal that the flash area is ready
    k_event_post(&flashAreaReadyEvent, FLASH_AREA_READY_EVENT);

    return STATUS_OK; // flash area device is ready
}

io_status_t flash_erase_sector(const struct flash_area *sector)
{
    int ret = 0;

    if (sector == NULL)
    {
        LOG_ERR("Flash area is NULL");
        return STATUS_ERROR; // invalid flash area
    }

    if ((ret = flash_area_erase(sector, 0, sector->fa_size)) < 0)
    {
        LOG_ERR("Failed to erase flash area %d: %d", sector->fa_id, ret);
        return STATUS_ERROR; // failed to erase flash area
    }

    return STATUS_OK; // flash area erased successfully
}

/**
 * @brief Write data to flash memory with checksum
 * @param sector The flash area to write data to
 * @param data The data to write
 * @param length The length of the data
 * @return STATUS_OK if the write is successful, otherwise STATUS_ERROR
 */
io_status_t flash_write_data_with_checksum(const struct flash_area *sector, uint8_t *data, size_t length)
{
    // earase the sector before writing data
    if (flash_erase_sector(sector) < 0)
    {
        LOG_ERR("Failed to erase flash area %d", sector->fa_id);
        return STATUS_ERROR; // failed to erase flash area
    }

    // compute checksum
    uint8_t checksum = 0;
    uint8_t *_data = data;
    uint8_t len = length;
    for (; len > 0; len--)
    {
        checksum = (checksum << 1) | (checksum >> 7);
        checksum += *_data;
        _data++;
    }
    if (flash_area_write(sector, 0, data, length) < 0)
    {
        LOG_ERR("Failed to write flash area %d", sector->fa_id);
        return STATUS_ERROR; // failed to write flash area
    }
    // write checksum to the last byte of the sector
    if (flash_area_write(sector, length, &checksum, sizeof(checksum)) < 0)
    {
        LOG_ERR("Failed to write checksum to flash area %d", sector->fa_id);
        return STATUS_ERROR; // failed to write checksum
    }

    return STATUS_OK;
}

/**
 * @brief Read data from flash memory with checksum
 * @param address The address to read data from
 * @param data The buffer to store the data
 * @param length The length of the data
 * @return STATUS_OK if the checksum is correct, otherwise STATUS_ERROR
 */
io_status_t flash_read_data_with_checksum(const struct flash_area *sector, uint8_t *data, size_t length)
{
    // read data from flash memory
    if (flash_area_read(sector, 0, data, length) < 0)
    {
        LOG_ERR("Failed to read flash area %d", sector->fa_id);
        return STATUS_ERROR; // failed to read flash area
    }
    // compute checksum
    uint8_t checksum = 0;
    uint8_t *_data = data;
    uint8_t len = length;
    for (; len > 0; len--)
    {
        checksum = (checksum << 1) | (checksum >> 7);
        checksum += *_data;
        _data++;
    }
    // read checksum from the last byte of the sector
    uint8_t checksum_from_flash = 0;
    if (flash_area_read(sector, length, &checksum_from_flash, sizeof(checksum_from_flash)) < 0)
    {
        LOG_ERR("Failed to read checksum from flash area %d", sector->fa_id);
        return STATUS_ERROR; // failed to read checksum
    }
    return (checksum == checksum_from_flash) ? STATUS_OK : STATUS_ERROR;
}