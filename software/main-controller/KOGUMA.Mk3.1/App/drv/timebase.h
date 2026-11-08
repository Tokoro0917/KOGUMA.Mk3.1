/*
 * timebase.h
 * 時間の取得
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/timebase.h（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：なし。
 */
#ifndef DRV_TIMEBASE_H
#define DRV_TIMEBASE_H

#include <stdint.h>

void timebase_init(void);
uint32_t timebase_get_ms(void);
uint32_t timebase_get_us(void);
void timebase_wait_ms(uint32_t wait_ms);
uint32_t timebase_get_sysclk_hz(void);

#endif /* DRV_TIMEBASE_H */
