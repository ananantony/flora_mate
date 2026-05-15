/**
 * @file    app_main_fsm.c
 * @brief   系统主状态机实现
 */
#include "app_main_fsm.h"
#include "app_pump_fsm.h"
#include "app_config.h"
#include "app_log.h"
#include "app_event.h"
#include "app_menu.h"
#include "app_display.h"
#include "bsp_relay.h"
#include "bsp_pump_pwm.h"
#include "bsp_tick.h"
#include "bsp_eeprom.h"
#include "bsp_oled.h"

#include "main.h"
#include "stm32f4xx_hal.h"

#define BOOT_DELAY_MS               (500U)
#define SELFTEST_DONE_DELAY_MS      (300U)

typedef struct {
    App_Main_FsmState state;
    uint32_t          state_enter_ms;
    uint32_t          auto_run_start_ms;
    uint8_t           current_zone_1based;   /* 1..4 = 当前执行第几路；0 = 未执行 */
    App_Pump_FsmCtx   pump_ctx;
    uint8_t           enabled_zones[FM_CHANNEL_NUM];
    uint8_t           enabled_count;
    uint8_t           enabled_idx;
    Fm_ErrorCode      last_err;
    bool              entered_menu_from_idle;
    uint32_t          last_heartbeat_ms;
} App_Main_Fsm_Ctx;

static App_Main_Fsm_Ctx s_ctx;

static void Heartbeat_Update(uint32_t period_ms) {
    if (Bsp_Tick_ElapsedMs(s_ctx.last_heartbeat_ms) >= period_ms) {
        HAL_GPIO_TogglePin(LED_HEARTBEAT_PC13_GPIO_Port, LED_HEARTBEAT_PC13_Pin);
        s_ctx.last_heartbeat_ms = Bsp_Tick_GetMs();
    }
}

static void Set_State(App_Main_FsmState s) {
    s_ctx.state          = s;
    s_ctx.state_enter_ms = Bsp_Tick_GetMs();
    LOGI("sys fsm: -> %d", (int)s);
}

App_Main_FsmState App_Main_Fsm_GetState(void)        { return s_ctx.state; }
uint8_t           App_Main_Fsm_CurrentZone(void)     { return s_ctx.current_zone_1based; }
uint32_t          App_Main_Fsm_EnterMs(void)         { return s_ctx.state_enter_ms; }
uint32_t          App_Main_Fsm_AutoRunStartMs(void)  { return s_ctx.auto_run_start_ms; }
Fm_ErrorCode      App_Main_Fsm_LastError(void)       { return s_ctx.last_err; }

void App_Main_Fsm_SignalError(Fm_ErrorCode err) {
    s_ctx.last_err = err;
    Set_State(APP_MAIN_FSM_STATE_ERROR);
}

void App_Main_Fsm_Init(void) {
    s_ctx.state                  = APP_MAIN_FSM_STATE_BOOT;
    s_ctx.state_enter_ms         = Bsp_Tick_GetMs();
    s_ctx.auto_run_start_ms      = 0U;
    s_ctx.current_zone_1based    = 0U;
    s_ctx.enabled_count          = 0U;
    s_ctx.enabled_idx            = 0U;
    s_ctx.last_err               = FM_OK;
    s_ctx.entered_menu_from_idle = false;
    s_ctx.last_heartbeat_ms      = Bsp_Tick_GetMs();
}

/* 从 channel_enable 计算使能的通道索引数组 */
static void Build_Enabled_Zones(void) {
    const App_Config *cfg = App_Config_Get();
    s_ctx.enabled_count = 0U;
    for (uint32_t i = 0U; i < FM_CHANNEL_NUM; i++) {
        if ((cfg->channel_enable & (uint8_t)(1U << i)) != 0U) {
            s_ctx.enabled_zones[s_ctx.enabled_count++] = (uint8_t)i;
        }
    }
}

