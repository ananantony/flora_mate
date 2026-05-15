/*
 * @File         : \code\App\pump_fsm\app_pump_fsm.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 单路浇灌子状态机实现
 * @note         顺序：阀ON → 等机械→ CH5 ON → 稳压 → 阶梯PWM → ramp → CH5 OFF → 阀OFF → 静默 → DONE
 *               关键铁律：先开分阀再开 CH5；先关 CH5 再关分阀（防水锤/空抽）。
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
#include "app_pump_fsm.h"
#include "app_config.h"
#include "app_log.h"
#include "bsp_pump_pwm.h"
#include "bsp_relay.h"
#include "bsp_tick.h"

#define VALVE_OPEN_SETTLE_MS (200U) /**< 分阀吸合后等待机械到位（ms） */
#define MAIN_STABILIZE_MS    (100U) /**< CH5 吸合后稳压等待（ms） */
#define RAMP_DOWN_MS         (500U) /**< PWM 线性降至 0 的时长（ms） */
#define CLOSE_MAIN_RELAX_MS  (500U) /**< 关 CH5 后卸压等待（ms） */
#define CLOSE_VALVE_MS       (50U)  /**< 关分阀后进入 GAP 前的等待（ms） */

/**
 * @brief   切换子状态并记录进入时刻
 * @param   ctx  子状态机上下文
 * @param   st   目标状态
 */
static void Set_State(App_Pump_FsmCtx *ctx, App_Pump_FsmState st)
{
    ctx->state          = st;
    ctx->state_enter_ms = Bsp_Tick_GetMs();
}

/**
 * @brief   将 Fm_ValveIndex 映射为 BSP 继电器通道
 * @param   v  阀索引（FM_VALVE_Z1..Z4）
 * @retval  对应的 Bsp_Relay_Channel（与 BSP_RELAY_VALVE_1..4 一一对应）
 */
static Bsp_Relay_Channel Valve_Channel(Fm_ValveIndex v)
{
    return (Bsp_Relay_Channel)v;
}

/**
 * @brief   启动单路浇灌
 * @param   ctx  上下文（必须由调用者持有）
 * @param   v    阀通道（FM_VALVE_Z1..Z4）
 */
void App_Pump_Fsm_Start(App_Pump_FsmCtx *ctx, Fm_ValveIndex v)
{
    ctx->valve         = v;
    ctx->step_idx      = 0U;
    ctx->max_duty_seen = 0U;
    ctx->last_err      = FM_OK;
    Set_State(ctx, APP_PUMP_FSM_STATE_INIT);
    LOG_INFO_WITH_ARG("pump fsm: start zone %u", (unsigned)v);
}

/**
 * @brief   紧急停止（K3 STOP 或系统超时）
 * @param   ctx     上下文
 * @param   reason  错误码（FM_OK=主动 STOP；其他=异常）
 */
void App_Pump_Fsm_Abort(App_Pump_FsmCtx *ctx, Fm_ErrorCode reason)
{
    Bsp_Pump_Pwm_Stop();
    Bsp_Relay_MainOffForce();
    ctx->last_err = reason;
    Set_State(ctx, (reason == FM_OK) ? APP_PUMP_FSM_STATE_DONE : APP_PUMP_FSM_STATE_ERROR);
    LOG_WARN_WITH_ARG("pump fsm: abort zone %u reason=0x%02X", (unsigned)ctx->valve, (unsigned)reason);
}

/**
 * @brief   暂停（V1.0 未实现，仅保留接口）
 * @param   ctx  上下文
 */
void App_Pump_Fsm_Pause(App_Pump_FsmCtx *ctx)
{
    /* 简化：暂停 = 立即 ramp_down + 关 CH5；恢复时重新进入当前阶梯
     * V1.0 仅记录意图，正式实现留 Phase 2。
     */
    (void)ctx;
}

/**
 * @brief   恢复（V1.0 未实现，仅保留接口）
 * @param   ctx  上下文
 */
void App_Pump_Fsm_Resume(App_Pump_FsmCtx *ctx)
{
    (void)ctx;
}

/**
 * @brief   读取当前子状态（用于 OLED 显示）
 * @param   ctx  上下文
 */
App_Pump_FsmState App_Pump_Fsm_State(const App_Pump_FsmCtx *ctx)
{
    return ctx->state;
}

/**
 * @brief   读取当前档位序号（用于 OLED 显示）
 * @param   ctx  上下文
 */
uint8_t App_Pump_Fsm_CurrentStep(const App_Pump_FsmCtx *ctx)
{
    return ctx->step_idx;
}

/**
 * @brief   推进一步子状态机（主循环每轮调用）
 * @param   ctx  上下文
 * @retval  true   仍在进行中
 * @retval  false  已 DONE 或 ERROR 终态，主 FSM 应推进到下一路
 * @note    INIT：吸合分阀 → OPEN_MAIN：吸合 CH5 → STEP：阶梯 PWM →
 *          RAMP_DOWN → CLOSE_MAIN → CLOSE_VALVE → GAP → DONE。
 */
