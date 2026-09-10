/*
 * @File         : \code\App\main_fsm\app_main_fsm.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-09-10 12:00:00
 * @Description  : 系统主状态机接口（Logo -> Idle -> Auto/Menu 产品路径）
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#ifndef APP_MAIN_FSM_H
#define APP_MAIN_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"
#include "floramate_types.h"

typedef enum
{
    APP_MAIN_FSM_STATE_LOGO = 0,
    APP_MAIN_FSM_STATE_IDLE,
    APP_MAIN_FSM_STATE_AUTO_RUN,
    APP_MAIN_FSM_STATE_DONE,
    APP_MAIN_FSM_STATE_MENU,
    APP_MAIN_FSM_STATE_SERIAL_DEBUG,
    APP_MAIN_FSM_STATE_ERROR
} App_Main_FsmState;

void App_Main_Fsm_Init(void);
void App_Main_Fsm_Tick(void);
void App_Main_Fsm_OnEvent(const App_Event *e);

App_Main_FsmState App_Main_Fsm_GetState(void);
uint8_t           App_Main_Fsm_CurrentZone(void);
uint32_t          App_Main_Fsm_EnterMs(void);
uint32_t          App_Main_Fsm_AutoRunStartMs(void);
Fm_ErrorCode      App_Main_Fsm_LastError(void);
bool              App_Main_Fsm_KeyInputEnabled(void);
void              App_Main_Fsm_SignalError(Fm_ErrorCode err);

/** 保留名：关输出并进入主菜单 */
void App_Main_Fsm_EnterBootWait(void);
/** 保留名：关输出并进入主菜单 */
void App_Main_Fsm_EnterManualSelect(void);

uint32_t App_Main_Fsm_GetIdleRemainMs(void);
uint8_t  App_Main_Fsm_GetIdleComboPercent(void);
bool     App_Main_Fsm_IsIdleTimerPaused(void);
bool     App_Main_Fsm_IsWateringStopped(void);

/* BootWait 旧 API = Idle 别名 */
uint32_t App_Main_Fsm_GetBootWaitRemainMs(void);
uint8_t  App_Main_Fsm_GetBootWaitComboPercent(void);
bool     App_Main_Fsm_IsBootWaitTimerPaused(void);

#endif /* APP_MAIN_FSM_H */
