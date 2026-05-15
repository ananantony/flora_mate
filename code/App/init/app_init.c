/*
 * @File         : \code\App\init\app_init.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 15:50:00
 * @Description  : FloraMate 应用装配与主循环（与软件设计方案 § 2.2 一致）
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

/**
 * @brief   把当前配置应用到各运行时模块
 * @note    适配点：日志等级 / 按键阈值 / OLED 对比度。
 *          EEPROM 存的 log_level 是 0..3（E/W/I/D），App_Log_LevelType 枚举为
 *          FATAL=1/ERROR=2/WARN=3/INFO=4/DEBUG=5，所以这里 +2。
 * @retval  无（static 内部函数）
 */
static void Apply_Runtime_Config(void)
{
    const App_Config *cfg = App_Config_Get();
    App_Log_SetLevel((App_Log_LevelType)((uint32_t)cfg->log_level + 2U));
    Bsp_Key_SetThresholds((uint16_t)((uint32_t)cfg->long_press_ms_x10 * 10U),
                          (uint16_t)((uint32_t)cfg->hold_press_ms_x10 * 10U));
    Bsp_Oled_SetContrast(cfg->oled_contrast);
}

/**
 * @brief   应用层装配（由 main 在 CubeMX 外设 Init 之后调用一次）
 * @note    顺序如下，确保依赖反向无空指针：
 *          1) Tick + USART 日志（最早起，后续模块即可 printf）；
 *          2) 输出层（继电器全 OFF → PWM=0，硬件先安全）；
 *          3) 存储 + 配置（提供给后续模块的参数）；
 *          4) HMI（OLED + 按键 + 事件队列）；
 *          5) 应用层（菜单 / 显示 / 主 FSM）。
 * @retval  无
 */
void App_Init(void)
{
    /* 1) 时基 + USART 日志（先于 App_Log_Init，因为日志依赖时间戳） */
    Bsp_Tick_Init();
    Bsp_Usart_Log_Init();

    /* 2) 日志客户端：初始化环形缓冲，默认等级 INFO */
    (void)App_Log_Init();
    App_Log_SetLevel(APP_LOG_LEVEL_INFO);
    LOG_INFO_WITH_ARG("==== %s ====", FM_FIRMWARE_VERSION_STR);
    LOG_INFO_WITH_ARG("Build: %s", FM_BUILD_DATE_STR);

    /* 3) 输出层 Fail-Safe */
    Bsp_Relay_Init();
    Bsp_Pump_Pwm_Init();

    /* 4) 存储 + 配置 */
    Bsp_Eeprom_Init();
    App_Config_Init();
    Apply_Runtime_Config();

    /* 5) HMI */
    Bsp_Oled_Init();
    App_Event_Init();
    Bsp_Key_Init(App_Event_PostKey);

    /* 6) 应用层 */
    App_Menu_Init();
    App_Display_Init();
    App_Main_Fsm_Init();

    LOG_INFO_WITH_ARG("init done. eeprom=%d oled=%d", (int)Bsp_Eeprom_IsOnline(), (int)Bsp_Oled_IsOnline());
}

/**
 * @brief   主循环（永不返回）
 * @note    每轮顺序：
 *          1) App_Log_Tick           — 把 log 环形缓冲搬运到 UART TX 缓冲；
 *          2) Bsp_Usart_Log_TxFlush  — 把 UART TX 缓冲推进 IT 发送；
 *          3) Bsp_Key_Scan           — 内部 20 ms 限速；
 *          4) 消费事件（每轮最多 4 个，避免单帧 IO 太久）；
 *          5) App_Main_Fsm_Tick      — 推进主 FSM 与子 FSM；
 *          6) App_Display_Tick       — 内部 100 ms 限速。
 *          实测每轮空载耗时 < 200 μs，主循环空转频率 > 5 kHz，
 *          按键 20 ms / 显示 100 ms / FSM 1 ms 任务都不会丢拍。
 * @retval  无（永不返回）
 */
void App_Loop(void)
{
    App_Event evt;

    while (1)
    {
        App_Log_Tick();
        Bsp_Usart_Log_TxFlush();

        Bsp_Key_Scan();

        for (uint32_t i = 0U; i < 4U; i++)
        {
            if (!App_Event_Pop(&evt))
            {
                break;
            }
            App_Main_Fsm_OnEvent(&evt);
        }

        App_Main_Fsm_Tick();

        App_Display_Tick();
    }
}
