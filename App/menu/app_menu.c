/*
 * @File         : \code\App\menu\app_menu.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-09-10 12:20:00
 * @Description  : HMI V2.0 菜单实现（主界面 / 设置 / 参数单项落盘 / 本地测试）
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#include "app_menu.h"
#include "app_config.h"
#include "app_display.h"
#include "app_log.h"
#include "bsp_valve.h"
#include "bsp_pump_pwm.h"
#include "bsp_key.h"
#include "bsp_oled.h"
#include "bsp_tick.h"
#include "floramate_types.h"

#include <string.h>

#define SCREEN_PATTERN_PERIOD_MS (800U)
#define SCREEN_PATTERN_COUNT     (6U)
#define KEY_TEST_IDLE_MS         (5000U)
#define PUMP_DUTY_STEP           (5)

static const char *s_main_items[]     = {"1.Auto Water", "2.Settings"};
static const char *s_settings_items[] = {"1.Params", "2.Local Test", "3.Serial Test"};
static const char *s_local_items[]    = {"1.Screen", "2.Key", "3.Water"};
static const char *s_water_items[]    = {"Z1", "Z2", "Z3", "Z4", "Z5", "PUMP %"};

/** 与 App_Config 短名一致 */
static const char *s_param_names[] = {
    "n_steps", "duty[0]", "duty[1]", "duty[2]", "duty[3]", "sec[0]",   "sec[1]",  "sec[2]",
    "sec[3]",  "ch_en",   "gap_ms",  "tmo_ch",   "tmo_all", "contrast", "long_ms", "hold_ms",
    "idle",    "selftest", "log"};

#define MAIN_ITEM_COUNT     ((uint8_t)(sizeof(s_main_items) / sizeof(s_main_items[0])))
#define SETTINGS_ITEM_COUNT ((uint8_t)(sizeof(s_settings_items) / sizeof(s_settings_items[0])))
#define LOCAL_ITEM_COUNT    ((uint8_t)(sizeof(s_local_items) / sizeof(s_local_items[0])))
#define WATER_ITEM_COUNT    ((uint8_t)(sizeof(s_water_items) / sizeof(s_water_items[0])))
#define PARAM_NAMES_COUNT   ((uint8_t)(sizeof(s_param_names) / sizeof(s_param_names[0])))

typedef struct
{
    App_Menu_Page   page;
    uint8_t         cursor;
    uint8_t         params_cursor;
    int32_t         edit_value;
    int32_t         edit_saved;
    bool            exit_request;
    App_Menu_Action exit_action;

    uint8_t  pump_duty;
    bool     pump_editing;
    uint8_t  ch_en_channel;
    uint8_t  screen_pattern;
    uint32_t screen_pattern_ms;
    uint8_t  key_mask;
    uint8_t  key_mask_prev;
    uint32_t key_last_edge_ms;
} App_Menu_Ctx;

static App_Menu_Ctx s_ctx;

/* --------------------------------- helpers --------------------------------- */

static void Cleanup_Outputs(void)
{
    Bsp_Pump_Pwm_Stop();
    Bsp_Valve_AllOff();
    s_ctx.pump_duty    = 0U;
    s_ctx.pump_editing = false;
}

static bool Is_Drive_Test_Page(App_Menu_Page p)
{
    return (p == APP_MENU_PAGE_WATER_TEST) || (p == APP_MENU_PAGE_SCREEN_TEST) ||
           (p == APP_MENU_PAGE_KEY_TEST);
}

static void Request_Exit(App_Menu_Action act)
{
    s_ctx.exit_request = true;
    s_ctx.exit_action  = act;
}

static uint8_t Page_Item_Count(App_Menu_Page page)
{
    switch (page)
    {
        case APP_MENU_PAGE_MAIN:
            return MAIN_ITEM_COUNT;
        case APP_MENU_PAGE_SETTINGS:
            return SETTINGS_ITEM_COUNT;
        case APP_MENU_PAGE_PARAMS:
            return PARAM_NAMES_COUNT;
        case APP_MENU_PAGE_LOCAL_TEST:
            return LOCAL_ITEM_COUNT;
        case APP_MENU_PAGE_WATER_TEST:
            return WATER_ITEM_COUNT;
        default:
            return 1U;
    }
}

