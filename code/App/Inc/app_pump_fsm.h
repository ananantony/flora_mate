/**
 * @file    app_pump_fsm.h
 * @brief   单路浇灌子状态机（PWM 阶梯）
 * @note    与《软件设计方案_V1.0.md》§ 4 对齐。
 *          非阻塞实现：每次 Tick 检查当前状态超时，状态切换由 Tick 推进。
 */
#ifndef APP_PUMP_FSM_H
#define APP_PUMP_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"
#include "bsp_relay.h"

typedef enum {
    APP_PUMP_FSM_STATE_IDLE = 0,         /* 未运行 */
    APP_PUMP_FSM_STATE_INIT,             /* 吸合 CHx 阀 → 等阀机械到位 */
    APP_PUMP_FSM_STATE_OPEN_MAIN,        /* 吸合 CH5 → 等稳压 */
    APP_PUMP_FSM_STATE_STEP,             /* 阶梯执行（内部维护 step_idx） */
    APP_PUMP_FSM_STATE_RAMP_DOWN,        /* PWM 线性降到 0 */
    APP_PUMP_FSM_STATE_CLOSE_MAIN,       /* 释放 CH5 → 卸压 */
    APP_PUMP_FSM_STATE_CLOSE_VALVE,      /* 释放 CHx 阀 */
    APP_PUMP_FSM_STATE_GAP,              /* 路间静默 */
    APP_PUMP_FSM_STATE_DONE,             /* 本路完成 */
    APP_PUMP_FSM_STATE_ERROR             /* 异常退出（已 force off） */
} App_Pump_FsmState;

typedef struct {
    Fm_ValveIndex     valve;
    App_Pump_FsmState state;
    uint32_t          state_enter_ms;
    uint8_t           step_idx;           /* 0..step_count-1 */
    uint8_t           max_duty_seen;      /* 用于统计 */
    Fm_ErrorCode      last_err;
} App_Pump_FsmCtx;

void App_Pump_Fsm_Start(App_Pump_FsmCtx *ctx, Fm_ValveIndex v);

/* 推进一步；返回 true=进行中，false=已 DONE 或 ERROR */
bool App_Pump_Fsm_Tick(App_Pump_FsmCtx *ctx);

/* 紧急停止（K3 或异常）：直接 force off 并跳到 ERROR/DONE */
void App_Pump_Fsm_Abort(App_Pump_FsmCtx *ctx, Fm_ErrorCode reason);

/* 暂停/恢复（K2，可选实现） */
void App_Pump_Fsm_Pause(App_Pump_FsmCtx *ctx);
void App_Pump_Fsm_Resume(App_Pump_FsmCtx *ctx);

App_Pump_FsmState App_Pump_Fsm_State(const App_Pump_FsmCtx *ctx);
uint8_t           App_Pump_Fsm_CurrentStep(const App_Pump_FsmCtx *ctx);

#endif /* APP_PUMP_FSM_H */
