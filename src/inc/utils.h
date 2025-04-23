#ifndef __UTILS_H
#define __UTILS_H

#define true 1
#define false 0

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

#endif