/*
 * @File         : \code\App\main_fsm\app_main_fsm.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 系统主状态机接口（BOOT → SELFTEST → IDLE → AUTO_RUN → DONE/SLEEP）
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
#ifndef APP_MAIN_FSM_H
#define APP_MAIN_FSM_H

#include <stdbool.h>
#include <stdint.h>
#include "app_event.h"
#include "floramate_types.h"

/**
 * @brief   系统主状态枚举
 * @note    转移图见《软件设计方案_V1.0.md》§ 3。
 *          BOOT 阶段已完成 HAL/peripherals init；SELFTEST 跑继电器自检；
 *          IDLE_3S 倒计时（K3 长按进 MENU，否则 → AUTO_RUN）；
 *          AUTO_RUN 调度 4 路子 FSM；DONE / ERROR / SLEEP 为终态。
 */
typedef enum
{
    APP_MAIN_FSM_STATE_SERIAL_WAIT = 0, /**< 上电等待：串口 debug 或 K1+K2 进手动 */
    APP_MAIN_FSM_STATE_SERIAL_DEBUG,    /**< 手动-串口命令（原串口调试）           */
    APP_MAIN_FSM_STATE_MANUAL_SELECT,   /**< 手动模式：选择串口/按键控制           */
    APP_MAIN_FSM_STATE_MANUAL_KEY,      /**< 手动-按键控制继电器/PWM               */
    APP_MAIN_FSM_STATE_BOOT,            /**< 自动：启动并准备自检                  */
    APP_MAIN_FSM_STATE_SELFTEST, /**< 继电器/外设自检                       */
    APP_MAIN_FSM_STATE_IDLE_3S,  /**< 倒计时等待用户进入菜单                */
    APP_MAIN_FSM_STATE_AUTO_RUN, /**< 4 路顺序执行子 FSM                    */
    APP_MAIN_FSM_STATE_MENU,     /**< 菜单（参数 / 手动测试 / 系统信息）     */
    APP_MAIN_FSM_STATE_DONE,     /**< 全部完成等待断电                       */
    APP_MAIN_FSM_STATE_ERROR,    /**< 异常：所有继电器 OFF，等待断电         */
    APP_MAIN_FSM_STATE_SLEEP     /**< 空闲休眠（预留）                       */
} App_Main_FsmState;

/**
 * @brief   主状态机初始化
 * @note    Reset 内部 ctx；不切换到 SELFTEST，由调用者用首个 Tick 推进。
 */
void App_Main_Fsm_Init(void);

/**
 * @brief   推进一次主状态机（主循环每轮调用）
 * @note    1ms 调度粒度即可；内部各状态的耗时操作均非阻塞。
 */
void App_Main_Fsm_Tick(void);

/**
 * @brief   将事件喂给主状态机
 * @param   e  非 NULL；通常由 App_Init 主循环从队列取出后转发
 */
void App_Main_Fsm_OnEvent(const App_Event *e);

/**
 * @brief   读取当前主状态
 */
App_Main_FsmState App_Main_Fsm_GetState(void);

/**
 * @brief   读取当前正在执行的水阀通道（1..4）
 * @retval  1..4=该路 / 0=未在 AUTO_RUN
 */
uint8_t App_Main_Fsm_CurrentZone(void);

/**
 * @brief   当前状态进入时刻（毫秒）
 * @note    显示模块用于绘制倒计时 / 已过时间。
 */
uint32_t App_Main_Fsm_EnterMs(void);

/**
 * @brief   AUTO_RUN 起始时刻（毫秒）
 * @note    用于总任务超时判断。
 */
uint32_t App_Main_Fsm_AutoRunStartMs(void);

/**
 * @brief   读取最近一次错误码
 */
Fm_ErrorCode App_Main_Fsm_LastError(void);

/**
 * @brief   当前主状态是否接受按键扫描/事件
 * @retval  true=允许按键输入；false=主循环应暂停按键扫描
 */
bool App_Main_Fsm_KeyInputEnabled(void);

/**
 * @brief   外部模块上报致命错误（强制进入 ERROR 状态）
 * @param   err  错误码
 * @note    内部立即 Bsp_Relay_MainOffForce + Bsp_Pump_Pwm_Stop。
 */
void App_Main_Fsm_SignalError(Fm_ErrorCode err);

/**
 * @brief   退出手动模式，回到上电等待窗（关输出、停串口调试）
 * @note    等待窗超时后自动进入 BOOT → 自动浇灌流程。
 */
void App_Main_Fsm_EnterBootWait(void);

/**
 * @brief   回到手动选择页（用于手动子模式返回上一级）
 */
void App_Main_Fsm_EnterManualSelect(void);

/** 上电等待窗内 K1+K2 组合键已按住时长百分比（0~100），未按住返回 0 */
uint8_t App_Main_Fsm_GetBootWaitComboPercent(void);

/** Boot Wait 剩余毫秒（仅 SERIAL_WAIT；组合键按下时冻结倒计时） */
uint32_t App_Main_Fsm_GetBootWaitRemainMs(void);

/** Boot Wait 是否因 K1+K2 按下而暂停自动倒计时 */
bool App_Main_Fsm_IsBootWaitTimerPaused(void);

#endif /* APP_MAIN_FSM_H */
