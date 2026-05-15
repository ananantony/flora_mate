/*
 * @File         : \code\App\menu\app_menu.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 调试菜单接口（主菜单 / 参数编辑 / 手动测试 / 系统信息）
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
#ifndef APP_MENU_H
#define APP_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"

/**
 * @brief   菜单退出动作
 * @note    主 FSM 在 MENU 状态轮询 ExitRequested；非 NONE 时按 action 跳转。
 */
typedef enum
{
    APP_MENU_ACTION_NONE = 0,       /**< 留在菜单                 */
    APP_MENU_ACTION_START_WATER,    /**< 主菜单选"立即开始"        */
    APP_MENU_ACTION_RETURN_TO_AUTO, /**< 退出菜单回到 AUTO_RUN     */
    APP_MENU_ACTION_GOTO_SLEEP      /**< 进入 SLEEP（保留）         */
} App_Menu_Action;

/**
 * @brief   菜单页面索引
 */
typedef enum
{
    APP_MENU_PAGE_MAIN = 0,     /**< 主菜单                   */
    APP_MENU_PAGE_PARAMS,       /**< 参数列表                  */
    APP_MENU_PAGE_PARAM_EDIT,   /**< 单参数编辑                */
    APP_MENU_PAGE_MANUAL,       /**< 手动测试                  */
    APP_MENU_PAGE_INFO,         /**< 系统信息                  */
    APP_MENU_PAGE_FACTORY_RESET /**< 恢复出厂确认页            */
} App_Menu_Page;

/**
 * @brief   菜单模块初始化
 * @note    清空内部状态；不进入菜单，需调用 App_Menu_Enter 才开始绘制。
 */
void App_Menu_Init(void);

/**
 * @brief   进入菜单（一次性切换到主菜单页）
 */
void App_Menu_Enter(void);

/**
 * @brief   主循环周期调用（节流到 ~ 30 ms）
 * @note    更新动画、自动取消手动测试残留电平等；本身不绘制 OLED，
 *          绘制由 App_Display 在 MENU 状态委托完成。
 */
void App_Menu_Tick(void);

/**
 * @brief   将事件喂给菜单
 * @param   e  非 NULL，通常是 K1..K4 SHORT/LONG
 */
void App_Menu_OnEvent(const App_Event *e);

/**
 * @brief   是否请求退出菜单
 * @retval  true=主 FSM 应读取 ExitAction 后调用 ClearExit / false=继续留在菜单
 */
bool App_Menu_ExitRequested(void);

/**
 * @brief   读取退出动作（仅在 ExitRequested 为 true 时有意义）
 */
App_Menu_Action App_Menu_GetExitAction(void);

/**
 * @brief   清除退出请求（主 FSM 已消费动作后调用）
 */
void App_Menu_ClearExit(void);

/* ==== 供 App_Display 绘制使用的只读查询 ============================ */

/**
 * @brief   当前页面
 */
App_Menu_Page App_Menu_GetPage(void);

/**
 * @brief   当前页面的光标行号
 */
uint8_t App_Menu_GetCursor(void);

/**
 * @brief   当前页面的菜单项数（动态计算）
 */
uint8_t App_Menu_GetItemCount(void);

/**
 * @brief   取第 idx 项的显示文字（NULL 表示越界）
 */
const char *App_Menu_GetItemLabel(uint8_t idx);

/**
 * @brief   PARAM_EDIT 页：当前正在编辑的字段名
 */
const char *App_Menu_GetEditFieldName(void);

/**
 * @brief   PARAM_EDIT 页：当前编辑值
 */
int32_t App_Menu_GetEditValue(void);

#endif /* APP_MENU_H */
