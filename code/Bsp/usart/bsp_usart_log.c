/*
 * @File         : \code\Bsp\usart\bsp_usart_log.c
 * @Author       : tonymeng
 * @Date         : 2026-05-15 11:30:00
 * @LastEditors  : tonymeng0910@gmail.com
 * @LastEditTime : 2026-05-15 14:50:00
 * @Description  : USART1 异步日志（TX 环形 + IT）与命令行 RX（IDLE + IT）实现
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
static uint8_t     s_rx_storage[BSP_USART_LOG_RX_SIZE];   /**< 行解析用 RX 环形       */
static Ring_Buffer s_rx_rb;
static uint8_t     s_rx_it_buf[BSP_USART_LOG_RX_CHUNK_SIZE]; /**< IDLE IT 落地缓冲 */
static volatile bool s_rx_segment_ready;                  /**< IDLE/满缓冲帧已入环    */
static uint32_t    s_rx_drop_count;
static uint32_t    s_rx_error_count;

/**
 * @brief   启动或恢复 USART1 IDLE 中断接收
 */
static void Rx_StartIdleIt(void)
{
    if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, s_rx_it_buf, BSP_USART_LOG_RX_CHUNK_SIZE) != HAL_OK)
    {
        huart1.RxState       = HAL_UART_STATE_READY;
        huart1.ReceptionType = HAL_UART_RECEPTION_STANDARD;
        (void)HAL_UARTEx_ReceiveToIdle_IT(&huart1, s_rx_it_buf, BSP_USART_LOG_RX_CHUNK_SIZE);
    }
}

/**
 * @brief   将 IDLE/TC 收到的一帧压入 RX 环
 */
static void Rx_OnChunk(uint16_t nbytes)
{
    if (nbytes == 0U)
    {
        return;
    }
    size_t pushed = Ring_Buffer_PushN(&s_rx_rb, s_rx_it_buf, nbytes);
    if (pushed < nbytes)
    {
        s_rx_drop_count += (uint32_t)(nbytes - pushed);
    }
    s_rx_segment_ready = true;
}

/**
 * @brief   USART 日志模块初始化
 */
void Bsp_Usart_Log_Init(void)
{
    Ring_Buffer_Init(&s_tx_rb, s_tx_storage, BSP_USART_LOG_TX_SIZE);
    Ring_Buffer_Init(&s_rx_rb, s_rx_storage, BSP_USART_LOG_RX_SIZE);
    s_tx_busy          = false;
    s_rx_segment_ready = false;
    s_rx_drop_count    = 0U;
    s_rx_error_count   = 0U;
    Rx_StartIdleIt();
}

/**
 * @brief   异步写入字节流（不阻塞）
 */
size_t Bsp_Usart_Log_Write(const uint8_t *data, size_t len)
{
    return Ring_Buffer_PushN(&s_tx_rb, data, len);
}

/**
 * @brief   异步写入 \0 结尾字符串（便捷封装）
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
        s_tx_busy = false;
    }
}

/**
 * @brief   尝试从 RX 缓冲取一行
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
        if (s_rx_segment_ready)
        {
            line_ready = true; /* USART IDLE：本帧结束，无换行也提交 */
        }
        else if (count >= (BSP_USART_LOG_RX_SIZE - 2U))
        {
            line_ready = true;
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

    if (Ring_Buffer_Count(&s_rx_rb) == 0U)
    {
        s_rx_segment_ready = false;
    }
    else if (line_bytes < count)
    {
        /* 本行以换行结束且环内仍有数据：同一次接收帧内的下一条命令 */
        s_rx_segment_ready = true;
    }
    return true;
}

uint32_t Bsp_Usart_Log_GetRxDropCount(void)
{
    return s_rx_drop_count;
}

uint32_t Bsp_Usart_Log_GetRxErrorCount(void)
{
    return s_rx_error_count;
}

/* ==== HAL 回调 ==================================================== */

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        s_tx_busy = false;
    }
}

/**
 * @brief   IDLE / 缓冲满：一帧接收完成
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    Rx_OnChunk(Size);
    Rx_StartIdleIt();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
    {
        return;
    }
    s_rx_error_count++;
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    (void)HAL_UART_AbortReceive_IT(huart);
    huart->RxState       = HAL_UART_STATE_READY;
    huart->ReceptionType = HAL_UART_RECEPTION_STANDARD;
    Rx_StartIdleIt();
}

/* ==== printf 重定向 ============================================== */

int __io_putchar(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)Bsp_Usart_Log_Write(&b, 1U);
    return ch;
}