static void Go_Page(App_Menu_Page p)
{
    App_Menu_Page prev = s_ctx.page;

    if (Is_Drive_Test_Page(prev) && (p != prev))
    {
        Cleanup_Outputs();
    }

    s_ctx.page         = p;
    s_ctx.cursor       = 0U;
    s_ctx.pump_editing = false;

    if (p == APP_MENU_PAGE_WATER_TEST)
    {
        Cleanup_Outputs();
    }
    else if (p == APP_MENU_PAGE_SCREEN_TEST)
    {
        Cleanup_Outputs();
        s_ctx.screen_pattern    = 0U;
        s_ctx.screen_pattern_ms = Bsp_Tick_GetMs();
    }
    else if (p == APP_MENU_PAGE_KEY_TEST)
    {
        uint8_t mask = 0U;
        Cleanup_Outputs();
        for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++)
        {
            if (Bsp_Key_IsPressed((Bsp_Key_Id)i))
            {
                mask |= (uint8_t)(1U << i);
            }
        }
        s_ctx.key_mask         = mask;
        s_ctx.key_mask_prev    = mask;
        s_ctx.key_last_edge_ms = Bsp_Tick_GetMs();
    }

    App_Display_MarkDirty();
    App_Display_FlushNow();
}

static void Cursor_Up(void)
{
    uint8_t n = Page_Item_Count(s_ctx.page);
    if (n == 0U)
    {
        return;
    }
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + n - 1U) % n);
    App_Display_MarkDirty();
}

static void Cursor_Down(void)
{
    uint8_t n = Page_Item_Count(s_ctx.page);
    if (n == 0U)
    {
        return;
    }
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + 1U) % n);
    App_Display_MarkDirty();
}

static bool Field_Is_ChEn(const char *name)
{
    return (strcmp(name, "ch_en") == 0);
}

static bool Field_Is_Contrast(const char *name)
{
    return (strcmp(name, "contrast") == 0);
}

static int32_t Field_Step(const char *name)
{
    if (strncmp(name, "duty", 4) == 0)
    {
        return 5;
    }
    if (strncmp(name, "sec", 3) == 0)
    {
        return 1;
    }
    if (strcmp(name, "tmo_ch") == 0)
    {
        return 5;
    }
    if (strcmp(name, "tmo_all") == 0)
    {
        return 30;
    }
    if (strcmp(name, "selftest") == 0)
    {
        return 10;
    }
    if (strcmp(name, "gap_ms") == 0)
    {
        return 10;
    }
    if (strcmp(name, "long_ms") == 0)
    {
        return 10;
    }
    if (strcmp(name, "hold_ms") == 0)
    {
        return 50;
    }
    if (strcmp(name, "contrast") == 0)
    {
        return 8;
    }
    return 1;
}

static void Apply_Contrast_Live(void)
{
    if (s_ctx.edit_value < 0)
    {
        s_ctx.edit_value = 0;
    }
    if (s_ctx.edit_value > 255)
    {
        s_ctx.edit_value = 255;
    }
    Bsp_Oled_SetContrast((uint8_t)s_ctx.edit_value);
}

static void Enter_Edit_For_Cursor(void)
{
    const char *name = s_param_names[s_ctx.cursor];
    int32_t     v    = 0;

    (void)App_Config_GetField(name, &v);
    s_ctx.params_cursor = s_ctx.cursor;
    s_ctx.edit_value    = v;
    s_ctx.edit_saved    = v;
    s_ctx.ch_en_channel = 0U;
    s_ctx.page          = APP_MENU_PAGE_PARAM_EDIT;
    s_ctx.cursor        = s_ctx.params_cursor;
    App_Display_MarkDirty();
    App_Display_FlushNow();
}

static void Commit_Edit(void)
{
    const char  *name = s_param_names[s_ctx.params_cursor];
    Fm_ErrorCode err  = App_Config_SetField(name, s_ctx.edit_value);

    if (err == FM_OK)
    {
        err = App_Config_Save();
        LOG_INFO_WITH_ARG("menu: save %s=%ld err=0x%02X", name, (long)s_ctx.edit_value,
                          (unsigned)err);
        if (Field_Is_Contrast(name))
        {
            Bsp_Oled_SetContrast((uint8_t)s_ctx.edit_value);
        }
    }
    else
    {
        LOG_ERROR_WITH_ARG("menu: set %s fail", name);
    }

    s_ctx.page   = APP_MENU_PAGE_PARAMS;
    s_ctx.cursor = s_ctx.params_cursor;
    App_Display_MarkDirty();
    App_Display_FlushNow();
}

