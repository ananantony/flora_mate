/*
 * @File         : \code\Bsp\valve\bsp_valve.c
 * @Author       : tonymeng
 * @Date         : 2026-05-27
 * @Description  : 固态驱动层实现：TLP281-4 光耦 + N-MOS（AO3400）阀驱动 + 水泵使能
 * @note         GPIO 逻辑：High = MOSFET 导通 = 负载 ON（通过 NPN 中间级二次反相后恢复正逻辑）。
 *               所有通道 GPIO 复位初始均为 Low → 失效安全（负载断电）。
 *               水泵调速（PWM）仍由 bsp_pump_pwm 模块通过 PA0 TIM2_CH1 控制，本模块不涉及。
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#include "bsp_valve.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#define VALVE_ON_LEVEL  GPIO_PIN_SET
#define VALVE_OFF_LEVEL GPIO_PIN_RESET

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} Bsp_Valve_PinMap;

/**
 * @brief   GPIO 引脚映射表（与 main.h 标签及 § 5 引脚分配表完全对齐）
 */
static const Bsp_Valve_PinMap s_pin_map[BSP_VALVE_CHANNEL_NUM] = {
    [BSP_VALVE_Z1]      = {VALVE_Z1_PB12_GPIO_Port, VALVE_Z1_PB12_Pin},       /* PB12 左侧1 */
    [BSP_VALVE_Z2]      = {VALVE_Z2_PB13_GPIO_Port, VALVE_Z2_PB13_Pin},       /* PB13 左侧2 */
    [BSP_VALVE_Z3]      = {VALVE_Z3_PB14_GPIO_Port, VALVE_Z3_PB14_Pin},       /* PB14 左侧3 */
    [BSP_VALVE_Z4]      = {VALVE_Z4_PB15_GPIO_Port, VALVE_Z4_PB15_Pin},       /* PB15 左侧4 */
    [BSP_VALVE_Z5_RSV]  = {VALVE_Z5_RSV_PA8_GPIO_Port, VALVE_Z5_RSV_PA8_Pin}, /* PA8  左侧5 */
    [BSP_VALVE_PUMP_EN] = {PUMP_EN_PB1_GPIO_Port, PUMP_EN_PB1_Pin},           /* PB1  右侧35 */
    [BSP_VALVE_RSV]     = {MOSFET_RSV_PB0_GPIO_Port, MOSFET_RSV_PB0_Pin},     /* PB0  右侧34 */
};

static bool s_state[BSP_VALVE_CHANNEL_NUM];

/**
 * @brief   直接写 GPIO，同步更新软件状态缓存
 */
static void Bsp_Valve_WriteRaw(Bsp_Valve_Channel ch, bool is_on)
{
    GPIO_TypeDef *port = s_pin_map[ch].port;
    uint32_t      pin  = s_pin_map[ch].pin;

    if (is_on)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BSRR = pin << 16U;
    }
    s_state[ch] = is_on;
}

/**
 * @brief   检查是否有任意一路阀（Z1..Z5）处于开启状态
 */
static bool Any_Valve_On(void)
{
    return s_state[BSP_VALVE_Z1] || s_state[BSP_VALVE_Z2] || s_state[BSP_VALVE_Z3] ||
           s_state[BSP_VALVE_Z4] || s_state[BSP_VALVE_Z5_RSV];
}

void Bsp_Valve_Init(void)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
    {
        s_state[i] = false;
        HAL_GPIO_WritePin(s_pin_map[i].port, s_pin_map[i].pin, VALVE_OFF_LEVEL);
    }
}

Fm_ErrorCode Bsp_Valve_Set(Bsp_Valve_Channel ch, bool is_on)
{
    if (!Bsp_Valve_IsValidChannel(ch))
    {
        return FM_ERR_012_INTERLOCK;
    }

    /* 互锁 1：水泵使能前须至少一路阀已开 */
    if ((ch == BSP_VALVE_PUMP_EN) && is_on && !Any_Valve_On())
    {
        return FM_ERR_012_INTERLOCK;
    }

    /* 互锁 2：最后一路阀关闭前须先关水泵 */
    if ((ch >= BSP_VALVE_Z1) && (ch <= BSP_VALVE_Z5_RSV) && !is_on && s_state[BSP_VALVE_PUMP_EN])
    {
        bool any_other = false;
        for (Bsp_Valve_Channel v = BSP_VALVE_Z1; v <= BSP_VALVE_Z5_RSV; v++)
        {
            if ((v != ch) && s_state[v])
            {
                any_other = true;
                break;
            }
        }
        if (!any_other)
        {
            return FM_ERR_012_INTERLOCK;
        }
    }

    Bsp_Valve_WriteRaw(ch, is_on);
    return FM_OK;
}

void Bsp_Valve_DebugSet(Bsp_Valve_Channel ch, bool is_on)
{
    if (!Bsp_Valve_IsValidChannel(ch))
    {
        return;
    }
    Bsp_Valve_WriteRaw(ch, is_on);
}

bool Bsp_Valve_Get(Bsp_Valve_Channel ch)
{
    if (!Bsp_Valve_IsValidChannel(ch))
    {
        return false;
    }
    return s_state[ch];
}

bool Bsp_Valve_GetGpio(Bsp_Valve_Channel ch)
{
    if (!Bsp_Valve_IsValidChannel(ch))
    {
        return false;
    }
    return (HAL_GPIO_ReadPin(s_pin_map[ch].port, s_pin_map[ch].pin) == VALVE_ON_LEVEL);
}

uint32_t Bsp_Valve_GetGpioBOdrMask(void)
{
    uint32_t odr = (uint32_t)GPIOB->ODR;
    return odr & (VALVE_Z1_PB12_Pin | VALVE_Z2_PB13_Pin | VALVE_Z3_PB14_Pin | VALVE_Z4_PB15_Pin |
                  PUMP_EN_PB1_Pin | MOSFET_RSV_PB0_Pin);
}

void Bsp_Valve_AllOff(void)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
    {
        Bsp_Valve_WriteRaw((Bsp_Valve_Channel)i, false);
    }
}

void Bsp_Valve_ForceAllOff(void)
{
    /* 先关水泵使能，再关各路阀，顺序与互锁无关（强制路径） */
    Bsp_Valve_WriteRaw(BSP_VALVE_PUMP_EN, false);
    Bsp_Valve_WriteRaw(BSP_VALVE_Z1, false);
    Bsp_Valve_WriteRaw(BSP_VALVE_Z2, false);
    Bsp_Valve_WriteRaw(BSP_VALVE_Z3, false);
    Bsp_Valve_WriteRaw(BSP_VALVE_Z4, false);
    Bsp_Valve_WriteRaw(BSP_VALVE_Z5_RSV, false);
    Bsp_Valve_WriteRaw(BSP_VALVE_RSV, false);
}

void Bsp_Valve_Selftest(uint16_t pulse_ms)
{
    /* 仅对分阀 Z1..Z4 做短脉冲自检（不触发水泵使能，避免无压空转） */
    for (Bsp_Valve_Channel v = BSP_VALVE_Z1; v <= BSP_VALVE_Z4; v++)
    {
        Bsp_Valve_WriteRaw(v, true);
        HAL_Delay(pulse_ms);
        Bsp_Valve_WriteRaw(v, false);
        HAL_Delay(pulse_ms);
    }
}
