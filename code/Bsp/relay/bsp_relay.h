/*
 * @File         : \code\Bsp\relay\bsp_relay.h
 * @Description  : 6 路继电器封装（与模块丝印 CH1~CH6 一致）
 */
#ifndef BSP_RELAY_H
#define BSP_RELAY_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

/**
 * @brief   继电器通道索引（对应模块 IN1~IN6 / 丝印 CH1~CH6）
 * @note    高电平触发；Fail-Safe 默认 GPIO=Low（释放）。
 *          CH1=水泵总电源（仅控制水泵供电，不直接控阀）；
 *          CH2~CH5=四路电磁阀；CH6=备用。
 */
typedef enum
{
    BSP_RELAY_PUMP_PWR_CH1 = 0, /**< 模块 CH1：水泵总电源（PB12）     */
    BSP_RELAY_VALVE_1,          /**< 模块 CH2：电磁阀 1（PB13）      */
    BSP_RELAY_VALVE_2,          /**< 模块 CH3：电磁阀 2（PB14）      */
    BSP_RELAY_VALVE_3,          /**< 模块 CH4：电磁阀 3（PB15）      */
    BSP_RELAY_VALVE_4,          /**< 模块 CH5：电磁阀 4（PB1）       */
    BSP_RELAY_RSV_CH6,          /**< 模块 CH6：备用（PB0）           */
    BSP_RELAY_CHANNEL_NUM
} Bsp_Relay_Channel;

void Bsp_Relay_Init(void);

/**
 * @brief   设置继电器（含互锁）
 * @retval  FM_OK / FM_ERR_012_INTERLOCK
 * @note    CH1 吸合前须至少一路阀 CH2~CH5 已开；末路阀释放前须先释放 CH1。
 */
Fm_ErrorCode Bsp_Relay_Set(Bsp_Relay_Channel ch, bool is_on);

bool Bsp_Relay_Get(Bsp_Relay_Channel ch);
bool Bsp_Relay_GetGpio(Bsp_Relay_Channel ch);
uint32_t Bsp_Relay_GetGpioPortOdrMask(void);

void Bsp_Relay_AllOff(void);
void Bsp_Relay_MainOffForce(void);
void Bsp_Relay_Selftest(uint16_t pulse_ms);
void Bsp_Relay_DebugSet(Bsp_Relay_Channel ch, bool is_on);

static inline bool Bsp_Relay_IsValidChannel(Bsp_Relay_Channel ch)
{
    return ((uint32_t)ch < (uint32_t)BSP_RELAY_CHANNEL_NUM);
}

#endif /* BSP_RELAY_H */
