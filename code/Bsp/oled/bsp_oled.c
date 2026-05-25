/*
 * @File         : \code\Bsp\oled\bsp_oled.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : 1.3 寸 128×64 OLED（SH1106 默认，可选 SSD1306/1315）
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
#include "bsp_oled.h"
#include "bsp_oled_config.h"
#include "bsp_oled_font.h"
#include "bsp_tick.h"
#include "i2c.h"

#define OLED_DEV_ADDR_W     ((uint16_t)(BSP_OLED_ADDR_7BIT << 1))
#define OLED_I2C_TIMEOUT_MS (50U)

#define OLED_CTRL_CMD  (0x00U)          /**< Co=0, D/C#=0 → 后续都是命令 */
#define OLED_CTRL_DATA (0x40U)          /**< Co=0, D/C#=1 → 后续都是数据 */

static uint8_t s_fb[BSP_OLED_BUF_SIZE]; /**< 1024 B 帧缓冲（8 page × 128 列） */
static bool    s_online;                /**< 在线状态影子                       */

/**
 * @brief   向 OLED 发送一个命令字节
 * @param   cmd  命令码
 * @retval  FM_OK / FM_ERR_002_I2C_OLED
 */
static Fm_ErrorCode Bsp_Oled_WriteCmd(uint8_t cmd)
{
    HAL_StatusTypeDef st =
        HAL_I2C_Mem_Write(&hi2c1, OLED_DEV_ADDR_W, OLED_CTRL_CMD, I2C_MEMADD_SIZE_8BIT, &cmd, 1U, OLED_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        s_online = false;
        return FM_ERR_002_I2C_OLED;
    }
    return FM_OK;
}

/**
 * @brief   向 OLED 发送一段数据字节流（写入 GDDRAM）
 * @param   data  数据起始
 * @param   len   字节数
 * @retval  FM_OK / FM_ERR_002_I2C_OLED
 */
static Fm_ErrorCode Bsp_Oled_WriteData(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, OLED_DEV_ADDR_W, OLED_CTRL_DATA, I2C_MEMADD_SIZE_8BIT,
                                             (uint8_t *)data, len, OLED_I2C_TIMEOUT_MS);
    if (st != HAL_OK)
    {
        s_online = false;
        return FM_ERR_002_I2C_OLED;
    }
    return FM_OK;
}

/**
 * @brief   返回 OLED 在线状态影子
 * @retval  true / false
 */
bool Bsp_Oled_IsOnline(void)
{
    return s_online;
}

#if BSP_OLED_USE_SH1106
/** SH1106：Raspberry Pi / Waveshare 常用序列（无 0x20 寻址命令） */
static const uint8_t s_init_sh1106[] = {
    0xAE,       /* Display OFF                                   */
    0xD5, 0x80, /* Display clock divide                          */
    0xA8, 0x3F, /* Multiplex ratio 63 (64 rows)                  */
    0xD3, 0x00, /* Display offset                              */
    0x40,       /* Display start line = 0                        */
    0x8D, 0x14, /* Charge pump ON                                */
    0xA1,       /* Segment remap                               */
    0xC8,       /* COM scan direction reverse                    */
    0xDA, 0x12, /* COM pins hardware config                      */
    0x81, 0x7F, /* Contrast                                    */
    0xD9, 0xF1, /* Pre-charge                                  */
    0xDB, 0x40, /* VCOMH                                       */
    0xA4,       /* Output from RAM                             */
    0xA6,       /* Normal display                              */
    0xAF        /* Display ON                                  */
};
#else
/** SSD1306 / SSD1315：页寻址模式，与帧缓冲 page 布局一致 */
static const uint8_t s_init_ssd1306[] = {
    0xAE,
    0xD5, 0x80,
    0xA8, 0x3F,
    0xD3, 0x00,
    0x40,
    0x8D, 0x14,
    0x20, 0x02, /* Page addressing（非 Horizontal）              */
    0xA1,
    0xC8,
    0xDA, 0x12,
    0x81, 0x7F,
    0xD9, 0xF1,
    0xDB, 0x40,
    0xA4,
    0xA6,
    0x2E,
    0xAF
};
#endif

