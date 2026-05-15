/**
 * @file    bsp_oled.c
 * @brief   SSD1315 / SSD1306 兼容 OLED 驱动实现
 * @note    与 AT24C64 共用 I²C1（地址不冲突：OLED=0x3C，EEPROM=0x50）。
 *          全屏 1024 字节缓冲；常规更新走"页段局部 flush"减少 I²C 占用。
 */
#include "bsp_oled.h"
#include "bsp_oled_font.h"
#include "bsp_tick.h"
#include "i2c.h"

#define OLED_DEV_ADDR_W      ((uint16_t)(BSP_OLED_ADDR_7BIT << 1))
#define OLED_I2C_TIMEOUT_MS  (50U)

#define OLED_CTRL_CMD        (0x00U)   /* Co=0, D/C#=0 → 后续都是命令 */
#define OLED_CTRL_DATA       (0x40U)   /* Co=0, D/C#=1 → 后续都是数据 */

static uint8_t s_fb[BSP_OLED_BUF_SIZE];
static bool    s_online;

static Fm_ErrorCode Bsp_Oled_WriteCmd(uint8_t cmd) {
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, OLED_DEV_ADDR_W,
                                             OLED_CTRL_CMD, I2C_MEMADD_SIZE_8BIT,
                                             &cmd, 1U, OLED_I2C_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_online = false;
        return FM_ERR_002_I2C_OLED;
    }
    return FM_OK;
}

static Fm_ErrorCode Bsp_Oled_WriteData(const uint8_t *data, uint16_t len) {
    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c1, OLED_DEV_ADDR_W,
                                             OLED_CTRL_DATA, I2C_MEMADD_SIZE_8BIT,
                                             (uint8_t *)data, len,
                                             OLED_I2C_TIMEOUT_MS);
    if (st != HAL_OK) {
        s_online = false;
        return FM_ERR_002_I2C_OLED;
    }
    return FM_OK;
}

bool Bsp_Oled_IsOnline(void) {
    return s_online;
}

Fm_ErrorCode Bsp_Oled_Init(void) {
    /* SSD1306/SSD1315 标准初始化序列 */
    static const uint8_t init_seq[] = {
        0xAE,             /* Display OFF */
        0xD5, 0x80,       /* Set Display Clock Divide Ratio / OSC Freq */
        0xA8, 0x3F,       /* Multiplex Ratio = 64 - 1 */
        0xD3, 0x00,       /* Display Offset = 0 */
        0x40,             /* Display Start Line = 0 */
        0x8D, 0x14,       /* Enable Charge Pump */
        0x20, 0x00,       /* Memory Addressing Mode = Horizontal */
        0xA1,             /* Segment remap (col127 → SEG0) */
        0xC8,             /* COM scan dir reverse */
        0xDA, 0x12,       /* COM Pins HW Cfg */
        0x81, 0x80,       /* Contrast = 128 */
        0xD9, 0xF1,       /* Pre-charge period */
        0xDB, 0x40,       /* VCOMH = 0.77 × Vcc */
        0xA4,             /* Output follows RAM */
        0xA6,             /* Normal display (not inverted) */
        0x2E,             /* Deactivate scroll */
        0xAF              /* Display ON */
    };

    if (HAL_I2C_IsDeviceReady(&hi2c1, OLED_DEV_ADDR_W, 3U, 50U) != HAL_OK) {
        s_online = false;
        return FM_ERR_002_I2C_OLED;
    }
    s_online = true;

    for (uint32_t i = 0U; i < sizeof(init_seq); i++) {
        if (Bsp_Oled_WriteCmd(init_seq[i]) != FM_OK) {
            return FM_ERR_002_I2C_OLED;
        }
    }

    Bsp_Oled_FbClear();
    return Bsp_Oled_Flush();
}

void Bsp_Oled_SetContrast(uint8_t value) {
    (void)Bsp_Oled_WriteCmd(0x81U);
    (void)Bsp_Oled_WriteCmd(value);
}

/* ---- 帧缓冲操作（不与芯片通信） ---- */

void Bsp_Oled_FbClear(void) {
    for (uint32_t i = 0U; i < sizeof(s_fb); i++) {
        s_fb[i] = 0U;
    }
}

void Bsp_Oled_FbFill(bool on) {
    uint8_t v = on ? 0xFFU : 0x00U;
    for (uint32_t i = 0U; i < sizeof(s_fb); i++) {
        s_fb[i] = v;
    }
}

void Bsp_Oled_FbDrawPixel(uint8_t x, uint8_t y, bool on) {
    if ((x >= BSP_OLED_WIDTH) || (y >= BSP_OLED_HEIGHT)) {
        return;
    }
    uint16_t idx = (uint16_t)((y / 8U) * BSP_OLED_WIDTH + x);
    uint8_t  bit = (uint8_t)(1U << (y & 7U));
    if (on) {
        s_fb[idx] |= bit;
    } else {
        s_fb[idx] = (uint8_t)(s_fb[idx] & ~bit);
    }
}

