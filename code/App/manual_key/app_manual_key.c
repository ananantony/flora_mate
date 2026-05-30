/*
 * @File         : \code\App\manual_key\app_manual_key.c
 * @Description  : 手动按键控制：CH1 水泵电源 / CH2~CH5 阀 / CH6 备用 / PWM
 */
#include "app_manual_key.h"

#include "app_display.h"
#include "app_log.h"
#include "bsp_pump_pwm.h"
#include "bsp_valve.h"
#include "floramate_types.h"

#include <string.h>

#define MANUAL_KEY_ITEM_COUNT (7U)

typedef struct
{
    uint8_t cursor;
    uint8_t pump_duty;
    bool    item_active;        /**< true：已按 K3 进入当前项，K1/K2 才修改该项 */
    bool    exit_to_boot_wait;
} App_ManualKey_Ctx;

static App_ManualKey_Ctx s_ctx;

static Bsp_Valve_Channel Relay_For_Cursor(uint8_t cur)
{
    switch (cur)
    {
        case 0U:
            return BSP_VALVE_PUMP_EN;
        case 1U:
            return BSP_VALVE_Z1;
        case 2U:
            return BSP_VALVE_Z2;
        case 3U:
            return BSP_VALVE_Z3;
        case 4U:
            return BSP_VALVE_Z4;
        case 5U:
            return BSP_VALVE_RSV;
        default:
            return BSP_VALVE_RSV;
    }
}

static void All_Outputs_Off(void)
{
    Bsp_Pump_Pwm_Stop();
    for (uint32_t i = 0U; i < (uint32_t)BSP_VALVE_CHANNEL_NUM; i++)
    {
        Bsp_Valve_DebugSet((Bsp_Valve_Channel)i, false);
    }
    s_ctx.pump_duty = 0U;
}

static void Pump_Off(void)
{
    Bsp_Pump_Pwm_Stop();
    Bsp_Valve_DebugSet(BSP_VALVE_PUMP_EN, false);
    s_ctx.pump_duty = 0U;
    App_Display_MarkDirty();
}

void App_ManualKey_Init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
}

void App_ManualKey_Enter(void)
{
    s_ctx.cursor            = 0U;
    s_ctx.pump_duty         = 0U;
    s_ctx.item_active       = false;
    s_ctx.exit_to_boot_wait = false;
    All_Outputs_Off();
    LOG_INFO("manual key: enter");
    App_Display_MarkDirty();
}

void App_ManualKey_Exit(void)
{
    All_Outputs_Off();
    s_ctx.item_active       = false;
    s_ctx.exit_to_boot_wait = false;
}

uint8_t App_ManualKey_GetCursor(void)
{
    return s_ctx.cursor;
}

uint8_t App_ManualKey_GetPumpDuty(void)
{
    return s_ctx.pump_duty;
}

bool App_ManualKey_IsItemActive(void)
{
    return s_ctx.item_active;
}

bool App_ManualKey_IsPumpEditing(void)
{
    return s_ctx.item_active && (s_ctx.cursor == 6U);
}

bool App_ManualKey_IsRelayOn(uint8_t idx)
{
    if (idx >= 6U)
    {
        return false;
    }
    return Bsp_Valve_Get(Relay_For_Cursor(idx));
}

bool App_ManualKey_ExitToBootWaitRequested(void)
{
    return s_ctx.exit_to_boot_wait;
}

void App_ManualKey_ClearExitRequest(void)
{
    s_ctx.exit_to_boot_wait = false;
}

static void Set_Relay_Cursor(bool on)
{
    if (s_ctx.cursor >= 6U)
    {
        return;
    }
    Bsp_Valve_Channel ch  = Relay_For_Cursor(s_ctx.cursor);
    Bsp_Valve_DebugSet(ch, on);
    LOG_INFO_WITH_ARG("manual key: ch%u -> %u", (unsigned)(s_ctx.cursor + 1U), on ? 1U : 0U);
    App_Display_MarkDirty();
}

static void Cursor_MoveDown(void)
{
    s_ctx.cursor = (uint8_t)((s_ctx.cursor + 1U) % MANUAL_KEY_ITEM_COUNT);
    App_Display_MarkDirty();
}

static void Cursor_MoveUp(void)
{
    s_ctx.cursor = (s_ctx.cursor == 0U) ? (uint8_t)(MANUAL_KEY_ITEM_COUNT - 1U) : (uint8_t)(s_ctx.cursor - 1U);
    App_Display_MarkDirty();
}

static void Adjust_Pump(int8_t delta)
{
    if (!s_ctx.item_active || (s_ctx.cursor != 6U))
    {
        return;
    }

    int16_t v = (int16_t)s_ctx.pump_duty + delta;
    if (v < 0)
    {
        v = 0;
    }
    if (v > FM_PUMP_DUTY_MAX_PERCENT)
    {
        v = FM_PUMP_DUTY_MAX_PERCENT;
    }
    s_ctx.pump_duty = (uint8_t)v;

    if (s_ctx.pump_duty == 0U)
    {
        Pump_Off();
    }
    else
    {
        if (!Bsp_Valve_Get(BSP_VALVE_PUMP_EN))
        {
            Bsp_Valve_DebugSet(BSP_VALVE_PUMP_EN, true);
        }
        (void)Bsp_Pump_Pwm_SetDutyPercent(s_ctx.pump_duty);
    }
    LOG_INFO_WITH_ARG("manual key: pwm %u%%", (unsigned)s_ctx.pump_duty);
    App_Display_MarkDirty();
}

void App_ManualKey_OnEvent(const App_Event *e)
{
    if (e == NULL)
    {
        return;
    }

    switch (e->id)
    {
        case APP_EVENT_KEY_K1_SHORT:
            if (!s_ctx.item_active)
            {
                Cursor_MoveDown();
            }
            else if (s_ctx.cursor == 6U)
            {
                Adjust_Pump(5);
            }
            else
            {
                Set_Relay_Cursor(true);
            }
            break;

        case APP_EVENT_KEY_K2_SHORT:
            if (!s_ctx.item_active)
            {
                Cursor_MoveUp();
            }
            else if (s_ctx.cursor == 6U)
            {
                Adjust_Pump(-5);
            }
            else
            {
                Set_Relay_Cursor(false);
            }
            break;

        case APP_EVENT_KEY_K3_SHORT:
            if (!s_ctx.item_active)
            {
                s_ctx.item_active = true;
                LOG_INFO_WITH_ARG("manual key: enter item %u", (unsigned)s_ctx.cursor);
                App_Display_MarkDirty();
            }
            break;

        case APP_EVENT_KEY_K4_SHORT:
            if (s_ctx.item_active)
            {
                if (s_ctx.cursor == 6U)
                {
                    Pump_Off();
                }
                s_ctx.item_active = false;
                LOG_INFO("manual key: leave item");
            }
            else
            {
                All_Outputs_Off();
                s_ctx.exit_to_boot_wait = true;
                LOG_INFO("manual key: exit -> manual main");
            }
            App_Display_MarkDirty();
            break;

        default:
            break;
    }
}
