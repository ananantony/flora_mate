/**
 * @file    app_pump_fsm.c
 * @brief   单路浇灌子状态机实现
 * @note    顺序：阀ON → 等机械→ CH5 ON → 稳压 → 阶梯PWM → ramp → CH5 OFF → 阀OFF → 静默 → DONE
 *          关键铁律：先开分阀再开 CH5；先关 CH5 再关分阀（防水锤/空抽）。
 */
#include "app_pump_fsm.h"
#include "app_config.h"
#include "app_log.h"
#include "bsp_pump_pwm.h"
#include "bsp_relay.h"
#include "bsp_tick.h"

#define VALVE_OPEN_SETTLE_MS   (200U)
#define MAIN_STABILIZE_MS      (100U)
#define RAMP_DOWN_MS           (500U)
#define CLOSE_MAIN_RELAX_MS    (500U)
#define CLOSE_VALVE_MS         (50U)

static void Set_State(App_Pump_FsmCtx *ctx, App_Pump_FsmState st) {
    ctx->state          = st;
    ctx->state_enter_ms = Bsp_Tick_GetMs();
}

static Bsp_Relay_Channel Valve_Channel(Fm_ValveIndex v) {
    /* Fm_ValveIndex 与 BSP_RELAY_VALVE_1..4 一一对应 */
    return (Bsp_Relay_Channel)v;
}

void App_Pump_Fsm_Start(App_Pump_FsmCtx *ctx, Fm_ValveIndex v) {
    ctx->valve          = v;
    ctx->step_idx       = 0U;
    ctx->max_duty_seen  = 0U;
    ctx->last_err       = FM_OK;
    Set_State(ctx, APP_PUMP_FSM_STATE_INIT);
    LOGI("pump fsm: start zone %u", (unsigned)v);
}

void App_Pump_Fsm_Abort(App_Pump_FsmCtx *ctx, Fm_ErrorCode reason) {
    Bsp_Pump_Pwm_Stop();
    Bsp_Relay_MainOffForce();
    ctx->last_err = reason;
    Set_State(ctx, (reason == FM_OK) ? APP_PUMP_FSM_STATE_DONE
                                      : APP_PUMP_FSM_STATE_ERROR);
    LOGW("pump fsm: abort zone %u reason=0x%02X", (unsigned)ctx->valve, (unsigned)reason);
}

void App_Pump_Fsm_Pause(App_Pump_FsmCtx *ctx) {
    /* 简化：暂停 = 立即 ramp_down + 关 CH5；恢复时重新进入当前阶梯
     * V1.0 仅记录意图，正式实现留 Phase 2。
     */
    (void)ctx;
}
void App_Pump_Fsm_Resume(App_Pump_FsmCtx *ctx) { (void)ctx; }

App_Pump_FsmState App_Pump_Fsm_State(const App_Pump_FsmCtx *ctx) { return ctx->state; }
uint8_t           App_Pump_Fsm_CurrentStep(const App_Pump_FsmCtx *ctx) { return ctx->step_idx; }

bool App_Pump_Fsm_Tick(App_Pump_FsmCtx *ctx) {
    const App_Config *cfg = App_Config_Get();
    uint32_t elapsed = Bsp_Tick_ElapsedMs(ctx->state_enter_ms);

    /* 单路硬超时（不管在哪个子状态） */
    uint32_t valve_run_total = elapsed; /* 简化：以当前状态停留判断；累计统计在调用方 */
    (void)valve_run_total;

    switch (ctx->state) {
    case APP_PUMP_FSM_STATE_IDLE:
        return false;

    case APP_PUMP_FSM_STATE_INIT: {
        /* 进入即吸合 CHx 阀；之后等机械到位 */
        Bsp_Relay_Channel ch = Valve_Channel(ctx->valve);
        if (elapsed == 0U) {
            if (Bsp_Relay_Set(ch, true) != FM_OK) {
                App_Pump_Fsm_Abort(ctx, FM_ERR_012_INTERLOCK);
                return false;
            }
        }
        if (elapsed >= VALVE_OPEN_SETTLE_MS) {
            Set_State(ctx, APP_PUMP_FSM_STATE_OPEN_MAIN);
        }
        return true;
    }

    case APP_PUMP_FSM_STATE_OPEN_MAIN:
        if (elapsed == 0U) {
            if (Bsp_Relay_Set(BSP_RELAY_MAIN_CH5, true) != FM_OK) {
                App_Pump_Fsm_Abort(ctx, FM_ERR_012_INTERLOCK);
                return false;
            }
        }
        if (elapsed >= MAIN_STABILIZE_MS) {
            ctx->step_idx = 0U;
            Set_State(ctx, APP_PUMP_FSM_STATE_STEP);
        }
        return true;

    case APP_PUMP_FSM_STATE_STEP: {
        if (ctx->step_idx >= cfg->step_count) {
            Set_State(ctx, APP_PUMP_FSM_STATE_RAMP_DOWN);
            return true;
        }
        /* 进入新档位时立即设 PWM */
        if (elapsed == 0U) {
            uint8_t duty = cfg->step_duty[ctx->step_idx];
            Fm_ErrorCode err = Bsp_Pump_Pwm_SetDutyPercent(duty);
            if (err != FM_OK) {
                App_Pump_Fsm_Abort(ctx, err);
                return false;
            }
            if (duty > ctx->max_duty_seen) {
                ctx->max_duty_seen = duty;
            }
            LOGI("pump fsm: zone %u step %u duty=%u%%",
                 (unsigned)ctx->valve, (unsigned)ctx->step_idx, (unsigned)duty);
        }
        uint32_t dur_ms = (uint32_t)cfg->step_seconds[ctx->step_idx] * 1000U;
        if (elapsed >= dur_ms) {
            ctx->step_idx++;
            Set_State(ctx, APP_PUMP_FSM_STATE_STEP);  /* 重新进入下一档 */
        }
        return true;
    }

    case APP_PUMP_FSM_STATE_RAMP_DOWN:
        if (elapsed == 0U) {
            Bsp_Pump_Pwm_StartRampDown(RAMP_DOWN_MS);
        }
        if (!Bsp_Pump_Pwm_RampTick()) {
            /* 已降到 0 */
            Set_State(ctx, APP_PUMP_FSM_STATE_CLOSE_MAIN);
        }
        return true;

    case APP_PUMP_FSM_STATE_CLOSE_MAIN:
        if (elapsed == 0U) {
            /* 释放 CH5：因为分阀仍开着，互锁允许 */
            (void)Bsp_Relay_Set(BSP_RELAY_MAIN_CH5, false);
        }
        if (elapsed >= CLOSE_MAIN_RELAX_MS) {
            Set_State(ctx, APP_PUMP_FSM_STATE_CLOSE_VALVE);
        }
        return true;

    case APP_PUMP_FSM_STATE_CLOSE_VALVE:
        if (elapsed == 0U) {
            Bsp_Relay_Channel ch = Valve_Channel(ctx->valve);
            /* 此时 CH5=OFF，互锁允许关阀 */
            (void)Bsp_Relay_Set(ch, false);
        }
        if (elapsed >= CLOSE_VALVE_MS) {
            Set_State(ctx, APP_PUMP_FSM_STATE_GAP);
        }
        return true;

    case APP_PUMP_FSM_STATE_GAP: {
        uint32_t gap_ms = (uint32_t)cfg->inter_gap_ms_x10 * 10U;
        if (elapsed >= gap_ms) {
            Set_State(ctx, APP_PUMP_FSM_STATE_DONE);
            LOGI("pump fsm: zone %u DONE", (unsigned)ctx->valve);
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
