/*
 * @File         : \code\Bsp\pump_pwm\bsp_pump_pwm.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 水泵 PWM (TIM2_CH1 / PA0) 安全封装
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
#ifndef BSP_PUMP_PWM_H
#define BSP_PUMP_PWM_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

#define BSP_PUMP_DUTY_MAX        FM_PUMP_DUTY_MAX_PERCENT /**< 占空比硬上限 95% */
#define BSP_PUMP_PWM_PERIOD_TICK (1000U)                  /**< TIM2 ARR + 1     */

/**
 * @brief   水泵 PWM 初始化
 * @note    1) 启动 TIM2_CH1 PWM 输出；2) 立即写 CCR=0；3) PA0 在 CubeMX 已配 AF1。
 *          上电默认 0% 占空比，配合 CH1=OFF，水泵静音。
 */
void Bsp_Pump_Pwm_Init(void);

/**
 * @brief   设置水泵 PWM 占空比（百分比）
 * @param   duty_percent  目标占空比 [0..95]；> 95 自动夹到 95
 * @retval  FM_OK                  写入成功
 * @retval  FM_ERR_013_PUMP_NO_POWER  CH1=OFF 时拒绝 duty>0（避免逻辑空转）
 * @note    duty=0 时强制写 CCR=0 而非比较寄存器低值，确保 MOS 完全关断。
 */
Fm_ErrorCode Bsp_Pump_Pwm_SetDutyPercent(uint8_t duty_percent);

/**
 * @brief   立即停止 PWM（写 CCR=0），并清空 ramp-down 状态
 * @note    可在 ISR / 任何上下文调用；用于 ERROR / STOP 紧急路径。
 */
void Bsp_Pump_Pwm_Stop(void);

/**
 * @brief   读取当前 PWM 占空比（影子寄存器，非硬件回读）
 * @retval  当前占空比百分比 [0..95]
 */
uint8_t Bsp_Pump_Pwm_GetDutyPercent(void);

/**
 * @brief   启动非阻塞的占空比线性降至 0 的过程
 * @param   ramp_ms  从当前占空比降到 0 的总时长（毫秒，0 → 立刻 Stop）
 * @note    主循环需周期性调用 Bsp_Pump_Pwm_RampTick() 推进；与 PUMP_FSM
 *          的 RAMP_DOWN 状态配合，避免水锤冲击。
 */
void Bsp_Pump_Pwm_StartRampDown(uint32_t ramp_ms);

/**
 * @brief   推进 ramp-down 进程（应每 5~10 ms 调用一次）
 * @retval  true   仍在 ramp 中（调用方继续轮询）
 * @retval  false  已到 0%（结束或本来就未启动）
 */
bool Bsp_Pump_Pwm_RampTick(void);

#endif /* BSP_PUMP_PWM_H */
