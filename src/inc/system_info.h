#ifndef __SYSTEM_INFO_H
#define __SYSTEM_INFO_H

/**
 * @brief Enumeration for system status codes.
 */
typedef enum {
    SYSTEM_STATUS_OK = 0,
    SYSTEM_STATUS_ERROR = 1,
    // Add more status codes as needed
    SYSTEM_STATUS_CHECKING_FOR_UPDATE,
    SYSTEM_STATUS_UPDATE_AVAILABLE,
    SYSTEM_STATUS_UPDATING,
    SYSTEM_STATUS_MENDER_DOWNLOADING,
    SYSTEM_STATUS_MENDER_INSTALLING,
    SYSTEM_STATUS_MENDER_REBOOTING,
} system_status_t;


typedef void (*system_info_callback_fn_t)(void *user_data, ...);

/* Public functions */
void system_info_print(void *user_data, system_info_callback_fn_t cb);
system_status_t system_info_get_status(void);
void system_info_set_status(system_status_t status);

#endif