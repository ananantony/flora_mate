/**
 * @file    app_log.h
 * @brief   4 等级日志宏（E/W/I/D），底层走 USART1 异步 TX
 * @note    使用 printf；syscalls.c 的 _write 已重定向到 Bsp_Usart_Log_Write。
 */
#ifndef APP_LOG_H
#define APP_LOG_H

#include <stdio.h>
#include <stdint.h>

typedef enum {
    APP_LOG_LEVEL_NONE  = 0,
    APP_LOG_LEVEL_ERROR = 1,
    APP_LOG_LEVEL_WARN  = 2,
    APP_LOG_LEVEL_INFO  = 3,
    APP_LOG_LEVEL_DEBUG = 4
} App_Log_Level;

void          App_Log_SetLevel(App_Log_Level lv);
App_Log_Level App_Log_GetLevel(void);
uint32_t      App_Log_NowMs(void);

#define APP_LOG_TAG_LINE(tag, lv, fmt, ...) \
    do { \
        if (App_Log_GetLevel() >= (lv)) { \
            printf("[%lu][" tag "] " fmt "\r\n", \
                   (unsigned long)App_Log_NowMs(), ##__VA_ARGS__); \
        } \
    } while (0)

#define LOGE(fmt, ...) APP_LOG_TAG_LINE("E", APP_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) APP_LOG_TAG_LINE("W", APP_LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) APP_LOG_TAG_LINE("I", APP_LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) APP_LOG_TAG_LINE("D", APP_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)

#endif /* APP_LOG_H */
