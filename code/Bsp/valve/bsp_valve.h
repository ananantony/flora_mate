/*
 * @File         : \code\Bsp\valve\bsp_valve.h
 * @Author       : tonymeng
 * @Date         : 2026-06-06
 * @Description  : 固态驱动层：TLP281-4 光�?+ AO4407A P-MOS 高边开关（阀 Z1~Z5�? * @note         硬件拓扑（每通道，主动低驱动）：
 *               3V3 �?510Ω �?TLP281 阳极 �?阴极 �?MCU GPIO �?ISO_GND
 *               TLP281_C �?AO4407A 栅极（[10kΩ 上拉�?+12V_DIRTY]�? *               GPIO=1（High）→ TLP281 截止 �?栅极=+12V �?Vgs�? �?P-MOS 截止 �?负载 OFF（失效安全）
 *               GPIO=0（Low�?�?TLP281 ON  �?栅极→GND �?Vgs=-12V�?P-MOS 导�?�?负载 ON
 *               水泵调速使�?PA0 TIM2_CH1（PUMP_PWM），同路径（TLP281+AO4407A），不经本模块�? *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#ifndef BSP_VALVE_H
#define BSP_VALVE_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

/**
 * @brief   阀门通道索引（与原理图及 § 5 引脚分配表完全对齐）
 * @note    Z1..Z5 �?PA1..PA5，对�?TLP281-4 �?U3/U4 两片
 *          失效安全：所有通道 GPIO 复位后默�?Low �?P-MOS 截止 �?负载断电 �? */
typedef enum
{
    BSP_VALVE_Z1 = 0, /**< 电磁阀 Z1（PA1），U3-ch1 */
    BSP_VALVE_Z2,     /**< 电磁阀 Z2（PA2），U3-ch2 */
    BSP_VALVE_Z3,     /**< 电磁阀 Z3（PA3），U3-ch3 */
    BSP_VALVE_Z4,     /**< 电磁阀 Z4（PA4），U3-ch4 */
    BSP_VALVE_Z5,     /**< 电磁阀 Z5（PA5），U4-ch1 */
    BSP_VALVE_CHANNEL_NUM
} Bsp_Valve_Channel;

/**
 * @brief   初始化：所有通道 GPIO 设为 Low（失效安全默认断开�? */
void Bsp_Valve_Init(void);

/**
 * @brief   设置单路阀门通道
 * @param   ch    通道（BSP_VALVE_Z1..BSP_VALVE_Z5�? * @param   is_on true = 开阀（P-MOS 导通），false = 关阀（P-MOS 截止�? * @retval  FM_OK / FM_ERR_012_INTERLOCK（通道越界�? */
Fm_ErrorCode Bsp_Valve_Set(Bsp_Valve_Channel ch, bool is_on);

/**
 * @brief   绕过合法性检查强制设置（调试菜单专用�? */
void Bsp_Valve_DebugSet(Bsp_Valve_Channel ch, bool is_on);

/**
 * @brief   读取通道软件状态（RAM 中缓存值）
 */
bool Bsp_Valve_Get(Bsp_Valve_Channel ch);

/**
 * @brief   读取通道实际 GPIO 电平
 */
bool Bsp_Valve_GetGpio(Bsp_Valve_Channel ch);

/**
 * @brief   检查是否有任意一路阀（Z1..Z5）处于开启状�? * @retval  true = 至少一路阀已开；false = 所有阀均关�? * @note    �?bsp_pump_pwm 做干转保护检查（泵启动前须有阀路导通）�? */
bool Bsp_Valve_AnyOn(void);

/**
 * @brief   读取 GPIOA ODR 与阀相关位的掩码（用于调试快照）
 */
uint32_t Bsp_Valve_GetGpioAOdrMask(void);

/**
 * @brief   关断所有阀（安全停止）
 */
void Bsp_Valve_AllOff(void);

/**
 * @brief   强制关断所有阀（紧急停止路径，忽略状态缓存）
 */
void Bsp_Valve_ForceAllOff(void);

/**
 * @brief   自检：逐路短脉冲触发各阀（不触发水泵�? * @param   pulse_ms  每路通断脉冲宽度（ms），建议 50
 */
void Bsp_Valve_Selftest(uint16_t pulse_ms);

static inline bool Bsp_Valve_IsValidChannel(Bsp_Valve_Channel ch)
{
    return ((uint32_t)ch < (uint32_t)BSP_VALVE_CHANNEL_NUM);
}

#endif /* BSP_VALVE_H */
