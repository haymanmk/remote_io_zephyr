#ifndef __SYSTEM_INFO_H
#define __SYSTEM_INFO_H


typedef void (*system_info_callback_fn_t)(void *user_data, ...);

/* Public functions */
void system_info_print(void *user_data, system_info_callback_fn_t cb);

#endif