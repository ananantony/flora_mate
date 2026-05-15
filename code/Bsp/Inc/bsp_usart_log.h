/**
 * @file    bsp_usart_log.h
 * @brief   USART1 异步日志 (TX 256B 环形 + IT) 与命令行 RX (64B 环形)
 * @note    与软件设计方案 § 8 对齐；printf 重定向通过 syscalls.c _write 调用本模块。
 */
#ifndef BSP_USART_LOG_H
#define BSP_USART_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BSP_USART_LOG_BAUD       (115200U)
#define BSP_USART_LOG_TX_SIZE    (512U)   /* 必须 2^N */
#define BSP_USART_LOG_RX_SIZE    (64U)    /* 必须 2^N */
#define BSP_USART_LOG_LINE_MAX   (96U)    /* 一行命令最大字节数 */

void Bsp_Usart_Log_Init(void);

/* 异步写入：拷贝到 TX 环形缓冲；调用方非阻塞 */
size_t Bsp_Usart_Log_Write(const uint8_t *data, size_t len);
size_t Bsp_Usart_Log_WriteString(const char *s);

/* 推进 TX：主循环每轮调用一次，把环形缓冲里待发的字节通过 HAL 推出去 */
void Bsp_Usart_Log_TxFlush(void);

/* RX：从环形缓冲读一行；遇到 \r 或 \n 视为一行结束。
 * 返回 true 时 out_line 以 \0 结尾，长度通过 *out_len 返回（不含 \0）。
 */
bool Bsp_Usart_Log_TryGetLine(char *out_line, size_t cap, size_t *out_len);

/* 由 HAL 中断回调路径调用：每收到 1 个字节就 push 进 RX 环形 */
void Bsp_Usart_Log_OnRxByte(uint8_t byte);

#endif /* BSP_USART_LOG_H */
