/*
 * @File         : \code\App\serial_debug\app_serial_debug_config.h
 * @Description  : 串口调试编译期开关（改此处后需重新编译）
 */
#ifndef APP_SERIAL_DEBUG_CONFIG_H
#define APP_SERIAL_DEBUG_CONFIG_H

/** 上电后等待进入手动模式的时间窗口（ms）；超时走自动浇灌 */
#define APP_SERIAL_DEBUG_BOOT_WAIT_MS (10000U)

/** 上电等待窗内 K1+K2 同时按住进入手动选择的时长（ms） */
#define APP_MODE_MANUAL_COMBO_HOLD_MS (2500U)

/** 手动-串口态下 K4 长按退出回 Boot Wait 的时长（ms） */
#define APP_SERIAL_DEBUG_K4_EXIT_HOLD_MS (2000U)

/**
 * 置 1：上电后直接进入串口调试态（跳过 10s 窗口与正常浇灌流程）
 * 置 0：上电 10s 内收到 debug 则进手动串口，否则走自动浇灌
 */
#ifndef APP_SERIAL_DEBUG_AUTO_ENTER_ON_BOOT
#define APP_SERIAL_DEBUG_AUTO_ENTER_ON_BOOT (0)
#endif

#endif /* APP_SERIAL_DEBUG_CONFIG_H */
