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
static mender_err_t __mender_storage_update_data_to_flash(const struct flash_area *fa, off_t offset, size_t len_size, const void *data, size_t data_length);

const struct flash_area *storage_partition;
static char *cached_artifact_name = NULL;

#ifdef CONFIG_MENDER_SEPARATE_HEAP
extern struct k_heap mender_heap;
#endif // CONFIG_MENDER_SEPARATE_HEAP

static void *
mender_storage_malloc(size_t size) {
#ifdef CONFIG_MENDER_SEPARATE_HEAP
    return k_heap_alloc(&mender_heap, size, K_NO_WAIT);
#else
    return k_malloc(size);
#endif
}

static void
mender_storage_free(void *ptr) {
#ifdef CONFIG_MENDER_SEPARATE_HEAP
    k_heap_free(&mender_heap, ptr);
#else
    k_free(ptr);
#endif
}

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
    if (MENDER_OK != (ret = __mender_storage_update_data_to_flash(storage_partition, MENDER_PRIVATE_KEY_OFFSET, LENGTH_SIZE, (void *)private_key, private_key_length))) {
        mender_log_error("Unable to write private key");
        return ret;
    }

    // write public key
    if (MENDER_OK != (ret = __mender_storage_update_data_to_flash(storage_partition, MENDER_PUBLIC_KEY_OFFSET, LENGTH_SIZE, (void *)public_key, public_key_length))) {
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

    mender_err_t ret;
    if (MENDER_OK != (ret = __mender_storage_update_data_to_flash(storage_partition, MENDER_DEPLOYMENT_DATA_OFFSET, LENGTH_SIZE, (void *)deployment_data, strlen(deployment_data)))) {
        mender_log_error("Unable to write deployment data");
        return ret;
    }

    return MENDER_OK;
}

