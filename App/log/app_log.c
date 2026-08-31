/*
 * @File         : \code\App\log\app_log.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 15:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 16:15:36
 * @Description  : 日志客户端实现
 *                 - 正常模式：格式化为字符串后写入环形缓冲，由 App_Log_Tick 异步消费
 *                 - 紧急模式：格式化后直接通过注册的回调同步发送（绕过缓冲）
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
#include "app_log.h"
#include "app_log_ring_buffer.h"
#include "bsp_tick.h"
#include "bsp_usart_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ========================================== 内部数据 ========================================== */

static bool                       s_init_done        = false;
static bool                       s_emergency_mode   = false;
static App_Log_LevelType          s_min_level        = APP_LOG_LEVEL_INFO;
static App_Log_DirectSendFuncType s_direct_send_func = NULL;
static App_Log_RingBufferType     s_ring_buffer;

/* App_Log_Tick 临时搬运缓冲：栈上分配避免占用全局 RAM；
 * 每次最多搬运 64B，足够喂饱 UART 而又不阻塞主循环。 */
#define APP_LOG_TICK_PUMP_SIZE (64U)

/* ========================================== 内部函数 ========================================== */

/**
 * @brief 将日志等级转换为单字符缩写
 * @param level 日志等级
 * @return 'F'/'E'/'W'/'I'/'D'/'V'/'U'
 */
static char App_Log_LevelToChar(App_Log_LevelType level)
{
    char result = 'U';

    switch (level)
    {
        case APP_LOG_LEVEL_FATAL:
            result = 'F';
            break;
        case APP_LOG_LEVEL_ERROR:
            result = 'E';
            break;
        case APP_LOG_LEVEL_WARN:
            result = 'W';
            break;
        case APP_LOG_LEVEL_INFO:
            result = 'I';
            break;
        case APP_LOG_LEVEL_DEBUG:
            result = 'D';
            break;
        case APP_LOG_LEVEL_VERBOSE:
            result = 'V';
            break;
        default:
            break;
    }

    return result;
}

/**
 * @brief 格式化一条完整日志到缓冲区
 * @param buf      输出缓冲区
 * @param buf_size 缓冲区大小
 * @param level    日志等级
 * @param func     函数名
 * @param line     行号
 * @param format   格式化字符串
 * @param args     可变参数列表
 * @return 格式化后的有效字节数（不含末尾 \0，含 \r\n）
 */
static uint16_t App_Log_FormatEntry(char *buf, uint16_t buf_size, App_Log_LevelType level, const char *func, int line,
                                    const char *format, va_list args)
{
    uint32_t timestamp_ms = Bsp_Tick_GetMs();
    char     level_char   = App_Log_LevelToChar(level);
    uint32_t used         = 0U;

    /* 头部: [毫秒数][等级][设备名] */
    used                  = (uint32_t)snprintf(buf, buf_size, "[%lu][%c][%s]", (unsigned long)timestamp_ms, level_char,
                                               APP_LOG_DEVICE_NAME);

    /* 函数名与行号 */
    if (used < buf_size)
    {
        used += (uint32_t)snprintf(buf + used, buf_size - used, " [%s,%d] ", func, line);
    }

    /* 用户内容 */
    if (used < buf_size)
    {
        used += (uint32_t)vsnprintf(buf + used, buf_size - used, format, args);
    }

    /* 追加换行 */
    if ((used + 2U) < buf_size)
    {
        buf[used++] = '\r';
        buf[used++] = '\n';
        buf[used]   = '\0';
    }
    else if (buf_size > 0U)
    {
        buf[buf_size - 1U] = '\0';
        used               = (uint32_t)strlen(buf);
    }

    return (uint16_t)used;
}

/* ========================================== 外部函数 ========================================== */

/**
 * @brief   日志模块初始化
 * @note    初始化环形缓冲并将内部状态清零；需在 Bsp_Tick_Init 之后调用，
 *          因为后续 Print 调用会读取时间戳。
 * @retval  true=成功，false=环形缓冲初始化失败（内存或参数异常）
 */
bool App_Log_Init(void)
{
    s_emergency_mode   = false;
    s_direct_send_func = NULL;

    if (App_Log_RingBufferInit(&s_ring_buffer) != true)
    {
        return false;
    }

    s_init_done = true;
    return true;
}

/**
 * @brief   查询日志模块是否已完成初始化
 * @retval  true=已初始化，false=尚未初始化
 */
bool App_Log_IsInit(void)
{
    return s_init_done;
}

/**
 * @brief   注册紧急模式直传回调
 * @param   send_func  发送函数指针；传 NULL 可清除注册
 * @note    进入紧急模式（App_Log_SetEmergencyMode）后，每条日志都会同步调用
 *          此函数直接发出，绕过环形缓冲，适合 HardFault/断言失败等场景。
 */
void App_Log_RegisterDirectSend(App_Log_DirectSendFuncType send_func)
{
    s_direct_send_func = send_func;
}

/**
 * @brief   设置日志过滤等级
 * @param   level  最低输出等级（数值越小越严重）；超过此值的日志被丢弃
 * @note    等级越高（如 VERBOSE=6）输出越多；建议生产版本设为 WARN 或 INFO。
 */
