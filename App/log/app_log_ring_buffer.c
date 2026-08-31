/*
 * @File         : \code\App\log\app_log_ring_buffer.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 15:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 15:30:00
 * @Description  : 日志环形缓冲区实现（多生产者单消费者）
 *                 - 生产者侧用 __disable_irq / __enable_irq 关全局中断保护临界区
 *                 - 缓冲区按字节寻址，预留 1 字节保证 head==tail 仅在空时成立
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 *
 *   _________________________________________________________________________
 *  | Date       | Version | Author      |  Description                       |
 *  |=========================================================================|
 *  | 2026-05-15 | 1.0.0   | tonymeng    | 初始版本（参考 cdd_log 实现）      |
 *  |-------------------------------------------------------------------------|
 *  |            |         |             |                                    |
 *  |-------------------------------------------------------------------------|
 */
#include "app_log_ring_buffer.h"
#include "cmsis_compiler.h"
#include <string.h>

/* ========================================== 内部函数 ========================================== */

/**
 * @brief 计算缓冲区已用字节数（不加锁，供内部使用）
 * @param head 写入指针位置
 * @param tail 读取指针位置
 * @return 已使用字节数
 */
static uint32_t App_Log_RingBuffer_CalcUsed(uint32_t head, uint32_t tail)
{
    return (head >= tail) ? (head - tail) : (APP_LOG_RING_DATA_MAX_SIZE - (tail - head));
}

/* ========================================== 外部函数 ========================================== */

/**
 * @brief   初始化日志环形缓冲区（清零 head/tail 与 data）
 * @param   rb 环形缓冲区指针
 * @retval  true  成功
 * @retval  false rb 为 NULL
 */
bool App_Log_RingBufferInit(App_Log_RingBufferType *rb)
{
    if (rb == NULL)
    {
        return false;
    }

    memset(rb, 0, sizeof(App_Log_RingBufferType));

    return true;
}

/**
 * @brief   写入数据到环形缓冲区（多生产者，关中断保护临界区）
 * @param   rb   环形缓冲区指针
 * @param   data 待写入数据
 * @param   len  待写入长度（字节）
 * @retval  实际写入字节数；空间不足或参数非法时返回 0
 */
uint16_t App_Log_RingBufferPush(App_Log_RingBufferType *rb, const uint8_t *data, uint16_t len)
{
    if ((rb == NULL) || (data == NULL) || (len == 0U))
    {
        return 0U;
    }

    __disable_irq();

    uint32_t head       = rb->head;
    uint32_t tail       = rb->tail;
    uint32_t used       = App_Log_RingBuffer_CalcUsed(head, tail);
    uint32_t free_space = APP_LOG_RING_DATA_MAX_SIZE - used - 1U;

    if ((uint32_t)len > free_space)
    {
        __enable_irq();
        return 0U;
    }

    for (uint16_t i = 0U; i < len; i++)
    {
        rb->data[head] = data[i];
        head           = (head + 1U) % APP_LOG_RING_DATA_MAX_SIZE;
    }

    rb->head = head;

    __enable_irq();

    return len;
}

/**
 * @brief   从环形缓冲区读取数据（单消费者，主循环调用）
 * @param   rb       环形缓冲区指针
 * @param   data_out 输出缓冲区
 * @param   read_len 请求读取的最大字节数
 * @retval  实际读取字节数
 */
uint16_t App_Log_RingBufferPop(App_Log_RingBufferType *rb, uint8_t *data_out, uint16_t read_len)
{
    if ((rb == NULL) || (data_out == NULL) || (read_len == 0U))
    {
        return 0U;
    }

    uint32_t head = rb->head;
    uint32_t tail = rb->tail;

    if (head == tail)
    {
        return 0U;
    }

    uint32_t available = App_Log_RingBuffer_CalcUsed(head, tail);
    uint32_t actual    = (available < (uint32_t)read_len) ? available : (uint32_t)read_len;

    for (uint32_t i = 0U; i < actual; i++)
    {
        data_out[i] = rb->data[tail];
        tail        = (tail + 1U) % APP_LOG_RING_DATA_MAX_SIZE;
    }

    rb->tail = tail;

    return (uint16_t)actual;
}

/**
 * @brief   查询缓冲区是否为空
 * @param   rb 环形缓冲区指针
 * @retval  true  为空（或 rb 为 NULL）
 * @retval  false 非空
 */
bool App_Log_RingBufferIsEmpty(const App_Log_RingBufferType *rb)
{
    if (rb == NULL)
    {
        return true;
    }

    return (rb->head == rb->tail) ? true : false;
}

/**
 * @brief   查询缓冲区已使用字节数
 * @param   rb 环形缓冲区指针
 * @retval  已使用字节数；rb 为 NULL 时返回 0
 */
uint32_t App_Log_RingBufferGetUsed(const App_Log_RingBufferType *rb)
{
    if (rb == NULL)
    {
        return 0U;
    }

    return App_Log_RingBuffer_CalcUsed(rb->head, rb->tail);
}
