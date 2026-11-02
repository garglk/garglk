//
//  ringbuffer.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-03-04.
//

#include "ringbuffer.h"
#include "memory_allocation.h"
#include <assert.h>

// The hidden definition of our circular buffer structure
struct circular_buf_t {
    uint8_t *buffer;
    size_t head;
    size_t tail;
    size_t max; //of the buffer
    bool full;
};

static size_t circular_buf_size(cbuf_handle_t me)
{
    if (me->full) return me->max;
    if (me->head >= me->tail) return me->head - me->tail;
    return me->max + me->head - me->tail;
}

static void circular_buf_reset(cbuf_handle_t me)
{
    assert(me);

    me->head = 0;
    me->tail = 0;
    me->full = false;
}

// Return a pointer to a struct instance
cbuf_handle_t circular_buf_init(uint8_t *buffer, size_t size)
{
    assert(buffer && size);
    assert(size % 2 == 0); // XY-pair semantics require even buffer size

    cbuf_handle_t cbuf = MemAlloc(sizeof(circular_buf_t));

    cbuf->buffer = buffer;
    cbuf->max = size;
    circular_buf_reset(cbuf);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

void circular_buf_free(cbuf_handle_t me)
{
    assert(me);
    free(me);
}

bool circular_buf_empty(cbuf_handle_t me)
{
    assert(me);

    return (!me->full && (me->head == me->tail));
}

static void advance_head(cbuf_handle_t me)
{
    assert(me);

    if (me->full) {
        if (++(me->tail) == me->max) {
            me->tail = 0;
        }
    }

    if (++(me->head) == me->max) {
        me->head = 0;
    }
    me->full = (me->head == me->tail);
}

static void advance_tail(cbuf_handle_t me)
{
    assert(me);

    me->full = false;
    if (++(me->tail) == me->max) {
        me->tail = 0;
    }
}

int circular_buf_putXY(cbuf_handle_t me, uint8_t x, uint8_t y)
{
    assert(me && me->buffer);

    if (circular_buf_size(me) + 2 > me->max)
        return -1;

    me->buffer[me->head] = x;
    advance_head(me);
    me->buffer[me->head] = y;
    advance_head(me);
    return 0;
}

int circular_buf_getXY(cbuf_handle_t me, uint8_t *x, uint8_t *y)
{
    assert(me && x && y && me->buffer);

    if (circular_buf_size(me) < 2)
        return -1;

    *x = me->buffer[me->tail];
    advance_tail(me);
    *y = me->buffer[me->tail];
    advance_tail(me);
    return 0;
}
