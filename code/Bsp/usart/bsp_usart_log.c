/*
 * @File         : \code\Bsp\usart\bsp_usart_log.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : USART1 异步日志（TX 环形 + IT）与命令行 RX 实现
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
#include "bsp_usart_log.h"
#include "ring_buffer.h"
#include "usart.h"
#include "stm32f4xx_hal.h"

/* ==== TX 状态 ===================================================== */
static uint8_t       s_tx_storage[BSP_USART_LOG_TX_SIZE]; /**< TX 环形存储区          */
static Ring_Buffer   s_tx_rb;                             /**< TX 环形控制块          */
static uint8_t       s_tx_dma_buf[64];                    /**< 单次推送的连续段缓冲   */
static volatile bool s_tx_busy = false;                   /**< HAL 是否正在搬当前段   */

/* ==== RX 状态 ===================================================== */
static uint8_t     s_rx_storage[BSP_USART_LOG_RX_SIZE]; /**< RX 环形存储区          */
static Ring_Buffer s_rx_rb;                             /**< RX 环形控制块          */
static uint8_t     s_rx_byte;                           /**< HAL 单字节落地缓冲     */

/**
 * @brief   USART 日志模块初始化
 * @note    1) 重置 TX/RX 两个环形；2) 启动一次 HAL_UART_Receive_IT 以
 *          打开接收链路。此后 printf / WriteString 立即可用。
 */
void Bsp_Usart_Log_Init(void)
{
    Ring_Buffer_Init(&s_tx_rb, s_tx_storage, BSP_USART_LOG_TX_SIZE);
    Ring_Buffer_Init(&s_rx_rb, s_rx_storage, BSP_USART_LOG_RX_SIZE);
    s_tx_busy = false;
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
}

/**
 * @brief   异步写入字节流（不阻塞）
 * @param   data  数据起始
 * @param   len   字节数
 * @retval  实际写入字节数（≤ len；不足说明 TX 满，超出被丢弃）
 */
size_t Bsp_Usart_Log_Write(const uint8_t *data, size_t len)
{
    return Ring_Buffer_PushN(&s_tx_rb, data, len);
}

/**
 * @brief   异步写入 \0 结尾字符串（便捷封装）
 * @param   s  字符串；NULL 时直接返回 0
 * @retval  实际写入字节数
 */
size_t Bsp_Usart_Log_WriteString(const char *s)
{
    if (s == 0)
    {
        return 0U;
    }
    size_t n = 0U;
    while (s[n] != '\0')
    {
        n++;
    }
    return Bsp_Usart_Log_Write((const uint8_t *)s, n);
}

/**
 * @brief   推进 TX：从环形缓冲取出一段交给 HAL IT 发送
 * @note    主循环每轮调一次；若 HAL 仍 busy 立刻返回；
 *          单次最多搬 sizeof(s_tx_dma_buf) = 64 B，刚好对应一次 IT 持续 ≈ 5 ms。
 */
void Bsp_Usart_Log_TxFlush(void)
{
    if (s_tx_busy)
    {
        return;
    }
    if (Ring_Buffer_IsEmpty(&s_tx_rb))
    {
        return;
    }
    size_t n = Ring_Buffer_PopN(&s_tx_rb, s_tx_dma_buf, sizeof(s_tx_dma_buf));
    if (n == 0U)
    {
        return;
    }
    s_tx_busy = true;
    if (HAL_UART_Transmit_IT(&huart1, s_tx_dma_buf, (uint16_t)n) != HAL_OK)
    {
        /* 启动失败：清 busy 让下一轮 flush 重试，数据仍在 s_tx_dma_buf 但已
         * Pop 出环形 → 本次小段会丢失，仅在 HAL 故障路径下出现 */
        s_tx_busy = false;
    }
}

