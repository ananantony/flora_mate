﻿/*
 * @File         : \code\App\serial_debug\app_serial_debug.c
 * @Description  : USART1 串口调试命令解析
 */
#include "app_serial_debug.h"
#include "app_serial_debug_config.h"

#include "app_config.h"
#include "app_display.h"
#include "app_main_fsm.h"
#include "bsp_key.h"
#include "bsp_pump_pwm.h"
#include "bsp_valve.h"
#include "bsp_tick.h"
#include "bsp_usart_log.h"
#include "floramate_types.h"
#include "stm32f4xx_hal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SERIAL_DEBUG_HB_PERIOD_MS (1000U)

static bool                  s_active;
static bool                  s_enter_requested;
static uint32_t              s_last_hb_ms;
static char                  s_line[BSP_USART_LOG_LINE_MAX];
static char                  s_out[128];
static char                  s_ui_cmd[40];
static char                  s_ui_result[40];
static App_SerialDebug_UiState s_ui_state = APP_SERIAL_DEBUG_UI_IDLE;
static bool                    s_key_test_active;
static uint8_t                 s_key_test_last_mask;
static bool                    s_k4_exit_tracking;
static uint32_t                s_k4_exit_hold_start_ms;

static void KeyTest_Stop(void)
{
    s_key_test_active    = false;
    s_key_test_last_mask = 0xFFU;
}

static void Ui_SetIdle(void)
{
    KeyTest_Stop();
    s_ui_cmd[0]     = '\0';
    s_ui_result[0]  = '\0';
    s_ui_state      = APP_SERIAL_DEBUG_UI_IDLE;
    App_Display_MarkDirty();
}

static void Ui_SetRunning(const char *cmd)
{
    KeyTest_Stop();
    if (cmd != NULL)
    {
        (void)snprintf(s_ui_cmd, sizeof(s_ui_cmd), "%s", cmd);
    }
    s_ui_result[0] = '\0';
    s_ui_state     = APP_SERIAL_DEBUG_UI_RUNNING;
    App_Display_MarkDirty();
}

static void Ui_SetResult(bool ok, const char *msg)
{
    if (msg != NULL)
    {
        (void)snprintf(s_ui_result, sizeof(s_ui_result), "%s", msg);
    }
    s_ui_state = ok ? APP_SERIAL_DEBUG_UI_OK : APP_SERIAL_DEBUG_UI_ERR;
    App_Display_MarkDirty();
}

static void Uart_Print(const char *s)
{
    (void)Bsp_Usart_Log_WriteString(s);
}

static void Uart_Printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    (void)vsnprintf(s_out, sizeof(s_out), fmt, args);
    va_end(args);
    Uart_Print(s_out);
}

static char *Trim(char *s)
{
    while ((*s != '\0') && isspace((unsigned char)*s))
    {
        s++;
    }
    char *end = s + strlen(s);
    while ((end > s) && isspace((unsigned char)end[-1]))
    {
        end--;
    }
    *end = '\0';
    return s;
}

static void Lowercase(char *s)
{
    while (*s != '\0')
    {
        *s = (char)tolower((unsigned char)*s);
        s++;
    }
}

static bool Parse_U8_0_100(const char *s, uint8_t *out)
{
    char *end = NULL;
    long  v   = strtol(s, &end, 10);
    if ((end == s) || (*Trim(end) != '\0') || (v < 0) || (v > 100))
    {
        return false;
    }
    *out = (uint8_t)v;
    return true;
}

/** 用户区号 1~6 → 模块 CH1~CH6：1=水泵总电源，2~5=阀，6=备用 */
static bool Parse_Relay_User_1_6(const char *s, Bsp_Valve_Channel *out, uint8_t *out_user_ch)
{
    static const Bsp_Valve_Channel s_map[6] = {
        BSP_VALVE_PUMP_EN,
        BSP_VALVE_Z1,
        BSP_VALVE_Z2,
        BSP_VALVE_Z3,
        BSP_VALVE_Z4,
        BSP_VALVE_RSV,
    };
    char *end = NULL;
    long  v   = strtol(s, &end, 10);
    if ((end == s) || (*Trim(end) != '\0') || (v < 1) || (v > 6))
    {
        return false;
    }
    if (out_user_ch != NULL)
    {
        *out_user_ch = (uint8_t)v;
    }
    *out = s_map[(uint32_t)v - 1U];
    return true;
}

