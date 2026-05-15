/**
 * @file    app_main_fsm.h
 * @brief   系统主状态机
 * @note    与《软件设计方案_V1.0.md》§ 3 状态机一致。
 */
#ifndef APP_MAIN_FSM_H
#define APP_MAIN_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"
#include "floramate_types.h"

typedef enum {
    APP_MAIN_FSM_STATE_BOOT = 0,
    APP_MAIN_FSM_STATE_SELFTEST,
    APP_MAIN_FSM_STATE_IDLE_3S,
    APP_MAIN_FSM_STATE_AUTO_RUN,
    APP_MAIN_FSM_STATE_MENU,
    APP_MAIN_FSM_STATE_DONE,
    APP_MAIN_FSM_STATE_ERROR,
    APP_MAIN_FSM_STATE_SLEEP
} App_Main_FsmState;

void              App_Main_Fsm_Init(void);
void              App_Main_Fsm_Tick(void);                 /* 主循环周期调用 */
void              App_Main_Fsm_OnEvent(const App_Event *e);

App_Main_FsmState App_Main_Fsm_GetState(void);
uint8_t           App_Main_Fsm_CurrentZone(void);          /* 1..4 = 第几路在执行 */
uint32_t          App_Main_Fsm_EnterMs(void);              /* 当前状态进入时刻 */
uint32_t          App_Main_Fsm_AutoRunStartMs(void);       /* SYS_AUTO_RUN 起点 */
Fm_ErrorCode      App_Main_Fsm_LastError(void);

void              App_Main_Fsm_SignalError(Fm_ErrorCode err);

#endif /* APP_MAIN_FSM_H */