bool App_Pump_Fsm_Tick(App_Pump_FsmCtx *ctx)
{
    const App_Config *cfg     = App_Config_Get();
    uint32_t          elapsed = Bsp_Tick_ElapsedMs(ctx->state_enter_ms);

    /* 单路硬超时（不管在哪个子状态） */
    uint32_t valve_run_total  = elapsed; /* 简化：以当前状态停留判断；累计统计在调用方 */
    (void)valve_run_total;

    switch (ctx->state)
    {
        case APP_PUMP_FSM_STATE_IDLE:
            return false;

        case APP_PUMP_FSM_STATE_INIT:
        {
            /* 进入即吸合 CHx 阀；之后等机械到位 */
            Bsp_Relay_Channel ch = Valve_Channel(ctx->valve);
            if (elapsed == 0U)
            {
                if (Bsp_Relay_Set(ch, true) != FM_OK)
                {
                    App_Pump_Fsm_Abort(ctx, FM_ERR_012_INTERLOCK);
                    return false;
                }
            }
            if (elapsed >= VALVE_OPEN_SETTLE_MS)
            {
                Set_State(ctx, APP_PUMP_FSM_STATE_OPEN_MAIN);
            }
            return true;
        }

        case APP_PUMP_FSM_STATE_OPEN_MAIN:
            if (elapsed == 0U)
            {
                if (Bsp_Relay_Set(BSP_RELAY_MAIN_CH5, true) != FM_OK)
                {
                    App_Pump_Fsm_Abort(ctx, FM_ERR_012_INTERLOCK);
                    return false;
                }
            }
            if (elapsed >= MAIN_STABILIZE_MS)
            {
                ctx->step_idx = 0U;
                Set_State(ctx, APP_PUMP_FSM_STATE_STEP);
            }
            return true;

        case APP_PUMP_FSM_STATE_STEP:
        {
            if (ctx->step_idx >= cfg->step_count)
            {
                Set_State(ctx, APP_PUMP_FSM_STATE_RAMP_DOWN);
                return true;
            }
            /* 进入新档位时立即设 PWM */
            if (elapsed == 0U)
            {
                uint8_t      duty = cfg->step_duty[ctx->step_idx];
                Fm_ErrorCode err  = Bsp_Pump_Pwm_SetDutyPercent(duty);
                if (err != FM_OK)
                {
                    App_Pump_Fsm_Abort(ctx, err);
                    return false;
                }
                if (duty > ctx->max_duty_seen)
                {
                    ctx->max_duty_seen = duty;
                }
                LOG_INFO_WITH_ARG("pump fsm: zone %u step %u duty=%u%%", (unsigned)ctx->valve, (unsigned)ctx->step_idx,
                                  (unsigned)duty);
            }
            uint32_t dur_ms = (uint32_t)cfg->step_seconds[ctx->step_idx] * 1000U;
            if (elapsed >= dur_ms)
            {
                ctx->step_idx++;
                Set_State(ctx, APP_PUMP_FSM_STATE_STEP); /* 重新进入下一档 */
            }
            return true;
        }

        case APP_PUMP_FSM_STATE_RAMP_DOWN:
            if (elapsed == 0U)
            {
                Bsp_Pump_Pwm_StartRampDown(RAMP_DOWN_MS);
            }
            if (!Bsp_Pump_Pwm_RampTick())
            {
                /* 已降到 0 */
                Set_State(ctx, APP_PUMP_FSM_STATE_CLOSE_MAIN);
            }
            return true;

        case APP_PUMP_FSM_STATE_CLOSE_MAIN:
            if (elapsed == 0U)
            {
                /* 释放 CH5：因为分阀仍开着，互锁允许 */
                (void)Bsp_Relay_Set(BSP_RELAY_MAIN_CH5, false);
            }
            if (elapsed >= CLOSE_MAIN_RELAX_MS)
            {
                Set_State(ctx, APP_PUMP_FSM_STATE_CLOSE_VALVE);
            }
            return true;

        case APP_PUMP_FSM_STATE_CLOSE_VALVE:
            if (elapsed == 0U)
            {
                Bsp_Relay_Channel ch = Valve_Channel(ctx->valve);
                /* 此时 CH5=OFF，互锁允许关阀 */
                (void)Bsp_Relay_Set(ch, false);
            }
            if (elapsed >= CLOSE_VALVE_MS)
            {
                Set_State(ctx, APP_PUMP_FSM_STATE_GAP);
            }
            return true;

        case APP_PUMP_FSM_STATE_GAP:
        {
            uint32_t gap_ms = (uint32_t)cfg->inter_gap_ms_x10 * 10U;
            if (elapsed >= gap_ms)
            {
                Set_State(ctx, APP_PUMP_FSM_STATE_DONE);
                LOG_INFO_WITH_ARG("pump fsm: zone %u DONE", (unsigned)ctx->valve);
                return false;
            }
            return true;
        }

        case APP_PUMP_FSM_STATE_DONE:
        case APP_PUMP_FSM_STATE_ERROR:
        default:
            return false;
    }
}