static void Print_Relay_State(const char *tag)
{
    Uart_Printf("[I] %s sw ch1=%u v2=%u v3=%u v4=%u v5=%u ch6=%u\r\n", tag,
                Bsp_Valve_Get(BSP_VALVE_PUMP_EN) ? 1U : 0U, Bsp_Valve_Get(BSP_VALVE_Z1) ? 1U : 0U,
                Bsp_Valve_Get(BSP_VALVE_Z2) ? 1U : 0U, Bsp_Valve_Get(BSP_VALVE_Z3) ? 1U : 0U,
                Bsp_Valve_Get(BSP_VALVE_Z4) ? 1U : 0U, Bsp_Valve_Get(BSP_VALVE_RSV) ? 1U : 0U);
    Uart_Printf("[I] %s gpio ch1=%u v2=%u v3=%u v4=%u v5=%u ch6=%u odr=0x%04lX\r\n", tag,
                Bsp_Valve_GetGpio(BSP_VALVE_PUMP_EN) ? 1U : 0U, Bsp_Valve_GetGpio(BSP_VALVE_Z1) ? 1U : 0U,
                Bsp_Valve_GetGpio(BSP_VALVE_Z2) ? 1U : 0U, Bsp_Valve_GetGpio(BSP_VALVE_Z3) ? 1U : 0U,
                Bsp_Valve_GetGpio(BSP_VALVE_Z4) ? 1U : 0U, Bsp_Valve_GetGpio(BSP_VALVE_RSV) ? 1U : 0U,
                (unsigned long)Bsp_Valve_GetGpioBOdrMask());
}

static const char *Relay_User_Name(uint8_t user_ch)
{
    switch (user_ch)
    {
        case 1:
            return "Pump";
        case 2:
            return "V1";
        case 3:
            return "V2";
        case 4:
            return "V3";
        case 5:
            return "V4";
        case 6:
            return "Rsv";
        default:
            return "?";
    }
}

static uint8_t KeyTest_ReadMask(void)
{
    uint8_t mask = 0U;
    if (Bsp_Key_IsPressed(BSP_KEY_K1))
    {
        mask |= 0x01U;
    }
    if (Bsp_Key_IsPressed(BSP_KEY_K2))
    {
        mask |= 0x02U;
    }
    if (Bsp_Key_IsPressed(BSP_KEY_K3))
    {
        mask |= 0x04U;
    }
    if (Bsp_Key_IsPressed(BSP_KEY_K4))
    {
        mask |= 0x08U;
    }
    return mask;
}

static void KeyTest_PrintMask(uint8_t mask)
{
    Uart_Printf("[I] keys k1=%u k2=%u k3=%u k4=%u\r\n", (unsigned)((mask & 0x01U) ? 1U : 0U),
                (unsigned)((mask & 0x02U) ? 1U : 0U), (unsigned)((mask & 0x04U) ? 1U : 0U),
                (unsigned)((mask & 0x08U) ? 1U : 0U));
}

static void KeyTest_Tick(void)
{
    Bsp_Key_Scan();
    uint8_t mask = KeyTest_ReadMask();
    if (mask != s_key_test_last_mask)
    {
        s_key_test_last_mask = mask;
        KeyTest_PrintMask(mask);
        App_Display_MarkDirty();
    }
}

static void Cmd_KeyTest(void)
{
    s_key_test_active    = true;
    s_key_test_last_mask = 0xFFU;
    (void)snprintf(s_ui_cmd, sizeof(s_ui_cmd), "key test");
    s_ui_result[0] = '\0';
    s_ui_state = APP_SERIAL_DEBUG_UI_KEY_TEST;
    App_Display_MarkDirty();
    App_Display_FlushNow();

    Bsp_Key_Scan();
    s_key_test_last_mask = KeyTest_ReadMask();
    Uart_Print("[I] ok: key test (oled shows K1-K4)\r\n");
    KeyTest_PrintMask(s_key_test_last_mask);
}

