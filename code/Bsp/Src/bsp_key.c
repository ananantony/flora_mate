/**
 * @file    bsp_key.c
 * @brief   4 按键扫描实现（去抖 + 长按 + 超长按）
 */
#include "bsp_key.h"
#include "bsp_tick.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#define BSP_KEY_SCAN_PERIOD_MS    (20U)
#define BSP_KEY_DEBOUNCE_SAMPLES  (3U)    /* 连续 3 次 = 60ms 稳定 */
#define BSP_KEY_LONG_MS_DEFAULT   (1000U)
#define BSP_KEY_HOLD_MS_DEFAULT   (3000U)

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} Bsp_Key_PinMap;

static const Bsp_Key_PinMap s_pin_map[BSP_KEY_NUM] = {
    [BSP_KEY_K1] = { KEY_K1_GPIO_Port, KEY_K1_Pin },
    [BSP_KEY_K2] = { KEY_K2_GPIO_Port, KEY_K2_Pin },
    [BSP_KEY_K3] = { KEY_K3_GPIO_Port, KEY_K3_Pin },
    [BSP_KEY_K4] = { KEY_K4_GPIO_Port, KEY_K4_Pin },
};

typedef struct {
    uint8_t  last_raw;          /* 上一次原始电平（0=未按 / 1=按下） */
    uint8_t  stable_state;      /* 稳定后认定的电平 */
    uint8_t  debounce_count;    /* 连续相同采样次数 */
    uint32_t press_start_ms;    /* 进入稳定按下的时刻 */
    bool     long_emitted;      /* 本次按下是否已发过 LONG */
    bool     hold_emitted;      /* 本次按下是否已发过 HOLD */
} Bsp_Key_State;

static Bsp_Key_State   s_state[BSP_KEY_NUM];
static Bsp_Key_EventCb s_cb;
static Bsp_Tick_Ticker s_ticker;
static uint16_t        s_long_ms = BSP_KEY_LONG_MS_DEFAULT;
static uint16_t        s_hold_ms = BSP_KEY_HOLD_MS_DEFAULT;

static uint8_t Bsp_Key_ReadRaw(Bsp_Key_Id id) {
    /* 按下 = GPIO 低电平（上拉到 VCC，按下接 GND） */
    return (HAL_GPIO_ReadPin(s_pin_map[id].port, s_pin_map[id].pin) == GPIO_PIN_RESET)
            ? 1U : 0U;
}

static void Bsp_Key_Emit(Bsp_Key_Id id, Bsp_Key_EventType type) {
    if (s_cb == 0) {
        return;
    }
    Bsp_Key_Event evt = { .id = id, .type = type };
    s_cb(&evt);
}

void Bsp_Key_Init(Bsp_Key_EventCb cb) {
    s_cb = cb;
    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++) {
        s_state[i].last_raw       = 0U;
        s_state[i].stable_state   = 0U;
        s_state[i].debounce_count = 0U;
        s_state[i].press_start_ms = 0U;
        s_state[i].long_emitted   = false;
        s_state[i].hold_emitted   = false;
    }
    Bsp_Tick_Ticker_Init(&s_ticker, BSP_KEY_SCAN_PERIOD_MS);
}

void Bsp_Key_SetThresholds(uint16_t long_ms, uint16_t hold_ms) {
    if (long_ms < 100U)  long_ms = 100U;
    if (hold_ms < long_ms) hold_ms = (uint16_t)(long_ms + 500U);
    s_long_ms = long_ms;
    s_hold_ms = hold_ms;
}

bool Bsp_Key_IsPressed(Bsp_Key_Id id) {
    if ((uint32_t)id >= (uint32_t)BSP_KEY_NUM) {
        return false;
    }
    return (s_state[id].stable_state != 0U);
}

void Bsp_Key_Scan(void) {
    if (!Bsp_Tick_Ticker_Due(&s_ticker)) {
        return;
    }
    uint32_t now = Bsp_Tick_GetMs();

    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++) {
        Bsp_Key_State *st = &s_state[i];
        uint8_t raw = Bsp_Key_ReadRaw((Bsp_Key_Id)i);

        /* 去抖：与上次一致就计数 */
        if (raw == st->last_raw) {
            if (st->debounce_count < BSP_KEY_DEBOUNCE_SAMPLES) {
                st->debounce_count++;
            }
        } else {
            st->debounce_count = 0U;
        }
        st->last_raw = raw;

        /* 状态稳定 → 边沿判定 */
        if (st->debounce_count >= BSP_KEY_DEBOUNCE_SAMPLES) {
            if (st->stable_state != raw) {
                st->stable_state = raw;
                if (raw == 1U) {
                    /* 下沿：开始计时 */
                    st->press_start_ms = now;
                    st->long_emitted   = false;
                    st->hold_emitted   = false;
                } else {
                    /* 上沿（释放） */
                    uint32_t held = now - st->press_start_ms;
                    if (!st->long_emitted && (held < s_long_ms)) {
                        Bsp_Key_Emit((Bsp_Key_Id)i, BSP_KEY_EVT_SHORT);
                    }
                    /* 已发过 LONG/HOLD 的就不再发 SHORT，避免重复 */
                }
            } else if (st->stable_state == 1U) {
                /* 仍按住中：检查 LONG / HOLD 阈值是否刚好跨过 */
                uint32_t held = now - st->press_start_ms;
                if (!st->long_emitted && (held >= s_long_ms)) {
                    st->long_emitted = true;
                    Bsp_Key_Emit((Bsp_Key_Id)i, BSP_KEY_EVT_LONG);
                }
                if (!st->hold_emitted && (held >= s_hold_ms)) {
                    st->hold_emitted = true;
                    Bsp_Key_Emit((Bsp_Key_Id)i, BSP_KEY_EVT_HOLD);
                }
            }
        }
    }
}
