/*
 * @File         : \code\Bsp\pump_pwm\bsp_pump_pwm.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 水泵 PWM (TIM2_CH1/PA0) 安全封装，含 PUMP_EN 使能互锁与 ramp-down
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
#include "bsp_pump_pwm.h"
#include "bsp_valve.h"
#include "bsp_tick.h"
#include "tim.h"

static uint8_t s_current_duty_percent; /**< 当前占空比百分比影子寄存器 */
static bool    s_pwm_started;          /**< TIM2_CH1 PWM 是否已 Start  */

/* Ramp-down 状态 */
static bool     s_ramp_active;      /**< ramp 进行中标志 */
static uint32_t s_ramp_start_ms;    /**< ramp 起始时刻   */
static uint32_t s_ramp_duration_ms; /**< ramp 总时长     */
static uint8_t  s_ramp_from_duty;   /**< ramp 起始占空比 */

/**
 * @brief   写入 PWM 比较寄存器
 * @param   duty_percent  占空比百分比（> 95 自动夹到 95）
 * @note    根据 BSP_PUMP_PWM_PERIOD_TICK (1000) 将百分比换算为 0..1000 的 pulse 值。
 *          这是模块内唯一直接写 CCR 的地方，便于集中加 trace。
 */
static void Bsp_Pump_Pwm_WriteCcr(uint8_t duty_percent)
{
    if (duty_percent > BSP_PUMP_DUTY_MAX)
    {
        duty_percent = BSP_PUMP_DUTY_MAX;
    }
    uint32_t pulse = ((uint32_t)duty_percent * BSP_PUMP_PWM_PERIOD_TICK) / 100U;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
    s_current_duty_percent = duty_percent;
}

/**
 * @brief   PWM 初始化（首次启动 TIM2_CH1 输出）
 * @note    Init 后占空比立即为 0，CH1=OFF，水泵无水流。
 */
void Bsp_Pump_Pwm_Init(void)
{
    s_current_duty_percent = 0U;
    s_ramp_active          = false;
    Bsp_Pump_Pwm_WriteCcr(0U);
    if (!s_pwm_started)
    {
        (void)HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
        s_pwm_started = true;
    }
}

/**
 * @brief   设置 PWM 占空比
 * @param   duty_percent  目标百分比 [0..95]
 * @retval  FM_OK / FM_ERR_013_PUMP_NO_POWER
 * @note    0% 永远允许（停泵）；非 0% 要求 PUMP_EN 已使能（水泵 12V 通路已建立）。
 *          任何成功 Set 都会取消正在进行的 ramp-down。
 */
Fm_ErrorCode Bsp_Pump_Pwm_SetDutyPercent(uint8_t duty_percent)
{
    if (duty_percent == 0U)
    {
        s_ramp_active = false;
        Bsp_Pump_Pwm_WriteCcr(0U);
        return FM_OK;
    }
    if (!Bsp_Valve_Get(BSP_VALVE_PUMP_EN))
    {
        return FM_ERR_013_PUMP_NO_POWER;
    }
    if (duty_percent > BSP_PUMP_DUTY_MAX)
    {
        duty_percent = BSP_PUMP_DUTY_MAX;
    }
    s_ramp_active = false;
    Bsp_Pump_Pwm_WriteCcr(duty_percent);
    return FM_OK;
}

/**
 * @brief   立即停止 PWM（占空比写 0）
 * @note    可安全用于 ISR / 紧急路径；不发 I/O，仅写寄存器。
 */
void Bsp_Pump_Pwm_Stop(void)
{
    s_ramp_active = false;
    Bsp_Pump_Pwm_WriteCcr(0U);
}

/**
 * @brief   读取当前占空比（影子寄存器）
 * @retval  当前百分比 [0..95]
 */
uint8_t Bsp_Pump_Pwm_GetDutyPercent(void)
{
    return s_current_duty_percent;
}

/**
 * @brief   启动非阻塞线性降占空比过程
 * @param   ramp_ms  总时长；< 10 ms 自动夹到 10 ms 避免步长退化为 0
 * @note    本调用立即返回；调用者需周期性调用 RampTick() 推进。
 *          若当前已经为 0，直接清 active 标志返回。
 */
void Bsp_Pump_Pwm_StartRampDown(uint32_t ramp_ms)
{
    if (s_current_duty_percent == 0U)
    {
        s_ramp_active = false;
        return;
    }
    if (ramp_ms < 10U)
    {
        ramp_ms = 10U;
    }
    s_ramp_from_duty   = s_current_duty_percent;
    s_ramp_start_ms    = Bsp_Tick_GetMs();
    s_ramp_duration_ms = ramp_ms;
    s_ramp_active      = true;
}

/**
 * @brief   推进 ramp-down 一次（应每 5~10 ms 调用）
 * @retval  true  仍在 ramp（duty 已更新）
 * @retval  false 已到 0% 或本来就未启动
 * @note    占空比按"剩余时间 / 总时间 × 起始占空比"线性插值；
 *          到时直接写 0 并清 active。
 */
bool Bsp_Pump_Pwm_RampTick(void)
{
    if (!s_ramp_active)
    {
        return false;
    }
    uint32_t elapsed = Bsp_Tick_ElapsedMs(s_ramp_start_ms);
    if (elapsed >= s_ramp_duration_ms)
    {
        Bsp_Pump_Pwm_WriteCcr(0U);
        s_ramp_active = false;
        return false;
    }
    uint32_t remaining = s_ramp_duration_ms - elapsed;
    uint32_t duty      = ((uint32_t)s_ramp_from_duty * remaining) / s_ramp_duration_ms;
    Bsp_Pump_Pwm_WriteCcr((uint8_t)duty);
    return true;
}
