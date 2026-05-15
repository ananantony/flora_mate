/**
 * @file    bsp_eeprom.h
 * @brief   AT24C64 EEPROM I²C 字节读写 (8KB, 32B 页)
 * @note    7-bit 地址 0x50（A0/A1/A2 = GND）；16-bit 内部地址；
 *          页大小 32 B；写后必须等待 5 ms tWR 或 ACK polling。
 *          本模块只做"裸读写"，双 Bank 与 CRC 由 App_Config 负责组装。
 */
#ifndef BSP_EEPROM_H
#define BSP_EEPROM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "floramate_types.h"

#define BSP_EEPROM_ADDR_7BIT     (0x50U)
#define BSP_EEPROM_SIZE          (8192U)
#define BSP_EEPROM_PAGE_SIZE     (32U)
#define BSP_EEPROM_TWR_MS        (5U)

void         Bsp_Eeprom_Init(void);
bool         Bsp_Eeprom_IsOnline(void);

/* 任意长度读写（自动按页拆分，跨页时分两次写） */
Fm_ErrorCode Bsp_Eeprom_Read (uint16_t addr, uint8_t *out, size_t len);
Fm_ErrorCode Bsp_Eeprom_Write(uint16_t addr, const uint8_t *src, size_t len);

/* 通过 ACK polling 等待写周期完成；不依赖 HAL_Delay */
Fm_ErrorCode Bsp_Eeprom_WaitWriteDone(uint32_t timeout_ms);

#endif /* BSP_EEPROM_H */
