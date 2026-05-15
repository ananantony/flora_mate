/*
 * @File         : \code\Bsp\eeprom\bsp_eeprom.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : AT24C64 字节读写实现（页对齐拆分 + ACK polling）
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
#include "bsp_eeprom.h"
#include "bsp_tick.h"
#include "i2c.h"

#define BSP_EEPROM_I2C_TIMEOUT_MS (50U)                                   /**< 单次 I²C 操作超时 */
#define BSP_EEPROM_DEV_ADDR_W     ((uint16_t)(BSP_EEPROM_ADDR_7BIT << 1)) /**< 8-bit 写地址      */

static bool s_online_cached; /**< 上次访问得到的在线缓存（避免每次 IO 都探测） */

/**
 * @brief   EEPROM 初始化（探测一次器件存在性）
 * @note    I²C1 已由 MX_I2C1_Init() 配为 Fast 400 kHz；本函数只发 0 字节 START
 *          并等待 ACK，最多 3 次重试、单次 50 ms 超时。
 */
void Bsp_Eeprom_Init(void)
{
    s_online_cached = false;
    if (HAL_I2C_IsDeviceReady(&hi2c1, BSP_EEPROM_DEV_ADDR_W, 3U, 50U) == HAL_OK)
    {
        s_online_cached = true;
    }
}

/**
 * @brief   返回上次缓存的在线状态
 * @retval  true=在线 / false=离线
 */
bool Bsp_Eeprom_IsOnline(void)
{
    return s_online_cached;
}

/**
 * @brief   ACK polling 等待写周期结束
 * @param   timeout_ms  超时
 * @retval  FM_OK / FM_ERR_003_I2C_EEPROM
 * @note    Atmel 标准做法：写命令期间器件 NACK，反复探测直到 ACK 即可继续。
 *          典型 < 5 ms，比固定 HAL_Delay(5) 更快，对配置存储热路径有意义。
 */
Fm_ErrorCode Bsp_Eeprom_WaitWriteDone(uint32_t timeout_ms)
{
    uint32_t start = Bsp_Tick_GetMs();
    while (Bsp_Tick_ElapsedMs(start) < timeout_ms)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, BSP_EEPROM_DEV_ADDR_W, 1U, 2U) == HAL_OK)
        {
            return FM_OK;
        }
    }
    return FM_ERR_003_I2C_EEPROM;
}

/**
 * @brief   读取任意长度数据
 * @param   addr  起始字节地址
 * @param   out   输出缓冲（必须非 NULL）
 * @param   len   字节数（≥ 1）
 * @retval  FM_OK / FM_ERR_003_I2C_EEPROM
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
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, BSP_EEPROM_DEV_ADDR_W, addr, I2C_MEMADD_SIZE_16BIT, out,
                                            (uint16_t)len, BSP_EEPROM_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        s_online_cached = false;
        return FM_ERR_003_I2C_EEPROM;
    }
    s_online_cached = true;
    return FM_OK;
}

/**
 * @brief   单页写入（调用方保证 [addr, addr+len) 不跨页）
 * @param   addr  页内起始地址
 * @param   src   数据源
 * @param   len   字节数（≤ 32）
 * @retval  FM_OK / FM_ERR_003_I2C_EEPROM
 */
static Fm_ErrorCode Bsp_Eeprom_WritePage(uint16_t addr, const uint8_t *src, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, BSP_EEPROM_DEV_ADDR_W, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t *)src,
                                             len, BSP_EEPROM_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        s_online_cached = false;
        return FM_ERR_003_I2C_EEPROM;
    }
    /* 写命令已被 ACK，立即开始 ACK polling 等待写周期完成 */
    return Bsp_Eeprom_WaitWriteDone(BSP_EEPROM_TWR_MS * 4U);
}

/**
 * @brief   任意长度写入（自动按 32 B 页拆分）
 * @param   addr  起始字节地址
 * @param   src   数据源（必须非 NULL）
 * @param   len   字节数（≥ 1）
 * @retval  FM_OK / FM_ERR_003_I2C_EEPROM
 * @note    首次写若不在页起点，第一片仅到本页末尾，避免器件做 page wrap。
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
