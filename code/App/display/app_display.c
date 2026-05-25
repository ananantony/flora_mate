/*
 * @File         : \code\App\display\app_display.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : OLED 页面绘制实现（100 ms 限速、全屏刷新）
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
#include "app_display.h"
#include "app_main_fsm.h"
#include <string.h>
#include "app_menu.h"
#include "app_pump_fsm.h"
#include "app_config.h"
#include "app_manual_key.h"
#include "app_manual_select.h"
#include "app_serial_debug.h"
#include "app_serial_debug_config.h"
#include "bsp_key.h"
#include "bsp_relay.h"
#include "bsp_oled.h"
#include "bsp_pump_pwm.h"
#include "bsp_tick.h"

#include <stdio.h>
#include <string.h>

#define DISPLAY_PERIOD_MS (100U) /**< 显示刷新周期 */

/** 布局：page0 标题；page1 标题下留白；page2~6 正文；page7 按键 */
#define DISP_PAGE_TITLE         (0U)
#define DISP_PAGE_AFTER_TITLE   (1U)
#define DISP_PAGE_BODY0         (2U)
#define DISP_PAGE_ABOVE_FOOTER  (6U)
#define DISP_PAGE_FOOTER        (7U)
#define DISP_TITLE_BAR_H        (11U) /**< 标题栏像素高（含底部分隔线） */
#define DISP_CONTENT_ROW(row)   ((uint8_t)(DISP_PAGE_BODY0 + (uint8_t)(row)))
static Bsp_Tick_Ticker      s_ticker;       /**< 100 ms 节拍器           */
static bool                 s_dirty = true; /**< 强制重绘标志            */
static App_Main_FsmState    s_last_fsm_state =
    (App_Main_FsmState)(-1);                /**< 上次已绘制的主状态     */
static char                 s_line[24];     /**< snprintf 局部缓冲      */

/**
 * @brief   拼一行 OLED 文本，超长部分截断（避免 -Wformat-truncation）
 */
static void Format_OledLine(char *dst, size_t cap, const char *prefix, const char *text)
{
    size_t pre_len;

    if ((dst == NULL) || (cap == 0U))
    {
        return;
    }
    if (prefix == NULL)
    {
        prefix = "";
    }
    if (text == NULL)
    {
        text = "";
    }
    pre_len = strlen(prefix);
    if (pre_len >= cap)
    {
        pre_len = cap - 1U;
    }
    size_t body_max = (cap > pre_len + 1U) ? (cap - pre_len - 1U) : 0U;
    (void)snprintf(dst, cap, "%.*s%.*s", (int)pre_len, prefix, (int)body_max, text);
}

/**
 * @brief   标脏：下次 Tick 强制全屏重绘
 */
void App_Display_MarkDirty(void)
{
    s_dirty = true;
}

/** @brief 按当前 FSM/菜单状态绘制一帧（不写屏） */
static void Display_Render(void);

/**
 * @brief   显示模块初始化
 */
void App_Display_Init(void)
{
    Bsp_Tick_Ticker_Init(&s_ticker, DISPLAY_PERIOD_MS);
    s_dirty            = true;
    s_last_fsm_state   = (App_Main_FsmState)(-1);
}

/**
 * @brief   绘制顶部标题栏（反白底 + 居中标题 + 底部分隔线）
 * @note    正文从 DISP_PAGE_BODY0 起，勿使用 page1，以免与 y=10 分隔线重叠。
 */
static void Draw_TitleBar(const char *title)
{
    size_t title_len;
    uint8_t x;

    if (title == NULL)
    {
        title = "";
    }
    title_len = strlen(title);
    if (title_len > 20U)
    {
        title_len = 20U;
    }
    x = (uint8_t)((128U - (uint8_t)(title_len * 6U)) / 2U);

    Bsp_Oled_FbFillRect(0U, 0U, 128U, DISP_TITLE_BAR_H, true);
    Bsp_Oled_FbDrawStr6x8(x, DISP_PAGE_TITLE, title, true);
    Bsp_Oled_FbDrawHLine(0U, 10U, 128U, true);
}

