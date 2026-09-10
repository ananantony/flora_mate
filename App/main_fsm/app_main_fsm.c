/*
 * @File         : \code\App\main_fsm\app_main_fsm.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-09-10 12:00:00
 * @Description  : 系统主状态机实现（HMI V2.0 产品路径）
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#include "app_main_fsm.h"
#include "app_pump_fsm.h"
#include "app_config.h"
#include "app_log.h"
#include "app_event.h"
#include "app_menu.h"
#include "app_display.h"
#include "app_serial_debug.h"
#include "bsp_key.h"
#include "bsp_valve.h"
#include "bsp_pump_pwm.h"
#include "bsp_tick.h"
#include "main.h"
#include "stm32f4xx_hal.h"

#include <string.h>

#define LOGO_MIN_MS        (2000U)
#define LOGO_FAIL_EXTRA_MS (2000U)
#define IDLE_MS            (5000U)
#define IDLE_COMBO_HOLD_MS (2000U)

typedef struct
{
    App_Main_FsmState state;
    uint32_t          state_enter_ms;
    uint32_t          auto_run_start_ms;
    uint8_t           current_zone_1based;
    App_Pump_FsmCtx   pump_ctx;
    uint8_t           enabled_zones[FM_CHANNEL_NUM];
    uint8_t           enabled_count;
    uint8_t           enabled_idx;
    Fm_ErrorCode      last_err;
    uint32_t          last_heartbeat_ms;
    bool              combo_active;
    uint32_t          combo_hold_start_ms;
    uint32_t          idle_accum_ms;
    uint32_t          idle_last_tick_ms;
    bool              watering_stopped;
    bool              serial_from_idle;
} App_Main_Fsm_Ctx;

static App_Main_Fsm_Ctx s_ctx;

static void All_Outputs_Off(void)
{
    Bsp_Pump_Pwm_Stop();
    Bsp_Valve_ForceAllOff();
}

static void Heartbeat_Update(uint32_t period_ms)
{
    if (Bsp_Tick_ElapsedMs(s_ctx.last_heartbeat_ms) >= period_ms)
    {
        HAL_GPIO_TogglePin(LED_HEARTBEAT_GPIO_Port, LED_HEARTBEAT_Pin);
        s_ctx.last_heartbeat_ms = Bsp_Tick_GetMs();
    }
}

static void Set_State(App_Main_FsmState s)
{
    s_ctx.state          = s;
    s_ctx.state_enter_ms = Bsp_Tick_GetMs();
    LOG_INFO_WITH_ARG("sys fsm: -> %d", (int)s);
    App_Display_MarkDirty();
    App_Display_FlushNow();
}

static void Enter_Menu(void)
{
    All_Outputs_Off();
    App_SerialDebug_SetActive(false);
    App_SerialDebug_ClearEnterRequest();
    s_ctx.combo_active        = false;
    s_ctx.combo_hold_start_ms = 0U;
    App_Menu_Enter();
    Set_State(APP_MAIN_FSM_STATE_MENU);
}

static void Idle_Timer_Reset(void)
{
    s_ctx.idle_accum_ms     = 0U;
    s_ctx.idle_last_tick_ms = Bsp_Tick_GetMs();
}

static uint32_t Idle_GetElapsedMs(void)
{
    uint32_t el = s_ctx.idle_accum_ms;
    if (!s_ctx.combo_active)
    {
        el += Bsp_Tick_ElapsedMs(s_ctx.idle_last_tick_ms);
    }
    return el;
}

static void Idle_AccumTick(void)
{
    if (s_ctx.combo_active)
    {
        return;
    }
    uint32_t now = Bsp_Tick_GetMs();
    s_ctx.idle_accum_ms += (now - s_ctx.idle_last_tick_ms);
    s_ctx.idle_last_tick_ms = now;
}

static void Build_Enabled_Zones(void)
{
    const App_Config *cfg = App_Config_Get();
    s_ctx.enabled_count   = 0U;
    for (uint32_t i = 0U; i < FM_CHANNEL_NUM; i++)
    {
        if ((cfg->channel_enable & (uint8_t)(1U << i)) != 0U)
        {
            s_ctx.enabled_zones[s_ctx.enabled_count++] = (uint8_t)i;
        }
    }
}

static void Enter_AutoRun(void)
{
    s_ctx.watering_stopped = false;
    Build_Enabled_Zones();
    if (s_ctx.enabled_count == 0U)
    {
        LOG_WARN("auto run: no zones enabled");
        Set_State(APP_MAIN_FSM_STATE_DONE);
        return;
    }
    s_ctx.enabled_idx         = 0U;
    s_ctx.auto_run_start_ms   = Bsp_Tick_GetMs();
    Fm_ValveIndex first       = (Fm_ValveIndex)s_ctx.enabled_zones[0];
    s_ctx.current_zone_1based = (uint8_t)(first + 1U);
    App_Pump_Fsm_Start(&s_ctx.pump_ctx, first);
    Set_State(APP_MAIN_FSM_STATE_AUTO_RUN);
    LOG_INFO_WITH_ARG("auto run: %u zones, start Z%u",
                      (unsigned)s_ctx.enabled_count, (unsigned)first + 1U);
}

static void Enter_Done_Stopped(void)
{
    App_Pump_Fsm_Abort(&s_ctx.pump_ctx, FM_OK);
    s_ctx.current_zone_1based = 0U;
    All_Outputs_Off();
    s_ctx.watering_stopped = true;
    Set_State(APP_MAIN_FSM_STATE_DONE);
    LOG_INFO("auto run: STOPPED by K4");
}

static void Enter_SerialDebug(bool from_idle)
{
    All_Outputs_Off();
    s_ctx.combo_active        = false;
    s_ctx.combo_hold_start_ms = 0U;
    s_ctx.serial_from_idle    = from_idle;
    App_SerialDebug_SetActive(true);
    Set_State(APP_MAIN_FSM_STATE_SERIAL_DEBUG);
    LOG_INFO_WITH_ARG("sys: serial debug (from_idle=%d)", (int)from_idle);
}

static void Enter_Menu_AfterSerial(void)
{
    All_Outputs_Off();
    App_SerialDebug_SetActive(false);
    App_SerialDebug_ClearEnterRequest();
    s_ctx.combo_active        = false;
    s_ctx.combo_hold_start_ms = 0U;
    if (s_ctx.serial_from_idle)
    {
        App_Menu_Enter();
    }
    else
    {
        App_Menu_EnterSettings();
    }
    Set_State(APP_MAIN_FSM_STATE_MENU);
}

static void Idle_Tick_Combo(void)
{
    Bsp_Key_Scan();
    bool both = Bsp_Key_IsPressed(BSP_KEY_K1) && Bsp_Key_IsPressed(BSP_KEY_K3);

    if (both)
    {
        if (!s_ctx.combo_active)
        {
            Idle_AccumTick();
            s_ctx.combo_active        = true;
            s_ctx.combo_hold_start_ms = Bsp_Tick_GetMs();
            LOG_INFO("idle: K1+K3 down, timer paused");
            App_Display_MarkDirty();
        }
        if (Bsp_Tick_ElapsedMs(s_ctx.combo_hold_start_ms) >= IDLE_COMBO_HOLD_MS)
        {
            LOG_INFO("idle: K1+K3 held -> MENU");
            s_ctx.combo_active        = false;
            s_ctx.combo_hold_start_ms = 0U;
            Enter_Menu();
        }
    }
    else if (s_ctx.combo_active)
    {
        LOG_INFO("idle: combo released, resume timer");
        s_ctx.combo_active        = false;
        s_ctx.combo_hold_start_ms = 0U;
        s_ctx.idle_last_tick_ms   = Bsp_Tick_GetMs();
        App_Display_MarkDirty();
    }
}

App_Main_FsmState App_Main_Fsm_GetState(void)
{
    return s_ctx.state;
}

uint8_t App_Main_Fsm_CurrentZone(void)
{
    return s_ctx.current_zone_1based;
}

uint32_t App_Main_Fsm_EnterMs(void)
{
    return s_ctx.state_enter_ms;
}

uint32_t App_Main_Fsm_AutoRunStartMs(void)
{
    return s_ctx.auto_run_start_ms;
}

Fm_ErrorCode App_Main_Fsm_LastError(void)
{
    return s_ctx.last_err;
}

bool App_Main_Fsm_IsWateringStopped(void)
{
    return s_ctx.watering_stopped;
}

void App_Main_Fsm_SignalError(Fm_ErrorCode err)
{
    s_ctx.last_err = err;
    All_Outputs_Off();
    Set_State(APP_MAIN_FSM_STATE_ERROR);
}

void App_Main_Fsm_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state              = APP_MAIN_FSM_STATE_LOGO;
    s_ctx.state_enter_ms     = Bsp_Tick_GetMs();
    s_ctx.last_heartbeat_ms = Bsp_Tick_GetMs();
    s_ctx.last_err          = FM_OK;
    LOG_INFO("sys fsm: init LOGO");
}

uint32_t App_Main_Fsm_GetIdleRemainMs(void)
{
    if (s_ctx.state != APP_MAIN_FSM_STATE_IDLE)
    {
        return 0U;
    }
    uint32_t el = Idle_GetElapsedMs();
    return (el >= IDLE_MS) ? 0U : (IDLE_MS - el);
}

bool App_Main_Fsm_IsIdleTimerPaused(void)
{
    return (s_ctx.state == APP_MAIN_FSM_STATE_IDLE) && s_ctx.combo_active;
}

uint8_t App_Main_Fsm_GetIdleComboPercent(void)
{
    if ((s_ctx.state != APP_MAIN_FSM_STATE_IDLE) || !s_ctx.combo_active)
    {
        return 0U;
    }
    uint32_t el = Bsp_Tick_ElapsedMs(s_ctx.combo_hold_start_ms);
    if (el >= IDLE_COMBO_HOLD_MS)
    {
        return 100U;
    }
    return (uint8_t)((el * 100U) / IDLE_COMBO_HOLD_MS);
}

uint32_t App_Main_Fsm_GetBootWaitRemainMs(void)
{
    return App_Main_Fsm_GetIdleRemainMs();
}

uint8_t App_Main_Fsm_GetBootWaitComboPercent(void)
{
    return App_Main_Fsm_GetIdleComboPercent();
}

bool App_Main_Fsm_IsBootWaitTimerPaused(void)
{
    return App_Main_Fsm_IsIdleTimerPaused();
}

void App_Main_Fsm_EnterBootWait(void)
{
    if (s_ctx.state == APP_MAIN_FSM_STATE_SERIAL_DEBUG)
    {
        Enter_Menu_AfterSerial();
    }
    else
    {
        Enter_Menu();
    }
    LOG_INFO("sys: EnterBootWait -> MENU");
}

void App_Main_Fsm_EnterManualSelect(void)
{
    Enter_Menu_AfterSerial();
    LOG_INFO("sys: EnterManualSelect -> MENU");
}

bool App_Main_Fsm_KeyInputEnabled(void)
{
    switch (s_ctx.state)
    {
        case APP_MAIN_FSM_STATE_LOGO:
        case APP_MAIN_FSM_STATE_IDLE:
        case APP_MAIN_FSM_STATE_SERIAL_DEBUG:
            return false;
        default:
            return true;
    }
}

void App_Main_Fsm_Tick(void)
{
    Heartbeat_Update((s_ctx.state == APP_MAIN_FSM_STATE_ERROR) ? 100U : 500U);

    const App_Config *cfg     = App_Config_Get();
    uint32_t          elapsed = Bsp_Tick_ElapsedMs(s_ctx.state_enter_ms);

    switch (s_ctx.state)
    {
        case APP_MAIN_FSM_STATE_LOGO:
        {
            bool     cfg_fail = (App_Config_GetSource() == APP_CONFIG_LOADED_FACTORY);
            uint32_t need_ms  = LOGO_MIN_MS + (cfg_fail ? LOGO_FAIL_EXTRA_MS : 0U);
            if (elapsed >= need_ms)
            {
                Idle_Timer_Reset();
                s_ctx.combo_active = false;
                Set_State(APP_MAIN_FSM_STATE_IDLE);
            }
            break;
        }

        case APP_MAIN_FSM_STATE_IDLE:
            Idle_AccumTick();
            Idle_Tick_Combo();
            if (s_ctx.state != APP_MAIN_FSM_STATE_IDLE)
            {
                break;
            }
            if (App_SerialDebug_ConsumeEnterRequest())
            {
                Enter_SerialDebug(true);
            }
            else if (!s_ctx.combo_active && (Idle_GetElapsedMs() >= IDLE_MS))
            {
                LOG_INFO("idle: timeout -> AUTO_RUN");
                Enter_AutoRun();
            }
            break;

        case APP_MAIN_FSM_STATE_AUTO_RUN:
        {
            uint32_t total_elapsed_s = Bsp_Tick_ElapsedMs(s_ctx.auto_run_start_ms) / 1000U;
            if (total_elapsed_s > cfg->total_timeout_s)
            {
                LOG_ERROR_WITH_ARG("auto run: TOTAL timeout (%us)", (unsigned)total_elapsed_s);
                App_Pump_Fsm_Abort(&s_ctx.pump_ctx, FM_ERR_011_TOTAL_TIMEOUT);
                App_Main_Fsm_SignalError(FM_ERR_011_TOTAL_TIMEOUT);
                break;
            }

            bool running = App_Pump_Fsm_Tick(&s_ctx.pump_ctx);
            if (!running)
            {
                App_Pump_FsmState st = App_Pump_Fsm_State(&s_ctx.pump_ctx);
                if (st == APP_PUMP_FSM_STATE_ERROR)
                {
                    App_Main_Fsm_SignalError(FM_ERR_010_CH_TIMEOUT);
                    break;
                }
                s_ctx.enabled_idx++;
                if (s_ctx.enabled_idx >= s_ctx.enabled_count)
                {
                    s_ctx.current_zone_1based = 0U;
                    s_ctx.watering_stopped    = false;
                    All_Outputs_Off();
                    Set_State(APP_MAIN_FSM_STATE_DONE);
                    LOG_INFO_WITH_ARG("auto run: DONE (%lu s)", (unsigned long)total_elapsed_s);
                }
                else
                {
                    Fm_ValveIndex v           = (Fm_ValveIndex)s_ctx.enabled_zones[s_ctx.enabled_idx];
                    s_ctx.current_zone_1based = (uint8_t)(v + 1U);
                    App_Pump_Fsm_Start(&s_ctx.pump_ctx, v);
                }
            }
            break;
        }

        case APP_MAIN_FSM_STATE_MENU:
            App_Menu_Tick();
            if (App_Menu_ExitRequested())
            {
                App_Menu_Action act = App_Menu_GetExitAction();
                App_Menu_ClearExit();
                if (act == APP_MENU_ACTION_START_WATER)
                {
                    Enter_AutoRun();
                }
                else if (act == APP_MENU_ACTION_ENTER_SERIAL)
                {
                    Enter_SerialDebug(false);
                }
            }
            break;

        case APP_MAIN_FSM_STATE_SERIAL_DEBUG:
        case APP_MAIN_FSM_STATE_DONE:
        case APP_MAIN_FSM_STATE_ERROR:
        default:
            break;
    }
}

void App_Main_Fsm_OnEvent(const App_Event *e)
{
    if (e == 0)
    {
        return;
    }

    switch (s_ctx.state)
    {
        case APP_MAIN_FSM_STATE_AUTO_RUN:
            if ((e->id == APP_EVENT_KEY_K4_SHORT) || (e->id == APP_EVENT_KEY_K4_LONG) ||
                (e->id == APP_EVENT_KEY_K4_HOLD))
            {
                Enter_Done_Stopped();
            }
            break;

        case APP_MAIN_FSM_STATE_DONE:
        case APP_MAIN_FSM_STATE_ERROR:
            if (e->id == APP_EVENT_KEY_K4_SHORT)
            {
                Enter_Menu();
            }
            break;

        case APP_MAIN_FSM_STATE_MENU:
            App_Menu_OnEvent(e);
            break;

        default:
            break;
    }
}
