/*
 * @File         : \code\Bsp\eeprom\bsp_eeprom.c
 * @Author       : tonymeng
 * @Date         : 2026-06-06
 * @Description  : AT24C08C 字节读写实现（分块设备地址 + 8-bit 片内地址 + 页对齐拆分 + ACK polling）
 * @note         AT24C08C 的 1KB 空间分为 4 个 256B 块，每块对应不同的 I²C 设备地址：
 *               dev_7bit = 0x50 | (((addr >> 8) & 0x03) << 1)
 *               读写均以 8-bit 片内地址（I2C_MEMADD_SIZE_8BIT）发送。
 *
 * Copyright (c) 2026 by tony.meng, All Rights Reserved.
 */
#include "bsp_eeprom.h"
#include "bsp_tick.h"
#include "i2c.h"

#define BSP_EEPROM_I2C_TIMEOUT_MS (50U) /**< 单次 I²C 操作超时 */

static bool s_online_cached; /**< 上次访问得到的在线缓存 */

/**
 * @brief   根据逻辑地址计算 HAL 所需的 8-bit I²C 写地址
 * @param   addr  逻辑字节地址 [0..1023]
 * @retval  HAL 格式 16-bit 写地址（7-bit 地址 << 1）
 */
static uint16_t Calc_DevAddr(uint16_t addr)
{
    uint8_t dev7 = (uint8_t)(BSP_EEPROM_BASE_ADDR_7BIT | (uint8_t)(((addr >> 8U) & 0x03U) << 1U));
    return (uint16_t)((uint16_t)dev7 << 1U);
}

/**
 * @brief   EEPROM 初始化（探测一次器件存在性）
 */
void Bsp_Eeprom_Init(void)
{
    s_online_cached = false;
    uint16_t dev_addr = Calc_DevAddr(0U);
    if (HAL_I2C_IsDeviceReady(&hi2c1, dev_addr, 3U, 50U) == HAL_OK)
    {
        s_online_cached = true;
    }
}

bool Bsp_Eeprom_IsOnline(void)
{
    return s_online_cached;
}

/**
 * @brief   ACK polling 等待写周期结束
 * @param   addr        当前写块对应的逻辑地址（用于计算设备地址）
 * @param   timeout_ms  超时
 */
Fm_ErrorCode Bsp_Eeprom_WaitWriteDone(uint16_t addr, uint32_t timeout_ms)
{
    uint16_t dev_addr = Calc_DevAddr(addr);
    uint32_t start    = Bsp_Tick_GetMs();
    while (Bsp_Tick_ElapsedMs(start) < timeout_ms)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, dev_addr, 1U, 2U) == HAL_OK)
        {
            return FM_OK;
        }
    }
    return FM_ERR_003_I2C_EEPROM;
}

/**
 * @brief   读取任意长度数据（自动处理块边界：每跨块重新计算设备地址）
 */
Fm_ErrorCode Bsp_Eeprom_Read(uint16_t addr, uint8_t *out, size_t len)
{
    if ((out == 0) || (len == 0U))
    {
        return FM_ERR_003_I2C_EEPROM;
    }
    if ((uint32_t)addr + len > BSP_EEPROM_SIZE)
    {
        return FM_ERR_003_I2C_EEPROM;
    }
    while (len > 0U)
    {
        /* 计算本次读到当前 256B 块末尾的字节数 */
        uint16_t block_offset = (uint16_t)(addr & 0xFFU);
        uint16_t chunk        = (uint16_t)(256U - block_offset);
        if ((size_t)chunk > len)
        {
            chunk = (uint16_t)len;
        }
        uint16_t          dev_addr = Calc_DevAddr(addr);
        uint8_t           mem_addr = (uint8_t)(addr & 0xFFU);
        HAL_StatusTypeDef st       = HAL_I2C_Mem_Read(&hi2c1, dev_addr, mem_addr, I2C_MEMADD_SIZE_8BIT, out,
                                                       chunk, BSP_EEPROM_I2C_TIMEOUT_MS);
        if (st != HAL_OK)
        {
            s_online_cached = false;
            return FM_ERR_003_I2C_EEPROM;
        }
        addr += chunk;
        out  += chunk;
        len  -= chunk;
    }
    s_online_cached = true;
    return FM_OK;
}

/**
 * @brief   单页写入（调用方保证 [addr, addr+len) 不跨页且不跨块）
 */
static Fm_ErrorCode Bsp_Eeprom_WritePage(uint16_t addr, const uint8_t *src, uint16_t len)
{
    uint16_t          dev_addr = Calc_DevAddr(addr);
    uint8_t           mem_addr = (uint8_t)(addr & 0xFFU);
    HAL_StatusTypeDef st       = HAL_I2C_Mem_Write(&hi2c1, dev_addr, mem_addr, I2C_MEMADD_SIZE_8BIT,
                                                    (uint8_t *)src, len, BSP_EEPROM_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        s_online_cached = false;
        return FM_ERR_003_I2C_EEPROM;
    }
    return Bsp_Eeprom_WaitWriteDone(addr, BSP_EEPROM_TWR_MS * 4U);
}

/**
 * @brief   任意长度写入（按 16 B 页拆分，自动处理页边界与块边界）
 */
Fm_ErrorCode Bsp_Eeprom_Write(uint16_t addr, const uint8_t *src, size_t len)
{
    if ((src == 0) || (len == 0U))
    {
        return FM_ERR_003_I2C_EEPROM;
    }
    if ((uint32_t)addr + len > BSP_EEPROM_SIZE)
    {
        return FM_ERR_003_I2C_EEPROM;
    }
    while (len > 0U)
    {
        uint16_t page_offset = (uint16_t)(addr & (BSP_EEPROM_PAGE_SIZE - 1U));
        uint16_t chunk       = (uint16_t)(BSP_EEPROM_PAGE_SIZE - page_offset);
        if ((size_t)chunk > len)
        {
            chunk = (uint16_t)len;
        }
        Fm_ErrorCode err = Bsp_Eeprom_WritePage(addr, src, chunk);
        if (err != FM_OK)
        {
            return err;
        }
        addr += chunk;
        src  += chunk;
        len  -= chunk;
    }
    s_online_cached = true;
    return FM_OK;
}