static Fm_ErrorCode Bsp_Oled_SendInitSeq(const uint8_t *seq, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++)
    {
        if (Bsp_Oled_WriteCmd(seq[i]) != FM_OK)
        {
            return FM_ERR_002_I2C_OLED;
        }
    }
    return FM_OK;
}

/**
 * @brief   OLED 上电初始化
 * @retval  FM_OK / FM_ERR_002_I2C_OLED
 */
Fm_ErrorCode Bsp_Oled_Init(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_DEV_ADDR_W, 3U, 50U) != HAL_OK)
    {
        s_online = false;
        return FM_ERR_002_I2C_OLED;
    }
    s_online = true;

#if BSP_OLED_USE_SH1106
    if (Bsp_Oled_SendInitSeq(s_init_sh1106, sizeof(s_init_sh1106)) != FM_OK)
    {
        return FM_ERR_002_I2C_OLED;
    }
#else
    if (Bsp_Oled_SendInitSeq(s_init_ssd1306, sizeof(s_init_ssd1306)) != FM_OK)
    {
        return FM_ERR_002_I2C_OLED;
    }
#endif

    Bsp_Oled_FbClear();
    return Bsp_Oled_Flush();
}

/**
 * @brief   动态调整对比度
 * @param   value  对比度值 [0..255]
 */
void Bsp_Oled_SetContrast(uint8_t value)
{
    (void)Bsp_Oled_WriteCmd(0x81U);
    (void)Bsp_Oled_WriteCmd(value);
}

/* ==== 帧缓冲操作（不发 I²C） ====================================== */

/**
 * @brief   清空帧缓冲（全 0，黑底）
 */
void Bsp_Oled_FbClear(void)
{
    for (uint32_t i = 0U; i < sizeof(s_fb); i++)
    {
        s_fb[i] = 0U;
    }
}

/**
 * @brief   填充帧缓冲
 * @param   on  true=全亮 / false=全灭
 */
void Bsp_Oled_FbFill(bool on)
{
    uint8_t v = on ? 0xFFU : 0x00U;
    for (uint32_t i = 0U; i < sizeof(s_fb); i++)
    {
        s_fb[i] = v;
    }
}

/**
 * @brief   绘点（坐标越界静默忽略）
 * @param   x  [0..127]
 * @param   y  [0..63]
 * @param   on true=亮 / false=灭
 */
void Bsp_Oled_FbDrawPixel(uint8_t x, uint8_t y, bool on)
{
    if ((x >= BSP_OLED_WIDTH) || (y >= BSP_OLED_HEIGHT))
    {
        return;
    }
    uint16_t idx = (uint16_t)((y / 8U) * BSP_OLED_WIDTH + x);
    uint8_t  bit = (uint8_t)(1U << (y & 7U));
    if (on)
    {
        s_fb[idx] |= bit;
    }
    else
    {
        s_fb[idx] = (uint8_t)(s_fb[idx] & ~bit);
    }
}

/**
 * @brief   实心矩形
 */
void Bsp_Oled_FbFillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on)
{
    for (uint8_t i = 0U; i < h; i++)
    {
        for (uint8_t j = 0U; j < w; j++)
        {
            Bsp_Oled_FbDrawPixel((uint8_t)(x + j), (uint8_t)(y + i), on);
        }
    }
}

/**
 * @brief   空心矩形（4 条线）
 */
void Bsp_Oled_FbDrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on)
{
    if ((w == 0U) || (h == 0U))
        return;
    for (uint8_t j = 0U; j < w; j++)
    {
        Bsp_Oled_FbDrawPixel((uint8_t)(x + j), y, on);
        Bsp_Oled_FbDrawPixel((uint8_t)(x + j), (uint8_t)(y + h - 1U), on);
    }
    for (uint8_t i = 0U; i < h; i++)
    {
        Bsp_Oled_FbDrawPixel(x, (uint8_t)(y + i), on);
        Bsp_Oled_FbDrawPixel((uint8_t)(x + w - 1U), (uint8_t)(y + i), on);
    }
}

/**
 * @brief   水平直线
 */
void Bsp_Oled_FbDrawHLine(uint8_t x, uint8_t y, uint8_t w, bool on)
{
    for (uint8_t i = 0U; i < w; i++)
    {
        Bsp_Oled_FbDrawPixel((uint8_t)(x + i), y, on);
    }
}

