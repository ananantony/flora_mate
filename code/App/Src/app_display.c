/**
 * @file    app_display.c
 * @brief   OLED 页面绘制实现
 * @note    100 ms 限速，全屏刷新；局部刷新留作 Phase 2 优化。
 */
#include "app_display.h"
#include "app_main_fsm.h"
#include "app_menu.h"
#include "app_pump_fsm.h"
#include "app_config.h"
#include "bsp_oled.h"
#include "bsp_tick.h"

#include <stdio.h>
#include <string.h>

#define DISPLAY_PERIOD_MS    (100U)

static Bsp_Tick_Ticker s_ticker;
static bool            s_dirty = true;
static char            s_line[24];

void App_Display_MarkDirty(void) { s_dirty = true; }

void App_Display_Init(void) {
    Bsp_Tick_Ticker_Init(&s_ticker, DISPLAY_PERIOD_MS);
    s_dirty = true;
}

/* ---- 通用绘制 ---- */
static void Draw_TitleBar(const char *title) {
    Bsp_Oled_FbFillRect(0U, 0U, 128U, 10U, false);
    Bsp_Oled_FbDrawStr6x8(2U, 0U, title, false);
    Bsp_Oled_FbDrawHLine(0U, 9U, 128U, true);
}

static void Draw_ProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent) {
    if (percent > 100U) percent = 100U;
    Bsp_Oled_FbDrawRect(x, y, w, h, true);
    uint8_t fill_w = (uint8_t)((uint32_t)(w - 2U) * percent / 100U);
    Bsp_Oled_FbFillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U), fill_w, (uint8_t)(h - 2U), true);
}

/* ---- 各页绘制 ---- */
static void Draw_Boot(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("FloraMate");
    Bsp_Oled_FbDrawStr6x8(2U, 2U, "Booting...", false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "V1.0", false);
}

static void Draw_Selftest(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Selftest");
    Bsp_Oled_FbDrawStr6x8(2U, 2U, "Relays click test", false);
    Draw_ProgressBar(2U, 32U, 124U, 8U, 60U);
}

static void Draw_Idle3s(void) {
    const App_Config *cfg = App_Config_Get();
    uint32_t elapsed = Bsp_Tick_ElapsedMs(App_Main_Fsm_EnterMs());
    uint32_t total   = (uint32_t)cfg->idle_seconds * 1000U;
    uint32_t remain  = (elapsed >= total) ? 0U : (total - elapsed);
    uint8_t  remain_s = (uint8_t)((remain + 999U) / 1000U);
    if (remain_s == 0U) remain_s = 1U;

    Bsp_Oled_FbClear();
    Draw_TitleBar("Idle");

    /* 大数字倒计时 */
    char d = (char)('0' + (remain_s % 10U));
    Bsp_Oled_FbDrawBigDigit(56U, 2U, d);

    Bsp_Oled_FbDrawStr6x8(2U, 6U, "Press any key:menu", false);
    Bsp_Oled_FbDrawStr6x8(2U, 7U, "Hold K1: start now", false);
}

static void Draw_AutoRun(void) {
    const App_Config *cfg = App_Config_Get();
    uint8_t zone = App_Main_Fsm_CurrentZone();   /* 1..4 */
    Bsp_Oled_FbClear();

    snprintf(s_line, sizeof(s_line), "AUTO Z%u/%u", zone, FM_CHANNEL_NUM);
    Draw_TitleBar(s_line);

    /* 当前阶梯条 */
    /* 简化：从 App_Pump_Fsm 暴露的 ctx 无法跨模块访问，
     * 这里基于 Config + 总 step 占空比 + 已用时间近似显示。
     * 真实实现可以在 App_Main_Fsm 中导出更详细的状态。 */
    snprintf(s_line, sizeof(s_line), "Steps: %u", cfg->step_count);
    Bsp_Oled_FbDrawStr6x8(2U, 2U, s_line, false);

    uint32_t auto_elapsed_s = Bsp_Tick_ElapsedMs(App_Main_Fsm_AutoRunStartMs()) / 1000U;
    snprintf(s_line, sizeof(s_line), "Elapsed: %lus", (unsigned long)auto_elapsed_s);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, s_line, false);

    /* 总进度条：相对 total_timeout */
    uint8_t percent = (cfg->total_timeout_s == 0U) ? 0U :
                      (uint8_t)((auto_elapsed_s * 100U) / cfg->total_timeout_s);
    if (percent > 100U) percent = 100U;
    Draw_ProgressBar(2U, 56U, 124U, 8U, percent);

    Bsp_Oled_FbDrawStr6x8(2U, 6U, "K3:STOP K4:menu", false);
}

