/*
 * @File         : bsp_oled_config.h
 * @Description  : 1.3 寸 OLED 驱动选型（与 flora_mate/屏幕/README.md 一致）
 */
#ifndef BSP_OLED_CONFIG_H
#define BSP_OLED_CONFIG_H

/**
 * 1 = SH1106（多数 1.3" 128×64 I²C 模块，内部 132 列，可见区列偏移 2）
 * 0 = SSD1306 / SSD1315（原 0.96" 或部分 1.3" 模块）
 */
#ifndef BSP_OLED_USE_SH1106
#define BSP_OLED_USE_SH1106 1
#endif

/** 列起始偏移：SH1106 常见为 2；若画面整体右偏/左偏一格，可改为 0 或 4 试 */
#ifndef BSP_OLED_COL_OFFSET
#if BSP_OLED_USE_SH1106
#define BSP_OLED_COL_OFFSET 2U
#else
#define BSP_OLED_COL_OFFSET 0U
#endif
#endif

#endif /* BSP_OLED_CONFIG_H */