void Bsp_Oled_FbFillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on) {
    for (uint8_t i = 0U; i < h; i++) {
        for (uint8_t j = 0U; j < w; j++) {
            Bsp_Oled_FbDrawPixel((uint8_t)(x + j), (uint8_t)(y + i), on);
        }
    }
}

void Bsp_Oled_FbDrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on) {
    if ((w == 0U) || (h == 0U)) return;
    for (uint8_t j = 0U; j < w; j++) {
        Bsp_Oled_FbDrawPixel((uint8_t)(x + j), y, on);
        Bsp_Oled_FbDrawPixel((uint8_t)(x + j), (uint8_t)(y + h - 1U), on);
    }
    for (uint8_t i = 0U; i < h; i++) {
        Bsp_Oled_FbDrawPixel(x, (uint8_t)(y + i), on);
        Bsp_Oled_FbDrawPixel((uint8_t)(x + w - 1U), (uint8_t)(y + i), on);
    }
}

void Bsp_Oled_FbDrawHLine(uint8_t x, uint8_t y, uint8_t w, bool on) {
    for (uint8_t i = 0U; i < w; i++) {
        Bsp_Oled_FbDrawPixel((uint8_t)(x + i), y, on);
    }
}

void Bsp_Oled_FbDrawChar6x8(uint8_t x_col, uint8_t page, char c, bool inverse) {
    if ((page >= BSP_OLED_PAGES) || (x_col >= (BSP_OLED_WIDTH - 5U))) {
        return;
    }
    uint8_t idx = ((c >= ' ') && (c <= '~')) ? (uint8_t)(c - ' ') : 0U;
    const uint8_t *glyph = g_font_6x8[idx];
    uint16_t fb_offset = (uint16_t)(page * BSP_OLED_WIDTH + x_col);
    for (uint8_t i = 0U; i < 6U; i++) {
        uint8_t b = glyph[i];
        if (inverse) b = (uint8_t)~b;
        s_fb[fb_offset + i] = b;
    }
}

void Bsp_Oled_FbDrawStr6x8(uint8_t x_col, uint8_t page, const char *s, bool inverse) {
    if (s == 0) return;
    while ((*s != '\0') && (x_col <= (BSP_OLED_WIDTH - 6U))) {
        Bsp_Oled_FbDrawChar6x8(x_col, page, *s, inverse);
        x_col = (uint8_t)(x_col + 6U);
        s++;
    }
}

void Bsp_Oled_FbDrawBigDigit(uint8_t x_col, uint8_t page, char c) {
    /* 16×16 占用 2 个 page 行 */
    if ((page + 1U) >= BSP_OLED_PAGES) return;
    if (x_col + 16U > BSP_OLED_WIDTH) return;

    uint8_t idx;
    if ((c >= '0') && (c <= '9')) {
        idx = (uint8_t)(c - '0');
    } else if (c == ':') {
        idx = 10U;
    } else {
        return;
    }
    const uint8_t *glyph = g_font_16x16[idx];
    uint16_t off_top    = (uint16_t)(page * BSP_OLED_WIDTH + x_col);
    uint16_t off_bottom = (uint16_t)((page + 1U) * BSP_OLED_WIDTH + x_col);
    for (uint8_t i = 0U; i < 16U; i++) {
        s_fb[off_top    + i] = glyph[i];
        s_fb[off_bottom + i] = glyph[16U + i];
    }
}

/* ---- 推送 ---- */

Fm_ErrorCode Bsp_Oled_FlushPages(uint8_t page_start, uint8_t page_count) {
    if (page_start >= BSP_OLED_PAGES) return FM_ERR_002_I2C_OLED;
    if ((page_start + page_count) > BSP_OLED_PAGES) {
        page_count = (uint8_t)(BSP_OLED_PAGES - page_start);
    }
    /* Horizontal addressing：设置 Column 0~127, Page page_start~end */
    Fm_ErrorCode err;
    err = Bsp_Oled_WriteCmd(0x21U);                                if (err != FM_OK) return err;
    err = Bsp_Oled_WriteCmd(0x00U);                                if (err != FM_OK) return err;
    err = Bsp_Oled_WriteCmd(0x7FU);                                if (err != FM_OK) return err;
    err = Bsp_Oled_WriteCmd(0x22U);                                if (err != FM_OK) return err;
    err = Bsp_Oled_WriteCmd(page_start);                           if (err != FM_OK) return err;
    err = Bsp_Oled_WriteCmd((uint8_t)(page_start + page_count - 1U)); if (err != FM_OK) return err;
    /* 按页发送（每页 128 字节），HAL_I2C_Mem_Write 一次最大 65535，但分块更可靠 */
    for (uint8_t p = 0U; p < page_count; p++) {
        uint16_t off = (uint16_t)((page_start + p) * BSP_OLED_WIDTH);
        err = Bsp_Oled_WriteData(&s_fb[off], BSP_OLED_WIDTH);
        if (err != FM_OK) return err;
    }
    return FM_OK;
}

Fm_ErrorCode Bsp_Oled_Flush(void) {
    return Bsp_Oled_FlushPages(0U, BSP_OLED_PAGES);
}
