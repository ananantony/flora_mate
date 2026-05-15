/*
 * @File         : \code\App\config\app_config.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 运行时配置接口（双 Bank + CRC，对应《配置项与存储映射_V1.0.md》）
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
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

#define APP_CONFIG_MAGIC      (0xFAC3U)      /**< 配置结构 magic        */
#define APP_CONFIG_VERSION    (2U)           /**< 当前配置版本           */
#define APP_CONFIG_SIZE_BYTES (64U)          /**< App_Config 字节数      */

#define APP_CONFIG_BANK_A_HDR  (0x0000U)     /**< Bank A header 偏移     */
#define APP_CONFIG_BANK_A_DATA (0x0040U)     /**< Bank A payload 偏移    */
#define APP_CONFIG_BANK_B_HDR  (0x0100U)     /**< Bank B header 偏移     */
#define APP_CONFIG_BANK_B_DATA (0x0140U)     /**< Bank B payload 偏移    */

#define APP_CONFIG_BANK_MAGIC (0xFAC30001UL) /**< Bank header magic     */

/**
 * @brief   运行时配置结构（与文档 § 3.7 1:1 对齐，总长 64 B，已 _Static_assert 保证）
 */
typedef struct __attribute__((packed))
{
    uint16_t magic;                    /**< +0  0xFAC3                       */
    uint8_t  version;                  /**< +2  当前 1                       */
    uint8_t  reserved0;                /**< +3  pad                          */
    uint32_t update_count;             /**< +4  累计写次数（寿命统计）        */

    uint8_t step_count;                /**< +8  PWM 阶梯档数 1~8             */
    uint8_t step_duty[FM_STEP_MAX];    /**< +9  每档占空比 %                 */
    uint8_t step_seconds[FM_STEP_MAX]; /**< +17 每档持续秒                   */
    uint8_t channel_enable;            /**< +25 bit0~3 = 4 路开关             */

    uint8_t  idle_seconds;             /**< +26 上电倒计时秒                  */
    uint8_t  inter_gap_ms_x10;         /**< +27 路间静默 ×10 ms              */
    uint16_t per_channel_timeout_s;    /**< +28 单路超时秒                    */
    uint16_t total_timeout_s;          /**< +30 总任务超时秒                  */
    uint16_t selftest_pulse_ms;        /**< +32 继电器自检脉冲 ms             */

    uint16_t long_press_ms_x10;        /**< +34 长按阈值 ×10 ms（30..300 → 300..3000ms） */
    uint16_t hold_press_ms_x10;        /**< +36 超长按阈值 ×10 ms（100..500 → 1..5s）    */
    uint8_t  oled_contrast;            /**< +38 OLED 对比度                              */
    uint8_t  log_level;                /**< +39 0=E 1=W 2=I 3=D                          */

    uint8_t  reserved1[22];            /**< +40 预留（升级用）                            */
    uint16_t crc16;                    /**< +62 前 62 字节的 CRC-16-CCITT                 */
} App_Config;

_Static_assert(sizeof(App_Config) == APP_CONFIG_SIZE_BYTES, "App_Config must be 64 bytes");

/**
 * @brief   Bank 头结构（64 字节，与文档 § 4.1 对齐）
 */
typedef struct __attribute__((packed))
{
    uint32_t bank_magic;     /**< 固定 0xFAC30001                  */
    uint32_t bank_seq;       /**< 单调递增，决定哪 bank 是"新"     */
    uint16_t payload_size;   /**< 当前 = 64                        */
    uint16_t payload_offset; /**< A=0x40, B=0x140                  */
    uint8_t  reserved[50];   /**< 升级预留                          */
    uint16_t header_crc16;   /**< 头自身 CRC                        */
} App_Config_BankHeader;

_Static_assert(sizeof(App_Config_BankHeader) == 64U, "App_Config_BankHeader must be 64 bytes");

/**
 * @brief   启动时配置加载源
 */
typedef enum
{
    APP_CONFIG_LOADED_BANK_A, /**< 来自 Bank A                              */
    APP_CONFIG_LOADED_BANK_B, /**< 来自 Bank B                              */
    APP_CONFIG_LOADED_FACTORY /**< 两 Bank 均坏，回工厂值（FM_ERR_001）     */
} App_Config_Source;

extern const App_Config g_factory_config; /**< 工厂默认值（位于代码段，只读） */

/**
 * @brief   配置模块初始化
 * @note    顺序：1) 探测 EEPROM；2) 读两 Bank header 选新；3) 校验 payload CRC；
 *          两 Bank 均坏 → 用 g_factory_config 兜底并落盘。完成后 RAM 中即有有效配置。
 */
void App_Config_Init(void);

/**
 * @brief   只读获取当前配置
 * @retval  指向 RAM 中运行配置的 const 指针（永不为 NULL）
 */
const App_Config *App_Config_Get(void);

/**
 * @brief   可写获取当前配置（写后需调用 App_Config_Save 才会落盘）
 * @retval  指向 RAM 中运行配置的可写指针
 */
App_Config *App_Config_GetMut(void);

/**
 * @brief   返回当前配置的加载源（启动一次后不再变化）
 */
App_Config_Source App_Config_GetSource(void);

/**
 * @brief   把 RAM 配置写入未使用的另一 Bank（双 Bank 切换）
 * @retval  FM_OK / FM_ERR_003_I2C_EEPROM
 * @note    内部自动：1) 调 Clamp；2) bank_seq+1；3) 算两个 CRC；
 *          4) 先 payload 后 header；5) 校验回读。
 */
Fm_ErrorCode App_Config_Save(void);

/**
 * @brief   重新从 EEPROM 加载（丢弃 RAM 改动）
 * @retval  FM_OK 或 FM_ERR_001_EEPROM_CRC
 * @note    若失败，RAM 仍保留旧值；不会留下"半个配置"。
 */
Fm_ErrorCode App_Config_Reload(void);

/**
 * @brief   工厂复位：拷贝 g_factory_config 到 RAM 并落盘双 Bank
 * @retval  FM_OK / FM_ERR_003_I2C_EEPROM
 */
Fm_ErrorCode App_Config_FactoryReset(void);

/**
 * @brief   范围裁剪：对所有字段做边界 clamp
 * @param   cfg  待裁剪结构体（NULL 直接返回）
 * @note    每个越界字段会被 LOGW 提示，便于排查；clamp 后仍是合法配置。
 */
void App_Config_Clamp(App_Config *cfg);

/**
 * @brief   按字段名设置值（串口 / 菜单使用）
 * @param   name   字段名（如 "step_count" / "channel_enable" / "log_level"）
 * @param   value  目标值（含义见各字段范围）
 * @retval  FM_OK    设置成功（仅修改 RAM，不立即落盘）
 * @retval  非 FM_OK 字段不存在或值越界
 */
Fm_ErrorCode App_Config_SetField(const char *name, int32_t value);

/**
 * @brief   按字段名读值
 * @param   name      字段名
 * @param   out_value 输出参数（不能为 NULL）
 * @retval  FM_OK / 非 FM_OK
 */
Fm_ErrorCode App_Config_GetField(const char *name, int32_t *out_value);

/**
 * @brief   把 RAM 配置打印到日志（用于调试/串口 dump 命令）
 */
void App_Config_Dump(void);

#endif /* APP_CONFIG_H */
