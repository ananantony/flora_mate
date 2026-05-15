/**
 * @file    bsp_relay.c
 * @brief   6 路继电器封装实现
 */
#include "bsp_relay.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* GPIO 物理低 = 继电器吸合（低电平触发） */
#define RELAY_ON_LEVEL   GPIO_PIN_RESET
#define RELAY_OFF_LEVEL  GPIO_PIN_SET

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} Bsp_Relay_PinMap;

static const Bsp_Relay_PinMap s_pin_map[BSP_RELAY_CHANNEL_NUM] = {
    [BSP_RELAY_VALVE_1 ] = { RLY_VALVE_Z1_GPIO_Port, RLY_VALVE_Z1_Pin },
    [BSP_RELAY_VALVE_2 ] = { RLY_VALVE_Z2_GPIO_Port, RLY_VALVE_Z2_Pin },
    [BSP_RELAY_VALVE_3 ] = { RLY_VALVE_Z3_GPIO_Port, RLY_VALVE_Z3_Pin },
    [BSP_RELAY_VALVE_4 ] = { RLY_VALVE_Z4_GPIO_Port, RLY_VALVE_Z4_Pin },
    [BSP_RELAY_MAIN_CH5] = { RLY_MAIN_CH5_GPIO_Port, RLY_MAIN_CH5_Pin },
    [BSP_RELAY_RSV_CH6 ] = { RLY_RSV_CH6_GPIO_Port,  RLY_RSV_CH6_Pin  },
};

static bool s_state[BSP_RELAY_CHANNEL_NUM];

static void Bsp_Relay_WriteRaw(Bsp_Relay_Channel ch, bool is_on) {
    HAL_GPIO_WritePin(s_pin_map[ch].port, s_pin_map[ch].pin,
                      is_on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
    s_state[ch] = is_on;
}

static bool Bsp_Relay_AnyValveOn(void) {
    return s_state[BSP_RELAY_VALVE_1] || s_state[BSP_RELAY_VALVE_2]
        || s_state[BSP_RELAY_VALVE_3] || s_state[BSP_RELAY_VALVE_4];
}

void Bsp_Relay_Init(void) {
    /* GPIO 已在 MX_GPIO_Init() 中配为推挽 + 初始高（释放）。
     * 这里只刷新软件影子寄存器，不重复写 GPIO，避免上电瞬间多余电平翻转。 */
    for (uint32_t i = 0U; i < (uint32_t)BSP_RELAY_CHANNEL_NUM; i++) {
        s_state[i] = false;
        HAL_GPIO_WritePin(s_pin_map[i].port, s_pin_map[i].pin, RELAY_OFF_LEVEL);
    }
}

Fm_ErrorCode Bsp_Relay_Set(Bsp_Relay_Channel ch, bool is_on) {
    if (!Bsp_Relay_IsValidChannel(ch)) {
        return FM_ERR_012_INTERLOCK;
    }

    /* CH5 软互锁：吸合前必须至少一路阀已开 */
    if ((ch == BSP_RELAY_MAIN_CH5) && is_on && !Bsp_Relay_AnyValveOn()) {
        return FM_ERR_012_INTERLOCK;
    }

    /* 任一分阀被请求 OFF 时，若它是当前最后一路开着的阀，
     * 且 CH5 仍然吸合，则拒绝（应先关 CH5 再关阀）。 */
    if ((ch >= BSP_RELAY_VALVE_1) && (ch <= BSP_RELAY_VALVE_4)
        && !is_on && s_state[BSP_RELAY_MAIN_CH5]) {
        bool any_other_valve_on = false;
        for (uint32_t i = 0U; i < 4U; i++) {
            if (((Bsp_Relay_Channel)i != ch) && s_state[i]) {
                any_other_valve_on = true;
                break;
            }
        }
        if (!any_other_valve_on) {
            return FM_ERR_012_INTERLOCK;
        }
    }

    Bsp_Relay_WriteRaw(ch, is_on);
    return FM_OK;
}

bool Bsp_Relay_Get(Bsp_Relay_Channel ch) {
    if (!Bsp_Relay_IsValidChannel(ch)) {
        return false;
    }
    return s_state[ch];
}

void Bsp_Relay_AllOff(void) {
    /* 顺序：先 CH5，再分阀；保证关泵优先 */
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, false);
    for (uint32_t i = 0U; i < 4U; i++) {
        Bsp_Relay_WriteRaw((Bsp_Relay_Channel)i, false);
    }
    Bsp_Relay_WriteRaw(BSP_RELAY_RSV_CH6, false);
}

void Bsp_Relay_MainOffForce(void) {
    /* 任何路径下都可调用的"硬刹车"，无任何前置检查 */
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_1, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_2, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_3, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_4, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_RSV_CH6, false);
}

void Bsp_Relay_Selftest(uint16_t pulse_ms) {
    /* 4 路阀依次咔嗒一下（不开 CH5，水泵无电） */
    for (uint32_t i = 0U; i < 4U; i++) {
        Bsp_Relay_WriteRaw((Bsp_Relay_Channel)i, true);
        HAL_Delay(pulse_ms);
        Bsp_Relay_WriteRaw((Bsp_Relay_Channel)i, false);
        HAL_Delay(pulse_ms);
    }
    /* CH5 自检：单独短脉冲（此时所有阀已释放，水泵也不通水管路 → 安全） */
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, true);
    HAL_Delay(pulse_ms);
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, false);
    HAL_Delay(pulse_ms);
}
