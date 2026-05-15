/**
 * @file    app_menu.c
 * @brief   调试菜单实现
 * @note    简化版菜单：主菜单 → 参数编辑/手动测试/系统信息/出厂/退出。
 *          参数编辑直接复用 App_Config_SetField 短名表，省得维护两套。
 */
#include "app_menu.h"
#include "app_config.h"
#include "app_log.h"
#include "bsp_relay.h"
#include "bsp_pump_pwm.h"
#include "bsp_key.h"
#include "bsp_tick.h"

#include <string.h>

#define MAIN_MENU_ITEM_COUNT     (6U)

static const char *s_main_items[MAIN_MENU_ITEM_COUNT] = {
    "1.Start Water",
    "2.Params",
    "3.Manual Test",
    "4.System Info",
    "5.Factory Reset",
    "6.Exit (sleep)"
};

/* 参数编辑短名表（与 App_Config 短名一致） */
static const char *s_param_names[] = {
    "n_steps", "ch_en", "idle", "gap_ms",
    "tmo_ch", "tmo_all", "selftest",
    "long_ms", "hold_ms", "contrast", "log",
    "duty[0]", "duty[1]", "duty[2]", "duty[3]",
    "sec[0]", "sec[1]", "sec[2]", "sec[3]"
};
#define PARAM_NAMES_COUNT (sizeof(s_param_names) / sizeof(s_param_names[0]))

/* 手动测试条目 */
static const char *s_manual_items[] = {
    "Z1 ON/OFF",
    "Z2 ON/OFF",
    "Z3 ON/OFF",
    "Z4 ON/OFF",
    "CH5 ON/OFF",
    "Pump duty +/-",
    "Back"
};
#define MANUAL_ITEMS_COUNT (sizeof(s_manual_items) / sizeof(s_manual_items[0]))

typedef struct {
    App_Menu_Page   page;
    uint8_t         cursor;
    int32_t         edit_value;
    bool            exit_request;
    App_Menu_Action exit_action;
    uint8_t         manual_pump_duty;
    uint32_t        page_enter_ms;
} App_Menu_Ctx;

static App_Menu_Ctx s_ctx;

void App_Menu_Init(void) {
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.page = APP_MENU_PAGE_MAIN;
}

void App_Menu_Enter(void) {
    s_ctx.page          = APP_MENU_PAGE_MAIN;
    s_ctx.cursor        = 0U;
    s_ctx.exit_request  = false;
    s_ctx.exit_action   = APP_MENU_ACTION_NONE;
    s_ctx.manual_pump_duty = 0U;
    s_ctx.page_enter_ms = Bsp_Tick_GetMs();
    LOGI("menu: enter");
}

bool            App_Menu_ExitRequested(void)  { return s_ctx.exit_request; }
App_Menu_Action App_Menu_GetExitAction(void)  { return s_ctx.exit_action; }
void            App_Menu_ClearExit(void)      { s_ctx.exit_request = false;
                                                s_ctx.exit_action  = APP_MENU_ACTION_NONE; }

App_Menu_Page   App_Menu_GetPage(void)        { return s_ctx.page; }
uint8_t         App_Menu_GetCursor(void)      { return s_ctx.cursor; }

uint8_t App_Menu_GetItemCount(void) {
    switch (s_ctx.page) {
    case APP_MENU_PAGE_MAIN:   return MAIN_MENU_ITEM_COUNT;
    case APP_MENU_PAGE_PARAMS: return (uint8_t)PARAM_NAMES_COUNT;
    case APP_MENU_PAGE_MANUAL: return (uint8_t)MANUAL_ITEMS_COUNT;
    case APP_MENU_PAGE_PARAM_EDIT:
    case APP_MENU_PAGE_INFO:
    case APP_MENU_PAGE_FACTORY_RESET:
    default:
        return 1U;
    }
}

const char * App_Menu_GetItemLabel(uint8_t idx) {
    switch (s_ctx.page) {
    case APP_MENU_PAGE_MAIN:
        return (idx < MAIN_MENU_ITEM_COUNT) ? s_main_items[idx] : "";
    case APP_MENU_PAGE_PARAMS:
        return (idx < PARAM_NAMES_COUNT) ? s_param_names[idx] : "";
    case APP_MENU_PAGE_MANUAL:
        return (idx < MANUAL_ITEMS_COUNT) ? s_manual_items[idx] : "";
    default: return "";
    }
}

const char * App_Menu_GetEditFieldName(void) {
    if (s_ctx.cursor < PARAM_NAMES_COUNT) {
        return s_param_names[s_ctx.cursor];
    }
    return "";
}
int32_t App_Menu_GetEditValue(void) { return s_ctx.edit_value; }

