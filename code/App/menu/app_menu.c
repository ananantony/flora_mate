/*
 * @File         : \code\App\menu\app_menu.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 16:45:00
 * @Description  : 调试菜单实现
 *                 简化版菜单：主菜单 -> 参数编辑/手动测试/系统信息/出厂/退出。
 *                 参数编辑直接复用 App_Config_SetField 短名表，省得维护两套。
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 *
 *   _________________________________________________________________________
 *  | Date       | Version | Author      |  Description                       |
 *  |=========================================================================|
 *  | 2026-05-15 | 1.0.0   | tonymeng    | 初始版本                           |
 *  |-------------------------------------------------------------------------|
 */
#include "app_menu.h"
#include "app_config.h"
#include "app_display.h"
#include "app_log.h"
#include "bsp_valve.h"
#include "bsp_pump_pwm.h"
#include "bsp_key.h"
#include "bsp_tick.h"

#include <string.h>

/* ========================================== 常量与表 ========================================== */

#define MAIN_MENU_ITEM_COUNT (6U) /**< 主菜单条目数 */

/** @brief 主菜单显示文案（英文，供 OLED 显示） */
static const char *s_main_items[MAIN_MENU_ITEM_COUNT] = {"1.Start Water", "2.Params",        "3.Manual Test",
                                                         "4.System Info", "5.Factory Reset", "6.Exit (sleep)"};

/** @brief 参数编辑短名表（与 App_Config 短名一致） */
static const char *s_param_names[] = {"n_steps", "ch_en",   "idle",     "gap_ms", "tmo_ch",  "tmo_all", "selftest",
                                      "long_ms", "hold_ms", "contrast", "log",    "duty[0]", "duty[1]", "duty[2]",
                                      "duty[3]", "sec[0]",  "sec[1]",   "sec[2]", "sec[3]"};
#define PARAM_NAMES_COUNT (sizeof(s_param_names) / sizeof(s_param_names[0]))

/** @brief 手动测试页条目 */
static const char *s_manual_items[] = {"V1 ON/OFF",  "V2 ON/OFF",     "V3 ON/OFF", "V4 ON/OFF",
                                       "Pump PWR",   "PWM duty +/-",  "Back"};
#define MANUAL_ITEMS_COUNT (sizeof(s_manual_items) / sizeof(s_manual_items[0]))

/* ========================================== 内部上下文 ========================================== */

/**
 * @brief   菜单模块运行时上下文（单例 s_ctx）
 */
typedef struct
{
    App_Menu_Page   page;             /**< 当前页面               */
    uint8_t         cursor;           /**< 光标位置（0-based）    */
    int32_t         edit_value;       /**< PARAM_EDIT 暂存编辑值  */
    bool            exit_request;     /**< 是否请求退出菜单       */
    App_Menu_Action exit_action;      /**< 退出时告知 FSM 的动作  */
    uint8_t         manual_pump_duty; /**< 手动页水泵占空比 %     */
    uint32_t        page_enter_ms;    /**< 进入当前页的 tick ms   */
} App_Menu_Ctx;

static App_Menu_Ctx s_ctx; /**< 菜单单例上下文 */

/* ========================================== 公开 API：生命周期 ========================================== */

/**
 * @brief   菜单模块初始化（上电调用一次）
 */
void App_Menu_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.page = APP_MENU_PAGE_MAIN;
}

/**
 * @brief   进入调试菜单（主 FSM 切到 MENU 态时调用）
 */
void App_Menu_Enter(void)
{
    s_ctx.page             = APP_MENU_PAGE_MAIN;
    s_ctx.cursor           = 0U;
    s_ctx.exit_request     = false;
    s_ctx.exit_action      = APP_MENU_ACTION_NONE;
    s_ctx.manual_pump_duty = 0U;
    s_ctx.page_enter_ms    = Bsp_Tick_GetMs();
    LOG_INFO("menu: enter");
}

/**
 * @brief   查询是否已请求退出菜单
 * @retval  true=已请求，FSM 应读取 GetExitAction / false=仍在菜单内
 */
bool App_Menu_ExitRequested(void)
{
    return s_ctx.exit_request;
}