void App_Log_SetLevel(App_Log_LevelType level)
{
    if ((level >= APP_LOG_LEVEL_FATAL) && (level < APP_LOG_LEVEL_MAX))
    {
        s_min_level = level;
    }
}

/**
 * @brief   获取当前日志过滤等级
 * @retval  当前最低输出等级（App_Log_LevelType 枚举值）
 */
App_Log_LevelType App_Log_GetLevel(void)
{
    return s_min_level;
}

/**
 * @brief   进入紧急模式
 * @note    设置后无法取消（单向锁）；进入紧急模式后所有日志通过直传回调同步发出，
 *          适用于 HardFault/断言失败等需要最终遗言的场景。
 */
void App_Log_SetEmergencyMode(void)
{
    s_emergency_mode = true;
}

/**
 * @brief   查询是否处于紧急模式
 * @retval  true=紧急模式，false=正常异步模式
 */
bool App_Log_IsEmergencyMode(void)
{
    return s_emergency_mode;
}

/**
 * @brief   限速检查：判断距上次打印是否已超过指定周期
 * @param   last_print_time_ms  上次打印时间戳的持久指针（调用方静态变量）；
 *                              初始值应设为 APP_LOG_LIMIT_TIME_INVALID
 * @param   period_ms           限速周期（毫秒）；0 表示不限速（每次都允许）
 * @retval  true=本次允许打印并更新时间戳，false=仍在冷却期
 */
bool App_Log_CheckPrintPeriod(uint32_t *last_print_time_ms, uint32_t period_ms)
{
    if (last_print_time_ms == NULL)
    {
        return false;
    }

    if (period_ms == 0U)
    {
        return true;
    }

    uint32_t current_time_ms = Bsp_Tick_GetMs();

    if ((*last_print_time_ms == APP_LOG_LIMIT_TIME_INVALID) ||
        ((uint32_t)(current_time_ms - *last_print_time_ms) >= period_ms))
    {
        *last_print_time_ms = current_time_ms;
        return true;
    }

    return false;
}

/**
 * @brief   格式化并提交一条日志（内部被 LOG_* 宏调用）
 * @param   level   日志等级
 * @param   func    调用函数名（由宏传入 __func__）
 * @param   line    调用行号（由宏传入 __LINE__）
 * @param   format  printf 格式字符串
 * @param   ...     可变参数
 * @note    正常模式下写入环形缓冲，由 App_Log_Tick 异步消费；
 *          紧急模式下通过注册的直传回调同步发送。
 */
void App_Log_Print(App_Log_LevelType level, const char *func, int line, const char *format, ...)
{
    if (s_init_done == false)
    {
        return;
    }

    /* 等级过滤：数值大的等级更冗长，超过当前阈值则丢弃 */
    if (level > s_min_level)
    {
        return;
    }

    char    log_entry[APP_LOG_ENTRY_MAX_SIZE];
    va_list args;

    va_start(args, format);
    uint16_t entry_len = App_Log_FormatEntry(log_entry, APP_LOG_ENTRY_MAX_SIZE, level, func, line, format, args);
    va_end(args);

    if (entry_len == 0U)
    {
        return;
    }

    /* 紧急模式：直接通过回调同步发送，绕过环形缓冲 */
    if ((s_emergency_mode == true) && (s_direct_send_func != NULL))
    {
        (void)s_direct_send_func((const uint8_t *)log_entry, entry_len);
        return;
    }

    /* 正常模式：写入环形缓冲，等待 App_Log_Tick 消费 */
    (void)App_Log_RingBufferPush(&s_ring_buffer, (const uint8_t *)log_entry, entry_len);
}

/**
 * @brief   从日志环形缓冲中读取原始字节（供外部消费者使用）
 * @param   out_data  输出缓冲区
 * @param   read_len  期望读取字节数
 * @retval  实际读出的字节数；0 表示缓冲为空或参数非法
 * @note    通常不需要直接调用此接口；App_Log_Tick 内部已自动消费并转发给 UART。
 */
uint16_t App_Log_ReadData(uint8_t *out_data, uint16_t read_len)
{
    if ((s_init_done == false) || (out_data == NULL) || (read_len == 0U))
    {
        return 0U;
    }

    return App_Log_RingBufferPop(&s_ring_buffer, out_data, read_len);
}

/**
 * @brief   日志周期任务（在主循环每轮调用一次）
 * @note    每次最多从环形缓冲搬运 APP_LOG_TICK_PUMP_SIZE 字节到 UART TX 缓冲；
 *          若 UART TX 缓冲已满则丢弃本批，避免阻塞主循环。
 *          需在 Bsp_Usart_Log_TxFlush 之前调用以保证数据能及时发出。
 */
void App_Log_Tick(void)
{
    uint8_t  pump_buf[APP_LOG_TICK_PUMP_SIZE];
    uint16_t pop_len;

    if (s_init_done == false)
    {
        return;
    }

    pop_len = App_Log_RingBufferPop(&s_ring_buffer, pump_buf, (uint16_t)APP_LOG_TICK_PUMP_SIZE);
    if (pop_len == 0U)
    {
        return;
    }

    /* 转交给 UART 异步发送。若 UART TX 缓冲已满，丢弃本批日志，
     * 避免阻塞主循环或让日志反压影响实时控制。 */
    (void)Bsp_Usart_Log_Write(pump_buf, pop_len);
}
