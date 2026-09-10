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
#include "app_menu.h"
#include "app_config.h"
#include "app_serial_debug.h"
#include "bsp_key.h"
#include "bsp_valve.h"
#include "bsp_oled.h"
#include "bsp_pump_pwm.h"
#include "bsp_tick.h"
#include "floramate_types.h"

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

#define DISP_MENU_VISIBLE_ROWS (4U)

static void Parse_Sw_Version(unsigned *maj, unsigned *min)
{
    const char *s = FM_FIRMWARE_VERSION_STR;
    const char *v = strchr(s, 'V');

    *maj = 0U;
    *min = 0U;
    if (v != NULL)
    {
        (void)sscanf(v + 1, "%u.%u", maj, min);
    }
}

/** @brief P0 Logo：FloraMate + SW/HW；读取失败显示 0.0 */
static void Draw_Logo(void)
{
    bool cfg_fail = (App_Config_GetSource() == APP_CONFIG_LOADED_FACTORY);

    Bsp_Oled_FbClear();
    Draw_TitleBar("FloraMate");

    if (cfg_fail)
    {
        Bsp_Oled_FbDrawStr6x8(2U, DISP_PAGE_BODY0, "SW 0.0", false);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), "HW 0.0", false);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(2U), "CFG LOAD FAIL", false);
    }
    else
    {
        unsigned sw_maj = 0U;
        unsigned sw_min = 0U;
        unsigned hw_maj = 0U;
        unsigned hw_min = 0U;
        const App_Config *cfg = App_Config_Get();

        Parse_Sw_Version(&sw_maj, &sw_min);
        hw_maj = (unsigned)(cfg->hw_version >> 4U);
        hw_min = (unsigned)(cfg->hw_version & 0x0FU);
        snprintf(s_line, sizeof(s_line), "SW %u.%u", sw_maj, sw_min);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_PAGE_BODY0, s_line, false);
        snprintf(s_line, sizeof(s_line), "HW %u.%u", hw_maj, hw_min);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
    }
}

/** @brief P1 Idle：5s 倒计时 + K1+K3 进主界面 */
static void Draw_Idle(void)
{
    uint32_t remain_ms = App_Main_Fsm_GetIdleRemainMs();
    uint8_t  remain_s  = (uint8_t)((remain_ms + 999U) / 1000U);
    uint8_t  combo_pct = App_Main_Fsm_GetIdleComboPercent();
    bool     paused    = App_Main_Fsm_IsIdleTimerPaused();

    if (remain_s == 0U)
    {
        remain_s = 1U;
    }

    Draw_ScreenBegin("IDLE");
    if (paused)
    {
        Bsp_Oled_FbDrawStr6x8At(2U, 14U, "Timer PAUSED", false);
        snprintf(s_line, sizeof(s_line), "Hold %u%%", (unsigned)combo_pct);
        Bsp_Oled_FbDrawStr6x8At(2U, 24U, s_line, false);
        Draw_ProgressBar(2U, 34U, 124U, 5U, combo_pct);
        Bsp_Oled_FbDrawStr6x8At(2U, 42U, "K1+K3 2s->Menu", false);
    }
    else
    {
        snprintf(s_line, sizeof(s_line), "Auto in %us", (unsigned)remain_s);
        Bsp_Oled_FbDrawStr6x8At(2U, 14U, s_line, false);
        Bsp_Oled_FbDrawStr6x8At(2U, 28U, "Hold K1+K3 2s", false);
        Bsp_Oled_FbDrawStr6x8At(2U, 38U, "  -> Main Menu", false);
        Bsp_Oled_FbDrawStr6x8At(2U, 48U, "UART: debug", false);
    }
    Draw_ScreenEndEx2("K1+K3: Menu", true);
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

    Draw_ScreenBegin("SERIAL TEST");

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
    snprintf(s_line, sizeof(s_line), "v%u%u%u%u%u Pwm:%u%%",
             Bsp_Valve_Get(BSP_VALVE_Z1) ? 1U : 0U, Bsp_Valve_Get(BSP_VALVE_Z2) ? 1U : 0U,
             Bsp_Valve_Get(BSP_VALVE_Z3) ? 1U : 0U, Bsp_Valve_Get(BSP_VALVE_Z4) ? 1U : 0U,
             Bsp_Valve_Get(BSP_VALVE_Z5) ? 1U : 0U, (unsigned)Bsp_Pump_Pwm_GetDutyPercent());
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(3U), s_line, false);
    Draw_ScreenEndEx("K4L 2s: exit debug");
}

