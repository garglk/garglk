//
//  ringbuffer.h
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-03-04.
//

#ifndef ringbuffer_h
#define ringbuffer_h

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Opaque circular buffer structure
typedef struct circular_buf_t circular_buf_t;
// Handle type, the way users interact with the API
typedef circular_buf_t* cbuf_handle_t;

/// Pass in a storage buffer and size
/// Returns a circular buffer handle
cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size);

/// Free a circular buffer structure.
/// Does not free data buffer; owner is responsible for that
void circular_buf_free(cbuf_handle_t me);

/// Reset the circular buffer to empty, head == tail
void circular_buf_reset(cbuf_handle_t me);

/// Rejects new data if the buffer is full
/// Returns 0 on success, -1 if buffer is full
int circular_buf_putXY(cbuf_handle_t me, uint8_t x, uint8_t y);

/// Retrieve a value from the buffer
/// Returns 0 on success, -1 if the buffer is empty
int circular_buf_getXY(cbuf_handle_t me, uint8_t *x,  uint8_t *y);

/// Returns true if the buffer is empty
bool circular_buf_empty(cbuf_handle_t me);

/// Returns true if the buffer is full
bool circular_buf_full(cbuf_handle_t me);

#endif /* ringbuffer_h */