static void Cmd_KeyTestExit(void)
{
    if (!s_key_test_active)
    {
        Uart_Print("[E] err: not in key test\r\n");
        Ui_SetResult(false, "not key test");
        return;
    }

    KeyTest_Stop();
    (void)snprintf(s_ui_cmd, sizeof(s_ui_cmd), "key test off");
    Ui_SetResult(true, "key test off");
    App_Display_FlushNow();
    Uart_Print("[I] ok: key test off\r\n");
}

static void Cmd_ExitManual(void)
{
    if (!s_active)
    {
        Uart_Print("[E] err: not in manual serial\r\n");
        Ui_SetResult(false, "not in debug");
        return;
    }
    KeyTest_Stop();
    s_active          = false;
    s_enter_requested = false;
    Ui_SetIdle();
    App_Main_Fsm_EnterBootWait();
    Uart_Print("[I] ok: exit -> boot wait (auto if timeout)\r\n");
}

static void Cmd_BackManualSelect(void)
{
    if (!s_active)
    {
        Uart_Print("[E] err: not in manual serial\r\n");
        Ui_SetResult(false, "not in debug");
        return;
    }
    KeyTest_Stop();
    s_active          = false;
    s_enter_requested = false;
    Ui_SetIdle();
    App_Main_Fsm_EnterManualSelect();
    Uart_Print("[I] ok: back -> manual select\r\n");
}

static void SerialDebug_Tick_K4Exit(void)
{
    if (!s_active || s_key_test_active)
    {
        s_k4_exit_tracking = false;
        return;
    }

    Bsp_Key_Scan();
    if (Bsp_Key_IsPressed(BSP_KEY_K4))
    {
        if (!s_k4_exit_tracking)
        {
            s_k4_exit_tracking      = true;
            s_k4_exit_hold_start_ms = Bsp_Tick_GetMs();
        }
        else if (Bsp_Tick_ElapsedMs(s_k4_exit_hold_start_ms) >= APP_SERIAL_DEBUG_K4_EXIT_HOLD_MS)
        {
            s_k4_exit_tracking = false;
            Cmd_BackManualSelect();
        }
    }
    else
    {
        s_k4_exit_tracking = false;
    }
}

static void Cmd_Help(void)
{
    Uart_Print("[I] help: debug off|exit|quit|K4L2s  help|?  status\r\n");
    Uart_Print("[I] help: key test | key test off  pump  valve  cfg  stop\r\n");
    Uart_Print("[I] help: pump <0-100>|off  valve open <1-6>  valve close[ <1-6>]\r\n");
    Uart_Print("[I] help: cfg dump|get|set|save|load|reset  stop  run(reserved)\r\n");
}

static void Cmd_Status(void)
{
    App_Main_FsmState st = App_Main_Fsm_GetState();
    Uart_Printf("[I] status state=%d tick_ms=%lu debug=%u\r\n", (int)st, (unsigned long)Bsp_Tick_GetMs(),
                s_active ? 1U : 0U);
    Print_Relay_State("status");
    Uart_Printf("[I] pump duty=%u%%\r\n", (unsigned)Bsp_Pump_Pwm_GetDutyPercent());
    Uart_Printf("[I] uart rx_drop=%lu rx_err=%lu\r\n", (unsigned long)Bsp_Usart_Log_GetRxDropCount(),
                (unsigned long)Bsp_Usart_Log_GetRxErrorCount());
}

static void Cmd_Stop(void)
{
    Bsp_Pump_Pwm_Stop();
    for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
    {
        Bsp_Valve_DebugSet((Bsp_Valve_Channel)i, false);
    }
    Uart_Print("[I] ok: all outputs off\r\n");
    Ui_SetResult(true, "stop: all off");
}

