/*
 * @File         : \code\Bsp\relay\bsp_relay.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 6 路继电器封装实现，含 CH5 双向软互锁与自检脉冲
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
#include "bsp_relay.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* 继电器模块低电平触发：GPIO=Low → 光耦导通 → 线圈通电 → 触点吸合 */
#define RELAY_ON_LEVEL  GPIO_PIN_RESET
#define RELAY_OFF_LEVEL GPIO_PIN_SET

/**
 * @brief   单路继电器的 GPIO 物理映射
 */
typedef struct
{
    GPIO_TypeDef *port; /**< GPIO 端口 */
    uint16_t      pin;  /**< GPIO 引脚 */
} Bsp_Relay_PinMap;

static const Bsp_Relay_PinMap s_pin_map[BSP_RELAY_CHANNEL_NUM] = {
    [BSP_RELAY_VALVE_1]  = {RLY_VALVE_Z1_GPIO_Port, RLY_VALVE_Z1_Pin},
    [BSP_RELAY_VALVE_2]  = {RLY_VALVE_Z2_GPIO_Port, RLY_VALVE_Z2_Pin},
    [BSP_RELAY_VALVE_3]  = {RLY_VALVE_Z3_GPIO_Port, RLY_VALVE_Z3_Pin},
    [BSP_RELAY_VALVE_4]  = {RLY_VALVE_Z4_GPIO_Port, RLY_VALVE_Z4_Pin},
    [BSP_RELAY_MAIN_CH5] = {RLY_MAIN_CH5_GPIO_Port, RLY_MAIN_CH5_Pin},
    [BSP_RELAY_RSV_CH6]  = { RLY_RSV_CH6_GPIO_Port,  RLY_RSV_CH6_Pin},
};

/** 软件影子寄存器：当前每路逻辑状态（true=吸合） */
static bool s_state[BSP_RELAY_CHANNEL_NUM];

/**
 * @brief   裸写 GPIO 并同步影子寄存器（不做任何互锁判断）
 * @param   ch     通道索引（合法性由调用方保证）
 * @param   is_on  true=吸合 / false=释放
 */
static void Bsp_Relay_WriteRaw(Bsp_Relay_Channel ch, bool is_on)
{
    HAL_GPIO_WritePin(s_pin_map[ch].port, s_pin_map[ch].pin, is_on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
    s_state[ch] = is_on;
}

/**
 * @brief   是否至少有一路分阀（CH1..CH4）当前吸合
 * @retval  true=有 / false=无
 */
static bool Bsp_Relay_AnyValveOn(void)
{
    return s_state[BSP_RELAY_VALVE_1] || s_state[BSP_RELAY_VALVE_2] || s_state[BSP_RELAY_VALVE_3] ||
           s_state[BSP_RELAY_VALVE_4];
}

/**
 * @brief   继电器子系统初始化
 * @note    GPIO 已由 MX_GPIO_Init() 设为推挽 + 初始高（释放）；此处统一刷新
 *          影子寄存器并显式重写一次电平，确保软硬一致。
 */
void Bsp_Relay_Init(void)
{
    for (uint32_t i = 0U; i < (uint32_t)BSP_RELAY_CHANNEL_NUM; i++)
    {
        s_state[i] = false;
        HAL_GPIO_WritePin(s_pin_map[i].port, s_pin_map[i].pin, RELAY_OFF_LEVEL);
    }
}

/**
 * @brief   设置某路继电器开/关（含 CH5 互锁）
 * @param   ch     通道索引
 * @param   is_on  目标状态
 * @retval  FM_OK 或 FM_ERR_012_INTERLOCK
 * @note    互锁规则：
 *          - CH5 吸合：要求至少一路分阀已开；
 *          - 分阀释放：若 CH5 仍吸合且该路是最后一路开着的分阀，拒绝；
 *            必须先释放 CH5 再释放分阀，避免水泵带阀关闭瞬间产生水锤。
 */
Fm_ErrorCode Bsp_Relay_Set(Bsp_Relay_Channel ch, bool is_on)
{
    if (!Bsp_Relay_IsValidChannel(ch))
    {
        return FM_ERR_012_INTERLOCK;
    }

    if ((ch == BSP_RELAY_MAIN_CH5) && is_on && !Bsp_Relay_AnyValveOn())
    {
        return FM_ERR_012_INTERLOCK;
    }

    if ((ch >= BSP_RELAY_VALVE_1) && (ch <= BSP_RELAY_VALVE_4) && !is_on && s_state[BSP_RELAY_MAIN_CH5])
    {
        bool any_other_valve_on = false;
        for (uint32_t i = 0U; i < 4U; i++)
        {
            if (((Bsp_Relay_Channel)i != ch) && s_state[i])
            {
                any_other_valve_on = true;
                break;
            }
        }
        if (!any_other_valve_on)
        {
            return FM_ERR_012_INTERLOCK;
        }
    }

    Bsp_Relay_WriteRaw(ch, is_on);
    return FM_OK;
}

/**
 * @brief   查询某路当前状态
 * @param   ch  通道索引
 * @retval  true=吸合 / false=释放（含非法通道）
 */
bool Bsp_Relay_Get(Bsp_Relay_Channel ch)
{
    if (!Bsp_Relay_IsValidChannel(ch))
    {
        return false;
    }
    return s_state[ch];
}

/**
 * @brief   释放全部 6 路（正常收尾）
 * @note    先 CH5、再分阀、最后保留通道；保证关泵优先于关阀，避免水锤。
 */
void Bsp_Relay_AllOff(void)
{
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, false);
    for (uint32_t i = 0U; i < 4U; i++)
    {
        Bsp_Relay_WriteRaw((Bsp_Relay_Channel)i, false);
    }
    Bsp_Relay_WriteRaw(BSP_RELAY_RSV_CH6, false);
}

/**
 * @brief   异常路径硬刹车（无延时、无日志、无互锁）
 * @note    适用于 HardFault / ERROR 状态；纯 GPIO 写，可在 ISR 调用。
 */
void Bsp_Relay_MainOffForce(void)
{
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_1, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_2, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_3, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_VALVE_4, false);
    Bsp_Relay_WriteRaw(BSP_RELAY_RSV_CH6, false);
}

/**
 * @brief   上电自检：依次给每路一个短脉冲（"咔嗒"听感）
 * @param   pulse_ms  单次吸合保持时长
 * @note    顺序：4 路分阀逐路咔嗒 → CH5 单独咔嗒。CH5 自检时所有分阀已释放，
 *          因此即使 PWM 异常也不会有水流（水泵管路无回路）。
 */
void Bsp_Relay_Selftest(uint16_t pulse_ms)
{
    for (uint32_t i = 0U; i < 4U; i++)
    {
        Bsp_Relay_WriteRaw((Bsp_Relay_Channel)i, true);
        HAL_Delay(pulse_ms);
        Bsp_Relay_WriteRaw((Bsp_Relay_Channel)i, false);
        HAL_Delay(pulse_ms);
    }
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, true);
    HAL_Delay(pulse_ms);
    Bsp_Relay_WriteRaw(BSP_RELAY_MAIN_CH5, false);
    HAL_Delay(pulse_ms);
}
