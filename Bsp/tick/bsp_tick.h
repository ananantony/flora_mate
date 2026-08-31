/*
 * @File         : \code\Bsp\tick\bsp_tick.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 1 ms 时基与软定时器接口
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
#ifndef BSP_TICK_H
#define BSP_TICK_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_TICK_HZ          (1000U) /**< 时基频率 = 1 kHz                */
#define BSP_TICK_MS_PER_TICK (1U)    /**< 单 tick = 1 ms                  */

/**
 * @brief   时基模块初始化
 * @note    HAL_Init() 已经把 SysTick 配为 1 ms；本函数仅占位、便于后续把
 *          SysTick_Handler 接管为本模块独立计时（如扩展为 64-bit 软计数）。
 */
void Bsp_Tick_Init(void);

/**
 * @brief   读取自上电以来的毫秒计数
 * @retval  32-bit 毫秒数（49.7 天后环绕；环绕由各 ElapsedMs/Expired 处理）
 */
uint32_t Bsp_Tick_GetMs(void);

/**
 * @brief   计算从 start_ms 到当前已经过去的毫秒数
 * @param   start_ms 起始时刻（由 Bsp_Tick_GetMs() 记录）
 * @retval  经过的毫秒数（无符号减法天然处理 32-bit 环绕，49 天内绝对准确）
 */
uint32_t Bsp_Tick_ElapsedMs(uint32_t start_ms);

/**
 * @brief   判断是否到达预定时长
 * @param   start_ms      起始时刻
 * @param   duration_ms   预定时长
 * @retval  true=已到期 / false=未到
 */
bool Bsp_Tick_Expired(uint32_t start_ms, uint32_t duration_ms);

/**
 * @brief   周期节拍器：用于"按固定周期到点才执行"的协作式任务
 * @note    与 Ticker_Init 配合使用；适合按键扫描 20 ms、显示刷新 100 ms 等场景
 */
typedef struct
{
    uint32_t next_due_ms; /**< 下一次到期时刻                 */
    uint32_t period_ms;   /**< 周期                            */
} Bsp_Tick_Ticker;

/**
 * @brief   初始化节拍器
 * @param   t          节拍器实例
 * @param   period_ms  周期；首次到期 = now + period_ms
 */
void Bsp_Tick_Ticker_Init(Bsp_Tick_Ticker *t, uint32_t period_ms);

/**
 * @brief   查询节拍器是否到期，到期则自动滚动到下一周期
 * @param   t  节拍器实例
 * @retval  true  本次调用刚到期，调用者应执行任务
 * @retval  false 未到期，调用者应跳过
 * @note    若被长时间阻塞错过多次周期，仅追赶一次（不补打多次），
 *          避免主循环被卡死后又一次性触发多次扫描造成"暴发"
 */
bool Bsp_Tick_Ticker_Due(Bsp_Tick_Ticker *t);

#endif /* BSP_TICK_H */
