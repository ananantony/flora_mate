/**
 * @file    app_log.c
 * @brief   日志等级状态
 */
#include "app_log.h"
#include "bsp_tick.h"

static App_Log_Level s_level = APP_LOG_LEVEL_INFO;

void App_Log_SetLevel(App_Log_Level lv) {
    if ((uint32_t)lv > (uint32_t)APP_LOG_LEVEL_DEBUG) {
        lv = APP_LOG_LEVEL_DEBUG;
    }
    s_level = lv;
}

App_Log_Level App_Log_GetLevel(void) {
    return s_level;
}

uint32_t App_Log_NowMs(void) {
    return Bsp_Tick_GetMs();
}