/**
 * @brief   获取退出动作（仅在 ExitRequested 为 true 时有效）
 * @retval  APP_MENU_ACTION_* 枚举值
 */
App_Menu_Action App_Menu_GetExitAction(void)
{
    return s_ctx.exit_action;
}

/**
 * @brief   清除退出请求（FSM 处理完退出动作后调用）
 */
void App_Menu_ClearExit(void)
{
    s_ctx.exit_request = false;
    s_ctx.exit_action  = APP_MENU_ACTION_NONE;
}

/* ========================================== 公开 API：生命周期 ========================================== */

/**
 * @brief   获取当前菜单页
 * @retval  App_Menu_Page 枚举值
 */
App_Menu_Page App_Menu_GetPage(void)
{
    return s_ctx.page;
}

/**
 * @brief   获取当前菜单页?
 * @retval  0-based 光标
 */
uint8_t App_Menu_GetCursor(void)
{
    return s_ctx.cursor;
}

/**
 * @brief   获取当前页列表项数量
 * @retval  随 page 变化（主菜单/参数/手动等）
 */
uint8_t App_Menu_GetItemCount(void)
{
    switch (s_ctx.page)
    {
        case APP_MENU_PAGE_MAIN:
            return MAIN_MENU_ITEM_COUNT;
        case APP_MENU_PAGE_PARAMS:
            return (uint8_t)PARAM_NAMES_COUNT;
        case APP_MENU_PAGE_MANUAL:
            return (uint8_t)MANUAL_ITEMS_COUNT;
        case APP_MENU_PAGE_PARAM_EDIT:
        case APP_MENU_PAGE_INFO:
        case APP_MENU_PAGE_FACTORY_RESET:
        default:
            return 1U;
    }
}

/**
 * @brief   查询是否已请求退出菜单
 * @param   idx  条目索引（0-based）
 * @retval  文案指针；越界返回 ""
 */
const char *App_Menu_GetItemLabel(uint8_t idx)
{
    switch (s_ctx.page)
    {
        case APP_MENU_PAGE_MAIN:
            return (idx < MAIN_MENU_ITEM_COUNT) ? s_main_items[idx] : "";
        case APP_MENU_PAGE_PARAMS:
            return (idx < PARAM_NAMES_COUNT) ? s_param_names[idx] : "";
        case APP_MENU_PAGE_MANUAL:
            return (idx < MANUAL_ITEMS_COUNT) ? s_manual_items[idx] : "";
        default:
            return "";
    }
}

/**
 * @brief   PARAM_EDIT 页：当前编辑字段名
 * @retval  字段短名；无效光标返回 ""
 */
const char *App_Menu_GetEditFieldName(void)
{
    if (s_ctx.cursor < PARAM_NAMES_COUNT)
    {
        return s_param_names[s_ctx.cursor];
    }
    return "";
}

/**
 * @brief   PARAM_EDIT 页：当前编辑数值
 * @retval  s_ctx.edit_value
 */
int32_t App_Menu_GetEditValue(void)
{
    return s_ctx.edit_value;
}

/* ========================================== 内部辅助：导航 ========================================== */

/**
 * @brief   获取当前菜单页?
 */
static void Cursor_Up(void)
{
    uint8_t n = App_Menu_GetItemCount();
    if (n == 0U)
        return;
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + n - 1U) % n);
    App_Display_MarkDirty();
}

/**
 * @brief   获取当前菜单页?
 */
static void Cursor_Down(void)
{
    uint8_t n = App_Menu_GetItemCount();
    if (n == 0U)
        return;
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + 1U) % n);
    App_Display_MarkDirty();
}

/**
 * @brief   获取当前光标索引
 * @param   p  目标页
 */
static void Go_Page(App_Menu_Page p)
{
    s_ctx.page          = p;
    s_ctx.cursor        = 0U;
    s_ctx.page_enter_ms = Bsp_Tick_GetMs();
    App_Display_MarkDirty();
    App_Display_FlushNow();
}

/**
 * @brief   查询是否已请求退出菜单
 * @note    先 App_Config_GetField 读出当前值写入 edit_value
 */
static void Enter_Edit_For_Cursor(void)
{
    const char *name = s_param_names[s_ctx.cursor];
    int32_t     v    = 0;
    (void)App_Config_GetField(name, &v);
    s_ctx.edit_value = v;
    Go_Page(APP_MENU_PAGE_PARAM_EDIT);
}