/** @brief 页脚反白条（文字居中）；text 为 NULL 时用默认四键提示 */
static void Draw_KeyFooterEx(const char *text)
{
    size_t  len;
    uint8_t x;

    if ((text == NULL) || (text[0] == '\0'))
    {
        text = "K1v K2^ K3OK K4X";
    }
    len = strlen(text);
    if (len > 21U)
    {
        len = 21U;
    }
    x = (uint8_t)((128U - (uint8_t)(len * 6U)) / 2U);
    Bsp_Oled_FbFillRect(0U, 56U, 128U, 8U, true);
    Bsp_Oled_FbDrawStr6x8(x, DISP_PAGE_FOOTER, text, true);
}

static void Draw_KeyFooter(void)
{
    Draw_KeyFooterEx(NULL);
}

/** @brief 全宽列表一行：仅 selected 时反白 */
static void Draw_ListRow(uint8_t page, const char *text, bool selected)
{
    uint8_t y = (uint8_t)(page * 8U);

    if (selected)
    {
        Bsp_Oled_FbFillRect(2U, y, 124U, 8U, true);
    }
    Bsp_Oled_FbDrawChar6x8(4U, page, selected ? '>' : ' ', selected);
    Bsp_Oled_FbDrawStr6x8(12U, page, text, selected);
}

/** @brief 像素级列表行：用于 64px 屏内做等距排版 */
static void Draw_ListRowY(uint8_t y, const char *text, bool selected)
{
    if (selected)
    {
        Bsp_Oled_FbFillRect(2U, y, 124U, 8U, true);
    }
    Bsp_Oled_FbDrawChar6x8At(4U, y, selected ? '>' : ' ', selected);
    Bsp_Oled_FbDrawStr6x8At(12U, y, text, selected);
}

/** @brief 清空一行（行距留白） */
static void Draw_RowGap(uint8_t page)
{
    Bsp_Oled_FbFillRect(0U, (uint8_t)(page * 8U), 128U, 8U, false);
}

/**
 * @brief   标准页框：清屏 + 标题 + 标题下留白
 * @param   extra_title_gap_pages  额外标题下留白页数（0=仅 page1，1=page1+2 …）
 */
static void Draw_ScreenBeginEx(const char *title, uint8_t extra_title_gap_pages)
{
    uint8_t p;

    Bsp_Oled_FbClear();
    Draw_TitleBar(title);
    Draw_RowGap(DISP_PAGE_AFTER_TITLE);
    for (p = 0U; p < extra_title_gap_pages; p++)
    {
        Draw_RowGap((uint8_t)(DISP_PAGE_AFTER_TITLE + 1U + p));
    }
}

static void Draw_ScreenBegin(const char *title)
{
    Draw_ScreenBeginEx(title, 0U);
}

/** @brief 标准页脚；body_to_page6=true 时正文占满至 page6，不再清空该行 */
static void Draw_ScreenEndEx2(const char *footer, bool body_to_page6)
{
    if (!body_to_page6)
    {
        Draw_RowGap(DISP_PAGE_ABOVE_FOOTER);
    }
    if ((footer == NULL) || (footer[0] == '\0'))
    {
        Draw_KeyFooter();
    }
    else
    {
        Draw_KeyFooterEx(footer);
    }
}

static void Draw_ScreenEnd(void)
{
    Draw_ScreenEndEx2(NULL, false);
}

static void Draw_ScreenEndEx(const char *footer)
{
    Draw_ScreenEndEx2(footer, false);
}

/**
 * @brief   绘制进度条（外框 + 内部填充）
 * @param   x,y     左上坐标
 * @param   w,h     宽 / 高
 * @param   percent 进度百分比（自动钳到 100）
 */
