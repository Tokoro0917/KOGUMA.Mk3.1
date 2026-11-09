/*
 * switch.h
 * スイッチ S2 の読み取り
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/switch.h（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：スイッチの名前（SW1 → S2）。
 */
#ifndef DRV_SWITCH_H
#define DRV_SWITCH_H

#include <stdbool.h>

bool switch_is_pressed(void);

#endif /* DRV_SWITCH_H */
