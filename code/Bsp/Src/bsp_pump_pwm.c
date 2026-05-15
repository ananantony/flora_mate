/**
 * @file    bsp_pump_pwm.c
 * @brief   水泵 PWM 安全封装实现
 */
#include "bsp_pump_pwm.h"
#include "bsp_relay.h"
#include "bsp_tick.h"
#include "tim.h"

static uint8_t  s_current_duty_percent;
static bool     s_pwm_started;

/* Ramp 状态 */
static bool     s_ramp_active;
static uint32_t s_ramp_start_ms;
static uint32_t s_ramp_duration_ms;
static uint8_t  s_ramp_from_duty;

static void Bsp_Pump_Pwm_WriteCcr(uint8_t duty_percent) {
    if (duty_percent > BSP_PUMP_DUTY_MAX) {
        duty_percent = BSP_PUMP_DUTY_MAX;
    }
    uint32_t pulse = ((uint32_t)duty_percent * BSP_PUMP_PWM_PERIOD_TICK) / 100U;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
    s_current_duty_percent = duty_percent;
}

void Bsp_Pump_Pwm_Init(void) {
    s_current_duty_percent = 0U;
    s_ramp_active          = false;
    Bsp_Pump_Pwm_WriteCcr(0U);
    if (!s_pwm_started) {
        (void)HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
        s_pwm_started = true;
    }
}

Fm_ErrorCode Bsp_Pump_Pwm_SetDutyPercent(uint8_t duty_percent) {
    /* 0 总是允许（停泵） */
    if (duty_percent == 0U) {
        s_ramp_active = false;
        Bsp_Pump_Pwm_WriteCcr(0U);
        return FM_OK;
    }
    /* 非 0 占空比需要 CH5 已吸合 */
    if (!Bsp_Relay_Get(BSP_RELAY_MAIN_CH5)) {
        return FM_ERR_013_PUMP_NO_POWER;
    }
    if (duty_percent > BSP_PUMP_DUTY_MAX) {
        duty_percent = BSP_PUMP_DUTY_MAX;
    }
    s_ramp_active = false;
    Bsp_Pump_Pwm_WriteCcr(duty_percent);
    return FM_OK;
}

void Bsp_Pump_Pwm_Stop(void) {
    s_ramp_active = false;
    Bsp_Pump_Pwm_WriteCcr(0U);
}

uint8_t Bsp_Pump_Pwm_GetDutyPercent(void) {
    return s_current_duty_percent;
}

void Bsp_Pump_Pwm_StartRampDown(uint32_t ramp_ms) {
    if (s_current_duty_percent == 0U) {
        s_ramp_active = false;
        return;
    }
    if (ramp_ms < 10U) {
        ramp_ms = 10U;
    }
    s_ramp_from_duty    = s_current_duty_percent;
    s_ramp_start_ms     = Bsp_Tick_GetMs();
    s_ramp_duration_ms  = ramp_ms;
    s_ramp_active       = true;
}

bool Bsp_Pump_Pwm_RampTick(void) {
    if (!s_ramp_active) {
        return false;
    }
    uint32_t elapsed = Bsp_Tick_ElapsedMs(s_ramp_start_ms);
    if (elapsed >= s_ramp_duration_ms) {
        Bsp_Pump_Pwm_WriteCcr(0U);
        s_ramp_active = false;
        return false;
    }
    uint32_t remaining = s_ramp_duration_ms - elapsed;
    uint32_t duty = ((uint32_t)s_ramp_from_duty * remaining) / s_ramp_duration_ms;
    Bsp_Pump_Pwm_WriteCcr((uint8_t)duty);
    return true;
}
