/*
 * @File         : \code\App\event\app_event.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 应用层 FIFO 事件队列接口（按键 / 串口命令 / 系统事件）
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
#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_key.h"

#define APP_EVENT_QUEUE_DEPTH (8U) /**< 队列深度（足够 800 ms / 100 ms 调度间隔） */

/**
 * @brief   应用层事件 ID
 * @note    生产者：按键回调（Bsp_Key）、串口命令解析；
 *          消费者：App_Main_Fsm。生产与消费均在主循环上下文，无需关中断。
 */
typedef enum
{
    /* 按键事件（与按键 ID + 类型组合，便于一处分发） */
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

    /* 串口命令事件（解析后投递） */
    APP_EVENT_CMD_HELP,
    APP_EVENT_CMD_STATUS,
    APP_EVENT_CMD_RESET,
    APP_EVENT_CMD_CFG_SAVE,
    APP_EVENT_CMD_CFG_LOAD,
    APP_EVENT_CMD_CFG_RESET,
    APP_EVENT_CMD_CFG_DUMP,
    APP_EVENT_CMD_CFG_GET_SET, /**< 参数取/设；详细字段从 line buffer 二次解析 */

    /* 系统内部事件 */
    APP_EVENT_TIMEOUT, /**< 子状态机超时通知 */
    APP_EVENT_NONE     /**< 哨兵：表示无事件  */
} App_Event_Id;

/**
 * @brief   事件载荷
 */
typedef struct
{
    App_Event_Id id;    /**< 事件 ID                */
    uint32_t     param; /**< 通用参数槽（含义见 id） */
} App_Event;

/**
 * @brief   事件队列初始化
 * @note    清零 head/tail 指针；建议在 App_Init 早期阶段调用。
 */
void App_Event_Init(void);

/**
 * @brief   投递一个事件
 * @param   id     事件 ID
 * @param   param  事件参数（含义视 id 而定）
 * @retval  true   投递成功
 * @retval  false  队列已满（被丢弃，由 App_Log 警告）
 */
bool App_Event_Post(App_Event_Id id, uint32_t param);

/**
 * @brief   取出一个事件
 * @param   out  输出参数（不能为 NULL）
 * @retval  true=取到 / false=队列为空
 */
bool App_Event_Pop(App_Event *out);

/**
 * @brief   查询队列是否为空
 * @retval  true / false
 */
bool App_Event_IsEmpty(void);

/**
 * @brief   按键事件 → 应用层事件桥接（注册到 Bsp_Key_Init 回调）
 * @param   kevt  按键事件
 * @note    内部把 (key_id, type) 映射到对应 APP_EVENT_KEY_xx 然后投递；
 *          因此应用层只关心 App_Event_Id，不再处理 BSP 层细节。
 */
void App_Event_PostKey(const Bsp_Key_Event *kevt);

#endif /* APP_EVENT_H */
