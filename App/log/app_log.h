/*
 * @File         : \code\App\log\app_log.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 15:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 15:30:00
 * @Description  : 日志客户端对外接口
 *                 - 提供初始化、格式化打印、限频、紧急模式、缓冲读取等接口
 *                 - 上层通过 LOG_XXX / LOG_XXX_WITH_ARG 宏输出日志
 *                 - 头部格式: [毫秒数][等级字符][设备名] [函数,行号] 用户内容\r\n
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 *
 *   _________________________________________________________________________
 *  | Date       | Version | Author      |  Description                       |
 *  |=========================================================================|
 *  | 2026-05-15 | 1.0.0   | tonymeng    | 初始版本（参考 cdd_log 实现）      |
 *  |-------------------------------------------------------------------------|
 *  |            |         |             |                                    |
 *  |-------------------------------------------------------------------------|
 */
#ifndef __APP_LOG_H__
#define __APP_LOG_H__

#include "app_log_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* ========================================== 回调类型 ========================================== */

    /**
     * @brief 日志直接发送回调函数类型
     * @note  由上层（如 USART 同步发送）注入。
     *        紧急模式下 log 客户端绕过环形缓冲，直接通过此回调发送。
     * @param data 日志数据指针
     * @param len  数据长度
     * @return true 成功 / false 失败
     */
    typedef bool (*App_Log_DirectSendFuncType)(const uint8_t *data, uint16_t len);

    /* ========================================== 公共接口 ========================================== */

    /**
     * @brief 初始化日志客户端
     * @return true 成功 / false 失败
     * @note  应在 Bsp_Tick_Init 之后调用（依赖系统时间戳）
     */
    bool App_Log_Init(void);

    /**
     * @brief 查询日志客户端是否已初始化
     * @return true 已初始化 / false 未初始化
     */
    bool App_Log_IsInit(void);

    /**
     * @brief 注册紧急模式直接发送回调
     * @param send_func 回调函数指针，NULL 时取消注册
     */
    void App_Log_RegisterDirectSend(App_Log_DirectSendFuncType send_func);

    /**
     * @brief 设置最低输出等级（高于此等级会被丢弃）
     * @param level 期望输出的最低等级（数值越大越冗长，VERBOSE 最冗长）
     * @note  例如 SetLevel(APP_LOG_LEVEL_WARN) 后，INFO/DEBUG/VERBOSE 将被过滤
     */
    void App_Log_SetLevel(App_Log_LevelType level);

    /**
     * @brief 查询当前最低输出等级
     * @return 最低输出等级
     */
    App_Log_LevelType App_Log_GetLevel(void);

    /**
     * @brief 切换到紧急模式
     * @note  切换后，后续日志将通过注册的回调同步发送，绕过环形缓冲。
     *        典型场景：HardFault 处理函数中调用。
     */
    void App_Log_SetEmergencyMode(void);

    /**
     * @brief 查询是否处于紧急模式
     * @return true 紧急模式 / false 正常模式
     */
    bool App_Log_IsEmergencyMode(void);

    /**
     * @brief 判断当前日志调用点是否满足限频周期
     * @param last_print_time_ms 调用点上次打印时间，调用方需为每个调用点独立保存
     * @param period_ms          最小打印间隔（毫秒），为 0 时不限制
     * @return true 允许打印 / false 仍在限制周期内
     */
    bool App_Log_CheckPrintPeriod(uint32_t *last_print_time_ms, uint32_t period_ms);

    /**
     * @brief 日志打印接口（底层实现，一般不直接调用）
     * @param level  日志等级
     * @param func   调用方函数名
     * @param line   调用方行号
     * @param format 格式化字符串（printf 风格）
     * @param ...    可变参数
     * @note  正常模式 → 写入环形缓冲，由 App_Log_Tick 搬运到 UART
     *        紧急模式 → 通过注册的回调同步发送
     */
    void App_Log_Print(App_Log_LevelType level, const char *func, int line, const char *format, ...);

    /**
     * @brief 从日志环形缓冲读取数据（单消费者）
     * @param out_data 输出缓冲
     * @param read_len 请求读取最大字节数
     * @return 实际读取字节数
     */
    uint16_t App_Log_ReadData(uint8_t *out_data, uint16_t read_len);

    /**
     * @brief 日志泵任务（主循环周期调用）
     * @note  从日志环形缓冲读出，转交给 USART 异步发送。
     *        建议每个主循环 tick 都调用一次。
     */
    void App_Log_Tick(void);

    /* ========================================== 日志输出宏 ========================================== */