/* ---- 内部辅助 ---- */
static void Cursor_Up(void) {
    uint8_t n = App_Menu_GetItemCount();
    if (n == 0U) return;
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + n - 1U) % n);
}
static void Cursor_Down(void) {
    uint8_t n = App_Menu_GetItemCount();
    if (n == 0U) return;
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + 1U) % n);
}
static void Go_Page(App_Menu_Page p) {
    s_ctx.page          = p;
    s_ctx.cursor        = 0U;
    s_ctx.page_enter_ms = Bsp_Tick_GetMs();
}

static void Enter_Edit_For_Cursor(void) {
    const char *name = s_param_names[s_ctx.cursor];
    int32_t v = 0;
    (void)App_Config_GetField(name, &v);
    s_ctx.edit_value = v;
    Go_Page(APP_MENU_PAGE_PARAM_EDIT);
}

/* 编辑步进：根据字段确定 +/- 步长 */
static int32_t Field_Step(const char *name) {
    if (strncmp(name, "duty", 4) == 0) return 5;
    if (strncmp(name, "sec",  3) == 0) return 1;
    if (strcmp (name, "tmo_ch")  == 0) return 5;
    if (strcmp (name, "tmo_all") == 0) return 30;
    if (strcmp (name, "selftest")== 0) return 10;
    if (strcmp (name, "gap_ms")  == 0) return 10;
    if (strcmp (name, "long_ms") == 0) return 10;
    if (strcmp (name, "hold_ms") == 0) return 50;
    if (strcmp (name, "contrast")== 0) return 16;
    return 1;
}

static void Commit_Edit(void) {
    const char *name = s_param_names[s_ctx.cursor];
    Fm_ErrorCode err = App_Config_SetField(name, s_ctx.edit_value);
    if (err == FM_OK) {
        LOGI("menu: %s = %ld (RAM only; save in main menu)",
             name, (long)s_ctx.edit_value);
    } else {
        LOGE("menu: set %s fail", name);
    }
    Go_Page(APP_MENU_PAGE_PARAMS);
}

/* ---- 手动测试动作 ---- */
static void Manual_Toggle_Valve(uint8_t z) {
    if (z >= FM_CHANNEL_NUM) return;
    Bsp_Relay_Channel ch = (Bsp_Relay_Channel)z;
    bool now = Bsp_Relay_Get(ch);
    Fm_ErrorCode err = Bsp_Relay_Set(ch, !now);
    LOGI("manual: Z%u -> %s (err=0x%02X)", (unsigned)(z+1U),
         now ? "OFF" : "ON", (unsigned)err);
}
static void Manual_Toggle_Main(void) {
    bool now = Bsp_Relay_Get(BSP_RELAY_MAIN_CH5);
    Fm_ErrorCode err = Bsp_Relay_Set(BSP_RELAY_MAIN_CH5, !now);
    LOGI("manual: CH5 -> %s (err=0x%02X)", now ? "OFF" : "ON", (unsigned)err);
}
static void Manual_Adjust_Pump(int8_t delta) {
    int16_t v = (int16_t)s_ctx.manual_pump_duty + delta;
    if (v < 0) v = 0;
    if (v > FM_PUMP_DUTY_MAX_PERCENT) v = FM_PUMP_DUTY_MAX_PERCENT;
    s_ctx.manual_pump_duty = (uint8_t)v;
    Fm_ErrorCode err = Bsp_Pump_Pwm_SetDutyPercent(s_ctx.manual_pump_duty);
    LOGI("manual: pump duty=%u%% (err=0x%02X)",
         (unsigned)s_ctx.manual_pump_duty, (unsigned)err);
}

/* 退出菜单时把所有继电器与 PWM 关掉 */
static void Cleanup_Outputs(void) {
    Bsp_Pump_Pwm_Stop();
    Bsp_Relay_AllOff();
    s_ctx.manual_pump_duty = 0U;
}