static void Draw_ProgressBar(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t percent)
{
    if (percent > 100U)
        percent = 100U;
    Bsp_Oled_FbDrawRect(x, y, w, h, true);
    uint8_t fill_w = (uint8_t)((uint32_t)(w - 2U) * percent / 100U);
    Bsp_Oled_FbFillRect((uint8_t)(x + 1U), (uint8_t)(y + 1U), fill_w, (uint8_t)(h - 2U), true);
}

/** @brief BOOT 页：固定 logo + 版本号 */
static void Draw_Boot(void)
{
    Bsp_Oled_FbClear();
    Draw_TitleBar("FloraMate");
    Bsp_Oled_FbDrawStr6x8(2U, DISP_PAGE_BODY0, "Booting...", false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "V1.0", false);
}

/** @brief 上电等待页：超时自动浇灌；debug 或 K1+K2 进手动 */
static void Draw_SerialWait(void)
{
    uint32_t remain_ms = App_Main_Fsm_GetBootWaitRemainMs();
    uint8_t  remain_s  = (uint8_t)((remain_ms + 999U) / 1000U);
    uint8_t  combo_pct = App_Main_Fsm_GetBootWaitComboPercent();
    bool     paused    = App_Main_Fsm_IsBootWaitTimerPaused();

    if (remain_s == 0U)
    {
        remain_s = 1U;
    }

    Draw_ScreenBegin("Boot Wait");
    if (paused)
    {
        Bsp_Oled_FbFillRect(2U, 15U, 124U, 8U, true);
        Bsp_Oled_FbDrawStr6x8At(8U, 15U, "Timer PAUSED", true);
        Bsp_Oled_FbDrawStr6x8At(2U, 29U, "Hold K1+K2 2.5s", false);
    }
    else
    {
        snprintf(s_line, sizeof(s_line), "Auto in %us", (unsigned)remain_s);
        Bsp_Oled_FbDrawStr6x8At(2U, 14U, s_line, false);
    }
    if (combo_pct > 0U)
    {
        snprintf(s_line, sizeof(s_line), "Manual %u%%", (unsigned)combo_pct);
        Bsp_Oled_FbDrawStr6x8At(2U, 40U, s_line, false);
        Draw_ProgressBar(2U, 50U, 124U, 5U, combo_pct);
    }
    else if (paused)
    {
        Bsp_Oled_FbDrawStr6x8At(2U, 42U, "Press both keys", false);
    }
    else
    {
        Bsp_Oled_FbDrawStr6x8At(2U, 28U, "UART: debug / dbg", false);
        Bsp_Oled_FbDrawStr6x8At(2U, 42U, "K1+K2 hold -> Manual", false);
    }
    /* body_to_page6=true：正文最后一行 y=42 延伸至 page6（y=48~49），
     * 进度条 y=50 完全在 page6，禁止 RowGap 清除该区域。 */
#if APP_SERIAL_DEBUG_AUTO_ENTER_ON_BOOT
    Draw_ScreenEndEx2("AUTO_ENTER=1", true);
#else
    Draw_ScreenEndEx2("K1+2:Man K3:UART", true);
#endif
}

/** @brief 手动模式：选择串口 / 按键 / 返回 */
static void Draw_ManualSelect(void)
{
    uint8_t n   = App_ManualSelect_GetItemCount();
    uint8_t cur = App_ManualSelect_GetCursor();

    /* 在标题栏(y0~10)和页脚(y56~63)之间按像素等距摆放 3 项。 */
    {
        const uint8_t item_y[3] = {15U, 29U, 43U};

        Draw_ScreenBegin("Manual");
        for (uint8_t i = 0U; i < n; i++)
        {
            Draw_ListRowY(item_y[i], App_ManualSelect_GetItemLabel(i), i == cur);
        }
        Draw_KeyFooter();
    }
}

#define MANUAL_KEY_CELL_W (62U)