/** @brief AUTO_RUN 页：当前路 + 已用秒 + 总进度 */
static void Draw_AutoRun(void)
{
    const App_Config *cfg  = App_Config_Get();
    uint8_t           zone = App_Main_Fsm_CurrentZone();

    snprintf(s_line, sizeof(s_line), "AUTO Z%u", zone);
    Draw_ScreenBegin(s_line);

    snprintf(s_line, sizeof(s_line), "Steps: %u", cfg->step_count);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);

    uint32_t auto_elapsed_s = Bsp_Tick_ElapsedMs(App_Main_Fsm_AutoRunStartMs()) / 1000U;
    snprintf(s_line, sizeof(s_line), "Elapsed: %lus", (unsigned long)auto_elapsed_s);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
    snprintf(s_line, sizeof(s_line), "Pump: %u%%", (unsigned)Bsp_Pump_Pwm_GetDutyPercent());
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(2U), s_line, false);

    uint8_t percent = (cfg->total_timeout_s == 0U) ? 0U
                                                    : (uint8_t)((auto_elapsed_s * 100U) / cfg->total_timeout_s);
    if (percent > 100U)
    {
        percent = 100U;
    }
    Draw_ProgressBar(2U, 40U, 124U, 6U, percent);
    Draw_ScreenEndEx("K4: E-STOP");
}

static void Draw_Menu_ScrollList(const char *title, const char *footer)
{
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    uint8_t start  = (cursor < DISP_MENU_VISIBLE_ROWS) ? 0U
                                                     : (uint8_t)(cursor - (DISP_MENU_VISIBLE_ROWS - 1U));

    Draw_ScreenBegin(title);
    for (uint8_t i = 0U; (i < DISP_MENU_VISIBLE_ROWS) && ((start + i) < n); i++)
    {
        uint8_t item = (uint8_t)(start + i);
        Draw_ListRow(DISP_CONTENT_ROW(i), App_Menu_GetItemLabel(item), item == cursor);
    }
    if ((footer != NULL) && (footer[0] != '\0'))
    {
        Draw_ScreenEndEx(footer);
    }
    else
    {
        Draw_ScreenEnd();
    }
}

static void Draw_MenuMain(void)
{
    Draw_Menu_ScrollList("MAIN", "K1v K2^ K3OK");
}

static void Draw_MenuSettings(void)
{
    Draw_Menu_ScrollList("SETTINGS", "K4: Back");
}

static void Draw_MenuParams(void)
{
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    uint8_t start  = (cursor < DISP_MENU_VISIBLE_ROWS) ? 0U
                                                     : (uint8_t)(cursor - (DISP_MENU_VISIBLE_ROWS - 1U));

    Draw_ScreenBegin("PARAMS");
    for (uint8_t i = 0U; (i < DISP_MENU_VISIBLE_ROWS) && ((start + i) < n); i++)
    {
        uint8_t     item = (uint8_t)(start + i);
        bool        sel  = (item == cursor);
        const char *name = App_Menu_GetItemLabel(item);
        int32_t     v    = 0;
        (void)App_Config_GetField(name, &v);
        snprintf(s_line, sizeof(s_line), "%-7s %ld", name, (long)v);
        Draw_ListRow(DISP_CONTENT_ROW(i), s_line, sel);
    }
    Draw_ScreenEndEx("K3:Edit K4:Back");
}

