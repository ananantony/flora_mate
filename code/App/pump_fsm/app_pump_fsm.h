/*
 * @File         : \code\App\pump_fsm\app_pump_fsm.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-27 00:00:00
 * @Description  : 单路浇灌子状态机接口（PWM 阶梯执行，固态 MOSFET 版）
 * @note         状态时序（固态版，无继电器 CH1）：
 *               阀 ON → 等稳定 → PUMP_EN ON → 等稳压 → 阶梯 PWM → ramp → PUMP_EN OFF → 阀 OFF → 静默 → DONE
 *               关键铁律：先开分阀再使能水泵；先关水泵再关阀（由互锁强制）。
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
#ifndef APP_PUMP_FSM_H
#define APP_PUMP_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"
#include "bsp_valve.h"

/**
 * @brief   单路浇灌子状态机的状态枚举
 * @note    OPEN_PUMP_EN / CLOSE_PUMP_EN 取代了原继电器方案的 OPEN_MAIN / CLOSE_MAIN；
 *          驱动对象从机械继电器 CH1 改为固态 PUMP_EN MOSFET 通道，语义完全等价。
 */
typedef enum
{
    APP_PUMP_FSM_STATE_IDLE = 0,       /**< 未运行                                   */
    APP_PUMP_FSM_STATE_INIT,           /**< 开分阀（CHx MOS）→ 等机械到位             */
    APP_PUMP_FSM_STATE_OPEN_PUMP_EN,   /**< 使能水泵 12V（PUMP_EN MOS） → 等稳压      */
    APP_PUMP_FSM_STATE_STEP,           /**< 阶梯执行（内部维护 step_idx）              */
    APP_PUMP_FSM_STATE_RAMP_DOWN,      /**< PWM 线性降到 0                            */
    APP_PUMP_FSM_STATE_CLOSE_PUMP_EN,  /**< 关断水泵 12V（PUMP_EN MOS） → 卸压        */
    APP_PUMP_FSM_STATE_CLOSE_VALVE,    /**< 关断分阀（CHx MOS）                       */
    APP_PUMP_FSM_STATE_GAP,            /**< 路间静默                                  */
    APP_PUMP_FSM_STATE_DONE,           /**< 本路完成                                  */
    APP_PUMP_FSM_STATE_ERROR           /**< 异常退出（已 force off）                   */
} App_Pump_FsmState;

/**
 * @brief   子状态机上下文
 */
typedef struct
{
    Fm_ValveIndex     valve;          /**< 当前驱动的阀（Z1..Z5）             */
    App_Pump_FsmState state;          /**< 当前状态                           */
    uint32_t          state_enter_ms; /**< 进入当前状态的时刻                  */
    uint8_t           step_idx;       /**< 当前档位 [0..step_count-1]         */
    uint8_t           max_duty_seen;  /**< 本次运行见过的最高占空比（统计）    */
    Fm_ErrorCode      last_err;       /**< 最近一次错误码（ERROR 状态时填充）  */
} App_Pump_FsmCtx;

/**
 * @brief   启动单路浇灌
 * @param   ctx  上下文（必须由调用者持有）
 * @param   v    阀通道（FM_VALVE_Z1..Z5）
 * @note    复位 step_idx=0、max_duty=0、last_err=FM_OK，跳到 INIT 状态。
 */
void App_Pump_Fsm_Start(App_Pump_FsmCtx *ctx, Fm_ValveIndex v);

/**
 * @brief   推进一步（主循环每轮调用）
 * @param   ctx  上下文
 * @retval  true   仍在进行中
 * @retval  false  已 DONE 或 ERROR 终态，主 FSM 应推进到下一路
 */
bool App_Pump_Fsm_Tick(App_Pump_FsmCtx *ctx);

/**
 * @brief   紧急停止（K3 STOP 或系统超时）
 * @param   ctx     上下文
 * @param   reason  错误码（FM_OK = 主动 STOP；其他 = 异常）
 * @note    立即调用 Bsp_Valve_ForceAllOff + Bsp_Pump_Pwm_Stop，跳转到 ERROR。
 */
void App_Pump_Fsm_Abort(App_Pump_FsmCtx *ctx, Fm_ErrorCode reason);

/**
 * @brief   暂停（保留：当前 V1.0 未启用）
 */
void App_Pump_Fsm_Pause(App_Pump_FsmCtx *ctx);

/**
 * @brief   恢复（保留：当前 V1.0 未启用）
 */
void App_Pump_Fsm_Resume(App_Pump_FsmCtx *ctx);

/**
 * @brief   读取当前状态（用于 OLED 显示）
 */
App_Pump_FsmState App_Pump_Fsm_State(const App_Pump_FsmCtx *ctx);

/**
 * @brief   读取当前档位序号（用于 OLED 显示）
 */
uint8_t App_Pump_Fsm_CurrentStep(const App_Pump_FsmCtx *ctx);

#endif /* APP_PUMP_FSM_H */
