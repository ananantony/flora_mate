/*
 * @File         : \code\App\log\app_log_types.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 15:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 15:30:00
 * @Description  : 日志组件公共类型定义
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
#ifndef __APP_LOG_TYPES_H__
#define __APP_LOG_TYPES_H__

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  日志等级（参考 DLT，数值从 1 起，0 保留作哨兵）
 * @note   过滤时：仅输出 level ≤ App_Log 当前等级的记录。
 */
typedef enum
{
    APP_LOG_LEVEL_FATAL   = 1, /**< 致命：系统无法继续运行 */
    APP_LOG_LEVEL_ERROR,       /**< 错误：业务异常，系统仍可运行 */
    APP_LOG_LEVEL_WARN,        /**< 警告：可疑情况，可能演化为错误 */
    APP_LOG_LEVEL_INFO,        /**< 信息：关键状态变迁、参数变化 */
    APP_LOG_LEVEL_DEBUG,       /**< 调试：开发期排查 */
    APP_LOG_LEVEL_VERBOSE,     /**< 详细：热路径/循环内细节，默认关闭 */
    APP_LOG_LEVEL_MAX          /**< 枚举上界（非有效等级） */
} App_Log_LevelType;

/** @def APP_LOG_RING_DATA_MAX_SIZE
 *  @brief 日志环形缓冲区容量（字节），默认 4096，可在编译前 #define 覆盖。 */
#ifndef APP_LOG_RING_DATA_MAX_SIZE
#define APP_LOG_RING_DATA_MAX_SIZE (4096U)
#endif

/** @def APP_LOG_ENTRY_MAX_SIZE
 *  @brief 单条日志最大长度（字节），含头、函数名、行号、正文、\\r\\n 与 \\0，默认 256。 */
#ifndef APP_LOG_ENTRY_MAX_SIZE
#define APP_LOG_ENTRY_MAX_SIZE (256U)
#endif

/** @def APP_LOG_DEVICE_NAME
 *  @brief 设备名字符串，出现在每条日志头部，便于多机区分，默认 "FloraMate"。 */
#ifndef APP_LOG_DEVICE_NAME
#define APP_LOG_DEVICE_NAME "FloraMate"
#endif

#endif /* __APP_LOG_TYPES_H__ */