/**
 * @brief   绘制 6×8 单字符（直接写帧缓冲，按字节对齐）
 * @param   x_col    起始列像素
 * @param   page     行号 [0..7]
 * @param   c        ASCII（< 0x20 或 > 0x7E 视为空格）
 * @param   inverse  true=反白
 * @note    超过 page 范围或 x_col + 6 > 128 时直接返回。
 */
void Bsp_Oled_FbDrawChar6x8(uint8_t x_col, uint8_t page, char c, bool inverse)
{
    if ((page >= BSP_OLED_PAGES) || (x_col >= (BSP_OLED_WIDTH - 5U)))
    {
        return;
    }
    uint8_t        idx       = ((c >= ' ') && (c <= '~')) ? (uint8_t)(c - ' ') : 0U;
    const uint8_t *glyph     = g_font_6x8[idx];
    uint16_t       fb_offset = (uint16_t)(page * BSP_OLED_WIDTH + x_col);
    for (uint8_t i = 0U; i < 6U; i++)
    {
        uint8_t b = glyph[i];
        if (inverse)
            b = (uint8_t)~b;
        s_fb[fb_offset + i] = b;
    }
}

/**
 * @brief   绘制 6×8 字符串（自动右截断）
 */
void Bsp_Oled_FbDrawStr6x8(uint8_t x_col, uint8_t page, const char *s, bool inverse)
{
    if (s == 0)
        return;
    while ((*s != '\0') && (x_col <= (BSP_OLED_WIDTH - 6U)))
    {
        Bsp_Oled_FbDrawChar6x8(x_col, page, *s, inverse);
        x_col = (uint8_t)(x_col + 6U);
        s++;
    }
}

/**
 * @brief   绘制 6×8 单字符（按像素 y 坐标，允许非 page 对齐）
 */
void Bsp_Oled_FbDrawChar6x8At(uint8_t x_col, uint8_t y, char c, bool inverse)
{
    if ((y >= BSP_OLED_HEIGHT) || (x_col >= (BSP_OLED_WIDTH - 5U)))
    {
        return;
    }

    uint8_t        idx   = ((c >= ' ') && (c <= '~')) ? (uint8_t)(c - ' ') : 0U;
    const uint8_t *glyph = g_font_6x8[idx];

    for (uint8_t col = 0U; col < 6U; col++)
    {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0U; row < 8U; row++)
        {
            uint8_t py = (uint8_t)(y + row);
            if (py >= BSP_OLED_HEIGHT)
            {
                break;
            }
            bool on = ((bits & (uint8_t)(1U << row)) != 0U);
            if (inverse)
            {
                on = !on;
            }
            Bsp_Oled_FbDrawPixel((uint8_t)(x_col + col), py, on);
        }
    }
}

/**
 * @brief   绘制 6×8 字符串（按像素 y 坐标，自动右截断）
 */
void Bsp_Oled_FbDrawStr6x8At(uint8_t x_col, uint8_t y, const char *s, bool inverse)
{
    if (s == 0)
        return;
    while ((*s != '\0') && (x_col <= (BSP_OLED_WIDTH - 6U)))
    {
        Bsp_Oled_FbDrawChar6x8At(x_col, y, *s, inverse);
        x_col = (uint8_t)(x_col + 6U);
        s++;
    }
}

/**
 * @brief   绘制 16×16 大字（占两个 page 行）
 * @param   x_col  起始列
 * @param   page   起始行号（占用 page 与 page+1）
 * @param   c      仅 '0'..'9' / ':' 有效，其他直接返回
 */
void Bsp_Oled_FbDrawBigDigit(uint8_t x_col, uint8_t page, char c)
{
    if ((page + 1U) >= BSP_OLED_PAGES)
        return;
    if (x_col + 16U > BSP_OLED_WIDTH)
        return;

    uint8_t idx;
    if ((c >= '0') && (c <= '9'))
    {
        idx = (uint8_t)(c - '0');
    }
    else if (c == ':')
    {
        idx = 10U;
    }
    else
    {
        return;
    }
    const uint8_t *glyph      = g_font_16x16[idx];
    uint16_t       off_top    = (uint16_t)(page * BSP_OLED_WIDTH + x_col);
    uint16_t       off_bottom = (uint16_t)((page + 1U) * BSP_OLED_WIDTH + x_col);
    for (uint8_t i = 0U; i < 16U; i++)
    {
        s_fb[off_top + i]    = glyph[i];
        s_fb[off_bottom + i] = glyph[16U + i];
    }
}

