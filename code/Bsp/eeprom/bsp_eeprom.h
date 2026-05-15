/*
 * @File         : \code\Bsp\eeprom\bsp_eeprom.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : AT24C64 EEPROM I²C 字节读写接口 (8 KB, 32 B 页)
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
#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "floramate_types.h"

#define BSP_EEPROM_ADDR_7BIT (0x50U) /**< 7-bit I²C 地址（A0/A1/A2 = GND） */
#define BSP_EEPROM_SIZE      (8192U) /**< 容量 8 KB                        */
#define BSP_EEPROM_PAGE_SIZE (32U)   /**< 页大小 32 B（写边界）           */
#define BSP_EEPROM_TWR_MS    (5U)    /**< 典型写周期 5 ms                  */

/**
 * @brief   EEPROM 模块初始化
 * @note    内部仅复位"在线状态"标志，不进行总线操作；I²C1 由 CubeMX 在
 *          MX_I2C1_Init() 中完成。建议初始化后调用一次 Bsp_Eeprom_IsOnline()
 *          确认器件可达。
 */
void Bsp_Eeprom_Init(void);

/**
 * @brief   探测 EEPROM 是否在线（通过 HAL_I2C_IsDeviceReady）
 * @retval  true   ACK 收到，器件存在
 * @retval  false  未识别到 ACK（断线/上拉缺失/Vcc 异常）
 * @note    一次最多重试 3 次，每次 1 ms 超时；总耗时 ≤ 5 ms。
 */
bool Bsp_Eeprom_IsOnline(void);

/**
 * @brief   读取任意长度数据
 * @param   addr  起始字节地址 [0..BSP_EEPROM_SIZE-1]
 * @param   out   输出缓冲（必须非 NULL，长度 ≥ len）
 * @param   len   字节数；可跨页跨边界
 * @retval  FM_OK                    读取成功
 * @retval  FM_ERR_003_I2C_EEPROM   I²C 通讯失败（已重试 3 次）
 * @note    读不需要分页；HAL_I2C_Mem_Read 直接 sequential read，
 *          器件内部地址指针自动递增。
 */
Fm_ErrorCode Bsp_Eeprom_Read(uint16_t addr, uint8_t *out, size_t len);

/**
 * @brief   写入任意长度数据（自动按 32 B 页拆分）
 * @param   addr  起始字节地址 [0..BSP_EEPROM_SIZE-1]
 * @param   src   源缓冲（必须非 NULL，长度 ≥ len）
 * @param   len   字节数；可跨页跨边界
 * @retval  FM_OK                    全部写入成功（含 ACK polling 等待完成）
 * @retval  FM_ERR_003_I2C_EEPROM   某次 I²C 写失败或写周期超时
 * @note    1) 首次写若不在页起点，第一次写仅到本页末尾；
 *          2) 每段写完用 WaitWriteDone 进行 ACK polling，比 HAL_Delay 更快；
 *          3) 建议单次 len ≤ 256（8 页），过长仅是耗时上升、功能仍正确。
 */
Fm_ErrorCode Bsp_Eeprom_Write(uint16_t addr, const uint8_t *src, size_t len);

/**
 * @brief   ACK polling 等待写周期完成
 * @param   timeout_ms  超时（毫秒），建议 ≥ 2 × BSP_EEPROM_TWR_MS
 * @retval  FM_OK                    ACK 已恢复（写周期结束）
 * @retval  FM_ERR_003_I2C_EEPROM   超时仍无 ACK
 * @note    Atmel 数据手册推荐方法：写命令后器件忙时不应答；
 *          反复 IsDeviceReady 直到 ACK 即可继续后续操作，
 *          通常 < 5 ms，比固定 HAL_Delay(5) 更快。
 */
Fm_ErrorCode Bsp_Eeprom_WaitWriteDone(uint32_t timeout_ms);

#endif /* BSP_EEPROM_H */
