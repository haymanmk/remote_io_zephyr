#include "stm32f7xx_remote_io.h"

// Extracts a floating point value from a string. The following code is based loosely on
// the avr-libc strtod() function by Michael Stumpf and Dmitry Xmelkov and many freely
// available conversion method examples, but has been highly optimized for Grbl. For known
// CNC applications, the typical decimal value is expected to be in the range of E0 to E-4.
// Scientific notation is officially not supported by g-code, and the 'E' character may
// be a g-code word on some CNC systems. So, 'E' notation will not be recognized.
// NOTE: Thanks to Radu-Eosif Mihailescu for identifying the issues with using strtod().
uint8_t utils_read_float(char *line, uint8_t *char_counter, float *float_ptr)
{
	char *ptr = line + *char_counter;
	unsigned char c;

	// Grab first character and increment pointer. No spaces assumed in line.
	c = *ptr++;

	// Capture initial positive/minus character
	bool isnegative = false;
	if (c == '-') {
		isnegative = true;
		c = *ptr++;
	} else if (c == '+') {
		c = *ptr++;
	}

	// Extract number into fast integer. Track decimal in terms of exponent value.
	uint32_t intval = 0;
	int8_t exp = 0;
	uint8_t ndigit = 0;
	bool isdecimal = false;
	while (1) {
		c -= '0';
		if (c <= 9) {
			ndigit++;
			if (ndigit <= MAX_INT_DIGITS) {
				if (isdecimal) {
					exp--;
				}
				intval = (((intval << 2) + intval) << 1) + c; // intval*10 + c
			} else {
				if (!(isdecimal)) {
					exp++;
				} // Drop overflow digits
			}
		} else if (c == (('.' - '0') & 0xff) && !(isdecimal)) {
			isdecimal = true;
		} else {
			break;
		}
		c = *ptr++;
	}

	// Return if no digits have been read.
	if (!ndigit) {
		return (false);
	};

	// Convert integer into floating point.
	float fval;
	fval = (float)intval;

	// Apply decimal. Should perform no more than two floating point multiplications for the
	// expected range of E0 to E-4.
	if (fval != 0) {
		while (exp <= -2) {
			fval *= 0.01;
			exp += 2;
		}
		if (exp < 0) {
			fval *= 0.1;
		} else if (exp > 0) {
			do {
				fval *= 10.0;
			} while (--exp > 0);
		}
	}

	// Assign floating point value with correct sign.
	if (isnegative) {
		*float_ptr = -fval;
	} else {
		*float_ptr = fval;
	}

	*char_counter = ptr - line - 1; // Set char_counter to next statement

	return (true);
}

/* Functions to manipulate ring buffer */
// increment the head of the ring buffer
io_status_t utils_increment_buffer_head(utils_ring_buffer_t *buffer)
{
	if (buffer == NULL) {
		return STATUS_FAIL;
	}

	UTILS_INCREMENT_BUFFER_HEAD(buffer->head, buffer->tail, buffer->size);
}

// increment the tail of the ring buffer
io_status_t utils_increment_buffer_tail(utils_ring_buffer_t *buffer)
{
	if (buffer == NULL) {
		return STATUS_FAIL;
	}

	UTILS_INCREMENT_BUFFER_TAIL(buffer->tail, buffer->head, buffer->size);
}

// check if the ring buffer is empty
io_status_t utils_is_buffer_empty(utils_ring_buffer_t *buffer)
{
	if (buffer == NULL) {
		return STATUS_FAIL;
	}

	return (buffer->head == buffer->tail) ? STATUS_OK : STATUS_FAIL;
}

// check if the ring buffer is full
io_status_t utils_is_buffer_full(utils_ring_buffer_t *buffer)
{
	if (buffer == NULL) {
		return STATUS_FAIL;
	}

	return (((buffer->head + 1) % buffer->size) == buffer->tail) ? STATUS_OK : STATUS_FAIL;
}

// append data to the ring buffer
io_status_t utils_append_to_buffer(utils_ring_buffer_t *buffer, char *data, uint8_t len)
{
	if (buffer == NULL || data == NULL) {
		return STATUS_FAIL;
	}

	// check if the buffer is full
	if (utils_is_buffer_full(buffer) == STATUS_OK) {
		return STATUS_FAIL;
	}

	// append data to the buffer
    // note: if the data length is greater than the rest of the buffer size, then
    // new data will overwrite the old data.
	for (uint8_t i = 0; i < len; i++) {
		buffer->buffer[buffer->head] = data[i];
		utils_increment_buffer_head(buffer);
	}

	return STATUS_OK;
}

// pop data from the ring buffer
io_status_t utils_pop_from_buffer(utils_ring_buffer_t *buffer, char *data)
{
    if (buffer == NULL || data == NULL) {
        return STATUS_FAIL;
    }

    // check if the buffer is empty
    if (utils_is_buffer_empty(buffer) == STATUS_OK) {
        return STATUS_FAIL;
    }

    // pop data from the buffer
    *data = buffer->buffer[buffer->tail];
    utils_increment_buffer_tail(buffer);

    return STATUS_OK;
}
