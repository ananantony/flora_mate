/*
 * @File         : \code\App\config\app_config.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 16:30:00
 * @Description  : AT24C64 双 Bank 配置加载/保存实现（与《配置项与存储映射_V1.0.md》§6 对齐）
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 *
 *   _________________________________________________________________________
 *  | Date       | Version | Author      |  Description                       |
 *  |=========================================================================|
 *  | 2026-05-15 | 1.0.0   | tonymeng    | 初始版本                           |
 *  |-------------------------------------------------------------------------|
 */
#include "app_config.h"
#include "app_log.h"
#include "bsp_eeprom.h"
#include "crc16.h"

#include <string.h>

/* ========================================== 工厂默认 ========================================== */

/**
 * @brief   工厂默认配置（与文档 §7 一致，位于 Flash 只读段）
 * @note    long_press_ms_x10 / hold_press_ms_x10 为 uint16_t，单位 ×10 ms
 */
const App_Config g_factory_config = {
    .magic                 = APP_CONFIG_MAGIC,
    .version               = APP_CONFIG_VERSION,
    .reserved0             = 0U,
    .update_count          = 0U,

    .step_count            = 4U,
    .step_duty             = {35U, 55U, 75U, 95U, 0U, 0U, 0U, 0U},
    .step_seconds          = {3U, 3U, 3U, 3U, 0U, 0U, 0U, 0U},
    .channel_enable        = 0x0FU,

    .idle_seconds          = 3U,
    .inter_gap_ms_x10      = 50U, /* 500 ms */
    .per_channel_timeout_s = 90U,
    .total_timeout_s       = 480U,
    .selftest_pulse_ms     = 50U,

    .long_press_ms_x10     = 100U, /* 1000 ms */
    .hold_press_ms_x10     = 300U, /* 3000 ms */
    .oled_contrast         = 128U,
    .log_level             = 2U,

    .reserved1             = {0},
    .crc16                 = 0U
};

/* ========================================== 内部数据 ========================================== */

static App_Config        s_runtime;     /**< RAM 中的运行时配置副本           */
static App_Config_Source s_source;      /**< 启动时配置加载源（Bank A/B/工厂） */
static char              s_active_bank; /**< 当前有效 Bank 标识：'A' 或 'B'    */
static uint32_t          s_last_seq;    /**< 当前 Bank 的 bank_seq 序号        */

/* ========================================== 内部工具：CRC ========================================== */

/**
 * @brief   计算 App_Config payload 的 CRC-16（前 62 字节）
 * @param   cfg  待校验的配置结构体指针
 * @retval  CRC-16-CCITT 值
 */
static uint16_t App_Config_CalcCrc(const App_Config *cfg)
{
    return Crc16_Compute((const uint8_t *)cfg, APP_CONFIG_SIZE_BYTES - 2U);
}

/**
 * @brief   计算 Bank 头的 CRC-16（除 header_crc16 字段外）
 * @param   h  Bank 头结构体指针
 * @retval  CRC-16-CCITT 值
 */
static uint16_t App_Config_HdrCrc(const App_Config_BankHeader *h)
{
    return Crc16_Compute((const uint8_t *)h, sizeof(*h) - 2U);
}

/* ========================================== 内部工具：范围裁剪 ========================================== */

/**
 * @brief   将 uint8_t 值裁剪到 [lo, hi] 区间
 * @param   v   输入值
 * @param   lo  下界
 * @param   hi  上界
 * @retval  裁剪后的值
 */
