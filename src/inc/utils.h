#ifndef __UTILS_H
#define __UTILS_H

#include "stm32f7xx_remote_io.h"

/* Type definitions */
// Ring buffer
typedef struct UtilsRingBuffer {
    char *buffer; // buffer pointer
    uint8_t head; // head index
    uint8_t tail; // tail index
    uint8_t size; // buffer size
} utils_ring_buffer_t;

/* Macros */
#define UTILS_INCREMENT_BUFFER_HEAD(HEAD, TAIL, SIZE) \
    do { \
        /* overwrite old data */ \
        HEAD = (HEAD+1) % SIZE; \
        return STATUS_OK; \
    } while (0)

#define UTILS_INCREMENT_BUFFER_TAIL(TAIL, HEAD, SIZE) \
    do { \
        /* check if buffer tail is allowed to increment */ \
        if (TAIL != HEAD) { \
            TAIL = (TAIL+1) % SIZE; \
            return STATUS_OK; \
        } \
        return STATUS_FAIL; \
    } while (0)

// seach for the end of a string depending on a given terminator
#define UTILS_SEARCH_FOR_END_OF_STRING(str, index, terminator) \
    do { \
        while (str[index] != (terminator)) { \
            index++; \
        } \
    } while (0)

/* Function prototypes */
uint8_t utils_read_float(char *line, uint8_t *char_counter, float *float_ptr);
io_status_t utils_increment_buffer_head(utils_ring_buffer_t *buffer);
io_status_t utils_increment_buffer_tail(utils_ring_buffer_t *buffer);
io_status_t utils_is_buffer_empty(utils_ring_buffer_t *buffer);
io_status_t utils_is_buffer_full(utils_ring_buffer_t *buffer);
io_status_t utils_append_to_buffer(utils_ring_buffer_t *buffer, char *data, uint8_t len);
io_status_t utils_pop_from_buffer(utils_ring_buffer_t *buffer, char *data);

#endif