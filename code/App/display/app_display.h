/*
 * @File         : \code\App\display\app_display.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : OLED 显示调度接口（按主 FSM/菜单状态选择页面并刷新）
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
#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   显示模块初始化
 * @note    初始化节拍器与"dirty"标志；BSP 层 OLED 必须已先 Init。
 */
void App_Display_Init(void);

/**
 * @brief   主循环周期调用（内部 100 ms 节流）
 * @note    根据 App_Main_Fsm_GetState() / App_Menu_GetPage() 决定渲染哪个页面；
 *          页面切换时整屏 Flush，停留时仅局部 FlushPages（倒计时区域）。
 */
void App_Display_Tick(void);

/**
 * @brief   强制下次 Tick 全屏重绘
 * @note    其他模块（菜单 / 主 FSM）状态切换后调用；本模块不主动监听状态变化，
 *          因此通过显式 MarkDirty 触发刷新更直观。
 */
void App_Display_MarkDirty(void);

/**
 * @brief   立即按当前状态全屏重绘并 Flush（状态切换时调用，避免残影叠加）
 */
void App_Display_FlushNow(void);

#endif /* APP_DISPLAY_H */
