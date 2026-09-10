/*
 * @File         : \code\App\menu\app_menu.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-09-10 12:10:00
 * @Description  : HMI V2.0 菜单接口（主界面 / 设置 / 参数 / 本地测试）
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
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
    APP_MENU_ACTION_START_WATER,    /**< 主界面选 Auto Water      */
    APP_MENU_ACTION_ENTER_SERIAL,   /**< 设置选 Serial Test       */
    APP_MENU_ACTION_RETURN_TO_AUTO, /**< 保留：回 AUTO_RUN        */
    APP_MENU_ACTION_GOTO_SLEEP      /**< 保留：进休眠             */
} App_Menu_Action;

/**
 * @brief   菜单页面索引（HMI V2.0）
 */
typedef enum
{
    APP_MENU_PAGE_MAIN = 0,    /**< 主界面：Auto Water / Settings */
    APP_MENU_PAGE_SETTINGS,    /**< 设置：Params / Local / Serial */
    APP_MENU_PAGE_PARAMS,      /**< 参数列表                       */
    APP_MENU_PAGE_PARAM_EDIT,  /**< 单参数编辑                     */
    APP_MENU_PAGE_LOCAL_TEST,  /**< 本地测试子菜单                 */
    APP_MENU_PAGE_SCREEN_TEST, /**< 屏幕图案测试                   */
    APP_MENU_PAGE_KEY_TEST,    /**< 按键测试                       */
    APP_MENU_PAGE_WATER_TEST   /**< 供水测试（阀+泵）              */
} App_Menu_Page;

void App_Menu_Init(void);
void App_Menu_Enter(void);
/** 进入设置页（串口调试从设置返回时使用） */
void App_Menu_EnterSettings(void);
void App_Menu_Tick(void);
void App_Menu_OnEvent(const App_Event *e);

bool            App_Menu_ExitRequested(void);
App_Menu_Action App_Menu_GetExitAction(void);
void            App_Menu_ClearExit(void);

/* ==== 供 App_Display 绘制使用的只读查询 ============================ */

App_Menu_Page App_Menu_GetPage(void);
uint8_t       App_Menu_GetCursor(void);
uint8_t       App_Menu_GetItemCount(void);
const char   *App_Menu_GetItemLabel(uint8_t idx);
const char   *App_Menu_GetEditFieldName(void);
int32_t       App_Menu_GetEditValue(void);

/** PARAM_EDIT 页：是否正在编辑参数 */
bool App_Menu_IsParamEditing(void);

/** ch_en 编辑中当前选中的通道索引 0..4（Z1..Z5） */
uint8_t App_Menu_GetChEnEditChannel(void);

/** WATER_TEST：泵百分比是否处于编辑态 */
bool App_Menu_IsPumpEditing(void);

/** WATER_TEST：当前泵占空比缓存（%） */
uint8_t App_Menu_GetPumpPercent(void);

/** SCREEN_TEST：当前图案索引 0..5 */
uint8_t App_Menu_GetScreenPattern(void);

/** KEY_TEST：K1..K4 按下掩码 bit0=K1 */
uint8_t App_Menu_GetKeyTestMask(void);

#endif /* APP_MENU_H */
