/**
 * @file    crc16.h
 * @brief   CRC-16-CCITT (XMODEM) 校验
 * @note    多项式 0x1021，初值 0xFFFF，输入/输出不反射，XorOut=0；
 *          "123456789" → 0x29B1（《配置项与存储映射_V1.0.md》§ 9）
 */
#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

#define CRC16_CCITT_INIT      (0xFFFFU)
#define CRC16_CCITT_POLY      (0x1021U)
#define CRC16_CCITT_CHECK     (0x29B1U)  /* 验证常量 */

uint16_t Crc16_Compute(const uint8_t *data, size_t len);
uint16_t Crc16_Update(uint16_t crc, uint8_t byte);

#endif /* CRC16_H */