static void Draw_MenuMain(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Menu");
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    /* 最多显示 6 行 */
    uint8_t start  = (cursor < 6U) ? 0U : (uint8_t)(cursor - 5U);
    for (uint8_t i = 0U; (i < 6U) && ((start + i) < n); i++) {
        bool sel = ((start + i) == cursor);
        uint8_t page = (uint8_t)(2U + i);
        const char *label = App_Menu_GetItemLabel((uint8_t)(start + i));
        if (sel) {
            Bsp_Oled_FbFillRect(0U, (uint8_t)(page * 8U), 128U, 8U, true);
        }
        Bsp_Oled_FbDrawChar6x8(0U, page, sel ? '>' : ' ', sel);
        Bsp_Oled_FbDrawStr6x8(8U, page, label, sel);
    }
}

static void Draw_MenuParams(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Params");
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    uint8_t start  = (cursor < 6U) ? 0U : (uint8_t)(cursor - 5U);
    for (uint8_t i = 0U; (i < 6U) && ((start + i) < n); i++) {
        bool sel = ((start + i) == cursor);
        uint8_t page = (uint8_t)(2U + i);
        const char *name = App_Menu_GetItemLabel((uint8_t)(start + i));
        int32_t v = 0;
        (void)App_Config_GetField(name, &v);
        snprintf(s_line, sizeof(s_line), "%-9s %ld", name, (long)v);
        if (sel) {
            Bsp_Oled_FbFillRect(0U, (uint8_t)(page * 8U), 128U, 8U, true);
        }
        Bsp_Oled_FbDrawChar6x8(0U, page, sel ? '>' : ' ', sel);
        Bsp_Oled_FbDrawStr6x8(8U, page, s_line, sel);
    }
    Bsp_Oled_FbDrawStr6x8(2U, 7U, "K4:edit K3hold:save", false);
}

static void Draw_MenuParamEdit(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Edit Param");
    snprintf(s_line, sizeof(s_line), "Field: %s", App_Menu_GetEditFieldName());
    Bsp_Oled_FbDrawStr6x8(2U, 2U, s_line, false);
    snprintf(s_line, sizeof(s_line), "Value: %ld", (long)App_Menu_GetEditValue());
    Bsp_Oled_FbDrawStr6x8(2U, 4U, s_line, false);
    Bsp_Oled_FbDrawStr6x8(2U, 6U, "K1:+ K2:- K4:OK", false);
    Bsp_Oled_FbDrawStr6x8(2U, 7U, "K3:cancel", false);
}

static void Draw_MenuManual(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Manual Test");
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    for (uint8_t i = 0U; (i < 6U) && (i < n); i++) {
        bool sel = (i == cursor);
        uint8_t page = (uint8_t)(2U + i);
        if (sel) {
            Bsp_Oled_FbFillRect(0U, (uint8_t)(page * 8U), 128U, 8U, true);
        }
        Bsp_Oled_FbDrawChar6x8(0U, page, sel ? '>' : ' ', sel);
        Bsp_Oled_FbDrawStr6x8(8U, page, App_Menu_GetItemLabel(i), sel);
    }
}

