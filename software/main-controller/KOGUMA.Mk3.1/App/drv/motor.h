/*
 * motor.h
 * 走行モーターの出力（モータードライバ ×2、TIM3 の PWM）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/motor.h（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：TIM4 → TIM3。
 */
#ifndef DRV_MOTOR_H
#define DRV_MOTOR_H

void motor_init(void);
void motor_set_voltage(float left_v, float right_v);
void motor_stop(void);

#endif /* DRV_MOTOR_H */
