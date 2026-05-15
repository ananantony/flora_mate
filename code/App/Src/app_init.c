/**
 * @file    app_init.c
 * @brief   FloraMate 应用装配与主循环
 * @note    与软件设计方案 § 2.2 主循环结构一致。
 */
#include "app_init.h"

#include "bsp_tick.h"
#include "bsp_usart_log.h"
#include "bsp_relay.h"
#include "bsp_pump_pwm.h"
#include "bsp_key.h"
#include "bsp_eeprom.h"
#include "bsp_oled.h"

#include "app_log.h"
#include "app_event.h"
#include "app_config.h"
#include "app_menu.h"
#include "app_display.h"
#include "app_main_fsm.h"

#include "floramate_types.h"

static void Apply_Runtime_Config(void) {
    const App_Config *cfg = App_Config_Get();
    App_Log_SetLevel((App_Log_Level)(cfg->log_level + 1U));  /* +1：EEPROM 0..3 → 等级 1..4 */
    Bsp_Key_SetThresholds((uint16_t)((uint32_t)cfg->long_press_ms_x10 * 10U),
                          (uint16_t)((uint32_t)cfg->hold_press_ms_x10 * 10U));
    Bsp_Oled_SetContrast(cfg->oled_contrast);
}

void App_Init(void) {
    /* 1. 时基与日志最早起来，让后续模块能 printf */
    Bsp_Tick_Init();
    Bsp_Usart_Log_Init();
    App_Log_SetLevel(APP_LOG_LEVEL_INFO);
    LOGI("==== %s ====", FM_FIRMWARE_VERSION_STR);
    LOGI("Build: %s", FM_BUILD_DATE_STR);

    /* 2. 输出层：先把所有继电器拉到 Fail-Safe，再启 PWM = 0 */
    Bsp_Relay_Init();
    Bsp_Pump_Pwm_Init();

    /* 3. 存储与配置 */
    Bsp_Eeprom_Init();
    App_Config_Init();
    Apply_Runtime_Config();

    /* 4. HMI：OLED + 按键 */
    Bsp_Oled_Init();
    App_Event_Init();
    Bsp_Key_Init(App_Event_PostKey);

    /* 5. 应用：菜单 + 显示 + 主状态机 */
    App_Menu_Init();
    App_Display_Init();
    App_Main_Fsm_Init();

    LOGI("init done. eeprom=%d oled=%d",
         (int)Bsp_Eeprom_IsOnline(), (int)Bsp_Oled_IsOnline());
}

void App_Loop(void) {
    App_Event evt;

    while (1) {
        /* 1. 推进异步 TX */
        Bsp_Usart_Log_TxFlush();

        /* 2. 按键扫描（内部 20ms 限速） */
        Bsp_Key_Scan();

        /* 3. 事件消费：每轮最多 4 个，避免单帧消耗过久 */
        for (uint32_t i = 0U; i < 4U; i++) {
            if (!App_Event_Pop(&evt)) break;
            App_Main_Fsm_OnEvent(&evt);
        }

        /* 4. 主状态机推进一步 */
        App_Main_Fsm_Tick();

        /* 5. OLED 刷新（内部 100ms 限速） */
        App_Display_Tick();
    }
}
