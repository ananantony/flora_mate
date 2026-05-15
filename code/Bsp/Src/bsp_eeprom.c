/**
 * @file    bsp_eeprom.c
 * @brief   AT24C64 字节读写实现（页对齐 + ACK polling）
 */
#include "bsp_eeprom.h"
#include "bsp_tick.h"
#include "i2c.h"

#define BSP_EEPROM_I2C_TIMEOUT_MS   (50U)
#define BSP_EEPROM_DEV_ADDR_W       ((uint16_t)(BSP_EEPROM_ADDR_7BIT << 1))

static bool s_online_cached;

void Bsp_Eeprom_Init(void) {
    /* I²C1 已由 MX_I2C1_Init() 初始化；这里只探测在不在线 */
    s_online_cached = false;
    if (HAL_I2C_IsDeviceReady(&hi2c1, BSP_EEPROM_DEV_ADDR_W, 3U, 50U) == HAL_OK) {
        s_online_cached = true;
    }
}

bool Bsp_Eeprom_IsOnline(void) {
    return s_online_cached;
}

Fm_ErrorCode Bsp_Eeprom_WaitWriteDone(uint32_t timeout_ms) {
    uint32_t start = Bsp_Tick_GetMs();
    while (Bsp_Tick_ElapsedMs(start) < timeout_ms) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, BSP_EEPROM_DEV_ADDR_W, 1U, 2U) == HAL_OK) {
            return FM_OK;
        }
    }
    return FM_ERR_003_I2C_EEPROM;
}

Fm_ErrorCode Bsp_Eeprom_Read(uint16_t addr, uint8_t *out, size_t len) {
    if ((out == 0) || (len == 0U)) {
        return FM_ERR_003_I2C_EEPROM;
    }
    if ((uint32_t)addr + len > BSP_EEPROM_SIZE) {
        return FM_ERR_003_I2C_EEPROM;
    }
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, BSP_EEPROM_DEV_ADDR_W,
                                            addr, I2C_MEMADD_SIZE_16BIT,
                                            out, (uint16_t)len,
                                            BSP_EEPROM_I2C_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_online_cached = false;
        return FM_ERR_003_I2C_EEPROM;
    }
    s_online_cached = true;
    return FM_OK;
}

static Fm_ErrorCode Bsp_Eeprom_WritePage(uint16_t addr, const uint8_t *src, uint16_t len) {
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, BSP_EEPROM_DEV_ADDR_W,
                                             addr, I2C_MEMADD_SIZE_16BIT,
                                             (uint8_t *)src, len,
                                             BSP_EEPROM_I2C_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_online_cached = false;
        return FM_ERR_003_I2C_EEPROM;
    }
    /* 等待写周期完成 */
    return Bsp_Eeprom_WaitWriteDone(BSP_EEPROM_TWR_MS * 4U);
}

Fm_ErrorCode Bsp_Eeprom_Write(uint16_t addr, const uint8_t *src, size_t len) {
    if ((src == 0) || (len == 0U)) {
        return FM_ERR_003_I2C_EEPROM;
    }
    if ((uint32_t)addr + len > BSP_EEPROM_SIZE) {
        return FM_ERR_003_I2C_EEPROM;
    }
    while (len > 0U) {
        /* 当前页内可写多少字节 */
        uint16_t page_offset = (uint16_t)(addr & (BSP_EEPROM_PAGE_SIZE - 1U));
        uint16_t chunk = (uint16_t)(BSP_EEPROM_PAGE_SIZE - page_offset);
        if ((size_t)chunk > len) {
            chunk = (uint16_t)len;
        }
        Fm_ErrorCode err = Bsp_Eeprom_WritePage(addr, src, chunk);
        if (err != FM_OK) {
            return err;
        }
        addr += chunk;
        src  += chunk;
        len  -= chunk;
    }
    s_online_cached = true;
    return FM_OK;
}