static void Enter_AutoRun(void) {
    Build_Enabled_Zones();
    if (s_ctx.enabled_count == 0U) {
        LOGW("auto run: no zones enabled");
        Set_State(APP_MAIN_FSM_STATE_DONE);
        return;
    }
    s_ctx.enabled_idx       = 0U;
    s_ctx.auto_run_start_ms = Bsp_Tick_GetMs();
    Fm_ValveIndex first = (Fm_ValveIndex)s_ctx.enabled_zones[0];
    s_ctx.current_zone_1based = (uint8_t)(first + 1U);
    App_Pump_Fsm_Start(&s_ctx.pump_ctx, first);
    Set_State(APP_MAIN_FSM_STATE_AUTO_RUN);
    LOGI("auto run: total %u zones, starting Z%u",
         s_ctx.enabled_count, (unsigned)first + 1U);
}

void App_Main_Fsm_Tick(void) {
    /* 心跳 LED：正常态 500ms、错误态 100ms */
    Heartbeat_Update((s_ctx.state == APP_MAIN_FSM_STATE_ERROR) ? 100U : 500U);

    const App_Config *cfg = App_Config_Get();
    uint32_t elapsed = Bsp_Tick_ElapsedMs(s_ctx.state_enter_ms);

    switch (s_ctx.state) {

    case APP_MAIN_FSM_STATE_BOOT:
        if (elapsed >= BOOT_DELAY_MS) {
            Set_State(APP_MAIN_FSM_STATE_SELFTEST);
            LOGI("selftest: relay click test, pulse=%ums", cfg->selftest_pulse_ms);
            Bsp_Relay_Selftest((uint16_t)cfg->selftest_pulse_ms);
            LOGI("selftest: done; eeprom=%s, oled=%s",
                 Bsp_Eeprom_IsOnline() ? "OK" : "OFFLINE",
                 Bsp_Oled_IsOnline()   ? "OK" : "OFFLINE");
        }
        break;

    case APP_MAIN_FSM_STATE_SELFTEST:
        if (elapsed >= SELFTEST_DONE_DELAY_MS) {
            Set_State(APP_MAIN_FSM_STATE_IDLE_3S);
        }
        break;

    case APP_MAIN_FSM_STATE_IDLE_3S: {
        uint32_t timeout_ms = (uint32_t)cfg->idle_seconds * 1000U;
        if (elapsed >= timeout_ms) {
            Enter_AutoRun();
        }
        break;
    }

    case APP_MAIN_FSM_STATE_AUTO_RUN: {
        /* 总任务超时检查 */
        uint32_t total_elapsed_s = Bsp_Tick_ElapsedMs(s_ctx.auto_run_start_ms) / 1000U;
        if (total_elapsed_s > cfg->total_timeout_s) {
            LOGE("auto run: TOTAL timeout (%us)", total_elapsed_s);
            App_Pump_Fsm_Abort(&s_ctx.pump_ctx, FM_ERR_011_TOTAL_TIMEOUT);
            App_Main_Fsm_SignalError(FM_ERR_011_TOTAL_TIMEOUT);
            break;
        }
        /* 单路软超时：进入子状态机的累计时长由调用方算（这里以单路启动时刻判定） */
        /* 简化版：仅依赖子 FSM 内部的 step 时序，软超时由总任务超时兜底 */

        /* 推进子状态机 */
        bool running = App_Pump_Fsm_Tick(&s_ctx.pump_ctx);
        if (!running) {
            App_Pump_FsmState s = App_Pump_Fsm_State(&s_ctx.pump_ctx);
            if (s == APP_PUMP_FSM_STATE_ERROR) {
                App_Main_Fsm_SignalError(FM_ERR_010_CH_TIMEOUT);
                break;
            }
            /* 进入下一路或完成 */
            s_ctx.enabled_idx++;
            if (s_ctx.enabled_idx >= s_ctx.enabled_count) {
                s_ctx.current_zone_1based = 0U;
                Set_State(APP_MAIN_FSM_STATE_DONE);
                LOGI("auto run: all zones DONE (%lu s total)",
                     (unsigned long)total_elapsed_s);
            } else {
                Fm_ValveIndex v = (Fm_ValveIndex)s_ctx.enabled_zones[s_ctx.enabled_idx];
                s_ctx.current_zone_1based = (uint8_t)(v + 1U);
                App_Pump_Fsm_Start(&s_ctx.pump_ctx, v);
            }
        }
        break;
    }

    case APP_MAIN_FSM_STATE_MENU:
        App_Menu_Tick();
        break;

    case APP_MAIN_FSM_STATE_DONE:
        /* 短暂停留 → 进入 SLEEP */
        if (elapsed >= 2000U) {
            Set_State(APP_MAIN_FSM_STATE_SLEEP);
        }
        break;

    case APP_MAIN_FSM_STATE_SLEEP:
        /* 永久驻留等待断电；不做任何事 */
        break;

    case APP_MAIN_FSM_STATE_ERROR:
        /* 已经在 SignalError 时 force off；这里只等断电 */
        break;

    default:
        break;
    }
}

