/**
 * @file    bsp_relay.h
 * @brief   6 路继电器封装（CH1..CH4 = 4 路分阀，CH5 = 水泵总闸，CH6 = 保留）
 * @note    继电器模块为低电平触发，Fail-Safe 默认 GPIO=High（线圈不通电=触点释放）。
 *          CH5 软件互锁：必须先打开任一分路阀才允许吸合 CH5；释放 CH5 顺序相反。
 */
#ifndef BSP_RELAY_H
#define BSP_RELAY_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

typedef enum {
    BSP_RELAY_VALVE_1 = 0,
    BSP_RELAY_VALVE_2,
    BSP_RELAY_VALVE_3,
    BSP_RELAY_VALVE_4,
    BSP_RELAY_MAIN_CH5,
    BSP_RELAY_RSV_CH6,
    BSP_RELAY_CHANNEL_NUM
} Bsp_Relay_Channel;

/* 初始化：把所有 GPIO 拉高（继电器全释放，Fail-Safe） */
void Bsp_Relay_Init(void);

/* 受控写：CH1~CH4/CH6 任意切换；CH5 受互锁检查
 * 返回 FM_OK 或 FM_ERR_012_INTERLOCK
 */
Fm_ErrorCode Bsp_Relay_Set(Bsp_Relay_Channel ch, bool is_on);
bool         Bsp_Relay_Get(Bsp_Relay_Channel ch);

/* 全关：把 6 路继电器全部释放（Fail-Safe） */
void Bsp_Relay_AllOff(void);

/* 异常硬刹车：先释放 CH5、再释放所有分阀；无前置检查、无日志（避免再阻塞）*/
void Bsp_Relay_MainOffForce(void);

/* 自检：依次给每路一个短脉冲（"咔嗒"），用于上电自检 */
void Bsp_Relay_Selftest(uint16_t pulse_ms);

/* 通道是否合法 */
static inline bool Bsp_Relay_IsValidChannel(Bsp_Relay_Channel ch) {
    return ((uint32_t)ch < (uint32_t)BSP_RELAY_CHANNEL_NUM);
}

#endif /* BSP_RELAY_H */
