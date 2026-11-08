/*
 * suction.h
 * 吸引ファンを、少しずつ上げて回し始める（調整の動き・吸引探索・ターンの調整で使う）
 * ToraNeko.Mk3 の App/sys/suction.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で変えたところ：suction_ramp_up_to() を足した（吸引の強さは config/run_params.h）。
 */
#ifndef SYS_SUCTION_H
#define SYS_SUCTION_H

void suction_ramp_up(void);
void suction_ramp_up_to(float voltage_v);

#endif /* SYS_SUCTION_H */
