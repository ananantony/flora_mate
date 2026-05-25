/*
 * @File         : \code\Bsp\usart\bsp_usart_log.h
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : USART1 异步日志 (TX 环形 + IT) 与命令行 RX (IDLE + IT) 接口
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
#ifndef BSP_USART_LOG_H
#define BSP_USART_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BSP_USART_LOG_BAUD         (115200U) /**< 波特率 115200 8N1            */
#define BSP_USART_LOG_TX_SIZE      (512U)    /**< 发送环形缓冲（必须 2^N）     */
#define BSP_USART_LOG_RX_SIZE      (128U)    /**< 接收环形缓冲（必须 2^N，≥ LINE_MAX） */
#define BSP_USART_LOG_LINE_MAX     (96U)     /**< 一行命令最大字节数           */
#define BSP_USART_LOG_RX_CHUNK_SIZE BSP_USART_LOG_LINE_MAX /**< IDLE 单帧 IT 缓冲 */

/**
 * @brief   日志模块初始化
 * @note    内部完成：
 *          1) TX/RX 两个环形缓冲 Init；
 *          2) 启动 HAL_UARTEx_ReceiveToIdle_IT 开启 IDLE 接收；
 *          3) `printf` 重定向（通过 `__io_putchar`）即可立即工作。
 *          需在 MX_USART1_UART_Init() 之后、第一次 printf 之前调用。
 */
void Bsp_Usart_Log_Init(void);

/**
 * @brief   异步写入字节流（拷贝进 TX 环形缓冲）
 * @param   data  数据起始
 * @param   len   字节数
 * @retval  实际写入的字节数（≤ len；不足说明 TX 缓冲已满，超出部分被丢弃）
 * @note    本函数永远不阻塞；ISR 内调用是安全的（非 8000 nibble 翻转）。
 */
size_t Bsp_Usart_Log_Write(const uint8_t *data, size_t len);

/**
 * @brief   以 0 结尾的 C 字符串便捷封装
 * @param   s  字符串；为 NULL 时返回 0
 * @retval  实际写入的字节数
 */
size_t Bsp_Usart_Log_WriteString(const char *s);

/**
 * @brief   推进 TX：从环形缓冲把待发字节交给 HAL 启动一次 IT 发送
 * @note    主循环每轮调用一次即可；若上一帧仍在发送（TX busy）会自动跳过，
 *          下一轮再尝试，因此**不会阻塞**且**不会丢字节**。
 */
void Bsp_Usart_Log_TxFlush(void);

/**
 * @brief   尝试从 RX 缓冲取一行命令（以 \r 或 \n 作为行结束符）
 * @param   out_line  输出缓冲；为 NULL 时函数返回 false
 * @param   cap       输出缓冲容量（含末尾 \0 位置）
 * @param   out_len   输出参数：返回行长度（不含 \0），允许 NULL
 * @retval  true   已凑到一整行，out_line 已以 \0 终止
 * @retval  false  尚未凑到完整行
 * @note    无换行时，USART IDLE 帧结束也会提交一行；
 *          超长行（>= cap）会被截断为 cap-1 字节并强制结束本行。
 */
bool Bsp_Usart_Log_TryGetLine(char *out_line, size_t cap, size_t *out_len);

uint32_t Bsp_Usart_Log_GetRxDropCount(void);
uint32_t Bsp_Usart_Log_GetRxErrorCount(void);

#endif /* BSP_USART_LOG_H */