/* ---- 事件分发 ---- */
void App_Menu_OnEvent(const App_Event *e) {
    if (e == 0) return;

    switch (s_ctx.page) {

    case APP_MENU_PAGE_MAIN:
        switch (e->id) {
        case APP_EVENT_KEY_K1_SHORT: Cursor_Up();   break;
        case APP_EVENT_KEY_K2_SHORT: Cursor_Down(); break;
        case APP_EVENT_KEY_K3_LONG:
            /* 退出菜单 = SLEEP */
            Cleanup_Outputs();
            s_ctx.exit_request = true;
            s_ctx.exit_action  = APP_MENU_ACTION_GOTO_SLEEP;
            break;
        case APP_EVENT_KEY_K4_SHORT:
            /* 进入子页 */
            switch (s_ctx.cursor) {
            case 0:  /* Start Water */
                Cleanup_Outputs();
                s_ctx.exit_request = true;
                s_ctx.exit_action  = APP_MENU_ACTION_START_WATER;
                break;
            case 1:  Go_Page(APP_MENU_PAGE_PARAMS);         break;
            case 2:  Go_Page(APP_MENU_PAGE_MANUAL);         break;
            case 3:  Go_Page(APP_MENU_PAGE_INFO);           break;
            case 4:  Go_Page(APP_MENU_PAGE_FACTORY_RESET);  break;
            case 5:
                Cleanup_Outputs();
                s_ctx.exit_request = true;
                s_ctx.exit_action  = APP_MENU_ACTION_GOTO_SLEEP;
                break;
            default: break;
            }
            break;
        default: break;
        }
        break;

    case APP_MENU_PAGE_PARAMS:
        switch (e->id) {
        case APP_EVENT_KEY_K1_SHORT: Cursor_Up();   break;
        case APP_EVENT_KEY_K2_SHORT: Cursor_Down(); break;
        case APP_EVENT_KEY_K3_SHORT: Go_Page(APP_MENU_PAGE_MAIN); break;
        case APP_EVENT_KEY_K4_SHORT: Enter_Edit_For_Cursor(); break;
        case APP_EVENT_KEY_K3_LONG: {
            /* 长按 K3 = 保存配置 */
            Fm_ErrorCode err = App_Config_Save();
            LOGI("menu: cfg save err=0x%02X", (unsigned)err);
            break;
        }
        default: break;
        }
        break;

    case APP_MENU_PAGE_PARAM_EDIT: {
        const char *name = s_param_names[s_ctx.cursor];
        int32_t step = Field_Step(name);
        switch (e->id) {
        case APP_EVENT_KEY_K1_SHORT: s_ctx.edit_value += step; break;
        case APP_EVENT_KEY_K2_SHORT: s_ctx.edit_value -= step; break;
        case APP_EVENT_KEY_K3_SHORT: Go_Page(APP_MENU_PAGE_PARAMS); break;  /* 取消 */
        case APP_EVENT_KEY_K4_SHORT: Commit_Edit(); break;
        default: break;
        }
        break;
    }

    case APP_MENU_PAGE_MANUAL:
        switch (e->id) {
        case APP_EVENT_KEY_K1_SHORT: Cursor_Up();   break;
        case APP_EVENT_KEY_K2_SHORT: Cursor_Down(); break;
        case APP_EVENT_KEY_K3_SHORT:
            Cleanup_Outputs();
            Go_Page(APP_MENU_PAGE_MAIN);
            break;
        case APP_EVENT_KEY_K4_SHORT:
            if (s_ctx.cursor < 4U) {
                Manual_Toggle_Valve(s_ctx.cursor);
            } else if (s_ctx.cursor == 4U) {
                Manual_Toggle_Main();
            } else if (s_ctx.cursor == 5U) {
                Manual_Adjust_Pump(+10);   /* 短按 K4 = +10% */
            } else {
                Cleanup_Outputs();
                Go_Page(APP_MENU_PAGE_MAIN);
            }
            break;
        case APP_EVENT_KEY_K1_LONG:
            if (s_ctx.cursor == 5U) Manual_Adjust_Pump(+5);
            break;
        case APP_EVENT_KEY_K2_LONG:
            if (s_ctx.cursor == 5U) Manual_Adjust_Pump(-5);
            break;
        default: break;
        }
        break;

    case APP_MENU_PAGE_INFO:
        if ((e->id == APP_EVENT_KEY_K3_SHORT) || (e->id == APP_EVENT_KEY_K4_SHORT)) {
            Go_Page(APP_MENU_PAGE_MAIN);
        }
        break;

    case APP_MENU_PAGE_FACTORY_RESET:
        /* 长按 K4 3s 确认 */
        if (e->id == APP_EVENT_KEY_K4_HOLD) {
            LOGW("menu: factory reset confirmed");
            (void)App_Config_FactoryReset();
            Go_Page(APP_MENU_PAGE_MAIN);
        } else if (e->id == APP_EVENT_KEY_K3_SHORT) {
            Go_Page(APP_MENU_PAGE_MAIN);
        }
        break;

    default: break;
    }
}

void App_Menu_Tick(void) {
    /* 目前无周期任务；预留页超时自动返回主菜单等 */
}
