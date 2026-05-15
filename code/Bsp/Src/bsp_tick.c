/**
 * @file    bsp_tick.c
 * @brief   1 ms 时基服务实现
 */
#include "bsp_tick.h"
#include "stm32f4xx_hal.h"

void Bsp_Tick_Init(void) {
    /* HAL_Init() 已经把 SysTick 配成 1ms；此处仅占位，便于将来把
     * SysTick_Handler 接管为本模块计时器（如改用 32-bit 64-bit 软扩展）。
     */
}

uint32_t Bsp_Tick_GetMs(void) {
    return HAL_GetTick();
}

uint32_t Bsp_Tick_ElapsedMs(uint32_t start_ms) {
    /* 32-bit 减法在无符号环绕下天然正确，49 天内安全 */
    return (uint32_t)(HAL_GetTick() - start_ms);
}

bool Bsp_Tick_Expired(uint32_t start_ms, uint32_t duration_ms) {
    return Bsp_Tick_ElapsedMs(start_ms) >= duration_ms;
}

void Bsp_Tick_Ticker_Init(Bsp_Tick_Ticker *t, uint32_t period_ms) {
    if (t == 0) {
        return;
    }
    t->period_ms  = period_ms;
    t->next_due_ms = HAL_GetTick() + period_ms;
}

bool Bsp_Tick_Ticker_Due(Bsp_Tick_Ticker *t) {
    uint32_t now = HAL_GetTick();
    /* (int32_t)(now - next_due) >= 0 等价于"已到期"，正确处理环绕 */
    if ((int32_t)(now - t->next_due_ms) >= 0) {
        t->next_due_ms += t->period_ms;
        /* 若被长时间阻塞错过多周期，追赶一次而非补打多次 */
        if ((int32_t)(now - t->next_due_ms) >= 0) {
            t->next_due_ms = now + t->period_ms;
        }
        return true;
    }
    return false;
}