/* ==== 推送 ======================================================== */

#if BSP_OLED_USE_SH1106
/**
 * @brief   SH1106 按页推送（列地址 = COL_OFFSET .. COL_OFFSET+127）
 * @note    参考 Raspberry Pi pico-examples i2c/1106oled/sh1106.py show()
 */
static Fm_ErrorCode Bsp_Oled_FlushPages_Sh1106(uint8_t page_start, uint8_t page_count)
{
    const uint8_t col_low  = (uint8_t)(0x00U | (BSP_OLED_COL_OFFSET & 0x0FU));
    const uint8_t col_high = (uint8_t)(0x10U | ((BSP_OLED_COL_OFFSET >> 4) & 0x0FU));
    Fm_ErrorCode  err;

    for (uint8_t p = 0U; p < page_count; p++)
    {
        const uint8_t page = (uint8_t)(page_start + p);

        err = Bsp_Oled_WriteCmd((uint8_t)(0xB0U | (page & 0x07U)));
        if (err != FM_OK)
            return err;
        err = Bsp_Oled_WriteCmd(col_low);
        if (err != FM_OK)
            return err;
        err = Bsp_Oled_WriteCmd(col_high);
        if (err != FM_OK)
            return err;

        uint16_t off = (uint16_t)(page * BSP_OLED_WIDTH);
        err          = Bsp_Oled_WriteData(&s_fb[off], BSP_OLED_WIDTH);
        if (err != FM_OK)
            return err;
    }
    return FM_OK;
}
#endif

/**
 * @brief   SSD1306：列/页窗口 + 逐页写 128 B
 */
static Fm_ErrorCode Bsp_Oled_FlushPages_Ssd1306(uint8_t page_start, uint8_t page_count)
{
    Fm_ErrorCode err;
    const uint8_t col_end = (uint8_t)(BSP_OLED_COL_OFFSET + BSP_OLED_WIDTH - 1U);

    err = Bsp_Oled_WriteCmd(0x21U);
    if (err != FM_OK)
        return err;
    err = Bsp_Oled_WriteCmd(BSP_OLED_COL_OFFSET);
    if (err != FM_OK)
        return err;
    err = Bsp_Oled_WriteCmd(col_end);
    if (err != FM_OK)
        return err;
    err = Bsp_Oled_WriteCmd(0x22U);
    if (err != FM_OK)
        return err;
    err = Bsp_Oled_WriteCmd(page_start);
    if (err != FM_OK)
        return err;
    err = Bsp_Oled_WriteCmd((uint8_t)(page_start + page_count - 1U));
    if (err != FM_OK)
        return err;

    for (uint8_t p = 0U; p < page_count; p++)
    {
        uint16_t off = (uint16_t)((page_start + p) * BSP_OLED_WIDTH);
        err          = Bsp_Oled_WriteData(&s_fb[off], BSP_OLED_WIDTH);
        if (err != FM_OK)
            return err;
    }
    return FM_OK;
}

/**
 * @brief   推送某几行 page 到 OLED 显存
 */
Fm_ErrorCode Bsp_Oled_FlushPages(uint8_t page_start, uint8_t page_count)
{
    if (page_start >= BSP_OLED_PAGES)
        return FM_ERR_002_I2C_OLED;
    if ((page_start + page_count) > BSP_OLED_PAGES)
    {
        page_count = (uint8_t)(BSP_OLED_PAGES - page_start);
    }
    if (page_count == 0U)
    {
        return FM_OK;
    }

#if BSP_OLED_USE_SH1106
    return Bsp_Oled_FlushPages_Sh1106(page_start, page_count);
#else
    return Bsp_Oled_FlushPages_Ssd1306(page_start, page_count);
#endif
}

/**
 * @brief   推送整屏（= FlushPages(0, 8)）
 */
Fm_ErrorCode Bsp_Oled_Flush(void)
{
    return Bsp_Oled_FlushPages(0U, BSP_OLED_PAGES);
}
