/*
 * @File         : \code\Common\crc16\crc16.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : CRC-16-CCITT (XMODEM) 校验接口
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
#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

/* ==== 算法参数（与《配置项与存储映射_V1.0.md》§ 9 对齐） ============ */

#define CRC16_CCITT_INIT  (0xFFFFU) /**< CRC 初值                    */
#define CRC16_CCITT_POLY  (0x1021U) /**< 多项式                      */
#define CRC16_CCITT_CHECK (0x29B1U) /**< 验证常量："123456789" 的 CRC */

/**
 * @brief   计算一段数据的 CRC-16-CCITT (XMODEM)
 * @param   data  数据起始地址；NULL 时返回初值 0xFFFF
 * @param   len   数据字节数；0 时返回初值 0xFFFF
 * @retval  16-bit CRC 校验值（多项式 0x1021，初值 0xFFFF，输入/输出不反射，XorOut=0）
 * @note    标准验证：Crc16_Compute("123456789", 9) == 0x29B1
 */
uint16_t Crc16_Compute(const uint8_t *data, size_t len);

/**
 * @brief   对一字节执行一次 CRC 更新（流式增量计算）
 * @param   crc   当前 CRC 状态（首次调用传入 CRC16_CCITT_INIT）
 * @param   byte  本次输入字节
 * @retval  更新后的 CRC 状态
 * @note    适合分段 / 流式数据；最终结果 = 连续 Crc16_Update 全部输入后的返回值
 */
uint16_t Crc16_Update(uint16_t crc, uint8_t byte);

#endif /* CRC16_H */
