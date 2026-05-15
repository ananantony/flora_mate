/*
 * @File         : \code\Bsp\relay\bsp_relay.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 6 路继电器封装：4 路分阀 + 1 路水泵总闸（CH5）+ 1 路保留（CH6）
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
#ifndef BSP_RELAY_H
#define BSP_RELAY_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

/**
 * @brief   继电器通道索引
 * @note    继电器模块为**低电平触发**，Fail-Safe 默认 GPIO=High（线圈不通电=触点释放）。
 *          CH5 软件互锁：必须先吸合任一分路阀才允许吸合 CH5；释放 CH5 顺序相反。
 */
typedef enum
{
    BSP_RELAY_VALVE_1 = 0, /**< 1# 区域电磁阀         */
    BSP_RELAY_VALVE_2,     /**< 2# 区域电磁阀         */
    BSP_RELAY_VALVE_3,     /**< 3# 区域电磁阀         */
    BSP_RELAY_VALVE_4,     /**< 4# 区域电磁阀         */
    BSP_RELAY_MAIN_CH5,    /**< 水泵总闸（安全主继电器）*/
    BSP_RELAY_RSV_CH6,     /**< 保留（如未来加雾化/补光）*/
    BSP_RELAY_CHANNEL_NUM
} Bsp_Relay_Channel;

/**
 * @brief   继电器子系统初始化
 * @note    将 6 路 GPIO 全部拉高（线圈断电、触点释放），满足上电 Fail-Safe；
 *          软件状态影子寄存器同步清零。
 */
void Bsp_Relay_Init(void);

/**
 * @brief   设置某一路继电器开/关
 * @param   ch     继电器通道
 * @param   is_on  true=吸合（通电触点闭合），false=释放
 * @retval  FM_OK                  操作成功
 * @retval  FM_ERR_012_INTERLOCK   CH5 吸合时未先打开任一分阀，被互锁拒绝
 * @note    非 CH5 通道始终通过；非法通道返回 FM_OK 但不做任何事。
 */
Fm_ErrorCode Bsp_Relay_Set(Bsp_Relay_Channel ch, bool is_on);

/**
 * @brief   查询某一路继电器当前状态
 * @param   ch  继电器通道
 * @retval  true=吸合 / false=释放（含非法通道，统一返回 false）
 */
bool Bsp_Relay_Get(Bsp_Relay_Channel ch);

/**
 * @brief   释放全部 6 路继电器（Fail-Safe 收尾）
 * @note    严格按 "先 CH5、再 4 路阀" 的顺序释放；适用于任务正常结束。
 */
void Bsp_Relay_AllOff(void);

/**
 * @brief   异常硬刹车：先释放 CH5、再释放所有分阀
 * @note    1) 不做日志、不做检查、不做延时；适用于 HardFault / ERROR 状态；
 *          2) 与 AllOff 的差别在于：本函数避免再阻塞，直接拉电平。
 */
void Bsp_Relay_MainOffForce(void);

/**
 * @brief   自检：依次给每路继电器一个短脉冲（吸合后立即释放）
 * @param   pulse_ms  每次吸合的保持时长（建议 80~200 ms，可听见"咔嗒"声）
 * @note    阻塞执行；仅用于上电 SELFTEST 状态；自检期间禁止吸合 CH5（互锁保护）。
 */
void Bsp_Relay_Selftest(uint16_t pulse_ms);

/**
 * @brief   通道索引合法性检查（内联，热路径常用）
 * @param   ch  继电器通道
 * @retval  true=合法 / false=越界
 */
static inline bool Bsp_Relay_IsValidChannel(Bsp_Relay_Channel ch)
{
    return ((uint32_t)ch < (uint32_t)BSP_RELAY_CHANNEL_NUM);
}

#endif /* BSP_RELAY_H */