static void Cmd_Pump(char *args)
{
    args = Trim(args);
    if (strcmp(args, "off") == 0)
    {
        Bsp_Pump_Pwm_Stop();
        (void)Bsp_Valve_Set(BSP_VALVE_PUMP_EN, false);
        Uart_Print("[I] ok: pump off\r\n");
        Ui_SetResult(true, "pump off");
        return;
    }

    uint8_t duty = 0U;
    if (!Parse_U8_0_100(args, &duty))
    {
        Uart_Print("[E] err: bad args\r\n");
        Ui_SetResult(false, "pump: bad args");
        return;
    }
    if (duty == 0U)
    {
        Bsp_Pump_Pwm_Stop();
        Uart_Print("[I] ok: pump 0%\r\n");
        Ui_SetResult(true, "pump 0%");
        return;
    }

    Fm_ErrorCode err = Bsp_Valve_Set(BSP_VALVE_PUMP_EN, true);
    if (err != FM_OK)
    {
        Uart_Print("[E] err: interlock\r\n");
        Ui_SetResult(false, "pump: need valve");
        return;
    }
    err = Bsp_Pump_Pwm_SetDutyPercent(duty);
    if (err != FM_OK)
    {
        Uart_Printf("[E] err: pump 0x%02X\r\n", (unsigned)err);
        Ui_SetResult(false, "pump fail");
        return;
    }
    Uart_Printf("[I] ok: pump %u%%\r\n", (unsigned)duty);
    Ui_SetResult(true, "pump ok");
}

static void Cmd_Valve(char *args)
{
    args = Trim(args);
    if (strncmp(args, "open ", 5U) == 0)
    {
        Bsp_Valve_Channel ch;
        uint8_t           user_ch = 0U;
        char             *num   = Trim(args + 5);
        if (!Parse_Relay_User_1_6(num, &ch, &user_ch))
        {
            Uart_Print("[E] err: bad args (1-6)\r\n");
            Ui_SetResult(false, "valve: bad ch");
            return;
        }
        Bsp_Valve_DebugSet(ch, true);
        Uart_Printf("[I] ok: valve open user=%u bsp=%u (%s)\r\n", (unsigned)user_ch, (unsigned)ch,
                    Relay_User_Name(user_ch));
        Print_Relay_State("after open");
        Ui_SetResult(true, "relay on");
        return;
    }

    if (strncmp(args, "close", 5U) == 0)
    {
        char *which = Trim(args + 5);
        if (*which == '\0')
        {
            for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
            {
                Bsp_Valve_DebugSet((Bsp_Valve_Channel)i, false);
            }
            Uart_Print("[I] ok: all relays off\r\n");
            Ui_SetResult(true, "all relays off");
            return;
        }

        Bsp_Valve_Channel ch;
        uint8_t           user_ch = 0U;
        if (!Parse_Relay_User_1_6(which, &ch, &user_ch))
        {
            Uart_Print("[E] err: bad args (1-6)\r\n");
            Ui_SetResult(false, "valve: bad ch");
            return;
        }
        Bsp_Valve_DebugSet(ch, false);
        Uart_Printf("[I] ok: valve close user=%u bsp=%u (%s)\r\n", (unsigned)user_ch, (unsigned)ch,
                    Relay_User_Name(user_ch));
        Print_Relay_State("after close");
        Ui_SetResult(true, "relay off");
        return;
    }

    Uart_Print("[E] err: bad args\r\n");
    Ui_SetResult(false, "valve: usage");
}

