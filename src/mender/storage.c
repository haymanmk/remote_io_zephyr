/**
 * @file      storage.c
 * @brief     Mender storage interface for STM32F7 platform
 *
 * Copyright joelguittet and mender-mcu-client contributors
 * Copyright Northern.tech AS
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <app_version.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

#include <mender/log.h>
#include <mender/utils.h>
#include <mender/client.h>
#include <mender/inventory.h>

#include "storage.h"

#define LENGTH_SIZE 4
#define MENDER_STORAGE_DEPLOYMENT_DATA_LENGTH (192)

#define STORAGE_PARTITION_ID FIXED_PARTITION_ID(storage_partition_1)
#define STORAGE_PARTITION_OFFSET 0
#define MENDER_PRIVATE_KEY_OFFSET STORAGE_PARTITION_OFFSET
#define MENDER_PUBLIC_KEY_OFFSET (STORAGE_PARTITION_OFFSET + 2048)
#define MENDER_DEPLOYMENT_DATA_OFFSET (STORAGE_PARTITION_OFFSET + 4096)
#define MENDER_PROVIDES_OFFSET (STORAGE_PARTITION_OFFSET + 6144)
#define MENDER_ARTIFACT_NAME_OFFSET (STORAGE_PARTITION_OFFSET + 7168)

/* private function prototype */
static bool mender_storage_is_empty(unsigned char *data, size_t length);
static mender_err_t __mender_storage_write_data_to_flash(const struct flash_area *fa, off_t offset, size_t len_size, const void *data, size_t data_length);
static mender_err_t __mender_storage_read_data_from_flash(const struct flash_area *fa, off_t offset, size_t len_size, void **data, size_t *data_length);

const struct flash_area *storage_partition;
static char mender_storage_deployment_data[MENDER_STORAGE_DEPLOYMENT_DATA_LENGTH];
static char *cached_artifact_name = NULL;