/** @brief Key Ctrl 双列单元：仅光标所在格反白 */
static void Draw_ManualKey_CellY(uint8_t y, uint8_t col, const char *name, bool on, bool focused, bool active)
{
    uint8_t x = (col == 0U) ? 0U : 66U;

    if (active)
    {
        Bsp_Oled_FbFillRect(x, y, MANUAL_KEY_CELL_W, 8U, true);
    }
    snprintf(s_line, sizeof(s_line), "%s %s", name, on ? "ON" : "OFF");
    Bsp_Oled_FbDrawChar6x8At((uint8_t)(x + 2U), y, focused ? '>' : ' ', active);
    Bsp_Oled_FbDrawStr6x8At((uint8_t)(x + 10U), y, s_line, active);
}

/** @brief Pump 行（始终显示，全宽；仅选中时反白） */
static void Draw_ManualKey_PumpRowY(uint8_t y, bool focused, bool active, uint8_t duty)
{
    if (active)
    {
        Bsp_Oled_FbFillRect(0U, y, 128U, 8U, true);
    }
    if (duty > 0U)
    {
        snprintf(s_line, sizeof(s_line), "Pump %u%%", (unsigned)duty);
    }
    else
    {
        (void)strncpy(s_line, "Pump", sizeof(s_line));
        s_line[sizeof(s_line) - 1U] = '\0';
    }
    Bsp_Oled_FbDrawChar6x8At(4U, y, focused ? '>' : ' ', active);
    Bsp_Oled_FbDrawStr6x8At(12U, y, s_line, active);
}

/** Key Ctrl：4 行在 y=11..55 可用区域内等距排布。 */
#define KEY_CTRL_Y_PWR_V1  (13U)
#define KEY_CTRL_Y_V2_V3   (24U)
#define KEY_CTRL_Y_V4_RSV  (35U)
#define KEY_CTRL_Y_PUMP    (46U)

/** @brief 手动按键控制页（双列 + 加大行距 + Pump 常驻） */
static void Draw_ManualKey(void)
{
    uint8_t cur  = App_ManualKey_GetCursor();
    uint8_t duty = App_ManualKey_GetPumpDuty();
    bool    item_active = App_ManualKey_IsItemActive();

    Draw_ScreenBegin("Key Ctrl");

    Draw_ManualKey_CellY(KEY_CTRL_Y_PWR_V1, 0U, "Pwr", App_ManualKey_IsRelayOn(0U), cur == 0U,
                         item_active && (cur == 0U));
    Draw_ManualKey_CellY(KEY_CTRL_Y_PWR_V1, 1U, "V1", App_ManualKey_IsRelayOn(1U), cur == 1U,
                         item_active && (cur == 1U));
    Draw_ManualKey_CellY(KEY_CTRL_Y_V2_V3, 0U, "V2", App_ManualKey_IsRelayOn(2U), cur == 2U,
                         item_active && (cur == 2U));
    Draw_ManualKey_CellY(KEY_CTRL_Y_V2_V3, 1U, "V3", App_ManualKey_IsRelayOn(3U), cur == 3U,
                         item_active && (cur == 3U));
    Draw_ManualKey_CellY(KEY_CTRL_Y_V4_RSV, 0U, "V4", App_ManualKey_IsRelayOn(4U), cur == 4U,
                         item_active && (cur == 4U));
    Draw_ManualKey_CellY(KEY_CTRL_Y_V4_RSV, 1U, "Rsv", App_ManualKey_IsRelayOn(5U), cur == 5U,
                         item_active && (cur == 5U));
    Draw_ManualKey_PumpRowY(KEY_CTRL_Y_PUMP, cur == 6U, item_active && (cur == 6U), duty);

    Draw_KeyFooter();
}

/** @brief 串口调试 / 按键测试页：K1~K4 实时按下状态 */
static void Draw_SerialDebugKeyTest(void)
{
    static const char *const s_labels[BSP_KEY_NUM] = {
        "K1 UP  :",
        "K2 DOWN:",
        "K3 OK  :",
        "K4 BACK:",
    };

    Draw_ScreenBegin("Key Test");

    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++)
    {
        bool    pressed = Bsp_Key_IsPressed((Bsp_Key_Id)i);
        uint8_t page    = DISP_CONTENT_ROW((uint8_t)i);
        snprintf(s_line, sizeof(s_line), "%s %s", s_labels[i], pressed ? "ON " : "-- ");
        Bsp_Oled_FbDrawStr6x8(2U, page, s_line, pressed);
    }
    Draw_ScreenEnd();
}

