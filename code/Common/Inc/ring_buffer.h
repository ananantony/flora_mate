/**
 * @file    ring_buffer.h
 * @brief   字节级单生产-单消费环形缓冲（ISR 写 / 主循环读 安全）
 * @note    容量必须是 2 的幂；用 head/tail 与容量掩码实现，无需关中断。
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t        *buf;          /* 由调用者提供的存储区，长度必须 = capacity */
    uint16_t        capacity;     /* 2^N，最大 32768 */
    uint16_t        mask;         /* capacity - 1 */
    volatile uint16_t head;       /* ISR/生产者写入 */
    volatile uint16_t tail;       /* 主循环/消费者读取 */
} Ring_Buffer;

void   Ring_Buffer_Init(Ring_Buffer *rb, uint8_t *storage, uint16_t capacity_pow2);
void   Ring_Buffer_Reset(Ring_Buffer *rb);

bool   Ring_Buffer_Push(Ring_Buffer *rb, uint8_t byte);          /* ISR 安全 */
bool   Ring_Buffer_Pop(Ring_Buffer *rb, uint8_t *out_byte);      /* 主循环安全 */

size_t Ring_Buffer_PushN(Ring_Buffer *rb, const uint8_t *src, size_t n);
size_t Ring_Buffer_PopN(Ring_Buffer *rb, uint8_t *dst, size_t n);

uint16_t Ring_Buffer_Count(const Ring_Buffer *rb);
uint16_t Ring_Buffer_Free(const Ring_Buffer *rb);
bool     Ring_Buffer_IsEmpty(const Ring_Buffer *rb);
bool     Ring_Buffer_IsFull(const Ring_Buffer *rb);

#endif /* RING_BUFFER_H */
