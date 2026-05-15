/**
 * @file    app_event.h
 * @brief   8 槽位事件队列（FIFO）
 * @note    生产者：Bsp_Key 回调（主循环）、串口命令解析（主循环）。
 *          消费者：App_Main_Fsm（主循环）。生产/消费均在主循环，无需关中断。
 */
#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_key.h"

#define APP_EVENT_QUEUE_DEPTH  (8U)

typedef enum {
    /* 按键事件 —— 与按键 ID + 类型组合，便于一处分发 */
    APP_EVENT_KEY_K1_SHORT = 0,
    APP_EVENT_KEY_K2_SHORT,
    APP_EVENT_KEY_K3_SHORT,
    APP_EVENT_KEY_K4_SHORT,
    APP_EVENT_KEY_K1_LONG,
    APP_EVENT_KEY_K2_LONG,
    APP_EVENT_KEY_K3_LONG,
    APP_EVENT_KEY_K4_LONG,
    APP_EVENT_KEY_K1_HOLD,
    APP_EVENT_KEY_K2_HOLD,
    APP_EVENT_KEY_K3_HOLD,
    APP_EVENT_KEY_K4_HOLD,

    /* 串口命令事件 —— 解析后投递 */
    APP_EVENT_CMD_HELP,
    APP_EVENT_CMD_STATUS,
    APP_EVENT_CMD_RESET,
    APP_EVENT_CMD_CFG_SAVE,
    APP_EVENT_CMD_CFG_LOAD,
    APP_EVENT_CMD_CFG_RESET,
    APP_EVENT_CMD_CFG_DUMP,
    APP_EVENT_CMD_CFG_GET_SET,   /* 参数取/设：详细字段从 line buffer 二次解析 */

    /* 系统内部事件 */
    APP_EVENT_TIMEOUT,            /* 子状态机超时通知 */
    APP_EVENT_NONE                /* 哨兵 */
} App_Event_Id;

typedef struct {
    App_Event_Id id;
    uint32_t     param;           /* 通用参数槽 */
} App_Event;

void App_Event_Init(void);
bool App_Event_Post(App_Event_Id id, uint32_t param);
bool App_Event_Pop(App_Event *out);
bool App_Event_IsEmpty(void);

/* 把 Bsp_Key 的事件转成 App_Event 投递（注册到 Bsp_Key_Init） */
void App_Event_PostKey(const Bsp_Key_Event *kevt);

#endif /* APP_EVENT_H */
