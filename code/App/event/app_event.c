/*
 * @File         : \code\App\event\app_event.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 8 槽位事件队列实现（生产/消费均在主循环上下文）
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
#include "app_event.h"

static App_Event s_queue[APP_EVENT_QUEUE_DEPTH]; /**< 环形数组          */
static uint8_t   s_head;                         /**< 生产者下次写入位置 */
static uint8_t   s_tail;                         /**< 消费者下次读取位置 */
static uint8_t   s_count;                        /**< 已占用槽位数       */

/**
 * @brief   事件队列初始化
 */
void App_Event_Init(void)
{
    s_head  = 0U;
    s_tail  = 0U;
    s_count = 0U;
}

/**
 * @brief   查询队列是否为空
 */
bool App_Event_IsEmpty(void)
{
    return (s_count == 0U);
}

/**
 * @brief   投递一个事件
 * @param   id     事件 ID
 * @param   param  事件参数
 * @retval  true=入队 / false=队列已满（直接丢弃，保证按键扫描不被阻塞）
 */
bool App_Event_Post(App_Event_Id id, uint32_t param)
{
    if (s_count >= APP_EVENT_QUEUE_DEPTH)
    {
        return false;
    }
    s_queue[s_head].id    = id;
    s_queue[s_head].param = param;
    s_head                = (uint8_t)((s_head + 1U) % APP_EVENT_QUEUE_DEPTH);
    s_count++;
    return true;
}

/**
 * @brief   取出一个事件
 * @param   out  输出参数；NULL 时仅消费不拷贝
 * @retval  true=取到 / false=队列为空
 */
bool App_Event_Pop(App_Event *out)
{
    if (s_count == 0U)
    {
        return false;
    }
    if (out != 0)
    {
        *out = s_queue[s_tail];
    }
    s_tail = (uint8_t)((s_tail + 1U) % APP_EVENT_QUEUE_DEPTH);
    s_count--;
    return true;
}

/**
 * @brief   按键事件 → 应用层事件
 * @param   kevt  非 NULL；按键 ID + 事件类型映射到 APP_EVENT_KEY_xx 后投递
 * @note    因为 APP_EVENT_KEY_K1_SHORT..K4_SHORT 是连续 4 个、LONG / HOLD 同理，
 *          这里用基址 + 偏移直接算出枚举值，省掉一大段 switch。
 */
void App_Event_PostKey(const Bsp_Key_Event *kevt)
{
    if (kevt == 0)
        return;
    App_Event_Id id = APP_EVENT_NONE;
    switch (kevt->type)
    {
        case BSP_KEY_EVT_SHORT:
            id = (App_Event_Id)(APP_EVENT_KEY_K1_SHORT + (uint32_t)kevt->id);
            break;
        case BSP_KEY_EVT_LONG:
            id = (App_Event_Id)(APP_EVENT_KEY_K1_LONG + (uint32_t)kevt->id);
            break;
        case BSP_KEY_EVT_HOLD:
            id = (App_Event_Id)(APP_EVENT_KEY_K1_HOLD + (uint32_t)kevt->id);
            break;
        default:
            return;
    }
    (void)App_Event_Post(id, 0U);
}
