/**
 * @file    crc16.c
 * @brief   CRC-16-CCITT (XMODEM) 校验 —— 位运算实现
 * @note    8KB 配置数据 + 64B 单条目场景下位运算够用；
 *          若后期统计区扩大可换 256B 表加速。
 */
#include "crc16.h"

uint16_t Crc16_Update(uint16_t crc, uint8_t byte) {
    crc ^= ((uint16_t)byte) << 8;
    for (uint8_t i = 0U; i < 8U; i++) {
        if ((crc & 0x8000U) != 0U) {
            crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_POLY);
        } else {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t Crc16_Compute(const uint8_t *data, size_t len) {
    uint16_t crc = CRC16_CCITT_INIT;
    if (data == NULL) {
        return crc;
    }
    while (len-- > 0U) {
        crc = Crc16_Update(crc, *data++);
    }
    return crc;
}
