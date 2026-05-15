/**
 * @file    ring_buffer.c
 * @brief   字节级单生产-单消费环形缓冲实现
 * @note    在 Cortex-M4 上，对 16-bit head/tail 的单字读写是原子的，
 *          因此本实现不需要 __disable_irq；前提：仅 1 个生产者 + 1 个消费者。
 */
#include "ring_buffer.h"

static bool Ring_Buffer_IsPowerOfTwo(uint16_t v) {
    return (v != 0U) && ((v & (v - 1U)) == 0U);
}

void Ring_Buffer_Init(Ring_Buffer *rb, uint8_t *storage, uint16_t capacity_pow2) {
    if ((rb == NULL) || (storage == NULL) || !Ring_Buffer_IsPowerOfTwo(capacity_pow2)) {
        return;
    }
    rb->buf      = storage;
    rb->capacity = capacity_pow2;
    rb->mask     = capacity_pow2 - 1U;
    rb->head     = 0U;
    rb->tail     = 0U;
}

void Ring_Buffer_Reset(Ring_Buffer *rb) {
    if (rb == NULL) {
        return;
    }
    rb->head = 0U;
    rb->tail = 0U;
}

uint16_t Ring_Buffer_Count(const Ring_Buffer *rb) {
    return (uint16_t)((rb->head - rb->tail) & rb->mask);
}

uint16_t Ring_Buffer_Free(const Ring_Buffer *rb) {
    return (uint16_t)(rb->capacity - 1U - Ring_Buffer_Count(rb));
}

bool Ring_Buffer_IsEmpty(const Ring_Buffer *rb) {
    return (rb->head == rb->tail);
}

bool Ring_Buffer_IsFull(const Ring_Buffer *rb) {
    return (Ring_Buffer_Count(rb) == (rb->capacity - 1U));
}

bool Ring_Buffer_Push(Ring_Buffer *rb, uint8_t byte) {
    uint16_t next = (uint16_t)((rb->head + 1U) & rb->mask);
    if (next == rb->tail) {
        return false; /* 满 */
    }
    rb->buf[rb->head] = byte;
    rb->head = next;
    return true;
}

bool Ring_Buffer_Pop(Ring_Buffer *rb, uint8_t *out_byte) {
    if (rb->head == rb->tail) {
        return false; /* 空 */
    }
    if (out_byte != NULL) {
        *out_byte = rb->buf[rb->tail];
    }
    rb->tail = (uint16_t)((rb->tail + 1U) & rb->mask);
    return true;
}

size_t Ring_Buffer_PushN(Ring_Buffer *rb, const uint8_t *src, size_t n) {
    size_t pushed = 0U;
    while (pushed < n) {
        if (!Ring_Buffer_Push(rb, src[pushed])) {
            break;
        }
        pushed++;
    }
    return pushed;
}

size_t Ring_Buffer_PopN(Ring_Buffer *rb, uint8_t *dst, size_t n) {
    size_t popped = 0U;
    while (popped < n) {
        if (!Ring_Buffer_Pop(rb, &dst[popped])) {
            break;
        }
        popped++;
    }
    return popped;
}
