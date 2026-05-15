/**
 * @file    app_config.h
 * @brief   FloraMate 运行时配置（与《配置项与存储映射_V1.0.md》§ 3 / § 4 对齐）
 * @note    AT24C64 双 Bank 持久化 + CRC-16-CCITT；启动加载工厂值兜底。
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

#define APP_CONFIG_MAGIC         (0xFAC3U)
#define APP_CONFIG_VERSION       (1U)
#define APP_CONFIG_SIZE_BYTES    (64U)

#define APP_CONFIG_BANK_A_HDR    (0x0000U)
#define APP_CONFIG_BANK_A_DATA   (0x0040U)
#define APP_CONFIG_BANK_B_HDR    (0x0100U)
#define APP_CONFIG_BANK_B_DATA   (0x0140U)

#define APP_CONFIG_BANK_MAGIC    (0xFAC30001UL)

/* 与文档 § 3.7 1:1 对齐的结构体；总长 64 字节，已 _Static_assert 保证 */
typedef struct __attribute__((packed)) {
    uint16_t magic;                      /* +0  0xFAC3 */
    uint8_t  version;                    /* +2  当前 1 */
    uint8_t  reserved0;                  /* +3  pad */
    uint32_t update_count;               /* +4  累计写次数 */

    uint8_t  step_count;                 /* +8  1~8 */
    uint8_t  step_duty[FM_STEP_MAX];     /* +9  每档占空比 % */
    uint8_t  step_seconds[FM_STEP_MAX];  /* +17 每档持续秒 */
    uint8_t  channel_enable;             /* +25 bit0~3 = 4 路开关 */

    uint8_t  idle_seconds;               /* +26 倒计时秒 */
    uint8_t  inter_gap_ms_x10;           /* +27 路间静默 ×10 ms */
    uint16_t per_channel_timeout_s;      /* +28 单路超时 s */
    uint16_t total_timeout_s;            /* +30 总任务超时 s */
    uint16_t selftest_pulse_ms;          /* +32 自检脉冲 ms */

    uint8_t  long_press_ms_x10;          /* +34 长按 ×10 ms */
    uint8_t  hold_press_ms_x10;          /* +35 超长按 ×10 ms */
    uint8_t  oled_contrast;              /* +36 OLED 对比度 */
    uint8_t  log_level;                  /* +37 0=E 1=W 2=I 3=D */

    uint8_t  reserved1[24];              /* +38 预留 */
    uint16_t crc16;                      /* +62 前 62 字节 CRC */
} App_Config;

_Static_assert(sizeof(App_Config) == APP_CONFIG_SIZE_BYTES,
               "App_Config must be 64 bytes");

/* Bank header（64 字节），与文档 § 4.1 对齐 */
typedef struct __attribute__((packed)) {
    uint32_t bank_magic;                 /* 0xFAC30001 */
    uint32_t bank_seq;                   /* 单调递增 */
    uint16_t payload_size;               /* 当前 = 64 */
    uint16_t payload_offset;             /* A=0x40, B=0x140 */
    uint8_t  reserved[50];
    uint16_t header_crc16;
} App_Config_BankHeader;

_Static_assert(sizeof(App_Config_BankHeader) == 64U,
               "App_Config_BankHeader must be 64 bytes");

typedef enum {
    APP_CONFIG_LOADED_BANK_A,
    APP_CONFIG_LOADED_BANK_B,
    APP_CONFIG_LOADED_FACTORY
} App_Config_Source;

extern const App_Config g_factory_config;

void               App_Config_Init(void);
const App_Config * App_Config_Get(void);
App_Config *       App_Config_GetMut(void);
App_Config_Source  App_Config_GetSource(void);

/* 把 g_runtime_config 写入未使用的 Bank；自动维护 seq / CRC */
Fm_ErrorCode App_Config_Save(void);

/* 重新从 EEPROM 读取（丢弃 RAM 改动）；失败返回原值不变 */
Fm_ErrorCode App_Config_Reload(void);

/* 工厂复位：拷贝 g_factory_config 到 RAM，并落盘双 Bank */
Fm_ErrorCode App_Config_FactoryReset(void);

/* 范围裁剪：对所有字段做边界 clamp，越界字段一并 LOGW */
void App_Config_Clamp(App_Config *cfg);

/* 串口/菜单使用：值域校验 + 写入字段；不立即落盘 */
Fm_ErrorCode App_Config_SetField(const char *name, int32_t value);
Fm_ErrorCode App_Config_GetField(const char *name, int32_t *out_value);

/* 调试输出：把 RAM 配置打印到日志 */
void App_Config_Dump(void);

#endif /* APP_CONFIG_H */
