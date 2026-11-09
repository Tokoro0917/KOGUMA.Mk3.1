/*
 * adjust.h
 * 動きを 1 つずつ走らせて調整する（モード2・4・6。MODES.md）
 *
 * ToraNeko.Mk3 の App/sys/adjust.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で、吸引探索のスラローム（モード2 No.3）と、
 * モーターの向きの確かめ（モード6 No.2）を足した。
 */
#ifndef SYS_ADJUST_H
#define SYS_ADJUST_H

void adjust_straight(void);
void adjust_pivot_180(void);
void adjust_slalom_90(void);
void adjust_suction_slalom_90(void);
void adjust_motor_check(void);
void adjust_straight_along_wall(void);
void adjust_uturn(void);
void adjust_front_align(void);
void adjust_print_last_result(void);
void adjust_suction_on(void);
void adjust_accel_test(int number);

#endif /* SYS_ADJUST_H */