static void Draw_MenuParamEdit(void)
{
    const char *name = App_Menu_GetEditFieldName();

    Draw_ScreenBegin("EDIT");
    if (strcmp(name, "ch_en") == 0)
    {
        uint8_t ch  = App_Menu_GetChEnEditChannel();
        int32_t val = App_Menu_GetEditValue();
        bool    on  = ((val & (int32_t)(1UL << ch)) != 0);

        snprintf(s_line, sizeof(s_line), "Z%u %s", (unsigned)(ch + 1U), on ? "ON" : "OFF");
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);
        snprintf(s_line, sizeof(s_line), "Mask 0x%02lX", (long)val);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(2U), "K1/K2: ch", false);
        Draw_ScreenEndEx("K3:tog K3L:save");
    }
    else
    {
        snprintf(s_line, sizeof(s_line), "%s", name);
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);
        snprintf(s_line, sizeof(s_line), "Val: %ld", (long)App_Menu_GetEditValue());
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), s_line, false);
        Draw_ScreenEndEx("K3:save K4:abort");
    }
}

static void Draw_MenuLocalTest(void)
{
    Draw_Menu_ScrollList("LOCAL TEST", "K4: Back");
}

static void Draw_ScreenPattern(uint8_t pat)
{
    uint8_t x;
    uint8_t y;

    Bsp_Oled_FbClear();
    switch (pat)
    {
        case 0U:
            Bsp_Oled_FbFill(true);
            break;
        case 1U:
            Bsp_Oled_FbClear();
            break;
        case 2U:
            for (y = 0U; y < 64U; y++)
            {
                for (x = 0U; x < 128U; x++)
                {
                    Bsp_Oled_FbDrawPixel(x, y, (((x / 8U) + (y / 8U)) & 1U) != 0U);
                }
            }
            break;
        case 3U:
            for (y = 0U; y < 64U; y++)
            {
                Bsp_Oled_FbDrawHLine(0U, y, 128U, ((y / 4U) & 1U) != 0U);
            }
            break;
        case 4U:
            for (x = 0U; x < 128U; x++)
            {
                Bsp_Oled_FbDrawHLine(x, 0U, 1U, ((x / 4U) & 1U) != 0U);
            }
            for (y = 1U; y < 64U; y++)
            {
                for (x = 0U; x < 128U; x++)
                {
                    Bsp_Oled_FbDrawPixel(x, y, ((x / 4U) & 1U) != 0U);
                }
            }
            break;
        default:
            Bsp_Oled_FbDrawRect(0U, 0U, 128U, 64U, true);
            Draw_TitleBar("SCREEN TEST");
            break;
    }
}

static void Draw_MenuScreenTest(void)
{
    Draw_ScreenPattern(App_Menu_GetScreenPattern());
    Draw_KeyFooterEx("K4:Back K3:Next");
}

static void Draw_MenuKeyTest(void)
{
    static const char *const s_labels[BSP_KEY_NUM] = {"K1", "K2", "K3", "K4"};
    uint8_t                  mask                  = App_Menu_GetKeyTestMask();

    Draw_ScreenBegin("KEY TEST");
    for (uint32_t i = 0U; i < (uint32_t)BSP_KEY_NUM; i++)
    {
        bool pressed = ((mask & (uint8_t)(1U << i)) != 0U);
        snprintf(s_line, sizeof(s_line), "%s %s", s_labels[i], pressed ? "ON" : "--");
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW((uint8_t)i), s_line, pressed);
    }
    Draw_ScreenEndEx("Idle 5s=Back");
}