/** @brief 串口调试页：显示当前命令与执行结果 */
static void Draw_SerialDebug(void)
{
    char                    ui_cmd[40];
    char                    ui_result[40];
    App_SerialDebug_UiState ui_st;

    App_SerialDebug_GetUi(ui_cmd, sizeof(ui_cmd), ui_result, sizeof(ui_result), &ui_st);

    if (ui_st == APP_SERIAL_DEBUG_UI_KEY_TEST)
    {
        Draw_SerialDebugKeyTest();
        return;
    }

    Draw_ScreenBegin("Serial Debug");

    if (ui_cmd[0] == '\0')
    {
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), "Cmd: (idle)", false);
    }
    else
    {
        Format_OledLine(s_line, sizeof(s_line), "> ", ui_cmd);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);
    }

    switch (ui_st)
    {
        case APP_SERIAL_DEBUG_UI_RUNNING:
            Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), "Status: RUN...", true);
            break;
        case APP_SERIAL_DEBUG_UI_OK:
            Format_OledLine(s_line, sizeof(s_line), "OK: ", ui_result);
            Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
            break;
        case APP_SERIAL_DEBUG_UI_ERR:
            Format_OledLine(s_line, sizeof(s_line), "ERR: ", ui_result);
            Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
            break;
        default:
            Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), "UART: debug on", false);
            break;
    }

    snprintf(s_line, sizeof(s_line), "Pwm:%u%% R:", (unsigned)Bsp_Pump_Pwm_GetDutyPercent());
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(2U), s_line, false);
    snprintf(s_line, sizeof(s_line), "P%uv%u%u%u%u r%u",
             Bsp_Relay_Get(BSP_RELAY_PUMP_PWR_CH1) ? 1U : 0U, Bsp_Relay_Get(BSP_RELAY_VALVE_1) ? 1U : 0U,
             Bsp_Relay_Get(BSP_RELAY_VALVE_2) ? 1U : 0U, Bsp_Relay_Get(BSP_RELAY_VALVE_3) ? 1U : 0U,
             Bsp_Relay_Get(BSP_RELAY_VALVE_4) ? 1U : 0U, Bsp_Relay_Get(BSP_RELAY_RSV_CH6) ? 1U : 0U);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(3U), s_line, false);
    Draw_ScreenEndEx("K4L 2s: exit debug");
}

/** @brief SELFTEST 页：自检文案 + 固定 60% 进度条（仅用作视觉反馈） */
static void Draw_Selftest(void)
{
    Bsp_Oled_FbClear();
    Draw_TitleBar("Selftest");
    Bsp_Oled_FbDrawStr6x8(2U, DISP_PAGE_BODY0, "Relays click test", false);
    Draw_ProgressBar(2U, 40U, 124U, 8U, 60U);
}

/**
 * @brief   IDLE_3S 页：大数字倒计时 + 操作提示
 * @note    倒计时秒数 = ceil((idle_seconds*1000 - elapsed)/1000)；
 *          最少显示 1，避免最后一帧一闪而过看到 0。
 */
static void Draw_Idle3s(void)
{
    const App_Config *cfg      = App_Config_Get();
    uint32_t          elapsed  = Bsp_Tick_ElapsedMs(App_Main_Fsm_EnterMs());
    uint32_t          total    = (uint32_t)cfg->idle_seconds * 1000U;
    uint32_t          remain   = (elapsed >= total) ? 0U : (total - elapsed);
    uint8_t           remain_s = (uint8_t)((remain + 999U) / 1000U);
    if (remain_s == 0U)
        remain_s = 1U;

    Draw_ScreenBegin("Idle");

    /* 大数字倒计时 */
    char d = (char)('0' + (remain_s % 10U));
    Bsp_Oled_FbDrawBigDigit(56U, DISP_CONTENT_ROW(0U), d);

    Draw_ScreenEndEx("K3:menu  K4:start now");
}