/**
 * @brief   按字段名返回 K1/K2 加减步长
 * @param   name  字段短名
 * @retval  步长（如 duty 为 5，tmo_all 为 30）
 */
static int32_t Field_Step(const char *name)
{
    if (strncmp(name, "duty", 4) == 0)
        return 5;
    if (strncmp(name, "sec", 3) == 0)
        return 1;
    if (strcmp(name, "tmo_ch") == 0)
        return 5;
    if (strcmp(name, "tmo_all") == 0)
        return 30;
    if (strcmp(name, "selftest") == 0)
        return 10;
    if (strcmp(name, "gap_ms") == 0)
        return 10;
    if (strcmp(name, "long_ms") == 0)
        return 10;
    if (strcmp(name, "hold_ms") == 0)
        return 50;
    if (strcmp(name, "contrast") == 0)
        return 16;
    return 1;
}

/**
 * @brief   提交 PARAM_EDIT：写 RAM（App_Config_SetField），不自动 Save
 * @note    持久化需在参数列表页长按 K3
 */
static void Commit_Edit(void)
{
    const char  *name = s_param_names[s_ctx.cursor];
    Fm_ErrorCode err  = App_Config_SetField(name, s_ctx.edit_value);
    if (err == FM_OK)
    {
        LOG_INFO_WITH_ARG("menu: %s = %ld (RAM only; save in main menu)", name, (long)s_ctx.edit_value);
    }
    else
    {
        LOG_ERROR_WITH_ARG("menu: set %s fail", name);
    }
    Go_Page(APP_MENU_PAGE_PARAMS);
}

/* ========================================== 内部辅助：手动测试 ========================================== */

/**
 * @brief   切换 Zone 电磁阀（Z1~Z4 → 继电器 CH2~CH5）
 * @param   z  Zone 索引 0..3
 */
static void Manual_Toggle_Valve(uint8_t z)
{
    if (z >= FM_CHANNEL_NUM)
        return;
    Bsp_Valve_Channel ch  = (Bsp_Valve_Channel)((uint32_t)BSP_VALVE_Z1 + (uint32_t)z);
    bool              now = Bsp_Valve_Get(ch);
    Fm_ErrorCode      err = Bsp_Valve_Set(ch, !now);
    LOG_INFO_WITH_ARG("manual: V%u -> %s (err=0x%02X)", (unsigned)(z + 1U), now ? "OFF" : "ON", (unsigned)err);
}

static void Manual_Toggle_PumpPwr(void)
{
    bool         now = Bsp_Valve_Get(BSP_VALVE_PUMP_EN);
    Fm_ErrorCode err = Bsp_Valve_Set(BSP_VALVE_PUMP_EN, !now);
    LOG_INFO_WITH_ARG("manual: PumpPWR -> %s (err=0x%02X)", now ? "OFF" : "ON", (unsigned)err);
}

/**
 * @brief   获取当前页 PWM 占空比
 * @param   delta  增减量（可为负）
 */
static void Manual_Adjust_Pump(int8_t delta)
{
    int16_t v = (int16_t)s_ctx.manual_pump_duty + delta;
    if (v < 0)
        v = 0;
    if (v > FM_PUMP_DUTY_MAX_PERCENT)
        v = FM_PUMP_DUTY_MAX_PERCENT;
    s_ctx.manual_pump_duty = (uint8_t)v;
    Fm_ErrorCode err       = Bsp_Pump_Pwm_SetDutyPercent(s_ctx.manual_pump_duty);
    LOG_INFO_WITH_ARG("manual: pump duty=%u%% (err=0x%02X)", (unsigned)s_ctx.manual_pump_duty, (unsigned)err);
}

/**
 * @brief   菜单模块初始化（上电调用一次）? PWM
 */
static void Cleanup_Outputs(void)
{
    Bsp_Pump_Pwm_Stop();
    Bsp_Valve_AllOff();
    s_ctx.manual_pump_duty = 0U;
}

/* ========================================== 公开 API：事件与周期 ========================================== */

