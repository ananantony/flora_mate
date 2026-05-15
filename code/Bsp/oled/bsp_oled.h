/*
 * @File         : \code\Bsp\oled\bsp_oled.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : SSD1315 / SSD1306 兼容 OLED 驱动接口 (128×64, I²C)
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
#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdbool.h>
#include <stdint.h>
#include "floramate_types.h"

#define BSP_OLED_ADDR_7BIT (0x3CU)                           /**< I²C 地址        */
#define BSP_OLED_WIDTH     (128U)                            /**< 像素宽          */
#define BSP_OLED_HEIGHT    (64U)                             /**< 像素高          */
#define BSP_OLED_PAGES     (BSP_OLED_HEIGHT / 8U)            /**< 8 页（垂直）    */
#define BSP_OLED_BUF_SIZE  (BSP_OLED_WIDTH * BSP_OLED_PAGES) /**< 帧缓冲字节数=1024 */

/**
 * @brief   字体规格
 */
typedef enum
{
    BSP_OLED_FONT_6X8 = 0, /**< 6×8 ASCII：屏可显 21 列 × 8 行       */
    BSP_OLED_FONT_16X16    /**< 16×16 大数字（仅 0~9 与 ':'）        */
} Bsp_Oled_Font;

/**
 * @brief   OLED 子系统初始化
 * @retval  FM_OK              初始化成功
 * @retval  FM_ERR_002_I2C_OLED  芯片不在线或初始化序列被 NACK
 * @note    上电序列固定（厂家典型值，对比度 0x7F）；初始化后帧缓冲已清零并 Flush 一次。
 */
Fm_ErrorCode Bsp_Oled_Init(void);

/**
 * @brief   探测 OLED 是否在线
 * @retval  true / false
 */
bool Bsp_Oled_IsOnline(void);

/**
 * @brief   动态调整对比度
 * @param   value  对比度寄存器 [0..255]
 * @note    主要给"夜间模式"或"休眠淡出"使用；当前 V1.0 默认 0x7F。
 */
void Bsp_Oled_SetContrast(uint8_t value);

/* ==== 帧缓冲绘制（不发送 I²C，仅修改本地 1024 B 帧缓冲） ============ */

/**
 * @brief   清屏（帧缓冲全 0，黑底）
 */
void Bsp_Oled_FbClear(void);

/**
 * @brief   填充整屏
 * @param   on  true=全亮 / false=全灭
 */
void Bsp_Oled_FbFill(bool on);

/**
 * @brief   绘点
 * @param   x   横坐标 [0..127]
 * @param   y   纵坐标 [0..63]
 * @param   on  true=亮 / false=灭
 * @note    超出屏幕范围的坐标会被静默忽略。
 */
void Bsp_Oled_FbDrawPixel(uint8_t x, uint8_t y, bool on);

/**
 * @brief   绘制空心矩形
 * @param   x,y  左上角坐标
 * @param   w,h  宽 / 高（≥ 1）
 * @param   on   true=亮线 / false=灭线
 */
void Bsp_Oled_FbDrawRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

/**
 * @brief   绘制实心矩形（常用于进度条 / 反白条）
 * @param   x,y  左上角坐标
 * @param   w,h  宽 / 高
 * @param   on   true=填亮 / false=填灭
 */
void Bsp_Oled_FbFillRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

/**
 * @brief   绘制水平直线
 * @param   x,y  起点坐标
 * @param   w    长度（像素）
 * @param   on   true=亮 / false=灭
 */
void Bsp_Oled_FbDrawHLine(uint8_t x, uint8_t y, uint8_t w, bool on);

/**
 * @brief   绘制 6×8 单字符
 * @param   x_col    列坐标（像素，不是字符列）
 * @param   page     行号 [0..7]（按 OLED 物理 page 计）
 * @param   c        ASCII 字符（< 0x20 视为空格）
 * @param   inverse  true=黑底白字反白 / false=正常
 */
void Bsp_Oled_FbDrawChar6x8(uint8_t x_col, uint8_t page, char c, bool inverse);

/**
 * @brief   绘制 6×8 字符串
 * @param   x_col    起始列坐标
 * @param   page     行号 [0..7]
 * @param   s        以 \0 结尾的字符串；NULL 时函数立即返回
 * @param   inverse  true=反白 / false=正常
 * @note    超出右边界 (x_col + 6*N > 128) 的字符会被自动截断。
 */
void Bsp_Oled_FbDrawStr6x8(uint8_t x_col, uint8_t page, const char *s, bool inverse);

/**
 * @brief   绘制 16×16 大字（仅支持 0~9 和 ':'）
 * @param   x_col  起始列坐标
 * @param   page   起始行号（占用 page 和 page+1 两行）
 * @param   c      字符：'0'..'9' 或 ':'，其余字符当空格处理
 * @note    用于倒计时显示 "MM:SS"，每位字 16 px 宽 × 16 px 高。
 */
void Bsp_Oled_FbDrawBigDigit(uint8_t x_col, uint8_t page, char c);

/* ==== Flush：把帧缓冲同步到 OLED 显存 =============================== */

/**
 * @brief   将整个帧缓冲推送到屏幕
 * @retval  FM_OK / FM_ERR_002_I2C_OLED
 * @note    一帧 1024 B + 命令，I²C Fast 400 kHz 下约 26 ms；
 *          满帧刷只在切换页面时使用，正常显示请用 FlushPages 局部刷。
 */
Fm_ErrorCode Bsp_Oled_Flush(void);

/**
 * @brief   仅推送某几行（page）
 * @param   page_start  起始行号 [0..7]
 * @param   page_count  行数（≥ 1，越界会自动 clamp 到屏幕末尾）
 * @retval  FM_OK / FM_ERR_002_I2C_OLED
 * @note    每行 128 B；用于"只刷倒计时区"等增量更新，单次 < 5 ms。
 */
Fm_ErrorCode Bsp_Oled_FlushPages(uint8_t page_start, uint8_t page_count);

#endif /* BSP_OLED_H */
