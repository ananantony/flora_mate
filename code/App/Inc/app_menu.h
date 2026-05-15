/**
 * @file    app_menu.h
 * @brief   调试菜单（主菜单 → 参数编辑/手动测试/系统信息/恢复出厂/退出）
 * @note    通过 App_Event 接收按键事件，绘制委托给 App_Display。
 */
#ifndef APP_MENU_H
#define APP_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"

typedef enum {
    APP_MENU_ACTION_NONE = 0,
    APP_MENU_ACTION_START_WATER,
    APP_MENU_ACTION_RETURN_TO_AUTO,
    APP_MENU_ACTION_GOTO_SLEEP
} App_Menu_Action;

typedef enum {
    APP_MENU_PAGE_MAIN = 0,
    APP_MENU_PAGE_PARAMS,
    APP_MENU_PAGE_PARAM_EDIT,
    APP_MENU_PAGE_MANUAL,
    APP_MENU_PAGE_INFO,
    APP_MENU_PAGE_FACTORY_RESET
} App_Menu_Page;

void              App_Menu_Init(void);
void              App_Menu_Enter(void);
void              App_Menu_Tick(void);
void              App_Menu_OnEvent(const App_Event *e);

bool              App_Menu_ExitRequested(void);
App_Menu_Action   App_Menu_GetExitAction(void);
void              App_Menu_ClearExit(void);

/* 状态查询（供 App_Display 绘制使用） */
App_Menu_Page     App_Menu_GetPage(void);
uint8_t           App_Menu_GetCursor(void);
uint8_t           App_Menu_GetItemCount(void);
const char *      App_Menu_GetItemLabel(uint8_t idx);
const char *      App_Menu_GetEditFieldName(void);
int32_t           App_Menu_GetEditValue(void);

#endif /* APP_MENU_H */
