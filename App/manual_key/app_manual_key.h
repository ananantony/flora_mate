/*
 * @File         : \code\App\manual_key\app_manual_key.h
 * @Description  : 手动模式 — 按键控制继电器与水泵
 */
#ifndef APP_MANUAL_KEY_H
#define APP_MANUAL_KEY_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"

void App_ManualKey_Init(void);
void App_ManualKey_Enter(void);
void App_ManualKey_Exit(void);
void App_ManualKey_OnEvent(const App_Event *e);

uint8_t App_ManualKey_GetCursor(void);
uint8_t App_ManualKey_GetPumpDuty(void);
bool    App_ManualKey_IsItemActive(void);
bool    App_ManualKey_IsPumpEditing(void);
bool    App_ManualKey_IsRelayOn(uint8_t idx);
bool App_ManualKey_ExitToBootWaitRequested(void);
void App_ManualKey_ClearExitRequest(void);

#endif /* APP_MANUAL_KEY_H */
