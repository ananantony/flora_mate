/*
 * @File         : \code\Common\ring_buffer\ring_buffer.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 字节级单生产-单消费环形缓冲实现
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 *
 *   _________________________________________________________________________
 *  | Date       | Version | Author      |  Description                       |
 *  |=========================================================================|
 *  |            |         |             |                                    |
 *  |-------------------------------------------------------------------------|
 *  |            |         |             |                                    |
 *  |-------------------------------------------------------------------------|
 */
#include "ring_buffer.h"

/**
 * @brief   判断容量是否为 2 的幂
 * @param   v 待判断值
 * @retval  true=是 2^N（且非 0）
 * @retval  false=不合法
 */
static bool Ring_Buffer_IsPowerOfTwo(uint16_t v)
{
    return (v != 0U) && ((v & (v - 1U)) == 0U);
}

/**
 * @brief   初始化环形缓冲（见 ring_buffer.h）
 * @param   rb            缓冲控制块
 * @param   storage       数据存储区
 * @param   capacity_pow2 容量（2 的幂）
 * @retval  无；非法参数时直接返回
 */
void Ring_Buffer_Init(Ring_Buffer *rb, uint8_t *storage, uint16_t capacity_pow2)
{
    if ((rb == NULL) || (storage == NULL) || !Ring_Buffer_IsPowerOfTwo(capacity_pow2))
    {
        return;
    }
    rb->buf      = storage;
    rb->capacity = capacity_pow2;
    rb->mask     = capacity_pow2 - 1U;
    rb->head     = 0U;
    rb->tail     = 0U;
}

/**
 * @brief   清空缓冲区（见 ring_buffer.h）
 * @param   rb 缓冲控制块
 * @retval  无
 */
void Ring_Buffer_Reset(Ring_Buffer *rb)
{
    if (rb == NULL)
    {
        return;
    }
    rb->head = 0U;
    rb->tail = 0U;
}

/**
 * @brief   查询当前已占用字节数（见 ring_buffer.h）
 * @param   rb 缓冲控制块
 * @retval  已占用字节数
 */
uint16_t Ring_Buffer_Count(const Ring_Buffer *rb)
{
    return (uint16_t)((rb->head - rb->tail) & rb->mask);
}

/**
 * @brief   查询当前可写入字节数（见 ring_buffer.h）
 * @param   rb 缓冲控制块
 * @retval  剩余可写入字节数
 */
uint16_t Ring_Buffer_Free(const Ring_Buffer *rb)
{
    return (uint16_t)(rb->capacity - 1U - Ring_Buffer_Count(rb));
}

/**
 * @brief   是否为空（见 ring_buffer.h）
 * @param   rb 缓冲控制块
 * @retval  true=空 / false=有数据
 */
bool Ring_Buffer_IsEmpty(const Ring_Buffer *rb)
{
    return (rb->head == rb->tail);
}

/**
 * @brief   是否为满（见 ring_buffer.h）
 * @param   rb 缓冲控制块
 * @retval  true=满 / false=尚有空闲
 */
bool Ring_Buffer_IsFull(const Ring_Buffer *rb)
{
    return (Ring_Buffer_Count(rb) == (rb->capacity - 1U));
}

/**
 * @brief   写入单字节（见 ring_buffer.h）
 * @param   rb   缓冲控制块
 * @param   byte 待写入字节
 * @retval  true  写入成功
 * @retval  false 缓冲已满
 */
bool Ring_Buffer_Push(Ring_Buffer *rb, uint8_t byte)
{
    /* 预计算 next，若与 tail 相遇即视为满（牺牲 1 槽位换无关中断的判定） */
    uint16_t next = (uint16_t)((rb->head + 1U) & rb->mask);
    if (next == rb->tail)
    {
        return false; /* 满 */
    }
    rb->buf[rb->head] = byte;
    rb->head          = next;
    return true;
}

/**
 * @brief   读取单字节（见 ring_buffer.h）
 * @param   rb       缓冲控制块
 * @param   out_byte 输出参数；NULL 表示丢弃但仍消费
 * @retval  true  成功出列一字节
 * @retval  false 缓冲为空
 */
bool Ring_Buffer_Pop(Ring_Buffer *rb, uint8_t *out_byte)
{
    if (rb->head == rb->tail)
    {
        return false; /* 空 */
    }
    if (out_byte != NULL)
    {
        *out_byte = rb->buf[rb->tail];
    }
    rb->tail = (uint16_t)((rb->tail + 1U) & rb->mask);
    return true;
}

/**
 * @brief   批量写入（见 ring_buffer.h）
 * @param   rb  缓冲控制块
 * @param   src 数据源
 * @param   n   期望写入字节数
 * @retval  实际写入字节数（≤ n）
 */
size_t Ring_Buffer_PushN(Ring_Buffer *rb, const uint8_t *src, size_t n)
{
    size_t pushed = 0U;
    while (pushed < n)
    {
        if (!Ring_Buffer_Push(rb, src[pushed]))
        {
            break;
        }
        pushed++;
    }
    return pushed;
}

/**
 * @brief   批量读取（见 ring_buffer.h）
 * @param   rb  缓冲控制块
 * @param   dst 数据目标
 * @param   n   期望读取字节数
 * @retval  实际读取字节数（≤ n）
 */
size_t Ring_Buffer_PopN(Ring_Buffer *rb, uint8_t *dst, size_t n)
{
    size_t popped = 0U;
    while (popped < n)
    {
        if (!Ring_Buffer_Pop(rb, &dst[popped]))
        {
            break;
        }
        popped++;
    }
    return popped;
}