static void Cmd_Cfg(char *args)
{
    args = Trim(args);
    if (strcmp(args, "dump") == 0)
    {
        App_Config_Dump();
        Uart_Print("[I] ok: cfg dump\r\n");
        Ui_SetResult(true, "cfg dump");
        return;
    }
    if (strcmp(args, "save") == 0)
    {
        bool ok = (App_Config_Save() == FM_OK);
        Uart_Print(ok ? "[I] ok: cfg saved\r\n" : "[E] err: cfg save\r\n");
        Ui_SetResult(ok, ok ? "cfg saved" : "save fail");
        return;
    }
    if (strcmp(args, "load") == 0)
    {
        bool ok = (App_Config_Reload() == FM_OK);
        Uart_Print(ok ? "[I] ok: cfg reload\r\n" : "[E] err: cfg reload\r\n");
        Ui_SetResult(ok, ok ? "cfg reload" : "load fail");
        return;
    }
    if (strcmp(args, "reset") == 0)
    {
        bool ok = (App_Config_FactoryReset() == FM_OK);
        Uart_Print(ok ? "[I] ok: factory reset\r\n" : "[E] err: factory reset\r\n");
        Ui_SetResult(ok, ok ? "factory rst" : "rst fail");
        return;
    }
    if (strncmp(args, "get ", 4U) == 0)
    {
        int32_t v    = 0;
        char   *name = Trim(args + 4);
        if (App_Config_GetField(name, &v) == FM_OK)
        {
            Uart_Printf("[I] %s=%ld\r\n", name, (long)v);
            Ui_SetResult(true, "cfg get ok");
        }
        else
        {
            Uart_Print("[E] err: unknown field\r\n");
            Ui_SetResult(false, "unknown field");
        }
        return;
    }
    if (strncmp(args, "set ", 4U) == 0)
    {
        char *name  = Trim(args + 4);
        char *value = name;
        while ((*value != '\0') && !isspace((unsigned char)*value))
        {
            value++;
        }
        if (*value == '\0')
        {
            Uart_Print("[E] err: bad args\r\n");
            Ui_SetResult(false, "cfg set usage");
            return;
        }
        *value = '\0';
        value++;
        int32_t v = (int32_t)strtol(Trim(value), NULL, 10);
        bool    ok = (App_Config_SetField(name, v) == FM_OK);
        Uart_Print(ok ? "[I] ok: cfg set\r\n" : "[E] err: unknown field\r\n");
        Ui_SetResult(ok, ok ? "cfg set ok" : "set fail");
        return;
    }
    Uart_Print("[E] err: bad args\r\n");
    Ui_SetResult(false, "cfg usage");
}

static void Process_Command(char *line)
{
    char *cmd = Trim(line);
    Lowercase(cmd);
    if (*cmd == '\0')
    {
        return;
    }

    if (s_active)
    {
        Uart_Printf("[I] rx line: [%s]\r\n", cmd);
    }

    if ((strcmp(cmd, "debug") == 0) || (strcmp(cmd, "debug on") == 0) || (strcmp(cmd, "dbg") == 0))
    {
        if (s_active)
        {
            Uart_Print("[I] ok: already in debug\r\n");
            Ui_SetResult(true, "already debug");
        }
        else
        {
            s_enter_requested = true;
            Uart_Print("[I] ok: enter debug\r\n");
            Ui_SetResult(true, "enter debug");
        }
        return;
    }

    if (!s_active)
    {
        Uart_Print("[E] err: send debug first\r\n");
        Ui_SetResult(false, "need debug");
        return;
    }

    if ((strcmp(cmd, "debug off") == 0) || (strcmp(cmd, "exit") == 0) || (strcmp(cmd, "quit") == 0) ||
        (strcmp(cmd, "manual exit") == 0))
    {
        Cmd_ExitManual();
        return;
    }

    if ((strcmp(cmd, "key test") == 0) || (strcmp(cmd, "keytest") == 0))
    {
        Cmd_KeyTest();
        return;
    }
    if ((strcmp(cmd, "key test off") == 0) || (strcmp(cmd, "key test exit") == 0) ||
        (strcmp(cmd, "keytest off") == 0))
    {
        Cmd_KeyTestExit();
        return;
    }

    Ui_SetRunning(cmd);

    if ((strcmp(cmd, "help") == 0) || (strcmp(cmd, "?") == 0))
    {
        Cmd_Help();
        Ui_SetResult(true, "help sent");
    }
    else if (strcmp(cmd, "ver") == 0)
    {
        Uart_Printf("[I] ver=%s build=%s\r\n", FM_FIRMWARE_VERSION_STR, FM_BUILD_DATE_STR);
        Ui_SetResult(true, "ver ok");
    }
    else if (strcmp(cmd, "status") == 0)
    {
        Cmd_Status();
        Ui_SetResult(true, "status ok");
    }
    else if (strncmp(cmd, "pump ", 5U) == 0)
    {
        Cmd_Pump(cmd + 5);
    }
    else if (strncmp(cmd, "valve ", 6U) == 0)
    {
        Cmd_Valve(cmd + 6);
    }
    else if (strncmp(cmd, "cfg ", 4U) == 0)
    {
        Cmd_Cfg(cmd + 4);
    }
    else if (strcmp(cmd, "stop") == 0)
    {
        Cmd_Stop();
    }
    else if (strcmp(cmd, "run") == 0)
    {
        Uart_Print("[E] err: not implemented\r\n");
        Ui_SetResult(false, "run N/A");
    }
    else if (strcmp(cmd, "reset") == 0)
    {
        Uart_Print("[I] reset...\r\n");
        Ui_SetResult(true, "resetting");
        NVIC_SystemReset();
    }
    else
    {
        Uart_Print("[E] err: unknown\r\n");
        Ui_SetResult(false, "unknown");
    }
}