static void Abort_Edit(void)
{
    const char *name = s_param_names[s_ctx.params_cursor];

    if (Field_Is_Contrast(name))
    {
        Bsp_Oled_SetContrast((uint8_t)s_ctx.edit_saved);
    }

    s_ctx.page   = APP_MENU_PAGE_PARAMS;
    s_ctx.cursor = s_ctx.params_cursor;
    App_Display_MarkDirty();
    App_Display_FlushNow();
}

static void Water_Toggle_Valve(uint8_t z)
{
    if (z >= (uint8_t)BSP_VALVE_CHANNEL_NUM)
    {
        return;
    }
    {
        Bsp_Valve_Channel ch  = (Bsp_Valve_Channel)((uint32_t)BSP_VALVE_Z1 + (uint32_t)z);
        bool              now = Bsp_Valve_Get(ch);
        Bsp_Valve_DebugSet(ch, !now);
        LOG_INFO_WITH_ARG("water: Z%u -> %s", (unsigned)(z + 1U), now ? "OFF" : "ON");
    }
    App_Display_MarkDirty();
}

static void Water_Adjust_Pump(int8_t delta)
{
    int16_t v = (int16_t)s_ctx.pump_duty + (int16_t)delta;
    if (v < 0)
    {
        v = 0;
    }
    if (v > (int16_t)FM_PUMP_DUTY_MAX_PERCENT)
    {
        v = (int16_t)FM_PUMP_DUTY_MAX_PERCENT;
    }
    s_ctx.pump_duty = (uint8_t)v;
    (void)Bsp_Pump_Pwm_SetDutyPercent(s_ctx.pump_duty);
    App_Display_MarkDirty();
}

static uint8_t Sample_Key_Mask(void)
{
    uint8_t mask = 0U;
    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++)
    {
        if (Bsp_Key_IsPressed((Bsp_Key_Id)i))
        {
            mask |= (uint8_t)(1U << i);
        }
    }
    return mask;
}

/* --------------------------------- public API --------------------------------- */

void App_Menu_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.page = APP_MENU_PAGE_MAIN;
}

void App_Menu_Enter(void)
{
    Cleanup_Outputs();
    s_ctx.page         = APP_MENU_PAGE_MAIN;
    s_ctx.cursor       = 0U;
    s_ctx.exit_request = false;
    s_ctx.exit_action  = APP_MENU_ACTION_NONE;
    s_ctx.pump_duty    = 0U;
    s_ctx.pump_editing = false;
    LOG_INFO("menu: enter MAIN");
    App_Display_MarkDirty();
}

void App_Menu_EnterSettings(void)
{
    Cleanup_Outputs();
    s_ctx.page         = APP_MENU_PAGE_SETTINGS;
    s_ctx.cursor       = 0U;
    s_ctx.exit_request = false;
    s_ctx.exit_action  = APP_MENU_ACTION_NONE;
    s_ctx.pump_duty    = 0U;
    s_ctx.pump_editing = false;
    LOG_INFO("menu: enter SETTINGS");
    App_Display_MarkDirty();
}

bool App_Menu_ExitRequested(void)
{
    return s_ctx.exit_request;
}

App_Menu_Action App_Menu_GetExitAction(void)
{
    return s_ctx.exit_action;
}

void App_Menu_ClearExit(void)
{
    s_ctx.exit_request = false;
    s_ctx.exit_action  = APP_MENU_ACTION_NONE;
}

App_Menu_Page App_Menu_GetPage(void)
{
    return s_ctx.page;
}

uint8_t App_Menu_GetCursor(void)
{
    return s_ctx.cursor;
}

uint8_t App_Menu_GetItemCount(void)
{
    return Page_Item_Count(s_ctx.page);
}

