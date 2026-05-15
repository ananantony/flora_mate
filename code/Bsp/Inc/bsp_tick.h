/**
 * @file    bsp_tick.h
 * @brief   1 ms 时基与软定时器
 * @note    依赖 HAL_GetTick()（SysTick 由 HAL_IncTick() 自增）。
 *          本模块只提供"协作式分时调度"判定函数，不真正驱动 SysTick。
 */
#ifndef BSP_TICK_H
#define BSP_TICK_H

#include <stdbool.h>
#include <stdint.h>

#define BSP_TICK_HZ            (1000U)
#define BSP_TICK_MS_PER_TICK   (1U)

void     Bsp_Tick_Init(void);
uint32_t Bsp_Tick_GetMs(void);
uint32_t Bsp_Tick_ElapsedMs(uint32_t start_ms);
bool     Bsp_Tick_Expired(uint32_t start_ms, uint32_t duration_ms);

/* 软任务节拍器：按周期返回 true（自动滚动 next_due） */
typedef struct {
    uint32_t next_due_ms;
    uint32_t period_ms;
} Bsp_Tick_Ticker;

void Bsp_Tick_Ticker_Init(Bsp_Tick_Ticker *t, uint32_t period_ms);
bool Bsp_Tick_Ticker_Due(Bsp_Tick_Ticker *t);

#endif /* BSP_TICK_H */
