/*
 * @File         : \code\App\manual_select\app_manual_select.h
 * @Description  : 手动模式入口选择（串口控制 / 按键控制）
 */
#ifndef APP_MANUAL_SELECT_H
#define APP_MANUAL_SELECT_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"

typedef enum
{
    APP_MANUAL_SELECT_RESULT_NONE = 0,
    APP_MANUAL_SELECT_RESULT_SERIAL,
    APP_MANUAL_SELECT_RESULT_KEY,
    APP_MANUAL_SELECT_RESULT_BOOT_WAIT
} App_ManualSelect_Result;

void App_ManualSelect_Init(void);
void App_ManualSelect_Enter(void);
void App_ManualSelect_Reset(void);
void App_ManualSelect_OnEvent(const App_Event *e);

uint8_t               App_ManualSelect_GetCursor(void);
uint8_t               App_ManualSelect_GetItemCount(void);
const char           *App_ManualSelect_GetItemLabel(uint8_t idx);
bool                  App_ManualSelect_ResultPending(void);
App_ManualSelect_Result App_ManualSelect_ConsumeResult(void);

#endif /* APP_MANUAL_SELECT_H */
