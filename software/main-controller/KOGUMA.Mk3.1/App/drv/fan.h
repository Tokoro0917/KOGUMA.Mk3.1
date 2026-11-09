/*
 * fan.h
 * 吸引ファン（中身は fan.c）。かける電圧 [V] で決める（電池の電圧が変わっても同じになるようにする）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/fan.h（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：なし。
 */
#ifndef DRV_FAN_H
#define DRV_FAN_H

void fan_init(void);
void fan_set_voltage(float voltage_v);
void fan_stop(void);
float fan_get_voltage(void);
void fan_update(void);

#endif /* DRV_FAN_H */