static uint8_t Clamp_u8(uint8_t v, uint8_t lo, uint8_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/**
 * @brief   将 uint16_t 值裁剪到 [lo, hi] 区间
 * @param   v   输入值
 * @param   lo  下界
 * @param   hi  上界
 * @retval  裁剪后的值
 */
static uint16_t Clamp_u16(uint16_t v, uint16_t lo, uint16_t hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/**
 * @brief   对配置结构体各字段做边界 clamp（就地修改）
 * @param   cfg  待裁剪结构体；NULL 时直接返回
 * @note    oled_contrast 为 0~255 自然范围，不裁剪
 */
void App_Config_Clamp(App_Config *cfg)
{
    if (cfg == 0)
        return;

    cfg->step_count = Clamp_u8(cfg->step_count, 1U, FM_STEP_MAX);
    for (uint32_t i = 0U; i < FM_STEP_MAX; i++)
    {
        cfg->step_duty[i]    = Clamp_u8(cfg->step_duty[i], 0U, FM_PUMP_DUTY_MAX_PERCENT);
        cfg->step_seconds[i] = Clamp_u8(cfg->step_seconds[i], 0U, 60U);
    }
    cfg->channel_enable &= 0x0FU;
    if (cfg->channel_enable == 0U)
    {
        cfg->channel_enable = 0x0FU;
    }

    cfg->idle_seconds          = Clamp_u8(cfg->idle_seconds, 1U, 30U);
    cfg->inter_gap_ms_x10      = Clamp_u8(cfg->inter_gap_ms_x10, 1U, 250U);
    cfg->per_channel_timeout_s = Clamp_u16(cfg->per_channel_timeout_s, 30U, FM_PER_CHANNEL_HARD_LIMIT_S);
    cfg->total_timeout_s       = Clamp_u16(cfg->total_timeout_s, 60U, FM_TOTAL_TASK_HARD_LIMIT_S);
    cfg->selftest_pulse_ms     = Clamp_u16(cfg->selftest_pulse_ms, 10U, 200U);

    cfg->long_press_ms_x10     = Clamp_u16(cfg->long_press_ms_x10, 30U, 300U);
    cfg->hold_press_ms_x10     = Clamp_u16(cfg->hold_press_ms_x10, 100U, 500U);
    cfg->log_level             = Clamp_u8(cfg->log_level, 0U, 3U);
    /* oled_contrast 0~255 自然范围，不裁 */
}

/* ========================================== 内部：Bank 加载 ========================================== */

/**
 * @brief   从 EEPROM 读取并校验单个 Bank（头 + payload）
 * @param   hdr_addr  Bank 头在 EEPROM 中的偏移地址
 * @param   out_hdr   输出：解析成功的 Bank 头
 * @param   out_cfg   输出：解析成功的配置 payload
 * @retval  true=校验通过 / false=任一步骤失败
 */
static bool App_Config_LoadBank(uint16_t hdr_addr, App_Config_BankHeader *out_hdr, App_Config *out_cfg)
{
    if (Bsp_Eeprom_Read(hdr_addr, (uint8_t *)out_hdr, sizeof(*out_hdr)) != FM_OK)
    {
        return false;
    }
    if (out_hdr->bank_magic != APP_CONFIG_BANK_MAGIC)
    {
        return false;
    }
    if (App_Config_HdrCrc(out_hdr) != out_hdr->header_crc16)
    {
        return false;
    }
    if (out_hdr->payload_size != APP_CONFIG_SIZE_BYTES)
    {
        return false;
    }
    if (Bsp_Eeprom_Read(out_hdr->payload_offset, (uint8_t *)out_cfg, APP_CONFIG_SIZE_BYTES) != FM_OK)
    {
        return false;
    }
    if (out_cfg->magic != APP_CONFIG_MAGIC)
    {
        return false;
    }
    if (App_Config_CalcCrc(out_cfg) != out_cfg->crc16)
    {
        return false;
    }
    return true;
}

/* ========================================== 公共 API ========================================== */

/**
 * @brief   配置模块初始化：探测双 Bank、选较新者加载到 RAM
 * @note    两 Bank 均坏或版本不匹配 → 使用 g_factory_config 兜底；
 *          加载成功后对字段做 Clamp，必要时 LOG_WARN
 */
void App_Config_Init(void)
{
    App_Config_BankHeader hdr_a = {0}, hdr_b = {0};
    App_Config            cfg_a = {0}, cfg_b = {0};

    bool a_ok = App_Config_LoadBank(APP_CONFIG_BANK_A_HDR, &hdr_a, &cfg_a);
    bool b_ok = App_Config_LoadBank(APP_CONFIG_BANK_B_HDR, &hdr_b, &cfg_b);

    if (a_ok && b_ok)
    {
        if (hdr_a.bank_seq >= hdr_b.bank_seq)
        {
            s_runtime     = cfg_a;
            s_source      = APP_CONFIG_LOADED_BANK_A;
            s_active_bank = 'A';
            s_last_seq    = hdr_a.bank_seq;
        }
        else
        {
            s_runtime     = cfg_b;
            s_source      = APP_CONFIG_LOADED_BANK_B;
            s_active_bank = 'B';
            s_last_seq    = hdr_b.bank_seq;
        }
    }
    else if (a_ok)
    {
        s_runtime     = cfg_a;
        s_source      = APP_CONFIG_LOADED_BANK_A;
        s_active_bank = 'A';
        s_last_seq    = hdr_a.bank_seq;
    }
    else if (b_ok)
    {
        s_runtime     = cfg_b;
        s_source      = APP_CONFIG_LOADED_BANK_B;
        s_active_bank = 'B';
        s_last_seq    = hdr_b.bank_seq;
    }
    else
    {
        s_runtime       = g_factory_config;
        s_runtime.crc16 = App_Config_CalcCrc(&s_runtime);
        s_source        = APP_CONFIG_LOADED_FACTORY;
        s_active_bank   = 'A';
        s_last_seq      = 0U;
        LOG_WARN("config: factory defaults loaded (EEPROM empty or corrupt)");
        return;
    }

    /* 版本兼容性 */
    if (s_runtime.version != APP_CONFIG_VERSION)
    {
        LOG_WARN_WITH_ARG("config: version mismatch (fw=%u, eep=%u), reverting to factory",
                          (unsigned)APP_CONFIG_VERSION, (unsigned)s_runtime.version);
        s_runtime       = g_factory_config;
        s_runtime.crc16 = App_Config_CalcCrc(&s_runtime);
        s_source        = APP_CONFIG_LOADED_FACTORY;
        return;
    }

    /* 范围裁剪 */
    App_Config s_before = s_runtime;
    App_Config_Clamp(&s_runtime);
    if (memcmp(&s_before, &s_runtime, sizeof(s_runtime)) != 0)
    {
        LOG_WARN("config: some fields out of range, clamped");
    }

    LOG_INFO_WITH_ARG("config: loaded from Bank %c, seq=%lu, update_count=%lu", s_active_bank,
                      (unsigned long)s_last_seq, (unsigned long)s_runtime.update_count);
}

/**
 * @brief   只读获取当前运行时配置
 * @retval  指向 s_runtime 的 const 指针（永不为 NULL）
 */
const App_Config *App_Config_Get(void)
{
    return &s_runtime;
}

/**
 * @brief   可写获取当前运行时配置
 * @retval  指向 s_runtime 的可写指针；修改后需 App_Config_Save 才落盘
 */
App_Config *App_Config_GetMut(void)
{
    return &s_runtime;
}

/**
 * @brief   返回当前配置的加载源
 * @retval  APP_CONFIG_LOADED_BANK_A / B / FACTORY
 */
App_Config_Source App_Config_GetSource(void)
{
    return s_source;
}

/**
 * @brief   将 RAM 配置写入 inactive Bank（双 Bank 切换写）
 * @retval  FM_OK 成功；否则 EEPROM 读写/校验错误码
 * @note    流程：Clamp → update_count++ → 算 CRC → 写 payload → 读回校验 → 写 header
 */
Fm_ErrorCode App_Config_Save(void)
{
    /* 写入 inactive bank */
    char     target    = (s_active_bank == 'A') ? 'B' : 'A';
    uint16_t hdr_addr  = (target == 'A') ? APP_CONFIG_BANK_A_HDR : APP_CONFIG_BANK_B_HDR;
    uint16_t data_addr = (target == 'A') ? APP_CONFIG_BANK_A_DATA : APP_CONFIG_BANK_B_DATA;

    App_Config_Clamp(&s_runtime);
    s_runtime.update_count += 1U;
    s_runtime.magic         = APP_CONFIG_MAGIC;
    s_runtime.version       = APP_CONFIG_VERSION;
    s_runtime.crc16         = App_Config_CalcCrc(&s_runtime);

    Fm_ErrorCode err        = Bsp_Eeprom_Write(data_addr, (const uint8_t *)&s_runtime, APP_CONFIG_SIZE_BYTES);
    if (err != FM_OK)
    {
        LOG_ERROR("config: payload write fail");
        return err;
    }

    /* 读回校验 */
    App_Config verify = {0};
    err               = Bsp_Eeprom_Read(data_addr, (uint8_t *)&verify, APP_CONFIG_SIZE_BYTES);
    if (err != FM_OK)
    {
        LOG_ERROR("config: payload readback fail");
        return err;
    }
    if (memcmp(&verify, &s_runtime, APP_CONFIG_SIZE_BYTES) != 0)
    {
        LOG_ERROR("config: payload verify mismatch");
        return FM_ERR_001_EEPROM_CRC;
    }

    /* 准备并写入 header */
    App_Config_BankHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.bank_magic     = APP_CONFIG_BANK_MAGIC;
    hdr.bank_seq       = s_last_seq + 1U;
    hdr.payload_size   = APP_CONFIG_SIZE_BYTES;
    hdr.payload_offset = data_addr;
    hdr.header_crc16   = App_Config_HdrCrc(&hdr);

    err                = Bsp_Eeprom_Write(hdr_addr, (const uint8_t *)&hdr, sizeof(hdr));
    if (err != FM_OK)
    {
        LOG_ERROR("config: header write fail");
        return err;
    }

    s_active_bank = target;
    s_last_seq    = hdr.bank_seq;
    s_source      = (target == 'A') ? APP_CONFIG_LOADED_BANK_A : APP_CONFIG_LOADED_BANK_B;
    LOG_INFO_WITH_ARG("config: saved to Bank %c, seq=%lu", target, (unsigned long)hdr.bank_seq);
    return FM_OK;
}

/**
 * @brief   重新从 EEPROM 加载配置（丢弃 RAM 中未保存的修改）
 * @retval  FM_OK 成功；FM_ERR_001_EEPROM_CRC 表示重载失败且已恢复旧 RAM 副本
 */
Fm_ErrorCode App_Config_Reload(void)
{
    App_Config_Source prev     = s_source;
    App_Config        prev_cfg = s_runtime;
    App_Config_Init();
    if (s_source == APP_CONFIG_LOADED_FACTORY)
    {
        /* 失败：恢复之前的运行时副本 */
        s_runtime = prev_cfg;
        s_source  = prev;
        return FM_ERR_001_EEPROM_CRC;
    }
    return FM_OK;
}

/**
 * @brief   工厂复位：拷贝 g_factory_config 到 RAM 并连续保存两次以刷新双 Bank
 * @retval  FM_OK 成功；否则 EEPROM 错误码
 */
Fm_ErrorCode App_Config_FactoryReset(void)
{
    s_runtime        = g_factory_config;
    s_runtime.crc16  = App_Config_CalcCrc(&s_runtime);
    /* 写两个 Bank 各一次，保证一致 */
    Fm_ErrorCode err = App_Config_Save();
    if (err != FM_OK)
        return err;
    /* 再保存一次让另一 Bank 也被刷新 */
    return App_Config_Save();
}

/* ========================================== 字段命令解析（短名表） ========================================== */

/**
 * @brief   配置字段元数据（串口/菜单 SetField/GetField 用）
 */
typedef struct
{
    const char *name;   /**< 短字段名，如 "n_steps" */
    uint32_t    offset; /**< 在 App_Config 内的字节偏移 */
    uint8_t     size;   /**< 字段宽度：1 / 2 / 4 字节 */
    int32_t     lo, hi; /**< 合法取值下界与上界 */
} App_Config_FieldMeta;

/** @brief 注册 uint8_t 配置字段到 s_fields */
#define FIELD_U8(NAME, MEMBER, LO, HI)  {NAME, (uint32_t)(uintptr_t)&((App_Config *)0)->MEMBER, 1U, (LO), (HI)}
/** @brief 注册 uint16_t 配置字段到 s_fields */
#define FIELD_U16(NAME, MEMBER, LO, HI) {NAME, (uint32_t)(uintptr_t)&((App_Config *)0)->MEMBER, 2U, (LO), (HI)}

/** @brief 静态字段短名查找表（与菜单 s_param_names 一致） */
static const App_Config_FieldMeta s_fields[] = {
    FIELD_U8("n_steps", step_count, 1, FM_STEP_MAX),
    FIELD_U8("ch_en", channel_enable, 0, 15),
    FIELD_U8("idle", idle_seconds, 1, 30),
    FIELD_U8("gap_ms", inter_gap_ms_x10, 1, 250),
    FIELD_U16("tmo_ch", per_channel_timeout_s, 30, FM_PER_CHANNEL_HARD_LIMIT_S),
    FIELD_U16("tmo_all", total_timeout_s, 60, FM_TOTAL_TASK_HARD_LIMIT_S),
    FIELD_U16("selftest", selftest_pulse_ms, 10, 200),
    FIELD_U16("long_ms", long_press_ms_x10, 30, 300),
    FIELD_U16("hold_ms", hold_press_ms_x10, 100, 500),
    FIELD_U8("contrast", oled_contrast, 0, 255),
    FIELD_U8("log", log_level, 0, 3),
};

/**
 * @brief   解析动态字段名 duty[N] / sec[N]（N=0..7）
 * @param   name       字段名字符串
 * @param   out_off    输出：字节偏移
 * @param   out_size   输出：字段宽度（恒为 1）
 * @param   out_lo     输出：下界
 * @param   out_hi     输出：上界
 * @retval  true=匹配成功 / false=非 duty/sec 模式
 */
static bool App_Config_ResolveDuty(const char *name, uint32_t *out_off, uint8_t *out_size, int32_t *out_lo,
                                   int32_t *out_hi)
{
    if ((name[0] == 'd') && (name[1] == 'u') && (name[2] == 't') && (name[3] == 'y') && (name[4] == '['))
    {
        char d = name[5];
        if ((d < '0') || (d > '7'))
            return false;
        if (name[6] != ']')
            return false;
        *out_off  = (uint32_t)(uintptr_t)&((App_Config *)0)->step_duty[d - '0'];
        *out_size = 1U;
        *out_lo   = 0;
        *out_hi   = FM_PUMP_DUTY_MAX_PERCENT;
        return true;
    }
    if ((name[0] == 's') && (name[1] == 'e') && (name[2] == 'c') && (name[3] == '['))
    {
        char d = name[4];
        if ((d < '0') || (d > '7'))
            return false;
        if (name[5] != ']')
            return false;
        *out_off  = (uint32_t)(uintptr_t)&((App_Config *)0)->step_seconds[d - '0'];
        *out_size = 1U;
        *out_lo   = 0;
        *out_hi   = 60;
        return true;
    }
    return false;
}

/**
 * @brief   按短名查找字段元数据（含 s_fields 与 duty/sec 动态名）
 * @param   name       字段名
 * @param   out_off    输出：字节偏移
 * @param   out_size   输出：1 / 2 / 4
 * @param   out_lo     输出：下界
 * @param   out_hi     输出：上界
 * @retval  true=找到 / false=未知字段
 */
static bool App_Config_FindField(const char *name, uint32_t *out_off, uint8_t *out_size, int32_t *out_lo,
                                 int32_t *out_hi)
{
    if (App_Config_ResolveDuty(name, out_off, out_size, out_lo, out_hi))
    {
        return true;
    }
    for (uint32_t i = 0U; i < (sizeof(s_fields) / sizeof(s_fields[0])); i++)
    {
        if (strcmp(s_fields[i].name, name) == 0)
        {
            *out_off  = s_fields[i].offset;
            *out_size = s_fields[i].size;
            *out_lo   = s_fields[i].lo;
            *out_hi   = s_fields[i].hi;
            return true;
        }
    }
    return false;
}

/**
 * @brief   按字段名设置配置值（仅修改 RAM，不自动 Save）
 * @param   name   短字段名；NULL 返回 FM_ERR_012_INTERLOCK
 * @param   value  目标值（越界时裁剪到 [lo, hi]）
 * @retval  FM_OK 成功；FM_ERR_012_INTERLOCK 字段不存在
 */
Fm_ErrorCode App_Config_SetField(const char *name, int32_t value)
{
    if (name == 0)
        return FM_ERR_012_INTERLOCK;
    uint32_t off;
    uint8_t  size;
    int32_t  lo, hi;
    if (!App_Config_FindField(name, &off, &size, &lo, &hi))
    {
        return FM_ERR_012_INTERLOCK;
    }
    if (value < lo)
        value = lo;
    if (value > hi)
        value = hi;
    uint8_t *p = ((uint8_t *)&s_runtime) + off;
    if (size == 1U)
    {
        *p = (uint8_t)value;
    }
    else if (size == 2U)
    {
        uint16_t v = (uint16_t)value;
        memcpy(p, &v, 2U);
    }
    else
    {
        uint32_t v = (uint32_t)value;
        memcpy(p, &v, 4U);
    }
    return FM_OK;
}

/**
 * @brief   按字段名读取配置值
 * @param   name       短字段名；NULL 返回错误
 * @param   out_value  输出参数，不能为 NULL
 * @retval  FM_OK 成功；FM_ERR_012_INTERLOCK 字段不存在或参数非法
 */
Fm_ErrorCode App_Config_GetField(const char *name, int32_t *out_value)
{
    if ((name == 0) || (out_value == 0))
        return FM_ERR_012_INTERLOCK;
    uint32_t off;
    uint8_t  size;
    int32_t  lo, hi;
    if (!App_Config_FindField(name, &off, &size, &lo, &hi))
    {
        return FM_ERR_012_INTERLOCK;
    }
    const uint8_t *p = ((const uint8_t *)&s_runtime) + off;
    if (size == 1U)
    {
        *out_value = (int32_t)(*p);
    }
    else if (size == 2U)
    {
        uint16_t v;
        memcpy(&v, p, 2U);
        *out_value = (int32_t)v;
    }
    else
    {
        uint32_t v;
        memcpy(&v, p, 4U);
        *out_value = (int32_t)v;
    }
    return FM_OK;
}

/**
 * @brief   将当前 RAM 配置完整打印到日志（调试用）
 * @note    含加载源、bank_seq、update_count 及各主要字段
 */
void App_Config_Dump(void)
{
    const App_Config *c = &s_runtime;
    LOG_INFO_WITH_ARG("---- config dump (src=%c, seq=%lu, upd=%lu) ----",
                      (s_source == APP_CONFIG_LOADED_FACTORY) ? 'F'
                                                              : ((s_source == APP_CONFIG_LOADED_BANK_A) ? 'A' : 'B'),
                      (unsigned long)s_last_seq, (unsigned long)c->update_count);
    LOG_INFO_WITH_ARG("n_steps=%u  ch_en=0x%02X  idle=%u  gap_ms=%u", c->step_count, c->channel_enable, c->idle_seconds,
                      (unsigned)c->inter_gap_ms_x10 * 10U);
    for (uint32_t i = 0U; i < c->step_count; i++)
    {
        LOG_INFO_WITH_ARG("  step %lu: duty=%u%%, sec=%u", (unsigned long)i, c->step_duty[i], c->step_seconds[i]);
    }
    LOG_INFO_WITH_ARG("tmo_ch=%us  tmo_all=%us  selftest=%ums", c->per_channel_timeout_s, c->total_timeout_s,
                      c->selftest_pulse_ms);
    LOG_INFO_WITH_ARG("long_ms=%u  hold_ms=%u  contrast=%u  log_lv=%u", (unsigned)c->long_press_ms_x10 * 10U,
                      (unsigned)c->hold_press_ms_x10 * 10U, c->oled_contrast, c->log_level);
}