#define APP_LOG_LIMIT_TIME_INVALID (0xFFFFFFFFU)
#define APP_LOG_DEFAULT_PERIOD_MS  (200U)

/* 不带参数日志的限频包装：同一调用点 period_ms 内最多打印一次 */
#define APP_LOG_PRINT_PERIOD(level, period_ms, message)                          \
    do                                                                           \
    {                                                                            \
        static uint32_t s_log_last_print_ms = APP_LOG_LIMIT_TIME_INVALID;        \
        if (App_Log_CheckPrintPeriod(&s_log_last_print_ms, (period_ms)) == true) \
        {                                                                        \
            App_Log_Print((level), __func__, __LINE__, "%s", (message));         \
        }                                                                        \
    }                                                                            \
    while (0)

/* 带参数日志的限频包装：同一调用点 period_ms 内最多打印一次 */
#define APP_LOG_PRINT_WITH_ARG_PERIOD(level, period_ms, format, ...)             \
    do                                                                           \
    {                                                                            \
        static uint32_t s_log_last_print_ms = APP_LOG_LIMIT_TIME_INVALID;        \
        if (App_Log_CheckPrintPeriod(&s_log_last_print_ms, (period_ms)) == true) \
        {                                                                        \
            App_Log_Print((level), __func__, __LINE__, (format), __VA_ARGS__);   \
        }                                                                        \
    }                                                                            \
    while (0)

/* 不带参数日志宏：默认 200ms 限频，避免热路径上重复日志刷屏 */
#define LOG_FATAL(message) APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_FATAL, APP_LOG_DEFAULT_PERIOD_MS, (message))
#define LOG_ERROR(message) APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_ERROR, APP_LOG_DEFAULT_PERIOD_MS, (message))
#define LOG_WARN(message)  APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_WARN, APP_LOG_DEFAULT_PERIOD_MS, (message))
#define LOG_INFO(message)  APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_INFO, APP_LOG_DEFAULT_PERIOD_MS, (message))
#define LOG_DEBUG(message) APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_DEBUG, APP_LOG_DEFAULT_PERIOD_MS, (message))

/* 带参数日志宏：tasking 编译器不支持 ##__VA_ARGS__，故强制要求至少一个可变参数 */
#define LOG_FATAL_WITH_ARG(format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_FATAL, APP_LOG_DEFAULT_PERIOD_MS, (format), __VA_ARGS__)
#define LOG_ERROR_WITH_ARG(format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_ERROR, APP_LOG_DEFAULT_PERIOD_MS, (format), __VA_ARGS__)
#define LOG_WARN_WITH_ARG(format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_WARN, APP_LOG_DEFAULT_PERIOD_MS, (format), __VA_ARGS__)
#define LOG_INFO_WITH_ARG(format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_INFO, APP_LOG_DEFAULT_PERIOD_MS, (format), __VA_ARGS__)
#define LOG_DEBUG_WITH_ARG(format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_DEBUG, APP_LOG_DEFAULT_PERIOD_MS, (format), __VA_ARGS__)

/* 自定义周期的不带参数日志宏 */
#define LOG_FATAL_PERIOD(period_ms, message) APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_FATAL, (period_ms), (message))
#define LOG_ERROR_PERIOD(period_ms, message) APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_ERROR, (period_ms), (message))
#define LOG_WARN_PERIOD(period_ms, message)  APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_WARN, (period_ms), (message))
#define LOG_INFO_PERIOD(period_ms, message)  APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_INFO, (period_ms), (message))
#define LOG_DEBUG_PERIOD(period_ms, message) APP_LOG_PRINT_PERIOD(APP_LOG_LEVEL_DEBUG, (period_ms), (message))

/* 自定义周期的带参数日志宏 */
#define LOG_FATAL_WITH_ARG_PERIOD(period_ms, format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_FATAL, (period_ms), (format), __VA_ARGS__)
#define LOG_ERROR_WITH_ARG_PERIOD(period_ms, format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_ERROR, (period_ms), (format), __VA_ARGS__)
#define LOG_WARN_WITH_ARG_PERIOD(period_ms, format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_WARN, (period_ms), (format), __VA_ARGS__)
#define LOG_INFO_WITH_ARG_PERIOD(period_ms, format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_INFO, (period_ms), (format), __VA_ARGS__)
#define LOG_DEBUG_WITH_ARG_PERIOD(period_ms, format, ...) \
    APP_LOG_PRINT_WITH_ARG_PERIOD(APP_LOG_LEVEL_DEBUG, (period_ms), (format), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __APP_LOG_H__ */
