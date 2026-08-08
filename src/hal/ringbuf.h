#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/*
 * Lock-free single-producer single-consumer ring buffer.
 *
 * Producer writes at head, consumer reads at tail.
 * No atomics needed when used as SPSC on same-core embedded:
 * head and tail are each written by exactly one side.
 *
 * Capacity must be a power of two.
 */

struct ringbuf {
    uint8_t *buf;
    size_t   mask;
    size_t   head; /* producer writes here */
    size_t   tail; /* consumer reads here  */
};

static inline int ringbuf_init(struct ringbuf *r, size_t capacity)
{
    if (capacity & (capacity - 1)) /* must be power of two */
        return -1;
    r->buf = malloc(capacity);
    if (!r->buf)
        return -1;
    r->mask = capacity - 1;
    r->head = 0;
    r->tail = 0;
    return 0;
}

static inline void ringbuf_destroy(struct ringbuf *r)
{
    free(r->buf);
    r->buf = NULL;
    r->mask = 0;
    r->head = 0;
    r->tail = 0;
}

static inline size_t ringbuf_used(const struct ringbuf *r)
{
    return r->head - r->tail;
}

static inline size_t ringbuf_free(const struct ringbuf *r)
{
    return r->mask + 1 - (r->head - r->tail);
}

static inline size_t ringbuf_capacity(const struct ringbuf *r)
{
    return r->mask + 1;
}

/* Contiguous write space for producer */
static inline size_t ringbuf_write_avail(const struct ringbuf *r, uint8_t **ptr)
{
    size_t head = r->head;
    size_t tail = r->tail;
    size_t cap  = r->mask + 1;
    size_t used = head - tail;
    size_t avail = cap - used;

    if (avail == 0) {
        *ptr = NULL;
        return 0;
    }

    *ptr = r->buf + (head & r->mask);

    /* If wrap-around would happen, only offer up to end of buffer */
    size_t contiguous = cap - (head & r->mask);
    return avail < contiguous ? avail : contiguous;
}

static inline void ringbuf_write_commit(struct ringbuf *r, size_t n)
{
    r->head += n;
}

static inline int ringbuf_put(struct ringbuf *r, const uint8_t *data, size_t len)
{
    size_t cap = r->mask + 1;
    size_t head = r->head;
    size_t tail = r->tail;

    if (head - tail + len > cap)
        return -1; /* full */

    size_t offset = head & r->mask;
    size_t first  = cap - offset;
    if (first >= len) {
        memcpy(r->buf + offset, data, len);
    } else {
        memcpy(r->buf + offset, data, first);
        memcpy(r->buf, data + first, len - first);
    }

    __sync_synchronize(); /* ensure data visible before head update */
    r->head = head + len;
    return 0;
}

static inline int ringbuf_get(struct ringbuf *r, uint8_t *dst, size_t len)
{
    size_t cap = r->mask + 1;
    size_t head = r->head;
    size_t tail = r->tail;

    if (head - tail < len)
        return -1; /* insufficient data */

    size_t offset = tail & r->mask;
    size_t first  = cap - offset;
    if (first >= len) {
        memcpy(dst, r->buf + offset, len);
    } else {
        memcpy(dst, r->buf + offset, first);
        memcpy(dst + first, r->buf, len - first);
    }

    __sync_synchronize(); /* ensure data consumed before tail update */
    r->tail = tail + len;
    return 0;
}

/* Read without consuming */
static inline int ringbuf_peek(const struct ringbuf *r, uint8_t *dst, size_t len)
{
    size_t cap = r->mask + 1;
    size_t head = r->head;
    size_t tail = r->tail;

    if (head - tail < len)
        return -1;

    size_t offset = tail & r->mask;
    size_t first  = cap - offset;
    if (first >= len) {
        memcpy(dst, r->buf + offset, len);
    } else {
        memcpy(dst, r->buf + offset, first);
        memcpy(dst + first, r->buf, len - first);
    }
    return 0;
}

static inline void ringbuf_drain(struct ringbuf *r, size_t len)
{
    r->tail += len;
}

static inline void ringbuf_reset(struct ringbuf *r)
{
    r->head = 0;
    r->tail = 0;
}
