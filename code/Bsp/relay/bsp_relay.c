/*
 * @File         : \code\Bsp\relay\bsp_relay.c
 * @Description  : 6 路继电器：CH1 水泵总电源；CH2~CH5 电磁阀；CH6 备用
 */
#include "bsp_relay.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#define RELAY_ON_LEVEL  GPIO_PIN_SET
#define RELAY_OFF_LEVEL GPIO_PIN_RESET

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} Bsp_Relay_PinMap;

/** GPIO 映射：CubeMX 符号名保留，逻辑按模块 CH1~CH6 */
static const Bsp_Relay_PinMap s_pin_map[BSP_RELAY_CHANNEL_NUM] = {
    [BSP_RELAY_PUMP_PWR_CH1] = {RLY_VALVE_Z1_GPIO_Port, RLY_VALVE_Z1_Pin},   /* CH1 PB12 */
    [BSP_RELAY_VALVE_1]      = {RLY_VALVE_Z2_GPIO_Port, RLY_VALVE_Z2_Pin},   /* CH2 PB13 */
    [BSP_RELAY_VALVE_2]      = {RLY_VALVE_Z3_GPIO_Port, RLY_VALVE_Z3_Pin},   /* CH3 PB14 */
    [BSP_RELAY_VALVE_3]      = {RLY_VALVE_Z4_GPIO_Port, RLY_VALVE_Z4_Pin},   /* CH4 PB15 */
    [BSP_RELAY_VALVE_4]      = {RLY_MAIN_CH5_GPIO_Port, RLY_MAIN_CH5_Pin},   /* CH5 PB1  */
    [BSP_RELAY_RSV_CH6]      = {RLY_RSV_CH6_GPIO_Port, RLY_RSV_CH6_Pin},     /* CH6 PB0  */
};

static bool s_state[BSP_RELAY_CHANNEL_NUM];

static void Bsp_Relay_WriteRaw(Bsp_Relay_Channel ch, bool is_on)
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

static bool Bsp_Relay_AnyValveOn(void)
{
    return s_state[BSP_RELAY_VALVE_1] || s_state[BSP_RELAY_VALVE_2] || s_state[BSP_RELAY_VALVE_3] ||
           s_state[BSP_RELAY_VALVE_4];
}

void Bsp_Relay_Init(void)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_RELAY_CHANNEL_NUM; i++)
    {
        s_state[i] = false;
        HAL_GPIO_WritePin(s_pin_map[i].port, s_pin_map[i].pin, RELAY_OFF_LEVEL);
    }
}

Fm_ErrorCode Bsp_Relay_Set(Bsp_Relay_Channel ch, bool is_on)
{
    if (!Bsp_Relay_IsValidChannel(ch))
    {
        return FM_ERR_012_INTERLOCK;
    }

    if ((ch == BSP_RELAY_PUMP_PWR_CH1) && is_on && !Bsp_Relay_AnyValveOn())
    {
        return FM_ERR_012_INTERLOCK;
    }

    if ((ch >= BSP_RELAY_VALVE_1) && (ch <= BSP_RELAY_VALVE_4) && !is_on && s_state[BSP_RELAY_PUMP_PWR_CH1])
    {
        bool any_other = false;
        for (Bsp_Relay_Channel v = BSP_RELAY_VALVE_1; v <= BSP_RELAY_VALVE_4; v++)
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

    Bsp_Relay_WriteRaw(ch, is_on);
    return FM_OK;
}

void Bsp_Relay_DebugSet(Bsp_Relay_Channel ch, bool is_on)
{
    if (!Bsp_Relay_IsValidChannel(ch))
    {
        return;
    }
    Bsp_Relay_WriteRaw(ch, is_on);
}

bool Bsp_Relay_Get(Bsp_Relay_Channel ch)
{
    if (!Bsp_Relay_IsValidChannel(ch))
    {
        return false;
    }
    return s_state[ch];
}

bool Bsp_Relay_GetGpio(Bsp_Relay_Channel ch)
{
    if (!Bsp_Relay_IsValidChannel(ch))
    {
        return false;
    }
    return (HAL_GPIO_ReadPin(s_pin_map[ch].port, s_pin_map[ch].pin) == RELAY_ON_LEVEL);
}

uint32_t Bsp_Relay_GetGpioPortOdrMask(void)
{
    uint32_t odr = (uint32_t)GPIOB->ODR;
    return odr & (RLY_RSV_CH6_Pin | RLY_MAIN_CH5_Pin | RLY_VALVE_Z1_Pin | RLY_VALVE_Z2_Pin | RLY_VALVE_Z3_Pin |
                  RLY_VALVE_Z4_Pin);
}

void Bsp_Relay_AllOff(void)
{
    Bsp_Relay_WriteRaw(BSP_RELAY_PUMP_PWR_CH1, false);
    for (Bsp_Relay_Channel v = BSP_RELAY_VALVE_1; v <= BSP_RELAY_VALVE_4; v++)
    {
        Bsp_Relay_WriteRaw(v, false);
    }
    Bsp_Relay_WriteRaw(BSP_RELAY_RSV_CH6, false);
}

void Bsp_Relay_MainOffForce(void)
{
    Bsp_Relay_WriteRaw(BSP_RELAY_PUMP_PWR_CH1, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_1, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_2, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_3, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_4, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_RSV_CH6, false);
}

void Bsp_Relay_Selftest(uint16_t pulse_ms)
{
    for (Bsp_Relay_Channel v = BSP_RELAY_VALVE_1; v <= BSP_RELAY_VALVE_4; v++)
    {
        Bsp_Relay_WriteRaw(v, true);
        HAL_Delay(pulse_ms);
        Bsp_Relay_WriteRaw(v, false);
        HAL_Delay(pulse_ms);
    }
    Bsp_Relay_WriteRaw(BSP_RELAY_PUMP_PWR_CH1, true);
    HAL_Delay(pulse_ms);
    Bsp_Relay_WriteRaw(BSP_RELAY_PUMP_PWR_CH1, false);
    HAL_Delay(pulse_ms);
}
