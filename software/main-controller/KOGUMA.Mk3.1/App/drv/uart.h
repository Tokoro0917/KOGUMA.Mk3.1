/*
 * uart.h
 * ログ送信（USART2 の TX = PA2 → STLINK-V3 の仮想COMポート → PC）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/uart.h（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：USART1 → USART2（KOGUMA の機体のピン）。
 *
 * printf で書いた文字もここから送られる。
 */
#ifndef DRV_UART_H
#define DRV_UART_H

#include <stdint.h>

void uart_init(void);
void uart_write(const uint8_t *data, uint32_t len);
void uart_wait_free(uint32_t count);
uint32_t uart_get_dropped_count(void);

#endif /* DRV_UART_H */
