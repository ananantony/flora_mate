/*
 * @File         : \code\App\serial_debug\app_serial_debug.h
 * @Description  : USART1 串口调试命令解析与调试态入口
 */
#ifndef APP_SERIAL_DEBUG_H
#define APP_SERIAL_DEBUG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    APP_SERIAL_DEBUG_UI_IDLE = 0,
    APP_SERIAL_DEBUG_UI_RUNNING,
    APP_SERIAL_DEBUG_UI_OK,
    APP_SERIAL_DEBUG_UI_ERR,
    APP_SERIAL_DEBUG_UI_KEY_TEST /**< OLED 实时显示 K1~K4 按下状态 */
} App_SerialDebug_UiState;

void App_SerialDebug_Init(void);
void App_SerialDebug_Tick(void);

void App_SerialDebug_SetActive(bool active);
bool App_SerialDebug_IsActive(void);
bool App_SerialDebug_ConsumeEnterRequest(void);
void App_SerialDebug_ClearEnterRequest(void);

/** 供 OLED：当前命令行、结果行、UI 状态 */
void App_SerialDebug_GetUi(char *cmd_line, size_t cmd_cap, char *result_line, size_t result_cap,
                           App_SerialDebug_UiState *out_state);

/** 上电等待窗剩余毫秒（仅 SERIAL_WAIT 态有效） */
uint32_t App_SerialDebug_GetBootWaitRemainMs(void);

#endif /* APP_SERIAL_DEBUG_H */
