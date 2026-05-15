/**
 * @file    bsp_usart_log.c
 * @brief   USART1 异步日志与命令行实现
 * @note    依赖 CubeMX 生成的 huart1（usart.c）。
 *          TX 用 HAL_UART_Transmit_IT 一次取 1 段连续字节推送，
 *          完成回调 HAL_UART_TxCpltCallback 中再启动下一段。
 *          RX 主循环里轮询 huart1.RxXferCount，简化为 1 字节单元接收回调。
 */
#include "bsp_usart_log.h"
#include "ring_buffer.h"
#include "usart.h"
#include "stm32f4xx_hal.h"

/* ---- TX 状态 ---- */
static uint8_t      s_tx_storage[BSP_USART_LOG_TX_SIZE];
static Ring_Buffer  s_tx_rb;
static uint8_t      s_tx_dma_buf[64];     /* 一次最多推送的连续段 */
static volatile bool s_tx_busy = false;

/* ---- RX 状态 ---- */
static uint8_t      s_rx_storage[BSP_USART_LOG_RX_SIZE];
static Ring_Buffer  s_rx_rb;
static uint8_t      s_rx_byte;            /* HAL 单字节接收落地缓冲 */

void Bsp_Usart_Log_Init(void) {
    Ring_Buffer_Init(&s_tx_rb, s_tx_storage, BSP_USART_LOG_TX_SIZE);
    Ring_Buffer_Init(&s_rx_rb, s_rx_storage, BSP_USART_LOG_RX_SIZE);
    s_tx_busy = false;
    (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
}

size_t Bsp_Usart_Log_Write(const uint8_t *data, size_t len) {
    size_t pushed = Ring_Buffer_PushN(&s_tx_rb, data, len);
    return pushed;
}

size_t Bsp_Usart_Log_WriteString(const char *s) {
    if (s == 0) return 0U;
    size_t n = 0U;
    while (s[n] != '\0') { n++; }
    return Bsp_Usart_Log_Write((const uint8_t *)s, n);
}

void Bsp_Usart_Log_TxFlush(void) {
    if (s_tx_busy) {
        return;
    }
    if (Ring_Buffer_IsEmpty(&s_tx_rb)) {
        return;
    }
    /* 一次最多搬 sizeof(s_tx_dma_buf) 字节，给 HAL_UART_Transmit_IT */
    size_t n = Ring_Buffer_PopN(&s_tx_rb, s_tx_dma_buf, sizeof(s_tx_dma_buf));
    if (n == 0U) {
        return;
    }
    s_tx_busy = true;
    if (HAL_UART_Transmit_IT(&huart1, s_tx_dma_buf, (uint16_t)n) != HAL_OK) {
        s_tx_busy = false;  /* 失败：让下次 flush 再试 */
    }
}

bool Bsp_Usart_Log_TryGetLine(char *out_line, size_t cap, size_t *out_len) {
    if ((out_line == 0) || (cap < 2U)) {
        return false;
    }
    /* 先窥探缓冲，找到 \r 或 \n 才确认成行；找不到不消费数据 */
    uint16_t count = Ring_Buffer_Count(&s_rx_rb);
    if (count == 0U) {
        return false;
    }
    /* 简单实现：peek 一遍找终止符 */
    Ring_Buffer *rb = &s_rx_rb;
    uint16_t scan_idx = rb->tail;
    bool   line_ready = false;
    uint16_t line_bytes = 0U;
    for (uint16_t i = 0U; i < count; i++) {
        uint8_t b = rb->buf[scan_idx];
        scan_idx = (uint16_t)((scan_idx + 1U) & rb->mask);
        if ((b == '\r') || (b == '\n')) {
            line_ready = true;
            break;
        }
        line_bytes++;
    }
    if (!line_ready) {
        /* 缓冲快满但还没遇到终止符 → 强制成行，避免一直占用 */
        if (count >= (BSP_USART_LOG_RX_SIZE - 2U)) {
            line_ready = true;
        } else {
            return false;
        }
    }
    /* 消费：把数据搬到 out_line（裁到 cap-1） */
    size_t copied = 0U;
    for (uint16_t i = 0U; i < line_bytes; i++) {
        uint8_t b = 0U;
        (void)Ring_Buffer_Pop(rb, &b);
        if (copied < (cap - 1U)) {
            out_line[copied++] = (char)b;
        }
    }
    /* 吃掉终止符 */
    uint8_t term = 0U;
    if (line_bytes < count) {
        (void)Ring_Buffer_Pop(rb, &term);
        /* 若是 \r\n 组合，把后续 \n 也吃掉 */
        if (term == '\r') {
            if (!Ring_Buffer_IsEmpty(rb)) {
                uint8_t peek_next = rb->buf[rb->tail];
                if (peek_next == '\n') {
                    (void)Ring_Buffer_Pop(rb, &term);
                }
            }
        }
    }
    out_line[copied] = '\0';
    if (out_len != 0) {
        *out_len = copied;
    }
    return true;
}

void Bsp_Usart_Log_OnRxByte(uint8_t byte) {
    (void)Ring_Buffer_Push(&s_rx_rb, byte);
}

/* ---- HAL 回调 ---- */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        s_tx_busy = false;
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        Bsp_Usart_Log_OnRxByte(s_rx_byte);
        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        /* 收包错误（ORE/NE/FE/PE）：清错并重新挂接收 */
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        (void)HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1U);
    }
}

/* ---- printf 重定向钩子（syscalls.c _write 调用本函数） ---- */
int __io_putchar(int ch) {
    uint8_t b = (uint8_t)ch;
    (void)Bsp_Usart_Log_Write(&b, 1U);
    return ch;
}
