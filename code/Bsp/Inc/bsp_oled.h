/**
 * @file    bsp_oled.h
 * @brief   SSD1315 / SSD1306 兼容 OLED I²C 驱动（128 × 64）
 * @note    地址 7-bit 0x3C；I²C1 总线共用 AT24C64。
 *          双缓冲：本机维护 1024 B frame buffer，主循环按区间局部 flush。
 *          字体：6×8 ASCII（满表）+ 16×16 大数字（仅 0..9 + ":"）。
 */
#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

#define BSP_OLED_ADDR_7BIT    (0x3CU)
#define BSP_OLED_WIDTH        (128U)
#define BSP_OLED_HEIGHT       (64U)
#define BSP_OLED_PAGES        (BSP_OLED_HEIGHT / 8U)        /* 8 页 */
#define BSP_OLED_BUF_SIZE     (BSP_OLED_WIDTH * BSP_OLED_PAGES)

typedef enum {
    BSP_OLED_FONT_6X8 = 0,    /* 21 列 × 8 行 */
    BSP_OLED_FONT_16X16       /* 大数字（仅 0~9，":"），用于倒计时 */
} Bsp_Oled_Font;

Fm_ErrorCode Bsp_Oled_Init(void);
bool         Bsp_Oled_IsOnline(void);
void         Bsp_Oled_SetContrast(uint8_t value);

/* 帧缓冲绘制（仅写本地 buf，不与芯片通信） */
void Bsp_Oled_FbClear(void);
void Bsp_Oled_FbFill(bool on);
void Bsp_Oled_FbDrawPixel(uint8_t x, uint8_t y, bool on);
void Bsp_Oled_FbDrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
void Bsp_Oled_FbFillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
void Bsp_Oled_FbDrawHLine(uint8_t x, uint8_t y, uint8_t w, bool on);

/* 字符绘制（page 行号 0~7） */
void Bsp_Oled_FbDrawChar6x8 (uint8_t x_col, uint8_t page, char c, bool inverse);
void Bsp_Oled_FbDrawStr6x8  (uint8_t x_col, uint8_t page, const char *s, bool inverse);
void Bsp_Oled_FbDrawBigDigit(uint8_t x_col, uint8_t page, char c); /* 16×16，仅 0~9, ':' */

/* 推送：把整个 fb 或某 page 段推给芯片 */
Fm_ErrorCode Bsp_Oled_Flush(void);
Fm_ErrorCode Bsp_Oled_FlushPages(uint8_t page_start, uint8_t page_count);

#endif /* BSP_OLED_H */
