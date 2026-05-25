/*
 * @File         : \code\Bsp\key\bsp_key.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 4 路独立按键模块（K1~K4=PA1~PA4，按下输出低电平）
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
#ifndef BSP_KEY_H
#define BSP_KEY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   按键 ID
 */
typedef enum
{
    BSP_KEY_K1 = 0, /**< PA1：向下移动 / 增加数值         */
    BSP_KEY_K2,     /**< PA2：向上移动 / 减少数值         */
    BSP_KEY_K3,     /**< PA3：确定 / 进入                 */
    BSP_KEY_K4,     /**< PA4：退出 / 返回                 */
    BSP_KEY_NUM
} Bsp_Key_Id;

/**
 * @brief   按键事件类型
 */
typedef enum
{
    BSP_KEY_EVT_NONE = 0, /**< 无事件                                       */
    BSP_KEY_EVT_SHORT,    /**< 短按：按下后稳定释放，按下时长 < long_ms     */
    BSP_KEY_EVT_LONG,     /**< 长按：按下时长跨过 long_ms 阈值（不等释放）   */
    BSP_KEY_EVT_HOLD      /**< 超长按：按下时长跨过 hold_ms 阈值             */
} Bsp_Key_EventType;

/**
 * @brief   按键事件载荷
 */
typedef struct
{
    Bsp_Key_Id        id;   /**< 按键 ID    */
    Bsp_Key_EventType type; /**< 事件类型   */
} Bsp_Key_Event;

/**
 * @brief   按键事件回调（main loop 上下文）
 * @param   evt  非 NULL，事件描述
 * @note    由 BSP 在 Scan() 内同步调用；回调内禁止再调用 Scan/IsPressed
 *          以免重入；建议只投递到 App_Event 队列然后立即返回。
 */
typedef void (*Bsp_Key_EventCb)(const Bsp_Key_Event *evt);

/**
 * @brief   按键模块初始化
 * @param   cb  事件回调；可传 NULL 表示仅做轮询不投递事件
 * @note    清零所有内部状态，复位上次扫描时间戳；建议在 OLED 初始化之后调用，
 *          这样按键事件触发的提示能立即可视化。
 */
void Bsp_Key_Init(Bsp_Key_EventCb cb);

/**
 * @brief   按键扫描入口（主循环每轮调用）
 * @note    1) 内部按 20 ms 节拍限速，调用过密自动跳过；
 *          2) 同一物理按键事件只触发一次（避免 LONG 后又触发 SHORT）；
 *          3) ISR 安全：本函数仅在主循环调用，禁止 ISR 调用。
 */
void Bsp_Key_Scan(void);

/**
 * @brief   运行时调整长按/超长按阈值
 * @param   long_ms  长按阈值（毫秒，建议 800 ~ 1500）
 * @param   hold_ms  超长按阈值（毫秒，建议 ≥ 3000）
 * @note    菜单"按键阈值"项设置后立即调用本函数；保证 hold_ms > long_ms。
 */
void Bsp_Key_SetThresholds(uint16_t long_ms, uint16_t hold_ms);

/**
 * @brief   立即查询某键当前是否处于按下状态
 * @param   id  按键 ID
 * @retval  true=按下 / false=释放（含非法 id，统一返回 false）
 * @note    适合"菜单中长按确认"等需要轮询硬件电平的场景；
 *          已内嵌一次 20 ms 软件去抖。
 */
bool Bsp_Key_IsPressed(Bsp_Key_Id id);

#endif /* BSP_KEY_H */
