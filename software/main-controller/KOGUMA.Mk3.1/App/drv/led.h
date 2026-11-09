/*
 * led.h
 * LED の点灯・消灯
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/led.h（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 * 表示 LED を 7 個から 8 個（LED1〜LED8）にした。壁の表示の LED（LEDF1〜LEDF4。ToraNeko にはない）を足した。
 */
#ifndef DRV_LED_H
#define DRV_LED_H

#include <stdbool.h>
#include <stdint.h>

#define LED_COUNT 8       /* 表示 LED（LED1〜LED8）の個数 */
#define WALL_LED_COUNT 4  /* 壁の表示の LED（LEDF1〜LEDF4）の個数 */

void led_init(void);
void led_set(uint8_t num, bool on);
void led_set_wall(uint8_t num, bool on);
void led_all_off(void);

#endif /* DRV_LED_H */
