/*
 * @File         : \code\Common\types\floramate_types.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : FloraMate 项目级公共类型、错误码与编译期常量
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
#ifndef FLORAMATE_TYPES_H
#define FLORAMATE_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @brief   项目错误码（与《软件设计方案_V1.0.md》§ 11.1 一一对应）
 * @note    - 数值经过精心规划，便于日志快速识别归属：
 *          - 0x00..0x0F：I/O 与持久化类（EEPROM、I²C 总线）
 *          - 0x10..0x1F：业务时序类（浇灌超时、互锁失败）
 *          - 0x20..0x2F：系统类（看门狗）
 *          - 0xFF：未知异常（HardFault/NMI）
 *          - 错误码 0 表示成功，与所有"返回 0 即成功"惯例兼容
 */
typedef enum
{
    FM_OK                    = 0,

    FM_ERR_001_EEPROM_CRC    = 0x01, /**< 两 Bank CRC 均错，已回工厂值        */
    FM_ERR_002_I2C_OLED      = 0x02, /**< OLED 写失败 ≥ 3 次                 */
    FM_ERR_003_I2C_EEPROM    = 0x03, /**< EEPROM 读写失败 ≥ 3 次             */

    FM_ERR_010_CH_TIMEOUT    = 0x10, /**< 单路浇灌超时                        */
    FM_ERR_011_TOTAL_TIMEOUT = 0x11, /**< 总任务超时                          */
    FM_ERR_012_INTERLOCK     = 0x12, /**< 互锁失败（无阀开就吸水泵总电源 CH1） */
    FM_ERR_013_PUMP_NO_POWER = 0x13, /**< CH1 水泵电源 OFF 时被要求设 PWM       */

    FM_ERR_020_WATCHDOG      = 0x20, /**< 看门狗复位（V1.0 预留，未启用）      */

    FM_ERR_FF_HARDFAULT      = 0xFF  /**< HardFault / NMI                     */
} Fm_ErrorCode;

/**
 * @brief   4 路水阀通道索引
 * @note    与 Bsp_Relay_Channel 中的 BSP_RELAY_VALVE_1..4（模块 CH2~CH5）一一对应，
 *          可直接 (Bsp_Relay_Channel)Fm_ValveIndex 互转。
 */
typedef enum
{
    FM_VALVE_Z1 = 0,
    FM_VALVE_Z2,
    FM_VALVE_Z3,
    FM_VALVE_Z4,
    FM_VALVE_NUM
} Fm_ValveIndex;

/* ==== 项目级编译期常量 ============================================== */

#define FM_FIRMWARE_VERSION_STR "FloraMate V1.0"      /**< 显示用版本串       */
#define FM_BUILD_DATE_STR       __DATE__ " " __TIME__ /**< 编译时刻       */

#define FM_STEP_MAX    (8U)                           /**< PWM 阶梯最多档数   */
#define FM_CHANNEL_NUM (4U)                           /**< 主用 4 路水阀      */

/* 单路 / 总任务硬限（代码内 const，不允许 EEPROM 配置覆盖） */
#define FM_PER_CHANNEL_HARD_LIMIT_S (120U) /**< 单路硬上限秒        */
#define FM_TOTAL_TASK_HARD_LIMIT_S  (600U) /**< 总任务硬上限秒      */

/* 水泵 PWM 占空比硬限（0..95，不允许 100%）*/
#define FM_PUMP_DUTY_MAX_PERCENT (95U)

#endif /* FLORAMATE_TYPES_H */
