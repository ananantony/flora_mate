/**
 * @file    floramate_types.h
 * @brief   FloraMate 项目级公共类型与错误码
 * @note    对应《配置项与存储映射_V1.0.md》§ 11.4 / 《软件设计方案_V1.0.md》§ 11.1
 */
#ifndef FLORAMATE_TYPES_H
#define FLORAMATE_TYPES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ---- 项目错误码（与软件设计方案 § 11.1 对齐） ---- */
typedef enum {
    FM_OK = 0,

    FM_ERR_001_EEPROM_CRC      = 0x01,  /* 两 Bank CRC 均错，已回工厂值 */
    FM_ERR_002_I2C_OLED        = 0x02,  /* OLED 写失败 ≥ 3 次 */
    FM_ERR_003_I2C_EEPROM      = 0x03,  /* EEPROM 读写失败 ≥ 3 次 */

    FM_ERR_010_CH_TIMEOUT      = 0x10,  /* 单路浇灌超时 */
    FM_ERR_011_TOTAL_TIMEOUT   = 0x11,  /* 总任务超时 */
    FM_ERR_012_INTERLOCK       = 0x12,  /* CH5 互锁失败（无阀开就吸 CH5） */
    FM_ERR_013_PUMP_NO_POWER   = 0x13,  /* CH5=OFF 时被要求设 PWM */

    FM_ERR_020_WATCHDOG        = 0x20,  /* 看门狗复位（V1 预留） */

    FM_ERR_FF_HARDFAULT        = 0xFF   /* HardFault/NMI */
} Fm_ErrorCode;

/* ---- 项目内部 4 路通道索引（与硬件方案 § 5.4 一致） ---- */
typedef enum {
    FM_VALVE_Z1 = 0,
    FM_VALVE_Z2,
    FM_VALVE_Z3,
    FM_VALVE_Z4,
    FM_VALVE_NUM
} Fm_ValveIndex;

/* ---- 项目级编译期常量 ---- */
#define FM_FIRMWARE_VERSION_STR  "FloraMate V1.0"
#define FM_BUILD_DATE_STR        __DATE__ " " __TIME__

#define FM_STEP_MAX              (8U)   /* PWM 阶梯最多档数 */
#define FM_CHANNEL_NUM           (4U)   /* 主用 4 路水阀 */

/* 单路/总任务硬限（代码内 const，不允许 EEPROM 配置覆盖） */
#define FM_PER_CHANNEL_HARD_LIMIT_S  (120U)
#define FM_TOTAL_TASK_HARD_LIMIT_S   (600U)

/* 水泵 PWM 占空比硬限（0~95，不允许 100%） */
#define FM_PUMP_DUTY_MAX_PERCENT     (95U)

#endif /* FLORAMATE_TYPES_H */