/**
 * @brief   AUTO_RUN 页：当前路 / 总路 + 已用秒 + 总进度条
 */
static void Draw_AutoRun(void)
{
    const App_Config *cfg  = App_Config_Get();
    uint8_t           zone = App_Main_Fsm_CurrentZone(); /* 1..4 */

    snprintf(s_line, sizeof(s_line), "AUTO Z%u/%u", zone, FM_CHANNEL_NUM);
    Draw_ScreenBegin(s_line);

    /* 当前阶梯条 */
    /* 简化：从 App_Pump_Fsm 暴露的 ctx 无法跨模块访问，
     * 这里基于 Config + 总 step 占空比 + 已用时间近似显示。
     * 真实实现可以在 App_Main_Fsm 中导出更详细的状态。 */
    snprintf(s_line, sizeof(s_line), "Steps: %u", cfg->step_count);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);

    uint32_t auto_elapsed_s = Bsp_Tick_ElapsedMs(App_Main_Fsm_AutoRunStartMs()) / 1000U;
    snprintf(s_line, sizeof(s_line), "Elapsed: %lus", (unsigned long)auto_elapsed_s);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);

    uint8_t percent = (cfg->total_timeout_s == 0U) ? 0U : (uint8_t)((auto_elapsed_s * 100U) / cfg->total_timeout_s);
    if (percent > 100U)
    {
        percent = 100U;
    }
    Draw_ProgressBar(2U, 40U, 124U, 6U, percent);

    Draw_ScreenEndEx("K3:stop   K4:menu");
}

#define DISP_MENU_VISIBLE_ROWS (3U)

/**
 * @brief   MENU/Main 页：最多 3 行，行距 2 page，仅当前项反白
 */
static void Draw_MenuMain(void)
{
    Draw_ScreenBegin("Menu");
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    uint8_t start  = (cursor < DISP_MENU_VISIBLE_ROWS) ? 0U : (uint8_t)(cursor - (DISP_MENU_VISIBLE_ROWS - 1U));
    for (uint8_t i = 0U; (i < DISP_MENU_VISIBLE_ROWS) && ((start + i) < n); i++)
    {
        uint8_t item = (uint8_t)(start + i);
        Draw_ListRow(DISP_CONTENT_ROW(i), App_Menu_GetItemLabel(item), item == cursor);
    }
    Draw_ScreenEnd();
}

/** @brief MENU/Params 页：参数名 + 当前值 */
static void Draw_MenuParams(void)
{
    Draw_ScreenBegin("Params");
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    uint8_t start  = (cursor < DISP_MENU_VISIBLE_ROWS) ? 0U : (uint8_t)(cursor - (DISP_MENU_VISIBLE_ROWS - 1U));
    for (uint8_t i = 0U; (i < DISP_MENU_VISIBLE_ROWS) && ((start + i) < n); i++)
    {
        uint8_t     item = (uint8_t)(start + i);
        bool        sel  = (item == cursor);
        const char *name = App_Menu_GetItemLabel(item);
        int32_t     v    = 0;
        (void)App_Config_GetField(name, &v);
        snprintf(s_line, sizeof(s_line), "%-9s %ld", name, (long)v);
        Draw_ListRow(DISP_CONTENT_ROW(i), s_line, sel);
    }
    Draw_ScreenEnd();
}

/** @brief MENU/ParamEdit 页：单字段编辑 */
static void Draw_MenuParamEdit(void)
{
    Draw_ScreenBegin("Edit Param");
    snprintf(s_line, sizeof(s_line), "Field: %s", App_Menu_GetEditFieldName());
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);
    snprintf(s_line, sizeof(s_line), "Value: %ld", (long)App_Menu_GetEditValue());
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
    Draw_ScreenEnd();
}

