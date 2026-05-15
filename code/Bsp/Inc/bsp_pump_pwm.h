/**
 * @file    bsp_pump_pwm.h
 * @brief   水泵 PWM (TIM2_CH1 / PA0) 安全封装
 * @note    1 kHz、千分占空比（0..1000）；对外仍提供 0..95 百分比接口。
 *          安全策略：CH5=OFF 时拒绝 set_duty(d>0)，避免空转改占空比。
 */
#ifndef BSP_PUMP_PWM_H
#define BSP_PUMP_PWM_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

#define BSP_PUMP_DUTY_MAX        FM_PUMP_DUTY_MAX_PERCENT   /* 95% */
#define BSP_PUMP_PWM_PERIOD_TICK (1000U)                    /* TIM2 ARR + 1 */

void         Bsp_Pump_Pwm_Init(void);
Fm_ErrorCode Bsp_Pump_Pwm_SetDutyPercent(uint8_t duty_percent);
void         Bsp_Pump_Pwm_Stop(void);
uint8_t      Bsp_Pump_Pwm_GetDutyPercent(void);

/* 非阻塞 ramp-down：调用一次启动；调用方在主循环周期性 Tick 推进 */
void Bsp_Pump_Pwm_StartRampDown(uint32_t ramp_ms);
bool Bsp_Pump_Pwm_RampTick(void);            /* 返回 true=ramp 进行中，false=已到 0 */

#endif /* BSP_PUMP_PWM_H */