const char *App_Menu_GetItemLabel(uint8_t idx)
{
    switch (s_ctx.page)
    {
        case APP_MENU_PAGE_MAIN:
            return (idx < MAIN_ITEM_COUNT) ? s_main_items[idx] : "";
        case APP_MENU_PAGE_SETTINGS:
            return (idx < SETTINGS_ITEM_COUNT) ? s_settings_items[idx] : "";
        case APP_MENU_PAGE_PARAMS:
            return (idx < PARAM_NAMES_COUNT) ? s_param_names[idx] : "";
        case APP_MENU_PAGE_LOCAL_TEST:
            return (idx < LOCAL_ITEM_COUNT) ? s_local_items[idx] : "";
        case APP_MENU_PAGE_WATER_TEST:
            return (idx < WATER_ITEM_COUNT) ? s_water_items[idx] : "";
        default:
            return "";
    }
}

const char *App_Menu_GetEditFieldName(void)
{
    if (s_ctx.params_cursor < PARAM_NAMES_COUNT)
    {
        return s_param_names[s_ctx.params_cursor];
    }
    return "";
}

int32_t App_Menu_GetEditValue(void)
{
    return s_ctx.edit_value;
}

bool App_Menu_IsParamEditing(void)
{
    return (s_ctx.page == APP_MENU_PAGE_PARAM_EDIT);
}

uint8_t App_Menu_GetChEnEditChannel(void)
{
    return s_ctx.ch_en_channel;
}

bool App_Menu_IsPumpEditing(void)
{
    return s_ctx.pump_editing;
}

uint8_t App_Menu_GetPumpPercent(void)
{
    return s_ctx.pump_duty;
}

uint8_t App_Menu_GetScreenPattern(void)
{
    return s_ctx.screen_pattern;
}

uint8_t App_Menu_GetKeyTestMask(void)
{
    return s_ctx.key_mask;
}