/** @brief MENU/Manual 页：手动测试条目列表 */
static void Draw_MenuManual(void)
{
    Draw_ScreenBegin("Manual Test");
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    for (uint8_t i = 0U; (i < DISP_MENU_VISIBLE_ROWS) && (i < n); i++)
    {
        Draw_ListRow(DISP_CONTENT_ROW(i), App_Menu_GetItemLabel(i), i == cursor);
    }
    Draw_ScreenEnd();
}

/** @brief MENU/Info 页：固件版本 / Uptime / 配置来源 / 写入计数 */
static void Draw_MenuInfo(void)
{
    Draw_ScreenBegin("System Info");
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), FM_FIRMWARE_VERSION_STR, false);
    snprintf(s_line, sizeof(s_line), "Uptime: %lus", (unsigned long)(Bsp_Tick_GetMs() / 1000U));
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);

    App_Config_Source src     = App_Config_GetSource();
    const char       *src_str = (src == APP_CONFIG_LOADED_BANK_A) ? "Bank A"
                              : (src == APP_CONFIG_LOADED_BANK_B) ? "Bank B"
                                                                  : "Factory";
    snprintf(s_line, sizeof(s_line), "Cfg src: %s", src_str);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(2U), s_line, false);

    snprintf(s_line, sizeof(s_line), "Updates: %lu", (unsigned long)App_Config_Get()->update_count);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(3U), s_line, false);

    Draw_ScreenEnd();
}

/** @brief MENU/FactoryReset 页：长按 K3 3s 确认 */
static void Draw_MenuFactoryReset(void)
{
    Draw_ScreenBegin("Factory Reset");
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), "Reset ALL params?", false);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), "Hold K3 3s = YES", false);
    Draw_ScreenEndEx("K4: cancel");
}

/** @brief DONE 页：等待断电 */
static void Draw_Done(void)
{
    Bsp_Oled_FbClear();
    Draw_TitleBar("Done");
    Bsp_Oled_FbDrawStr6x8(2U, 3U, "Task complete.", false);
    Bsp_Oled_FbDrawStr6x8(2U, 5U, "Wait for plug off.", false);
}

/** @brief SLEEP 页：等待米家插座下次定时上电 */
static void Draw_Sleep(void)
{
    Bsp_Oled_FbClear();
    Draw_TitleBar("Sleep");
    Bsp_Oled_FbDrawStr6x8(2U, 3U, "Idle. Waiting for", false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "scheduled power-off.", false);
}

/** @brief ERROR 页：错误码 + 输出已关 + 长按 K3 复位 */
static void Draw_Error(void)
{
    Bsp_Oled_FbClear();
    Draw_TitleBar("ERROR");
    Fm_ErrorCode err = App_Main_Fsm_LastError();
    snprintf(s_line, sizeof(s_line), "Code: 0x%02X", (unsigned)err);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_PAGE_BODY0, s_line, false);
    Bsp_Oled_FbDrawStr6x8(2U, 4U, "All outputs OFF.", false);
    Bsp_Oled_FbDrawStr6x8(2U, 6U, "K3 HOLD: reset MCU", false);
}

static App_SerialDebug_UiState s_last_serial_ui = APP_SERIAL_DEBUG_UI_IDLE;