/**
 * @brief   尝试从 RX 缓冲取一行
 * @param   out_line  输出缓冲（NULL 或 cap<2 时返回 false）
 * @param   cap       输出缓冲容量
 * @param   out_len   输出长度（不含 \0），允许 NULL
 * @retval  true=取到一行 / false=尚未成行
 * @note    实现策略：先 peek 找终止符，确认成行才 Pop；
 *          缓冲快满 (≥ cap-2) 强制成行避免死等。
 */
bool Bsp_Usart_Log_TryGetLine(char *out_line, size_t cap, size_t *out_len)
{
    if ((out_line == 0) || (cap < 2U))
    {
        return false;
    }
    uint16_t count = Ring_Buffer_Count(&s_rx_rb);
    if (count == 0U)
    {
        return false;
    }

    Ring_Buffer *rb         = &s_rx_rb;
    uint16_t     scan_idx   = rb->tail;
    bool         line_ready = false;
    uint16_t     line_bytes = 0U;
    for (uint16_t i = 0U; i < count; i++)
    {
        uint8_t b = rb->buf[scan_idx];
        scan_idx  = (uint16_t)((scan_idx + 1U) & rb->mask);
        if ((b == '\r') || (b == '\n'))
        {
            line_ready = true;
            break;
        }
        line_bytes++;
    }
    if (!line_ready)
    {
        if (count >= (BSP_USART_LOG_RX_SIZE - 2U))
        {
            line_ready = true; /* 强制成行，防止缓冲死锁 */
        }
        else
        {
            return false;
        }
    }

    size_t copied = 0U;
    for (uint16_t i = 0U; i < line_bytes; i++)
    {
        uint8_t b = 0U;
        (void)Ring_Buffer_Pop(rb, &b);
        if (copied < (cap - 1U))
        {
            out_line[copied++] = (char)b;
        }
    }
    /* 吃掉单个终止符；若是 \r\n 还要吃掉后面的 \n */
    if (line_bytes < count)
    {
        uint8_t term = 0U;
        (void)Ring_Buffer_Pop(rb, &term);
        if (term == '\r')
        {
            if (!Ring_Buffer_IsEmpty(rb))
            {
                uint8_t peek_next = rb->buf[rb->tail];
                if (peek_next == '\n')
                {
                    (void)Ring_Buffer_Pop(rb, &term);
                }
            }
        }
    }
    out_line[copied] = '\0';
    if (out_len != 0)
    {
        *out_len = copied;
    }
    return true;
}

/**
 * @brief   RX 中断回调：HAL 单字节接收完成时调用
 * @param   byte  本次接收字节
 * @note    仅 Push 一个字节；ISR 上下文，禁止重 IO。
 */
void Bsp_Usart_Log_OnRxByte(uint8_t byte)
{
    (void)Ring_Buffer_Push(&s_rx_rb, byte);
}

/* ==== HAL 回调 ==================================================== */

/**
 * @brief   HAL TX 完成回调（弱符号覆盖）
 * @note    清除 busy 标志，下一轮 TxFlush 即可继续推送。
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        s_tx_busy = false;
    }
}

/**
 * @brief   HAL RX 完成回调（弱符号覆盖）
 * @note    把刚收到的字节 push 进环形并立即重新挂起一次单字节 IT 接收。
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        Bsp_Usart_Log_OnRxByte(s_rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
    }
}

/**
 * @brief   HAL UART 错误回调（ORE/NE/FE/PE）
 * @note    清错误标志并重启 RX，确保后续数据不被卡死。
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
    }
}

/* ==== printf 重定向 ============================================== */

/**
 * @brief   syscalls.c 弱符号 _write 调用的 putchar 钩子
 * @param   ch  字符
 * @retval  ch  始终返回字符本身（兼容标准 putchar 语义）
 * @note    把字符塞入 TX 环形即返回；真正的 UART 发送由 TxFlush 推进，
 *          因此 printf 在主循环 / ISR 都是非阻塞的。
 */
int __io_putchar(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)Bsp_Usart_Log_Write(&b, 1U);
    return ch;
}