/* ---- 事件分发 ---- */
void App_Main_Fsm_OnEvent(const App_Event *e) {
    if (e == 0) return;

    switch (s_ctx.state) {

    case APP_MAIN_FSM_STATE_IDLE_3S:
        /* 任意短按 → 进菜单 */
        if ((e->id >= APP_EVENT_KEY_K1_SHORT) && (e->id <= APP_EVENT_KEY_K4_SHORT)) {
            s_ctx.entered_menu_from_idle = true;
            App_Menu_Enter();
            Set_State(APP_MAIN_FSM_STATE_MENU);
        } else if (e->id == APP_EVENT_KEY_K1_LONG) {
            /* K1 长按 = 立即开始 */
            Enter_AutoRun();
        }
        break;

    case APP_MAIN_FSM_STATE_AUTO_RUN:
        switch (e->id) {
        case APP_EVENT_KEY_K3_SHORT:
        case APP_EVENT_KEY_K3_LONG:
            LOGI("auto run: user STOP (K3)");
            App_Pump_Fsm_Abort(&s_ctx.pump_ctx, FM_OK);
            s_ctx.current_zone_1based = 0U;
            Set_State(APP_MAIN_FSM_STATE_DONE);
            break;

        case APP_EVENT_KEY_K1_SHORT:
            LOGI("auto run: SKIP zone (K1) — not implemented in V1.0");
            break;

        case APP_EVENT_KEY_K2_SHORT:
            LOGI("auto run: PAUSE (K2) — not implemented in V1.0");
            break;

        case APP_EVENT_KEY_K4_SHORT:
            /* 运行时菜单：仅在 GAP 状态下接受切入 */
            if (App_Pump_Fsm_State(&s_ctx.pump_ctx) == APP_PUMP_FSM_STATE_GAP) {
                LOGI("auto run: enter runtime menu (K4)");
                App_Menu_Enter();
                Set_State(APP_MAIN_FSM_STATE_MENU);
            } else {
                LOGD("auto run: K4 ignored (not in GAP)");
            }
            break;

        default: break;
        }
        break;

    case APP_MAIN_FSM_STATE_MENU:
        /* 把事件转给菜单 */
        App_Menu_OnEvent(e);
        /* 菜单可能请求退出 */
        if (App_Menu_ExitRequested()) {
            App_Menu_Action next_act = App_Menu_GetExitAction();
            App_Menu_ClearExit();
            switch (next_act) {
            case APP_MENU_ACTION_START_WATER:
                Enter_AutoRun();
                break;
            case APP_MENU_ACTION_GOTO_SLEEP:
                Set_State(APP_MAIN_FSM_STATE_SLEEP);
                break;
            case APP_MENU_ACTION_RETURN_TO_AUTO:
                Set_State(APP_MAIN_FSM_STATE_AUTO_RUN);
                break;
            default:
                Set_State(APP_MAIN_FSM_STATE_SLEEP);
                break;
            }
        }
        break;

    case APP_MAIN_FSM_STATE_DONE:
    case APP_MAIN_FSM_STATE_SLEEP:
    case APP_MAIN_FSM_STATE_ERROR:
        /* 长按 K3 强制重启（V1 不启用 NVIC reset，仅记录） */
        if (e->id == APP_EVENT_KEY_K3_HOLD) {
            LOGW("user request reset, performing NVIC_SystemReset()");
            NVIC_SystemReset();
        }
        break;

    default:
        break;
    }
}