/** @brief 按当前 FSM / 菜单 / 串口 UI 子状态绘制一帧 */
static void Display_Render(void)
{
    App_Main_FsmState st = App_Main_Fsm_GetState();

    if (st == APP_MAIN_FSM_STATE_MENU)
    {
        switch (App_Menu_GetPage())
        {
            case APP_MENU_PAGE_MAIN:
                Draw_MenuMain();
                break;
            case APP_MENU_PAGE_PARAMS:
                Draw_MenuParams();
                break;
            case APP_MENU_PAGE_PARAM_EDIT:
                Draw_MenuParamEdit();
                break;
            case APP_MENU_PAGE_MANUAL:
                Draw_MenuManual();
                break;
            case APP_MENU_PAGE_INFO:
                Draw_MenuInfo();
                break;
            case APP_MENU_PAGE_FACTORY_RESET:
                Draw_MenuFactoryReset();
                break;
            default:
                Draw_MenuMain();
                break;
        }
        return;
    }

    switch (st)
    {
        case APP_MAIN_FSM_STATE_SERIAL_WAIT:
            Draw_SerialWait();
            break;
        case APP_MAIN_FSM_STATE_SERIAL_DEBUG:
            Draw_SerialDebug();
            break;
        case APP_MAIN_FSM_STATE_MANUAL_SELECT:
            Draw_ManualSelect();
            break;
        case APP_MAIN_FSM_STATE_MANUAL_KEY:
            Draw_ManualKey();
            break;
        case APP_MAIN_FSM_STATE_BOOT:
            Draw_Boot();
            break;
        case APP_MAIN_FSM_STATE_SELFTEST:
            Draw_Selftest();
            break;
        case APP_MAIN_FSM_STATE_IDLE_3S:
            Draw_Idle3s();
            break;
        case APP_MAIN_FSM_STATE_AUTO_RUN:
            Draw_AutoRun();
            break;
        case APP_MAIN_FSM_STATE_DONE:
            Draw_Done();
            break;
        case APP_MAIN_FSM_STATE_SLEEP:
            Draw_Sleep();
            break;
        case APP_MAIN_FSM_STATE_ERROR:
            Draw_Error();
            break;
        default:
            Draw_Boot();
            break;
    }
}

void App_Display_FlushNow(void)
{
    App_SerialDebug_UiState ui_st = APP_SERIAL_DEBUG_UI_IDLE;

    if (!Bsp_Oled_IsOnline())
    {
        return;
    }

    Display_Render();

    if (App_Main_Fsm_GetState() == APP_MAIN_FSM_STATE_SERIAL_DEBUG)
    {
        char dummy_cmd[1];
        char dummy_res[1];
        App_SerialDebug_GetUi(dummy_cmd, 1U, dummy_res, 1U, &ui_st);
        s_last_serial_ui = ui_st;
    }

    (void)Bsp_Oled_Flush();
    s_last_fsm_state = App_Main_Fsm_GetState();
    s_dirty          = false;
}

/**
 * @brief   主循环周期调用：按状态/页面选绘制函数 + 全屏 Flush
 * @note    OLED 离线时直接返回（不浪费 I²C 时间）；
 *          每帧整屏 1024 B I²C 传输 ≈ 26 ms，DISPLAY_PERIOD_MS=100 ms 安全。
 */
void App_Display_Tick(void)
{
    App_Main_FsmState     st              = App_Main_Fsm_GetState();
    bool                  force_refresh   =
        (st == APP_MAIN_FSM_STATE_SERIAL_WAIT) || (st == APP_MAIN_FSM_STATE_SERIAL_DEBUG) ||
        (st == APP_MAIN_FSM_STATE_MANUAL_SELECT) || (st == APP_MAIN_FSM_STATE_MANUAL_KEY) ||
        (st == APP_MAIN_FSM_STATE_MENU);
    bool                  state_changed   = (st != s_last_fsm_state);
    App_SerialDebug_UiState serial_ui       = APP_SERIAL_DEBUG_UI_IDLE;
    bool                  serial_ui_changed = false;

    if (st == APP_MAIN_FSM_STATE_SERIAL_DEBUG)
    {
        char dummy_cmd[1];
        char dummy_res[1];
        App_SerialDebug_GetUi(dummy_cmd, 1U, dummy_res, 1U, &serial_ui);
        serial_ui_changed = (serial_ui != s_last_serial_ui);
    }

    if (!state_changed && !serial_ui_changed && !Bsp_Tick_Ticker_Due(&s_ticker) && !s_dirty && !force_refresh)
    {
        return;
    }

    if (!Bsp_Oled_IsOnline())
    {
        return;
    }

    Display_Render();
    (void)Bsp_Oled_Flush();

    s_last_fsm_state = st;
    s_dirty          = false;
    if (st == APP_MAIN_FSM_STATE_SERIAL_DEBUG)
    {
        s_last_serial_ui = serial_ui;
    }
}