mender_err_t
mender_storage_init(void) {

    /* Open storage partition and assign a pointer for it  */
    if (flash_area_open(STORAGE_PARTITION_ID, &storage_partition)) {
        mender_log_error("Unable to open storage partition");
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

mender_err_t
mender_storage_set_authentication_keys(unsigned char *private_key, size_t private_key_length, unsigned char *public_key, size_t public_key_length) {
    assert(NULL != private_key);
    assert(NULL != public_key);

    mender_err_t ret;

    // write private key
    if (MENDER_OK != (ret = __mender_storage_write_data_to_flash(storage_partition, MENDER_PRIVATE_KEY_OFFSET, LENGTH_SIZE, (void *)private_key, private_key_length))) {
        mender_log_error("Unable to write private key");
        return ret;
    }

    // write public key
    if (MENDER_OK != (ret = __mender_storage_write_data_to_flash(storage_partition, MENDER_PUBLIC_KEY_OFFSET, LENGTH_SIZE, (void *)public_key, public_key_length))) {
        mender_log_error("Unable to write public key");
        return ret;
    }

    return MENDER_OK;
}

mender_err_t
mender_storage_get_authentication_keys(unsigned char **private_key, size_t *private_key_length, unsigned char **public_key, size_t *public_key_length) {
    assert(NULL != private_key);
    assert(NULL != private_key_length);
    assert(NULL != public_key);
    assert(NULL != public_key_length);

    mender_err_t ret;
    // read private key and its length
    if (MENDER_OK != (ret = __mender_storage_read_data_from_flash(storage_partition, MENDER_PRIVATE_KEY_OFFSET, LENGTH_SIZE, (void **)private_key, private_key_length))) {
        mender_log_error("Unable to read private key");
        return MENDER_NOT_FOUND;
    }

    // read public key and its length
    if (MENDER_OK != (ret = __mender_storage_read_data_from_flash(storage_partition, MENDER_PUBLIC_KEY_OFFSET, LENGTH_SIZE, (void **)public_key, public_key_length))) {
        mender_log_error("Unable to read public key");
        return MENDER_NOT_FOUND;
    }

    return MENDER_OK;
}

mender_err_t
mender_storage_delete_authentication_keys(void) {

    /**
     * The STM32 flash memory is erased on a sector basis.
     * This means that when we want to delete the authentication keys,
     * we need to erase the entire sector that contains other data as well.
     */
    return MENDER_NOT_IMPLEMENTED;
}

mender_err_t
mender_storage_set_deployment_data(char *deployment_data) {
    assert(NULL != deployment_data);

    /* Retrieve length of the deployment data */
    size_t deployment_data_length = strlen(deployment_data);
    if (deployment_data_length >= MENDER_STORAGE_DEPLOYMENT_DATA_LENGTH) {
        mender_log_error("Unable to set deployment data");
        return MENDER_FAIL;
    }

    /* Copy deployment data */
    memcpy(mender_storage_deployment_data, deployment_data, deployment_data_length);
    memset(&mender_storage_deployment_data[deployment_data_length], '\0', MENDER_STORAGE_DEPLOYMENT_DATA_LENGTH - deployment_data_length);

    return MENDER_OK;
}

mender_err_t
mender_storage_get_deployment_data(char **deployment_data) {
    assert(NULL != deployment_data);

    /* Retrieve length of the deployment data */
    if (0 == strlen(mender_storage_deployment_data)) {
        mender_log_info("Deployment data not available");
        return MENDER_NOT_FOUND;
    }

    /* Read deployment data */
    if (NULL == (*deployment_data = strdup(mender_storage_deployment_data))) {
        mender_log_error("Unable to read deployment data");
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

mender_err_t
mender_storage_delete_deployment_data(void) {

    /**
     * The STM32 flash memory is erased on a sector basis.
     * This means that when we want to delete the authentication keys,
     * we need to erase the entire sector that contains other data as well.
     */
    return MENDER_NOT_IMPLEMENTED;
}

mender_err_t
mender_storage_set_artifact_name(const char *artifact_name) {
    assert(NULL != artifact_name);

    // write artifact name to flash
    if (MENDER_OK != __mender_storage_write_data_to_flash(storage_partition, MENDER_ARTIFACT_NAME_OFFSET, LENGTH_SIZE, (void *)artifact_name, strlen(artifact_name))) {
        mender_log_error("Unable to write artifact name");
        return MENDER_FAIL;
    }
    // free cached artifact name if it exists
    // because it has been stored in flash, we can safely free it to save memory
    if (cached_artifact_name) {
        free(cached_artifact_name);
        cached_artifact_name = NULL;
    }

    return MENDER_OK;
}

mender_err_t mender_storage_get_artifact_name(const char **artifact_name)
{
	assert(NULL != artifact_name);
	assert(NULL == *artifact_name); // prevent memory leak

	// return cached artifact name if it exists
	if (NULL != cached_artifact_name) {
		// return cached artifact name if it exists
		*artifact_name = cached_artifact_name;
		return MENDER_OK;
	}

	// read artifact name and its length from flash
	// it might be empty if no artifact name has been set
	size_t artifact_name_length = 0;
	mender_err_t ret = MENDER_OK;
	ret = __mender_storage_read_data_from_flash(storage_partition, MENDER_ARTIFACT_NAME_OFFSET,
						    LENGTH_SIZE, (void **)artifact_name,
						    &artifact_name_length);
	if (MENDER_OK !=
	    ret) { // not found or error. get artifact name from the build, if it exists
		const char *artifact_name_literal;
#ifdef CONFIG_USER_MENDER_ARTIFACT_NAME
        char __artifact_name_buf[64] = {0};
		if (strlen(CONFIG_USER_MENDER_ARTIFACT_NAME) > 0) {
            snprintf(__artifact_name_buf, sizeof(__artifact_name_buf), "%s_%s", CONFIG_USER_MENDER_ARTIFACT_NAME, APP_VERSION_STRING);
			artifact_name_literal = __artifact_name_buf; // use the artifact name defined in the build
		} else {
			artifact_name_literal = "unknown"; // no artifact name defined in the build
		}
#else  /* CONFIG_USER_MENDER_ARTIFACT_NAME */
		artifact_name_literal = "unknown"; // no artifact name defined in the build
#endif /* CONFIG_USER_MENDER_ARTIFACT_NAME */

		// copy artifact name literal to artifact_name
		if (NULL == (*artifact_name = strdup(artifact_name_literal))) {
			mender_log_error("Unable to allocate memory for artifact name");
			return MENDER_FAIL;
		}
		// cache artifact name for future use
		cached_artifact_name = (char *)*artifact_name; // cache the artifact name
        ret = MENDER_OK; // set return value to OK since we set the artifact name from the build
        mender_log_info("Artifact name not found in storage, using build artifact name: %s", *artifact_name);
	} else { // artifact name was read successfully
		// update cached artifact name
		cached_artifact_name = (char *)*artifact_name;
	}

	return ret;
}

mender_err_t
mender_storage_exit(void) {

    /* Nothing to do */
    return MENDER_OK;
}

/* check if memory space is empty by seeing if this space is filled with 0xff */
static bool
mender_storage_is_empty(unsigned char *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] != 0xFF) {
            return false;
        }
    }
    return true;
}

static mender_err_t
__mender_storage_write_data_to_flash(const struct flash_area *fa, off_t offset, size_t len_size, const void *data, size_t data_length) {
    // write the length of the data
    if (flash_area_write(fa, offset, &data_length, len_size)) {
        mender_log_error("Unable to write data length");
        return MENDER_FAIL;
    }
    // write the data
    if (flash_area_write(fa, offset + len_size, data, data_length)) {
        mender_log_error("Unable to write data");
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

/**
 * @brief Read data from flash area
 * @param fa Flash area to read from
 * @param offset Offset in the flash area to start reading from
 * @param len_size Number of bytes to read for the length of the data
 * @param data Pointer to the pointer where the data will be stored
 * @param data_length Pointer to the size of the data read
 * @return MENDER_OK on success, MENDER_FAIL on failure, MENDER_NOT_FOUND if data is not available
 * @note This function reads the first `len_size` bytes to get the length of the data,
 *       then allocates memory for the data and reads it from the flash area.
 *       If the data is empty (filled with 0xFF), it returns MENDER_NOT_FOUND.
 *       The caller is responsible for freeing the allocated memory for `data` after use.
 */
static mender_err_t
__mender_storage_read_data_from_flash(const struct flash_area *fa, off_t offset, size_t len_size, void **data, size_t *data_length) {
    // get the data size from the first len_size bytes
    if (flash_area_read(fa, offset, data_length, len_size)) {
        mender_log_error("Unable to read data length");
        return MENDER_FAIL;
    }
    // check if data is empty
    if (mender_storage_is_empty((unsigned char *)data_length, len_size) ||
        *data_length == 0) {
        mender_log_info("Data not available");
        return MENDER_NOT_FOUND;
    }
    // check if data length is valid. if data length is larger than the flash area size, return error
    if (*data_length > fa->fa_size - offset - len_size) {
        mender_log_error("Data length is larger than flash area size: %zu > %lu",
                         *data_length, fa->fa_size - offset - len_size);
        return MENDER_FAIL;
    }
    // allocate memory for data
    if (NULL == (*data = malloc(*data_length))) {
        mender_log_error("Unable to allocate memory");
        return MENDER_FAIL;
    }
    // read data
    if (flash_area_read(fa, offset + len_size, *data, *data_length)) {
        mender_log_error("Unable to read data");
        free(*data);
        *data = NULL;
        return MENDER_FAIL;
    }
    // check if data is empty
    if (mender_storage_is_empty(*data, *data_length)) {
        mender_log_info("Data not available");
        free(*data);
        *data = NULL;
        return MENDER_NOT_FOUND;
    }

    return MENDER_OK;
}