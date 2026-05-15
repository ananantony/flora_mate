/*
 * @File         : \code\Common\ring_buffer\ring_buffer.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 字节级单生产-单消费环形缓冲接口（ISR/主循环 lock-free）
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
#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief   字节环形缓冲控制结构（单生产-单消费）
 * @note    Cortex-M4 上对 16-bit head/tail 的单字读写是原子的，
 *          因此 ISR 写、主循环读 / 反之 不需要关中断；
 *          但禁止"两个生产者"或"两个消费者"并发。
 */
typedef struct
{
    uint8_t          *buf;      /**< 调用者提供的存储区，长度 = capacity */
    uint16_t          capacity; /**< 容量；必须是 2^N，最大 32768       */
    uint16_t          mask;     /**< capacity - 1，用于索引环绕         */
    volatile uint16_t head;     /**< 生产者写入索引                      */
    volatile uint16_t tail;     /**< 消费者读取索引                      */
} Ring_Buffer;

/**
 * @brief   初始化环形缓冲
 * @param   rb            缓冲控制块（必须由调用者分配）
 * @param   storage       数据存储区，长度需 ≥ capacity_pow2
 * @param   capacity_pow2 容量；必须是 2 的幂（如 64/128/512/1024）
 * @note    head/tail 复位为 0；非法容量直接返回不做任何事
 */
void Ring_Buffer_Init(Ring_Buffer *rb, uint8_t *storage, uint16_t capacity_pow2);

/**
 * @brief   清空缓冲区
 * @param   rb 缓冲控制块
 * @note    仅 reset 索引，不擦数据；ISR 安全（但避免在生产/消费途中调用）
 */
void Ring_Buffer_Reset(Ring_Buffer *rb);

/**
 * @brief   写入单字节（生产者侧）
 * @param   rb    缓冲控制块
 * @param   byte  待写入字节
 * @retval  true  写入成功
 * @retval  false 缓冲已满，被丢弃
 */
bool Ring_Buffer_Push(Ring_Buffer *rb, uint8_t byte);

/**
 * @brief   读取单字节（消费者侧）
 * @param   rb         缓冲控制块
 * @param   out_byte   输出参数；NULL 表示丢弃但仍消费一个字节
 * @retval  true       成功，缓冲已出列一字节
 * @retval  false      缓冲为空
 */
bool Ring_Buffer_Pop(Ring_Buffer *rb, uint8_t *out_byte);

/**
 * @brief   批量写入（按字节逐个调用 Push，遇满立即停止）
 * @param   rb   缓冲控制块
 * @param   src  数据源
 * @param   n    期望写入字节数
 * @retval  实际写入字节数（≤ n）
 */
size_t Ring_Buffer_PushN(Ring_Buffer *rb, const uint8_t *src, size_t n);

/**
 * @brief   批量读取（按字节逐个调用 Pop，遇空立即停止）
 * @param   rb   缓冲控制块
 * @param   dst  数据目标
 * @param   n    期望读取字节数
 * @retval  实际读取字节数（≤ n）
 */
size_t Ring_Buffer_PopN(Ring_Buffer *rb, uint8_t *dst, size_t n);

/**
 * @brief   查询当前已占用字节数
 * @param   rb 缓冲控制块
 * @retval  已占用字节数
 */
uint16_t Ring_Buffer_Count(const Ring_Buffer *rb);

/**
 * @brief   查询当前可写入字节数
 * @param   rb 缓冲控制块
 * @retval  剩余可写入字节数（= capacity - 1 - Count，浪费 1 字节区分满/空）
 */
uint16_t Ring_Buffer_Free(const Ring_Buffer *rb);

/**
 * @brief   是否为空
 * @param   rb 缓冲控制块
 * @retval  true=空 / false=有数据
 */
bool Ring_Buffer_IsEmpty(const Ring_Buffer *rb);

/**
 * @brief   是否为满
 * @param   rb 缓冲控制块
 * @retval  true=满 / false=尚有空闲
 */
bool Ring_Buffer_IsFull(const Ring_Buffer *rb);

#endif /* RING_BUFFER_H */