static void Draw_MenuWaterTest(void)
{
    uint8_t cursor = App_Menu_GetCursor();
    uint8_t n      = App_Menu_GetItemCount();
    uint8_t start  = (cursor < DISP_MENU_VISIBLE_ROWS) ? 0U
                                                     : (uint8_t)(cursor - (DISP_MENU_VISIBLE_ROWS - 1U));

    Draw_ScreenBegin("WATER TEST");
    for (uint8_t i = 0U; (i < DISP_MENU_VISIBLE_ROWS) && ((start + i) < n); i++)
    {
        uint8_t item = (uint8_t)(start + i);
        bool    sel  = (item == cursor);

        if (item < 5U)
        {
            Bsp_Valve_Channel ch = (Bsp_Valve_Channel)((uint32_t)BSP_VALVE_Z1 + (uint32_t)item);
            snprintf(s_line, sizeof(s_line), "Z%u %s", (unsigned)(item + 1U),
                     Bsp_Valve_Get(ch) ? "ON" : "OFF");
        }
        else
        {
            if (App_Menu_IsPumpEditing())
            {
                snprintf(s_line, sizeof(s_line), "PUMP %u%% *", (unsigned)App_Menu_GetPumpPercent());
            }
            else
            {
                snprintf(s_line, sizeof(s_line), "PUMP %u%%", (unsigned)App_Menu_GetPumpPercent());
            }
        }
        Draw_ListRow(DISP_CONTENT_ROW(i), s_line, sel);
    }
    Draw_ScreenEndEx("K3:Sel K4:Back");
}

/** @brief DONE 页：正常完成或紧急停止 */
static void Draw_Done(void)
{
    Draw_ScreenBegin(App_Main_Fsm_IsWateringStopped() ? "STOPPED" : "DONE");
    if (App_Main_Fsm_IsWateringStopped())
    {
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), "Watering stopped", false);
    }
    else
    {
        Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), "Watering OK", false);
    }
    Draw_ScreenEndEx("K4: Main Menu");
}

/** @brief ERROR 页：错误码 + 输出已关 */
static void Draw_Error(void)
{
    Draw_ScreenBegin("ERROR");
    Fm_ErrorCode err = App_Main_Fsm_LastError();
    snprintf(s_line, sizeof(s_line), "Code: 0x%02X", (unsigned)err);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(0U), s_line, false);
    Bsp_Oled_FbDrawStr6x8(2U, DISP_CONTENT_ROW(1U), "All outputs OFF", false);
    Draw_ScreenEndEx("K4: Main Menu");
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
            case APP_MENU_PAGE_SETTINGS:
                Draw_MenuSettings();
                break;
            case APP_MENU_PAGE_PARAMS:
                Draw_MenuParams();
                break;
            case APP_MENU_PAGE_PARAM_EDIT:
                Draw_MenuParamEdit();
                break;
            case APP_MENU_PAGE_LOCAL_TEST:
                Draw_MenuLocalTest();
                break;
            case APP_MENU_PAGE_SCREEN_TEST:
                Draw_MenuScreenTest();
                break;
            case APP_MENU_PAGE_KEY_TEST:
                Draw_MenuKeyTest();
                break;
            case APP_MENU_PAGE_WATER_TEST:
                Draw_MenuWaterTest();
                break;
            default:
                Draw_MenuMain();
                break;
        }
        return;
    }

    switch (st)
    {
        case APP_MAIN_FSM_STATE_LOGO:
            Draw_Logo();
            break;
        case APP_MAIN_FSM_STATE_IDLE:
            Draw_Idle();
            break;
        case APP_MAIN_FSM_STATE_SERIAL_DEBUG:
            Draw_SerialDebug();
            break;
        case APP_MAIN_FSM_STATE_AUTO_RUN:
            Draw_AutoRun();
            break;
        case APP_MAIN_FSM_STATE_DONE:
            Draw_Done();
            break;
        case APP_MAIN_FSM_STATE_ERROR:
            Draw_Error();
            break;
        default:
            Draw_Logo();
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
    bool force_refresh = (st == APP_MAIN_FSM_STATE_LOGO) || (st == APP_MAIN_FSM_STATE_IDLE) ||
                         (st == APP_MAIN_FSM_STATE_AUTO_RUN) || (st == APP_MAIN_FSM_STATE_MENU);
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
