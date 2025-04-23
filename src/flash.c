#include "stm32f7xx_remote_io.h"

HAL_StatusTypeDef flash_erase_sector(uint32_t sector, uint32_t num_sectors);


HAL_StatusTypeDef flash_erase_sector(uint32_t sector, uint32_t num_sectors)
{
    HAL_FLASH_Unlock();

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS); 

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = num_sectors;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t SectorError = 0;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK)
    {
        HAL_FLASH_Lock(); 
        return HAL_OK;
    }
    else
    {
        HAL_FLASH_Lock(); 
        return HAL_FLASH_GetError ();
    }
}

HAL_StatusTypeDef flash_put_byte(uint32_t address, uint8_t data)
{
    HAL_FLASH_Unlock();

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS); 

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address, data) == HAL_OK)
    {
        HAL_FLASH_Lock(); 
        return HAL_OK;
    }
    else
    {
        HAL_FLASH_Lock(); 
        return HAL_FLASH_GetError ();
    }
}

uint8_t flash_get_byte(uint32_t address)
{
    return *(__IO uint8_t *)address;
}

// The method to calculate checksum is referenced from GRBL.
io_status_t flash_write_data_with_checksum(uint32_t sector, uint8_t *data, size_t length)
{
    // get sector base address by sector number
    uint32_t address;
    switch (sector)
    {
        case FLASH_SECTOR_SETTINGS:
            address = FLASH_SECTOR_SETTINGS_BASE_ADDRESS;
            break;
        default:
            return STATUS_ERROR;
    }

    // earase the sector before writing data
    flash_erase_sector(sector, 1);

    uint8_t checksum = 0;
    for (; length > 0; length--)
    {
        checksum = (checksum << 1) | (checksum >> 7);
        checksum += *data;
        flash_put_byte(address++, *(data++));
    }
    flash_put_byte(address, checksum);

    return STATUS_OK;
}

/**
 * @brief Read data from flash memory with checksum
 * @param address The address to read data from
 * @param data The buffer to store the data
 * @param length The length of the data
 * @return STATUS_OK if the checksum is correct, otherwise STATUS_ERROR
 */
io_status_t flash_read_data_with_checksum(uint32_t sector, uint8_t *data, size_t length)
{
    // get the base address of the sector by sector number
    uint32_t address;
    switch (sector)
    {
        case FLASH_SECTOR_SETTINGS:
            address = FLASH_SECTOR_SETTINGS_BASE_ADDRESS;
            break;
        default:
            return STATUS_ERROR;
    }

    uint8_t checksum = 0;
    for (; length > 0; length--)
    {
        *data = flash_get_byte(address++);
        checksum = (checksum << 1) | (checksum >> 7);
        checksum += *data;
        data++;
    }
    return (checksum == flash_get_byte(address)) ? STATUS_OK : STATUS_ERROR;
}