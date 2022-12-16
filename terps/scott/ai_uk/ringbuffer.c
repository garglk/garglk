//
//  ringbuffer.c
//  part of ScottFree, an interpreter for adventures in Scott Adams format
//
//  Created by Petter Sjölund on 2022-03-04.
//

#include <assert.h>
#include "ringbuffer.h"

// The hidden definition of our circular buffer structure
struct circular_buf_t {
    uint8_t * buffer;
    size_t head;
    size_t tail;
    size_t max; //of the buffer
    bool full;
};

// Return a pointer to a struct instance
cbuf_handle_t circular_buf_init(uint8_t* buffer, size_t size)
{
    assert(buffer && size);

    cbuf_handle_t cbuf = malloc(sizeof(circular_buf_t));
    assert(cbuf);

    cbuf->buffer = buffer;
    cbuf->max = size;
    circular_buf_reset(cbuf);

    assert(circular_buf_empty(cbuf));

    return cbuf;
}

void circular_buf_reset(cbuf_handle_t me)
{
    assert(me);

    me->head = 0;
    me->tail = 0;
    me->full = false;
}

void circular_buf_free(cbuf_handle_t me)
{
    assert(me);
    free(me);
}

bool circular_buf_full(cbuf_handle_t me)
{
    assert(me);

    return me->full;
}

bool circular_buf_empty(cbuf_handle_t me)
{
    assert(me);

    return (!me->full && (me->head == me->tail));
}


static void advance_pointer(cbuf_handle_t me)
{
    assert(me);

    if(me->full) {
        if (++(me->tail) == me->max) {
            me->tail = 0;
        }
    }

    if (++(me->head) == me->max) {
        me->head = 0;
    }
    me->full = (me->head == me->tail);
}

static void retreat_pointer(cbuf_handle_t me)
{
    assert(me);

    me->full = false;
    if (++(me->tail) == me->max) {
        me->tail = 0;
    }
}

int circular_buf_putXY(cbuf_handle_t me, uint8_t x, uint8_t y)
{
    int r = -1;

    assert(me && me->buffer);

    if(!circular_buf_full(me)) {
        me->buffer[me->head] = x;
        advance_pointer(me);
        if(!circular_buf_full(me)) {
            me->buffer[me->head] = y;
            advance_pointer(me);
            r = 0;
        }
    }
    return r;
}

int circular_buf_getXY(cbuf_handle_t me, uint8_t *x,  uint8_t *y)
{
    assert(me && x && y && me->buffer);

    int r = -1;

    if(!circular_buf_empty(me)) {
        *x = me->buffer[me->tail];
        retreat_pointer(me);
        if(!circular_buf_empty(me)) {
            *y = me->buffer[me->tail];
            retreat_pointer(me);
            r = 0;
        }
    }
    return r;
}