mender_err_t
mender_storage_get_deployment_data(char **deployment_data) {
    assert(NULL != deployment_data);

    // read deployment data and its length from flash
    size_t deployment_data_length = 0;
    mender_err_t ret = __mender_storage_read_data_from_flash(storage_partition, MENDER_DEPLOYMENT_DATA_OFFSET, LENGTH_SIZE, (void **)deployment_data, &deployment_data_length);
    if (MENDER_OK != ret && MENDER_NOT_FOUND != ret) {
        mender_log_error("Unable to read deployment data");
        return ret;
    }

    return ret;
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
    if (MENDER_OK != __mender_storage_update_data_to_flash(storage_partition, MENDER_ARTIFACT_NAME_OFFSET, LENGTH_SIZE, (void *)artifact_name, strlen(artifact_name))) {
        mender_log_error("Unable to write artifact name");
        return MENDER_FAIL;
    }
    // free cached artifact name if it exists
    // because it has been stored in flash, we can safely free it to save memory
    if (cached_artifact_name) {
        mender_storage_free(cached_artifact_name);
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
            snprintf(__artifact_name_buf, sizeof(__artifact_name_buf), "%s_%s", CONFIG_USER_MENDER_ARTIFACT_NAME, APP_VERSION_EXTENDED_STRING);
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

static inline mender_err_t
__mender_storage_write_data_to_flash(const struct flash_area *fa, off_t offset, size_t len_size, const void *data, size_t data_length) {
    // if data length is zero, we don't need to write anything
    if (data_length == 0) return MENDER_OK;

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
        *data_length = 0; // set data length to 0
        return MENDER_NOT_FOUND;
    }
    // check if data length is valid. if data length is larger than the flash area size, return data not found
    if (*data_length > fa->fa_size - offset - len_size) {
        mender_log_error("Data length is larger than flash area size: %zu > %lu",
                         *data_length, fa->fa_size - offset - len_size);
        *data_length = 0; // set data length to 0
        return MENDER_NOT_FOUND;
    }
    // allocate memory for data
    if (NULL == (*data = mender_storage_malloc(*data_length))) {
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
        mender_storage_free(*data);
        *data = NULL;
        *data_length = 0; // set data length to 0
        return MENDER_NOT_FOUND;
    }

    return MENDER_OK;
}

static mender_err_t
__mender_storage_update_data_to_flash(const struct flash_area *fa, off_t offset, size_t len_size, const void *data, size_t data_length) {
    mender_err_t ret = MENDER_OK;

    // read all data from flash before erasing all flash area
    // get private key
    unsigned char *private_key = NULL;
    size_t private_key_length = 0;
    ret =__mender_storage_read_data_from_flash(fa, MENDER_PRIVATE_KEY_OFFSET, len_size, (void **)&private_key, &private_key_length);
    if (MENDER_NOT_FOUND == ret) {
        mender_log_info("Private key not found");
    }
    else if (MENDER_OK != ret) {
        mender_log_error("Unable to get private key");
        return ret;
    }

    // get public key
    unsigned char *public_key = NULL;
    size_t public_key_length = 0;
    ret = __mender_storage_read_data_from_flash(fa, MENDER_PUBLIC_KEY_OFFSET, len_size, (void **)&public_key, &public_key_length);
    if (MENDER_NOT_FOUND == ret) {
        mender_log_info("Public key not found");
    }
    else if (MENDER_OK != ret) {
        mender_log_error("Unable to get public key");
        goto exit;
    }

    // get deployment data
    char *deployment_data = NULL;
    size_t deployment_data_length = 0;
    ret = __mender_storage_read_data_from_flash(fa, MENDER_DEPLOYMENT_DATA_OFFSET, len_size, (void **)&deployment_data, &deployment_data_length);
    if (MENDER_NOT_FOUND == ret) {
        mender_log_info("Deployment data not found");
    }
    else if (MENDER_OK != ret) {
        mender_log_error("Unable to get deployment data");
        goto exit;
    }

    // get artifact name
    char *artifact_name = NULL;
    size_t artifact_name_length = 0;
    ret = __mender_storage_read_data_from_flash(fa, MENDER_ARTIFACT_NAME_OFFSET, len_size, (void **)&artifact_name, &artifact_name_length);
    if (MENDER_NOT_FOUND == ret) {
        mender_log_info("Artifact name not found");
    }
    else if (MENDER_OK != ret) {
        mender_log_error("Unable to get artifact name");
        goto exit;
    }

    // erase the entire flash area before writing new data
    if (flash_area_erase(fa, 0, fa->fa_size)) {
        mender_log_error("Unable to erase flash area");
        ret = MENDER_FAIL;
        goto exit;
    }

    // identify which data set to update according to the offset
    if (offset == MENDER_PRIVATE_KEY_OFFSET) {
        // free private key and assign new data
        if (private_key) mender_storage_free(private_key);
        private_key = mender_storage_malloc(data_length);
        if (NULL == private_key) {
            mender_log_error("Unable to allocate memory for private key");
            ret = MENDER_FAIL;
            goto exit;
        }
        memcpy(private_key, data, data_length);
        private_key_length = data_length;
    } else if (offset == MENDER_PUBLIC_KEY_OFFSET) {
        // free public key and assign new data
        if (public_key) mender_storage_free(public_key);
        public_key = mender_storage_malloc(data_length);
        if (NULL == public_key) {
            mender_log_error("Unable to allocate memory for public key");
            ret = MENDER_FAIL;
            goto exit;
        }
        memcpy(public_key, data, data_length);
        public_key_length = data_length;
    } else if (offset == MENDER_DEPLOYMENT_DATA_OFFSET) {
        // free deployment data and assign new data
        if (deployment_data) mender_storage_free(deployment_data);
        deployment_data = mender_utils_strdup((const char *)data);
        deployment_data_length = data_length;
    } else if (offset == MENDER_ARTIFACT_NAME_OFFSET) {
        // free artifact name and assign new data
        if (artifact_name) mender_storage_free(artifact_name);
        artifact_name = mender_utils_strdup((const char *)data);
        artifact_name_length = data_length;
    } else {
        mender_log_error("Unknown offset: %ld", offset);
        ret = MENDER_FAIL;
        goto exit;
    }

    // write data back to flash
    // write private key
    if (MENDER_OK != (ret = __mender_storage_write_data_to_flash(fa, MENDER_PRIVATE_KEY_OFFSET, len_size, private_key, private_key_length))) {
        mender_log_error("Unable to write private key");
        goto exit;
    }
    // write public key
    if (MENDER_OK != (ret = __mender_storage_write_data_to_flash(fa, MENDER_PUBLIC_KEY_OFFSET, len_size, public_key, public_key_length))) {
        mender_log_error("Unable to write public key");
        goto exit;
    }
    // write deployment data
    if (MENDER_OK != (ret = __mender_storage_write_data_to_flash(fa, MENDER_DEPLOYMENT_DATA_OFFSET, len_size, deployment_data, deployment_data_length))) {
        mender_log_error("Unable to write deployment data");
        goto exit;
    }
    // write artifact name
    if (MENDER_OK != (ret = __mender_storage_write_data_to_flash(fa, MENDER_ARTIFACT_NAME_OFFSET, len_size, artifact_name, artifact_name_length))) {
        mender_log_error("Unable to write artifact name");
        goto exit;
    }

exit:
    if (private_key) mender_storage_free(private_key);
    if (public_key) mender_storage_free(public_key);
    if (deployment_data) mender_storage_free(deployment_data);
    if (artifact_name) mender_storage_free(artifact_name);

    return ret;
}