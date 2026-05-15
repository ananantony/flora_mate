/**
 * @file    bsp_key.h
 * @brief   4 按键扫描（K1=PA1, K2=PA2, K3=PA3, K4=PA4）
 * @note    20 ms 周期采样，3 次稳定计采纳；输出 SHORT/LONG/HOLD 事件。
 *          事件投递到由外部注入的回调（避免本模块强依赖 App_Event）。
 */
#ifndef BSP_KEY_H
#define BSP_KEY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BSP_KEY_K1 = 0,
    BSP_KEY_K2,
    BSP_KEY_K3,
    BSP_KEY_K4,
    BSP_KEY_NUM
} Bsp_Key_Id;

typedef enum {
    BSP_KEY_EVT_NONE = 0,
    BSP_KEY_EVT_SHORT,       /* 按下后稳定释放，按下时长 < long_ms */
    BSP_KEY_EVT_LONG,        /* 按下时长跨过 long_ms 阈值（不等释放就发） */
    BSP_KEY_EVT_HOLD         /* 按下时长跨过 hold_ms 阈值（不等释放就发） */
} Bsp_Key_EventType;

typedef struct {
    Bsp_Key_Id        id;
    Bsp_Key_EventType type;
} Bsp_Key_Event;

typedef void (*Bsp_Key_EventCb)(const Bsp_Key_Event *evt);

void Bsp_Key_Init(Bsp_Key_EventCb cb);

/* 主循环周期调用：内部限速 20 ms。 */
void Bsp_Key_Scan(void);

/* 运行时调阈值（菜单可改） */
void Bsp_Key_SetThresholds(uint16_t long_ms, uint16_t hold_ms);

/* 立即查询某键当前是否按下（适合菜单"长按确认"轮询） */
bool Bsp_Key_IsPressed(Bsp_Key_Id id);

#endif /* BSP_KEY_H */
