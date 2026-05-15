/*
 * @File         : \code\App\log\app_log_ring_buffer.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 15:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 15:30:00
 * @Description  : 日志环形缓冲区接口（多生产者单消费者）
 *                 - 生产者侧（多个任务/ISR）调用 Push，内部关中断保护
 *                 - 消费者侧（主循环单线程）调用 Pop，无需加锁
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
#ifndef __APP_LOG_RING_BUFFER_H__
#define __APP_LOG_RING_BUFFER_H__

#include "app_log_types.h"

/**
 * @brief 日志环形缓冲区控制块
 * @note  data 数组容量由 APP_LOG_RING_DATA_MAX_SIZE 决定；
 *        head/tail 声明为 volatile，防止编译器将 ISR 与主循环之间的
 *        指针更新优化掉。
 */
typedef struct
{
    uint8_t           data[APP_LOG_RING_DATA_MAX_SIZE];
    volatile uint32_t head; /* 写入指针，由 Push 推进 */
    volatile uint32_t tail; /* 读取指针，由 Pop 推进 */
} App_Log_RingBufferType;

/**
 * @brief 初始化环形缓冲区
 * @param rb 环形缓冲区指针
 * @return true 成功 / false 参数为空
 */
bool App_Log_RingBufferInit(App_Log_RingBufferType *rb);

/**
 * @brief 写入数据到环形缓冲区（多生产者安全，内部关中断保护）
 * @param rb   环形缓冲区指针
 * @param data 待写入数据
 * @param len  待写入长度（字节）
 * @return 实际写入字节数；空间不足或参数非法时返回 0
 */
uint16_t App_Log_RingBufferPush(App_Log_RingBufferType *rb, const uint8_t *data, uint16_t len);

/**
 * @brief 从环形缓冲区读取数据（单消费者，无需加锁）
 * @param rb       环形缓冲区指针
 * @param data_out 输出缓冲区
 * @param read_len 请求读取的最大字节数
 * @return 实际读取字节数
 */
uint16_t App_Log_RingBufferPop(App_Log_RingBufferType *rb, uint8_t *data_out, uint16_t read_len);

/**
 * @brief 查询缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true 为空 / false 非空
 */
bool App_Log_RingBufferIsEmpty(const App_Log_RingBufferType *rb);

/**
 * @brief 查询缓冲区已使用字节数
 * @param rb 环形缓冲区指针
 * @return 已使用字节数
 */
uint32_t App_Log_RingBufferGetUsed(const App_Log_RingBufferType *rb);

#endif /* __APP_LOG_RING_BUFFER_H__ */
