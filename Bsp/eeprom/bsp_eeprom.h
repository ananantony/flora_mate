/*
 * @File         : \code\Bsp\eeprom\bsp_eeprom.h
 * @Author       : tonymeng
 * @Date         : 2026-06-06
 * @Description  : AT24C08C EEPROM I²C 字节读写接口 (1 KB, 16 B 页)
 * @note         AT24C08C（Microchip）寻址说明：
 *               总容量 1024 字节，内部 10-bit 地址，分 4 个 256 字节块。
 *               块选择通过 I²C 设备地址中的 A2/A1 位实现：
 *                 块0 (0x000~0x0FF) → 设备地址 7-bit = 0x50
 *                 块1 (0x100~0x1FF) → 设备地址 7-bit = 0x52
 *                 块2 (0x200~0x2FF) → 设备地址 7-bit = 0x54
 *                 块3 (0x300~0x3FF) → 设备地址 7-bit = 0x56
 *               公式：dev_7bit = 0x50 | (((addr >> 8) & 0x03) << 1)
 *               片内字节地址：addr & 0xFF（8-bit，I2C_MEMADD_SIZE_8BIT）
 *               硬件：E0/E1/E2 全接 GND；WCB 接 3V3（允许写入）
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "floramate_types.h"

#define BSP_EEPROM_BASE_ADDR_7BIT (0x50U) /**< 7-bit I²C 基地址（E0/E1/E2=GND）  */
#define BSP_EEPROM_SIZE           (1024U) /**< 容量 1 KB                          */
#define BSP_EEPROM_PAGE_SIZE      (16U)   /**< 页大小 16 B（写边界）              */
#define BSP_EEPROM_TWR_MS         (5U)    /**< 典型写周期 5 ms                    */

/**
 * @brief   EEPROM 模块初始化（探测一次器件存在性）
 * @note    I²C1 已由 MX_I2C1_Init() 配为 Fast 400 kHz；本函数只发 0 字节 START
 *          并等待 ACK，最多 3 次重试、单次 50 ms 超时。
 */
void Bsp_Eeprom_Init(void);

/**
 * @brief   探测 EEPROM 是否在线（通过 HAL_I2C_IsDeviceReady）
 * @retval  true   ACK 收到，器件存在
 * @retval  false  未识别到 ACK（断线/上拉缺失/Vcc 异常）
 */
bool Bsp_Eeprom_IsOnline(void);

/**
 * @brief   读取任意长度数据
 * @param   addr  起始字节地址 [0..BSP_EEPROM_SIZE-1]
 * @param   out   输出缓冲（必须非 NULL，长度 ≥ len）
 * @param   len   字节数；可跨页跨块边界
 * @retval  FM_OK                    读取成功
 * @retval  FM_ERR_003_I2C_EEPROM   I²C 通讯失败
 */
Fm_ErrorCode Bsp_Eeprom_Read(uint16_t addr, uint8_t *out, size_t len);

/**
 * @brief   写入任意长度数据（自动按 16 B 页拆分，跨块自动切换设备地址）
 * @param   addr  起始字节地址 [0..BSP_EEPROM_SIZE-1]
 * @param   src   源缓冲（必须非 NULL，长度 ≥ len）
 * @param   len   字节数；可跨页跨块边界
 * @retval  FM_OK                    全部写入成功（含 ACK polling 等待完成）
 * @retval  FM_ERR_003_I2C_EEPROM   某次 I²C 写失败或写周期超时
 */
Fm_ErrorCode Bsp_Eeprom_Write(uint16_t addr, const uint8_t *src, size_t len);

/**
 * @brief   ACK polling 等待写周期完成
 * @param   addr        当前写操作所在块的地址（用于计算正确的设备地址）
 * @param   timeout_ms  超时（毫秒），建议 ≥ 2 × BSP_EEPROM_TWR_MS
 * @retval  FM_OK                    ACK 已恢复（写周期结束）
 * @retval  FM_ERR_003_I2C_EEPROM   超时仍无 ACK
 */
Fm_ErrorCode Bsp_Eeprom_WaitWriteDone(uint16_t addr, uint32_t timeout_ms);

#endif /* BSP_EEPROM_H */
