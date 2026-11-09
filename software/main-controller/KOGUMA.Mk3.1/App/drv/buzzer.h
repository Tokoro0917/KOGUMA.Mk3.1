/*
 * buzzer.h
 * ブザー（TIM4 CH4 = PB9 の PWM）で音を鳴らす
 *
 * ToraNeko.Mk3 にはない（ToraNeko はブザーがない）。KOGUMA.MkIIl の UI.c の鳴らし方を、ToraNeko の書き方にした。
 */
#ifndef DRV_BUZZER_H
#define DRV_BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_on(uint32_t freq_hz);
void buzzer_off(void);
void buzzer_beep(uint32_t freq_hz, uint32_t length_ms);

#endif /* DRV_BUZZER_H */
