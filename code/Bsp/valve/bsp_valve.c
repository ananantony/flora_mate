/*
 * @File         : \code\Bsp\valve\bsp_valve.c
 * @Author       : tonymeng
 * @Date         : 2026-06-06
 * @Description  : 固态驱动层实现：TLP281-4 光耦 + AO4407A P-MOS 高边开关（阀 Z1~Z5）
 * @note         GPIO 逻辑（高电平有效）：
 *               MCU GPIO → 510Ω → TLP281 阳极 → 阴极 → ISO_GND；栅极经 10kΩ 上拉到 +12V_DIRTY
 *               GPIO High = TLP281 ON  → Gate→GND_DIRTY → Vgs=-12V → P-MOS 导通 → 负载 ON
 *               GPIO Low  = TLP281 截止 → Gate=+12V（10kΩ上拉）→ Vgs≈0 → P-MOS 截止 → 负载 OFF（失效安全）
 *               所有通道 GPIO 初始均为 Low → 失效安全（负载断电）。
 *               水泵调速（PWM）由 bsp_pump_pwm 模块通过 PA0 TIM2_CH1 控制，本模块不涉及。
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#include "bsp_valve.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* 高电平有效：GPIO High → TLP281 ON → P-MOS 导通 → 阀开 */
#define VALVE_ON_LEVEL  GPIO_PIN_SET
#define VALVE_OFF_LEVEL GPIO_PIN_RESET

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} Bsp_Valve_PinMap;

/**
 * @brief   GPIO 引脚映射表（与 main.h 标签及 § 5 引脚分配表完全对齐）
 *          CubeMX 生成标签：VALVE_Z1~VALVE_Z5，对应 PA1~PA5
 */
static const Bsp_Valve_PinMap s_pin_map[BSP_VALVE_CHANNEL_NUM] = {
    [BSP_VALVE_Z1] = {VALVE_Z1_GPIO_Port, VALVE_Z1_Pin}, /* PA1 */
    [BSP_VALVE_Z2] = {VALVE_Z2_GPIO_Port, VALVE_Z2_Pin}, /* PA2 */
    [BSP_VALVE_Z3] = {VALVE_Z3_GPIO_Port, VALVE_Z3_Pin}, /* PA3 */
    [BSP_VALVE_Z4] = {VALVE_Z4_GPIO_Port, VALVE_Z4_Pin}, /* PA4 */
    [BSP_VALVE_Z5] = {VALVE_Z5_GPIO_Port, VALVE_Z5_Pin}, /* PA5 */
};

static bool s_state[BSP_VALVE_CHANNEL_NUM];

/**
 * @brief   直接写 GPIO，同步更新软件状态缓存
 * @note    高电平有效：is_on=true → GPIO HIGH（TLP281 ON → P-MOS 导通 → 阀开）
 *                      is_on=false → GPIO LOW（TLP281 OFF → P-MOS 截止 → 阀关）
 */
static void Bsp_Valve_WriteRaw(Bsp_Valve_Channel ch, bool is_on)
{
    GPIO_TypeDef *port = s_pin_map[ch].port;
    uint32_t      pin  = (uint32_t)s_pin_map[ch].pin;

    if (is_on)
    {
        port->BSRR = pin;        /* BSRR[15:0]  Set   → GPIO HIGH → TLP281 ON → P-MOS 导通 */
    }
    else
    {
        port->BSRR = pin << 16U; /* BSRR[31:16] Reset → GPIO LOW → TLP281 截止 → P-MOS 截止 */
    }
    s_state[ch] = is_on;
}

bool Bsp_Valve_AnyOn(void)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
    {
        if (s_state[i])
        {
            return true;
        }
    }
    return false;
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

uint32_t Bsp_Valve_GetGpioAOdrMask(void)
{
    uint32_t odr = (uint32_t)GPIOA->ODR;
    return odr & (VALVE_Z1_Pin | VALVE_Z2_Pin | VALVE_Z3_Pin | VALVE_Z4_Pin | VALVE_Z5_Pin);
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
    for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
    {
        Bsp_Valve_WriteRaw((Bsp_Valve_Channel)i, false);
    }
}

void Bsp_Valve_Selftest(uint16_t pulse_ms)
{
    for (Bsp_Valve_Channel v = BSP_VALVE_Z1; v < BSP_VALVE_CHANNEL_NUM; v++)
    {
        Bsp_Valve_WriteRaw(v, true);
        HAL_Delay(pulse_ms);
        Bsp_Valve_WriteRaw(v, false);
        HAL_Delay(pulse_ms);
    }
}
