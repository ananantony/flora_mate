/**
 * @file    app_display.h
 * @brief   OLED 8 个页面绘制调度（与软件设计方案 § 7.2 一致）
 * @note    根据主状态机 + 菜单状态决定当前页面；
 *          主循环每 100 ms 调一次 Tick；页面切换或 Dirty 时全屏 flush。
 */
#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

void App_Display_Init(void);
void App_Display_Tick(void);
void App_Display_MarkDirty(void);   /* 强制下次 Tick 重绘 */

#endif /* APP_DISPLAY_H */