static void Draw_MenuInfo(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("System Info");
    Bsp_Oled_FbDrawStr6x8(2U, 2U, FM_FIRMWARE_VERSION_STR, false);
    snprintf(s_line, sizeof(s_line), "Uptime: %lus",
             (unsigned long)(Bsp_Tick_GetMs() / 1000U));
    Bsp_Oled_FbDrawStr6x8(2U, 3U, s_line, false);

    App_Config_Source src = App_Config_GetSource();
    const char *src_str =
        (src == APP_CONFIG_LOADED_BANK_A) ? "Bank A" :
        (src == APP_CONFIG_LOADED_BANK_B) ? "Bank B" : "Factory";
    snprintf(s_line, sizeof(s_line), "Cfg src: %s", src_str);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, s_line, false);

    snprintf(s_line, sizeof(s_line), "Updates: %lu",
             (unsigned long)App_Config_Get()->update_count);
    Bsp_Oled_FbDrawStr6x8(2U, 5U, s_line, false);

    Bsp_Oled_FbDrawStr6x8(2U, 7U, "K3/K4: back", false);
}

static void Draw_MenuFactoryReset(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Factory Reset");
    Bsp_Oled_FbDrawStr6x8(2U, 2U, "Reset ALL params?", false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "HOLD K4 3s = YES", false);
    Bsp_Oled_FbDrawStr6x8(2U, 5U, "K3 = NO (back)", false);
}

static void Draw_Done(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Done");
    Bsp_Oled_FbDrawStr6x8(2U, 3U, "Task complete.", false);
    Bsp_Oled_FbDrawStr6x8(2U, 5U, "Wait for plug off.", false);
}

static void Draw_Sleep(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("Sleep");
    Bsp_Oled_FbDrawStr6x8(2U, 3U, "Idle. Waiting for", false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "scheduled power-off.", false);
}

static void Draw_Error(void) {
    Bsp_Oled_FbClear();
    Draw_TitleBar("ERROR");
    Fm_ErrorCode err = App_Main_Fsm_LastError();
    snprintf(s_line, sizeof(s_line), "Code: 0x%02X", (unsigned)err);
    Bsp_Oled_FbDrawStr6x8(2U, 2U, s_line, false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "All outputs OFF.", false);
    Bsp_Oled_FbDrawStr6x8(2U, 6U, "Hold K3 = reset", false);
}

void App_Display_Tick(void) {
    if (!Bsp_Tick_Ticker_Due(&s_ticker) && !s_dirty) {
        return;
    }
    s_dirty = false;

    if (!Bsp_Oled_IsOnline()) {
        return;
    }

    App_Main_FsmState st = App_Main_Fsm_GetState();
    if (st == APP_MAIN_FSM_STATE_MENU) {
        switch (App_Menu_GetPage()) {
        case APP_MENU_PAGE_MAIN:           Draw_MenuMain();         break;
        case APP_MENU_PAGE_PARAMS:         Draw_MenuParams();       break;
        case APP_MENU_PAGE_PARAM_EDIT:     Draw_MenuParamEdit();    break;
        case APP_MENU_PAGE_MANUAL:         Draw_MenuManual();       break;
        case APP_MENU_PAGE_INFO:           Draw_MenuInfo();         break;
        case APP_MENU_PAGE_FACTORY_RESET:  Draw_MenuFactoryReset(); break;
        default:                           Draw_MenuMain();         break;
        }
    } else {
        switch (st) {
        case APP_MAIN_FSM_STATE_BOOT:      Draw_Boot();      break;
        case APP_MAIN_FSM_STATE_SELFTEST:  Draw_Selftest();  break;
        case APP_MAIN_FSM_STATE_IDLE_3S:   Draw_Idle3s();    break;
        case APP_MAIN_FSM_STATE_AUTO_RUN:  Draw_AutoRun();   break;
        case APP_MAIN_FSM_STATE_DONE:      Draw_Done();      break;
        case APP_MAIN_FSM_STATE_SLEEP:     Draw_Sleep();     break;
        case APP_MAIN_FSM_STATE_ERROR:     Draw_Error();     break;
        default:                           Draw_Boot();      break;
        }
    }

    (void)Bsp_Oled_Flush();
}
