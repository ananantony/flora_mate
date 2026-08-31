/*
 * @File         : \code\Common\crc16\crc16.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : CRC-16-CCITT (XMODEM) 位运算实现
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
#include "crc16.h"

/**
 * @brief   对一字节执行一次 CRC-16-CCITT 更新（位运算实现）
 * @param   crc   当前 CRC 状态
 * @param   byte  待累加字节
 * @retval  累加后的 CRC 状态
 * @note    每字节 8 次循环 = 8 次条件移位；F401 84 MHz 下处理 64 B 配置约 5 μs，
 *          相比 ACK polling 等待 5 ms 完全可忽略，不需要查表加速。
 */
uint16_t Crc16_Update(uint16_t crc, uint8_t byte)
{
    crc ^= ((uint16_t)byte) << 8;
    for (uint8_t i = 0U; i < 8U; i++)
    {
        if ((crc & 0x8000U) != 0U)
        {
            crc = (uint16_t)((crc << 1) ^ CRC16_CCITT_POLY);
        }
        else
        {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/**
 * @brief   计算一段数据的 CRC-16-CCITT
 * @param   data  数据指针；为 NULL 时直接返回 CRC 初值
 * @param   len   数据字节数；为 0 时直接返回 CRC 初值
 * @retval  16-bit CRC 结果
 */
uint16_t Crc16_Compute(const uint8_t *data, size_t len)
{
    uint16_t crc = CRC16_CCITT_INIT;
    if (data == NULL)
    {
        return crc;
    }
    while (len-- > 0U)
    {
        crc = Crc16_Update(crc, *data++);
    }
    return crc;
}
