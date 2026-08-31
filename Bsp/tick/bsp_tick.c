/*
 * @File         : \code\Bsp\tick\bsp_tick.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 1 ms 时基与软定时器实现，封装 HAL_GetTick 并支持环绕安全
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
#include "bsp_tick.h"
#include "stm32f4xx_hal.h"

/**
 * @brief   时基模块初始化（占位）
 * @note    HAL_Init() 已经把 SysTick 配成 1 ms；此处保留接口以便将来把
 *          SysTick_Handler 接管为本模块计时器（例如 32→64 bit 软扩展）。
 */
void Bsp_Tick_Init(void)
{
}

/**
 * @brief   读取 32-bit 毫秒计数
 * @retval  当前时刻（49.7 天后环绕）
 */
uint32_t Bsp_Tick_GetMs(void)
{
    return HAL_GetTick();
}

/**
 * @brief   计算从 start_ms 起经过的毫秒数
 * @param   start_ms  起始时刻
 * @retval  经过的毫秒数
 * @note    32-bit 无符号减法天然处理环绕，区间 < 49 天时绝对准确。
 */
uint32_t Bsp_Tick_ElapsedMs(uint32_t start_ms)
{
    return (uint32_t)(HAL_GetTick() - start_ms);
}

/**
 * @brief   是否到达指定时长
 * @param   start_ms     起始时刻
 * @param   duration_ms  目标时长
 * @retval  true=已到 / false=未到
 */
bool Bsp_Tick_Expired(uint32_t start_ms, uint32_t duration_ms)
{
    return Bsp_Tick_ElapsedMs(start_ms) >= duration_ms;
}

/**
 * @brief   初始化周期节拍器
 * @param   t          节拍器实例（NULL 直接返回）
 * @param   period_ms  周期
 */
void Bsp_Tick_Ticker_Init(Bsp_Tick_Ticker *t, uint32_t period_ms)
{
    if (t == 0)
    {
        return;
    }
    t->period_ms   = period_ms;
    t->next_due_ms = HAL_GetTick() + period_ms;
}

/**
 * @brief   判断是否到期；到期时自动滚动到下一周期
 * @param   t  节拍器实例
 * @retval  true=本次到期 / false=未到
 * @note    用 (int32_t)(now - next_due) >= 0 而非直接比较，正确处理
 *          32-bit 环绕；错过多周期时仅追赶一次（避免暴发）。
 */
bool Bsp_Tick_Ticker_Due(Bsp_Tick_Ticker *t)
{
    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - t->next_due_ms) >= 0)
    {
        t->next_due_ms += t->period_ms;
        if ((int32_t)(now - t->next_due_ms) >= 0)
        {
            /* 长时间被阻塞，重新对齐当前时刻 */
            t->next_due_ms = now + t->period_ms;
        }
        return true;
    }
    return false;
}