/**
 * @brief   菜单按键/事件处理（由主 FSM 转发）
 * @param   e  应用事件；NULL 忽略
 * @note    统一按键：K1 下移/加 K2 上移/减 K3 确定 K4 退出
 */
void App_Menu_OnEvent(const App_Event *e)
{
    if (e == 0)
        return;

    switch (s_ctx.page)
    {

        case APP_MENU_PAGE_MAIN:
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    Cursor_Down();
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    Cursor_Up();
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    /* 确定：进入子页或开始浇水 */
                    switch (s_ctx.cursor)
                    {
                        case 0: /* Start Water */
                            Cleanup_Outputs();
                            s_ctx.exit_request = true;
                            s_ctx.exit_action  = APP_MENU_ACTION_START_WATER;
                            break;
                        case 1:
                            Go_Page(APP_MENU_PAGE_PARAMS);
                            break;
                        case 2:
                            Go_Page(APP_MENU_PAGE_MANUAL);
                            break;
                        case 3:
                            Go_Page(APP_MENU_PAGE_INFO);
                            break;
                        case 4:
                            Go_Page(APP_MENU_PAGE_FACTORY_RESET);
                            break;
                        case 5:
                            Cleanup_Outputs();
                            s_ctx.exit_request = true;
                            s_ctx.exit_action  = APP_MENU_ACTION_GOTO_SLEEP;
                            break;
                        default:
                            break;
                    }
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Cleanup_Outputs();
                    s_ctx.exit_request = true;
                    s_ctx.exit_action  = APP_MENU_ACTION_GOTO_SLEEP;
                    break;
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_PARAMS:
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    Cursor_Down();
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    Cursor_Up();
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    Enter_Edit_For_Cursor();
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Go_Page(APP_MENU_PAGE_MAIN);
                    break;
                case APP_EVENT_KEY_K3_LONG:
                {
                    /* 长按 K3 = 保存配置到 EEPROM */
                    Fm_ErrorCode err = App_Config_Save();
                    LOG_INFO_WITH_ARG("menu: cfg save err=0x%02X", (unsigned)err);
                    break;
                }
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_PARAM_EDIT:
        {
            const char *name = s_param_names[s_ctx.cursor];
            int32_t     step = Field_Step(name);
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    s_ctx.edit_value += step;
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    s_ctx.edit_value -= step;
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    Commit_Edit();
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Go_Page(APP_MENU_PAGE_PARAMS);
                    break;
                default:
                    break;
            }
            break;
        }

        case APP_MENU_PAGE_MANUAL:
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    if (s_ctx.cursor == 5U)
                    {
                        Manual_Adjust_Pump(+5);
                    }
                    else
                    {
                        Cursor_Down();
                    }
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    if (s_ctx.cursor == 5U)
                    {
                        Manual_Adjust_Pump(-5);
                    }
                    else
                    {
                        Cursor_Up();
                    }
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    if (s_ctx.cursor < 4U)
                    {
                        Manual_Toggle_Valve(s_ctx.cursor);
                    }
                    else if (s_ctx.cursor == 4U)
                    {
                        Manual_Toggle_PumpPwr();
                    }
                    else
                    {
                        Cleanup_Outputs();
                        Go_Page(APP_MENU_PAGE_MAIN);
                    }
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Cleanup_Outputs();
                    Go_Page(APP_MENU_PAGE_MAIN);
                    break;
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_INFO:
            if (e->id == APP_EVENT_KEY_K4_SHORT)
            {
                Go_Page(APP_MENU_PAGE_MAIN);
            }
            break;

        case APP_MENU_PAGE_FACTORY_RESET:
            if (e->id == APP_EVENT_KEY_K3_HOLD)
            {
                LOG_WARN("menu: factory reset confirmed");
                (void)App_Config_FactoryReset();
                Go_Page(APP_MENU_PAGE_MAIN);
            }
            else if (e->id == APP_EVENT_KEY_K4_SHORT)
            {
                Go_Page(APP_MENU_PAGE_MAIN);
            }
            break;

        default:
            break;
    }
}

/**
 * @brief   菜单模块初始化（上电调用一次）
 * @note    可在此实现页超时自动返回主菜单等
 */
void App_Menu_Tick(void)
{
    /* 预留：无操作超时返回主菜单 */
}
