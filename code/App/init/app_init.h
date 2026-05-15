/*
 * @File         : \code\App\init\app_init.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : FloraMate 应用入口与主循环接口（由 main() 调用）
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
#ifndef APP_INIT_H
#define APP_INIT_H

/**
 * @brief   应用层初始化：装配所有子模块
 * @note    调用顺序（由 main.c 在 CubeMX 外设 Init 之后调用）：
 *          USART 日志 → 时基 → 事件队列 → 配置 → 按键 → 继电器 → 水泵 PWM
 *          → OLED → 菜单 → 显示 → 主 FSM。
 *          完成后系统已具备最小可观测性（日志 + OLED 启动屏）。
 */
void App_Init(void);

/**
 * @brief   应用主循环（永不返回）
 * @note    替代 main.c 的 while(1)；内部按节拍调用：
 *          按键 / TX flush / 显示 / 主 FSM Tick / 命令行解析 / 心跳 LED。
 */
void App_Loop(void);

#endif /* APP_INIT_H */