void App_Menu_OnEvent(const App_Event *e)
{
    if (e == NULL)
    {
        return;
    }

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
                    if (s_ctx.cursor == 0U)
                    {
                        Cleanup_Outputs();
                        Request_Exit(APP_MENU_ACTION_START_WATER);
                    }
                    else if (s_ctx.cursor == 1U)
                    {
                        Go_Page(APP_MENU_PAGE_SETTINGS);
                    }
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    break;
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_SETTINGS:
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    Cursor_Down();
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    Cursor_Up();
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    if (s_ctx.cursor == 0U)
                    {
                        Go_Page(APP_MENU_PAGE_PARAMS);
                    }
                    else if (s_ctx.cursor == 1U)
                    {
                        Go_Page(APP_MENU_PAGE_LOCAL_TEST);
                    }
                    else if (s_ctx.cursor == 2U)
                    {
                        Cleanup_Outputs();
                        Request_Exit(APP_MENU_ACTION_ENTER_SERIAL);
                    }
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Go_Page(APP_MENU_PAGE_MAIN);
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
                    Go_Page(APP_MENU_PAGE_SETTINGS);
                    break;
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_PARAM_EDIT:
        {
            const char *name = s_param_names[s_ctx.params_cursor];

            if (Field_Is_ChEn(name))
            {
                switch (e->id)
                {
                    case APP_EVENT_KEY_K1_SHORT:
                        s_ctx.ch_en_channel =
                            (uint8_t)((s_ctx.ch_en_channel + 1U) % FM_CHANNEL_NUM);
                        App_Display_MarkDirty();
                        break;
                    case APP_EVENT_KEY_K2_SHORT:
                        s_ctx.ch_en_channel =
                            (uint8_t)((s_ctx.ch_en_channel + FM_CHANNEL_NUM - 1U) % FM_CHANNEL_NUM);
                        App_Display_MarkDirty();
                        break;
                    case APP_EVENT_KEY_K3_SHORT:
                        s_ctx.edit_value ^= (int32_t)(1UL << s_ctx.ch_en_channel);
                        App_Display_MarkDirty();
                        break;
                    case APP_EVENT_KEY_K3_LONG:
                    case APP_EVENT_KEY_K3_HOLD:
                        Commit_Edit();
                        break;
                    case APP_EVENT_KEY_K4_SHORT:
                        Abort_Edit();
                        break;
                    default:
                        break;
                }
            }
            else
            {
                int32_t step = Field_Step(name);
                switch (e->id)
                {
                    case APP_EVENT_KEY_K1_SHORT:
                        s_ctx.edit_value += step;
                        if (Field_Is_Contrast(name))
                        {
                            Apply_Contrast_Live();
                        }
                        App_Display_MarkDirty();
                        break;
                    case APP_EVENT_KEY_K2_SHORT:
                        s_ctx.edit_value -= step;
                        if (Field_Is_Contrast(name))
                        {
                            Apply_Contrast_Live();
                        }
                        App_Display_MarkDirty();
                        break;
                    case APP_EVENT_KEY_K3_SHORT:
                        Commit_Edit();
                        break;
                    case APP_EVENT_KEY_K4_SHORT:
                        Abort_Edit();
                        break;
                    default:
                        break;
                }
            }
            break;
        }

        case APP_MENU_PAGE_LOCAL_TEST:
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    Cursor_Down();
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    Cursor_Up();
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    if (s_ctx.cursor == 0U)
                    {
                        Go_Page(APP_MENU_PAGE_SCREEN_TEST);
                    }
                    else if (s_ctx.cursor == 1U)
                    {
                        Go_Page(APP_MENU_PAGE_KEY_TEST);
                    }
                    else if (s_ctx.cursor == 2U)
                    {
                        Go_Page(APP_MENU_PAGE_WATER_TEST);
                    }
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Go_Page(APP_MENU_PAGE_SETTINGS);
                    break;
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_SCREEN_TEST:
            switch (e->id)
            {
                case APP_EVENT_KEY_K3_SHORT:
                    s_ctx.screen_pattern =
                        (uint8_t)((s_ctx.screen_pattern + 1U) % SCREEN_PATTERN_COUNT);
                    s_ctx.screen_pattern_ms = Bsp_Tick_GetMs();
                    App_Display_MarkDirty();
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    Go_Page(APP_MENU_PAGE_LOCAL_TEST);
                    break;
                default:
                    break;
            }
            break;

        case APP_MENU_PAGE_KEY_TEST:
            break;

        case APP_MENU_PAGE_WATER_TEST:
            switch (e->id)
            {
                case APP_EVENT_KEY_K1_SHORT:
                    if (s_ctx.pump_editing)
                    {
                        Water_Adjust_Pump(+PUMP_DUTY_STEP);
                    }
                    else
                    {
                        Cursor_Down();
                    }
                    break;
                case APP_EVENT_KEY_K2_SHORT:
                    if (s_ctx.pump_editing)
                    {
                        Water_Adjust_Pump(-PUMP_DUTY_STEP);
                    }
                    else
                    {
                        Cursor_Up();
                    }
                    break;
                case APP_EVENT_KEY_K3_SHORT:
                    if (s_ctx.cursor < 5U)
                    {
                        Water_Toggle_Valve(s_ctx.cursor);
                    }
                    else
                    {
                        s_ctx.pump_editing = !s_ctx.pump_editing;
                        App_Display_MarkDirty();
                    }
                    break;
                case APP_EVENT_KEY_K4_SHORT:
                    if (s_ctx.pump_editing)
                    {
                        s_ctx.pump_editing = false;
                        s_ctx.pump_duty    = 0U;
                        Bsp_Pump_Pwm_Stop();
                        App_Display_MarkDirty();
                    }
                    else
                    {
                        Cleanup_Outputs();
                        Go_Page(APP_MENU_PAGE_LOCAL_TEST);
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

void App_Menu_Tick(void)
{
    if (s_ctx.page == APP_MENU_PAGE_SCREEN_TEST)
    {
        if (Bsp_Tick_ElapsedMs(s_ctx.screen_pattern_ms) >= SCREEN_PATTERN_PERIOD_MS)
        {
            s_ctx.screen_pattern =
                (uint8_t)((s_ctx.screen_pattern + 1U) % SCREEN_PATTERN_COUNT);
            s_ctx.screen_pattern_ms = Bsp_Tick_GetMs();
            App_Display_MarkDirty();
        }
    }
    else if (s_ctx.page == APP_MENU_PAGE_KEY_TEST)
    {
        uint8_t mask = Sample_Key_Mask();
        if (mask != s_ctx.key_mask_prev)
        {
            s_ctx.key_mask_prev    = mask;
            s_ctx.key_mask         = mask;
            s_ctx.key_last_edge_ms = Bsp_Tick_GetMs();
            App_Display_MarkDirty();
        }
        else
        {
            s_ctx.key_mask = mask;
        }

        if (Bsp_Tick_ElapsedMs(s_ctx.key_last_edge_ms) >= KEY_TEST_IDLE_MS)
        {
            Go_Page(APP_MENU_PAGE_LOCAL_TEST);
        }
    }
}
