/*
 * @File         : \code\Bsp\valve\bsp_valve.h
 * @Author       : tonymeng
 * @Date         : 2026-05-27
 * @Description  : 固态驱动层：TLP281-4 光耦 + N 沟道 MOSFET（阀 AO3400 / 泵使能 AO3400）
 * @note         硬件拓扑（每通道）：
 *               MCU GPIO ─[220Ω]─ TLP281 LED ─ GND_ISO
 *               TLP281_C ─[47kΩ拉高到12V]─ Q1(NPN S8050) 基极
 *               Q1_C ─[10kΩ拉高到12V]─ MOSFET 栅极 ─[100kΩ拉低]─ GND_DIRTY
 *               GPIO=1 → TLP281 ON → Q1 OFF → 栅极=12V → MOSFET 导通 → 负载 ON
 *               GPIO=0 → TLP281 OFF→ Q1 ON  → 栅极≈0V  → MOSFET 截止 → 负载 OFF（失效安全）
 *               水泵调速仍使用 PA0 TIM2_CH1 直接驱动 LR7843 模块内置 PC817（不经本模块）。
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#ifndef BSP_VALVE_H
#define BSP_VALVE_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

/**
 * @brief   固态驱动通道索引
 * @note    通道映射（与 main.h GPIO 标签一一对应）：
 *          Z1..Z4  ← IC1(TLP281-4) ch1..4，PB12..PB15，左侧排针 1..4
 *          Z5_RSV  ← IC2(TLP281-4) ch1，PA8，左侧排针 5（原 SENSOR_RSV_PA8 已重用为输出）
 *          PUMP_EN ← IC2(TLP281-4) ch2，PB1，右侧排针 35；控制水泵 12V 总电源通断
 *          RSV     ← IC2(TLP281-4) ch3，PB0，右侧排针 34；备用通道（暂无负载）
 *          IC2 ch4 ← 空置，备用
 *          失效安全：所有通道 GPIO 复位后默认 Low → MOSFET 截止 → 负载断电 ✓
 */
typedef enum
{
    BSP_VALVE_Z1 = 0, /**< 电磁阀 Z1（PB12），IC1-ch1   */
    BSP_VALVE_Z2,     /**< 电磁阀 Z2（PB13），IC1-ch2   */
    BSP_VALVE_Z3,     /**< 电磁阀 Z3（PB14），IC1-ch3   */
    BSP_VALVE_Z4,     /**< 电磁阀 Z4（PB15），IC1-ch4   */
    BSP_VALVE_Z5_RSV, /**< 电磁阀 Z5 备用（PA8），IC2-ch1 */
    BSP_VALVE_PUMP_EN,/**< 水泵 12V 总电源使能（PB1），IC2-ch2 */
    BSP_VALVE_RSV,    /**< 备用 MOSFET 通道（PB0），IC2-ch3 */
    BSP_VALVE_CHANNEL_NUM
} Bsp_Valve_Channel;

/**
 * @brief   初始化：所有通道 GPIO 设为 Low（失效安全默认断开）
 */
void Bsp_Valve_Init(void);

/**
 * @brief   设置单路通道（含互锁）
 * @param   ch    通道（BSP_VALVE_Z1..PUMP_EN..RSV）
 * @param   is_on true = 导通（阀开/泵使能），false = 截止
 * @retval  FM_OK / FM_ERR_012_INTERLOCK
 * @note    互锁规则：
 *          1. PUMP_EN 吸合前须至少一路阀（Z1..Z5）已开；
 *          2. 最后一路阀关闭前须先关 PUMP_EN。
 */
Fm_ErrorCode Bsp_Valve_Set(Bsp_Valve_Channel ch, bool is_on);

/**
 * @brief   绕过互锁强制设置（调试菜单专用）
 */
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
 * @brief   读取 GPIOB ODR 与阀/泵相关位的掩码（用于调试快照）
 */
uint32_t Bsp_Valve_GetGpioBOdrMask(void);

/**
 * @brief   关断所有通道（安全停止，无论互锁）
 */
void Bsp_Valve_AllOff(void);

/**
 * @brief   强制关断水泵与所有阀（紧急停止路径，忽略互锁）
 */
void Bsp_Valve_ForceAllOff(void);

/**
 * @brief   自检：逐路短脉冲触发分阀（不触发 PUMP_EN）
 * @param   pulse_ms  每路通断脉冲宽度（ms），建议 50
 */
void Bsp_Valve_Selftest(uint16_t pulse_ms);

static inline bool Bsp_Valve_IsValidChannel(Bsp_Valve_Channel ch)
{
    return ((uint32_t)ch < (uint32_t)BSP_VALVE_CHANNEL_NUM);
}

#endif /* BSP_VALVE_H */
