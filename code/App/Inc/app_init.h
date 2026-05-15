/**
 * @file    app_init.h
 * @brief   FloraMate 应用入口（CubeMX 外设初始化完成后由 main() 调用）
 * @note    一处装配所有子模块初始化与主循环调度，保持 main.c 干净。
 */
#ifndef APP_INIT_H
#define APP_INIT_H

void App_Init(void);
void App_Loop(void);

#endif /* APP_INIT_H */