void App_SerialDebug_Init(void)
{
    s_active          = false;
    s_enter_requested = false;
    s_last_hb_ms      = Bsp_Tick_GetMs();
    s_key_test_active = false;
    Ui_SetIdle();

#if APP_SERIAL_DEBUG_AUTO_ENTER_ON_BOOT
    s_active          = true;
    s_enter_requested = true;
    Uart_Print("[I] boot: APP_SERIAL_DEBUG_AUTO_ENTER_ON_BOOT=1\r\n");
#else
    Uart_Printf("[I] boot: %us window: debug=UART | K1+K2 2.5s=manual\r\n",
                (unsigned)(APP_SERIAL_DEBUG_BOOT_WAIT_MS / 1000U));
#endif
}

void App_SerialDebug_Tick(void)
{
    if (s_active && s_key_test_active)
    {
        KeyTest_Tick();
    }
    else if (s_active)
    {
        SerialDebug_Tick_K4Exit();
    }

    /* 仅在等待窗（未进入调试）打印心跳 */
    if (!s_active)
    {
        if (Bsp_Tick_ElapsedMs(s_last_hb_ms) >= SERIAL_DEBUG_HB_PERIOD_MS)
        {
            s_last_hb_ms = Bsp_Tick_GetMs();
            uint32_t remain = App_SerialDebug_GetBootWaitRemainMs();
            if (remain > 0U)
            {
                Uart_Printf("[I] hb uptime_ms=%lu wait_remain_ms=%lu\r\n", (unsigned long)s_last_hb_ms,
                            (unsigned long)remain);
            }
        }
    }

    size_t line_len = 0U;
    for (uint32_t n = 0U; n < 4U; n++)
    {
        if (!Bsp_Usart_Log_TryGetLine(s_line, sizeof(s_line), &line_len))
        {
            break;
        }
        Process_Command(s_line);
    }
}

void App_SerialDebug_SetActive(bool active)
{
    s_active = active;
    if (active)
    {
        Ui_SetIdle();
        Uart_Print("[I] serial debug active (no hb)\r\n");
        Cmd_Help();
        App_Display_FlushNow();
    }
    else
    {
        Ui_SetIdle();
        App_Display_FlushNow();
    }
}

bool App_SerialDebug_IsActive(void)
{
    return s_active;
}

bool App_SerialDebug_ConsumeEnterRequest(void)
{
    bool requested    = s_enter_requested;
    s_enter_requested = false;
    return requested;
}

void App_SerialDebug_ClearEnterRequest(void)
{
    s_enter_requested = false;
}

void App_SerialDebug_GetUi(char *cmd_line, size_t cmd_cap, char *result_line, size_t result_cap,
                           App_SerialDebug_UiState *out_state)
{
    if (cmd_line != NULL && cmd_cap > 0U)
    {
        (void)snprintf(cmd_line, cmd_cap, "%s", s_ui_cmd);
    }
    if (result_line != NULL && result_cap > 0U)
    {
        (void)snprintf(result_line, result_cap, "%s", s_ui_result);
    }
    if (out_state != NULL)
    {
        *out_state = s_ui_state;
    }
}

uint32_t App_SerialDebug_GetBootWaitRemainMs(void)
{
    return App_Main_Fsm_GetBootWaitRemainMs();
}
