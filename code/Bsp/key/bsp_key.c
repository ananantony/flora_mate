/*
 * @File         : \code\Bsp\key\bsp_key.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 独立按键模块×4（按下低电平）；20ms 扫描 + 去抖 + LONG/HOLD
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
#include "bsp_key.h"
#include "bsp_tick.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#define BSP_KEY_SCAN_PERIOD_MS   (20U)   /**< 扫描周期 20 ms                  */
#define BSP_KEY_DEBOUNCE_SAMPLES (3U)    /**< 连续 3 次稳定 = 60 ms 去抖窗口  */
#define BSP_KEY_LONG_MS_DEFAULT  (1000U) /**< 默认长按阈值                    */
#define BSP_KEY_HOLD_MS_DEFAULT  (3000U) /**< 默认超长按阈值                  */

/**
 * @brief   单键 GPIO 映射
 */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} Bsp_Key_PinMap;

static const Bsp_Key_PinMap s_pin_map[BSP_KEY_NUM] = {
    [BSP_KEY_K1] = {KEY_K1_GPIO_Port, KEY_K1_Pin},
    [BSP_KEY_K2] = {KEY_K2_GPIO_Port, KEY_K2_Pin},
    [BSP_KEY_K3] = {KEY_K3_GPIO_Port, KEY_K3_Pin},
    [BSP_KEY_K4] = {KEY_K4_GPIO_Port, KEY_K4_Pin},
};

/**
 * @brief   单键运行时状态
 */
typedef struct
{
    uint8_t  last_raw;       /**< 上一次原始电平（0=松开 / 1=按下）          */
    uint8_t  stable_state;   /**< 稳定后认定的电平                           */
    uint8_t  debounce_count; /**< 与 last_raw 一致的连续采样次数             */
    uint32_t press_start_ms; /**< 进入"稳定按下"的时刻                       */
    bool     long_emitted;   /**< 本次按下是否已发过 LONG                    */
    bool     hold_emitted;   /**< 本次按下是否已发过 HOLD                    */
} Bsp_Key_State;

static Bsp_Key_State   s_state[BSP_KEY_NUM];
static Bsp_Key_EventCb s_cb;
static Bsp_Tick_Ticker s_ticker;
static uint16_t        s_long_ms = BSP_KEY_LONG_MS_DEFAULT;
static uint16_t        s_hold_ms = BSP_KEY_HOLD_MS_DEFAULT;

/**
 * @brief   读取一个按键的原始电平（电平归一化为"按下=1"）
 * @param   id  按键 ID
 * @retval  1=按下 / 0=松开
 * @note    独立按键模块：常态高、按下低（MCU 内部上拉，按下接 GND）。
 */
static uint8_t Bsp_Key_ReadRaw(Bsp_Key_Id id)
{
    return (HAL_GPIO_ReadPin(s_pin_map[id].port, s_pin_map[id].pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

/**
 * @brief   向回调投递一次事件
 * @param   id    按键 ID
 * @param   type  事件类型
 * @note    回调可能为 NULL（Init 时未指定），此时事件丢弃。
 */
static void Bsp_Key_Emit(Bsp_Key_Id id, Bsp_Key_EventType type)
{
    if (s_cb == 0)
    {
        return;
    }
    Bsp_Key_Event evt = {.id = id, .type = type};
    s_cb(&evt);
}

/**
 * @brief   按键模块初始化
 * @param   cb  事件回调
 */
void Bsp_Key_Init(Bsp_Key_EventCb cb)
{
    s_cb = cb;
    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++)
    {
        s_state[i].last_raw       = 0U;
        s_state[i].stable_state   = 0U;
        s_state[i].debounce_count = 0U;
        s_state[i].press_start_ms = 0U;
        s_state[i].long_emitted   = false;
        s_state[i].hold_emitted   = false;
    }
    Bsp_Tick_Ticker_Init(&s_ticker, BSP_KEY_SCAN_PERIOD_MS);
}

/**
 * @brief   设置 LONG / HOLD 阈值（带合法性钳位）
 * @param   long_ms  长按阈值（< 100 自动夹到 100）
 * @param   hold_ms  超长按阈值（必须 > long_ms，否则自动设为 long_ms + 500）
 */
void Bsp_Key_SetThresholds(uint16_t long_ms, uint16_t hold_ms)
{
    if (long_ms < 100U)
        long_ms = 100U;
    if (hold_ms < long_ms)
        hold_ms = (uint16_t)(long_ms + 500U);
    s_long_ms = long_ms;
    s_hold_ms = hold_ms;
}

/**
 * @brief   立即查询某键是否处于稳定按下状态
 * @param   id  按键 ID
 * @retval  true / false
 */
bool Bsp_Key_IsPressed(Bsp_Key_Id id)
{
    if ((uint32_t)id >= (uint32_t)BSP_KEY_NUM)
    {
        return false;
    }
    return (s_state[id].stable_state != 0U);
}

/**
 * @brief   按键扫描入口（主循环每轮调用）
 * @note    状态机：
 *          1) 与 last_raw 一致 → debounce_count++ 直到达到阈值；
 *          2) 阈值达到且与上次 stable_state 不同 → 触发边沿（press_start 或释放）；
 *          3) 持续按下时检测 LONG / HOLD 是否跨阈值；
 *          4) 若已发过 LONG/HOLD，释放时不再补发 SHORT。
 */
void Bsp_Key_Scan(void)
{
    if (!Bsp_Tick_Ticker_Due(&s_ticker))
    {
        return;
    }
    uint32_t now = Bsp_Tick_GetMs();

    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++)
    {
        Bsp_Key_State *st  = &s_state[i];
        uint8_t        raw = Bsp_Key_ReadRaw((Bsp_Key_Id)i);

        if (raw == st->last_raw)
        {
            if (st->debounce_count < BSP_KEY_DEBOUNCE_SAMPLES)
            {
                st->debounce_count++;
            }
        }
        else
        {
            st->debounce_count = 0U;
        }
        st->last_raw = raw;

        if (st->debounce_count >= BSP_KEY_DEBOUNCE_SAMPLES)
        {
            if (st->stable_state != raw)
            {
                st->stable_state = raw;
                if (raw == 1U)
                {
                    st->press_start_ms = now;
                    st->long_emitted   = false;
                    st->hold_emitted   = false;
                }
                else
                {
                    uint32_t held = now - st->press_start_ms;
                    if (!st->long_emitted && (held < s_long_ms))
                    {
                        Bsp_Key_Emit((Bsp_Key_Id)i, BSP_KEY_EVT_SHORT);
                    }
                }
            }
            else if (st->stable_state == 1U)
            {
                uint32_t held = now - st->press_start_ms;
                if (!st->long_emitted && (held >= s_long_ms))
                {
                    st->long_emitted = true;
                    Bsp_Key_Emit((Bsp_Key_Id)i, BSP_KEY_EVT_LONG);
                }
                if (!st->hold_emitted && (held >= s_hold_ms))
                {
                    st->hold_emitted = true;
                    Bsp_Key_Emit((Bsp_Key_Id)i, BSP_KEY_EVT_HOLD);
                }
            }
        }
    }
}
