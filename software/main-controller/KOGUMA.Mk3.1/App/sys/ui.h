/*
 * ui.h
 * 機体の操作：LED とブザーの表示と合図、タイヤ・スイッチ・前センサの読み取り（MODES.md の 1〜3）
 *
 * ToraNeko.Mk3 の App/sys/ui.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で変えたところ：LED 8 個の並び、ブザーの音、
 * タイヤ 1 段の量（ui.c の先頭のコメント）。外側・真ん中の LED を点ける関数を、ほかのファイルでも使えるように出した。
 * モードを選ぶ流れ（どのモードで何をするか）は sys/mode.c にある。ここは、その部品。
 */
#ifndef SYS_UI_H
#define SYS_UI_H

#include <stdbool.h>
#include <stdint.h>

/* 入力を読む間隔 [ms]。待つ処理は、この間隔でタイヤ・スイッチ・前センサを読む */
#define UI_POLL_MS 10

/* タイヤを何カウント回すと 1 段（モードや No. が 1 つ）変わるか。
 * KOGUMA.MkIIl と同じ 300（PL_encoder.c の Encorder_count_mode。エンコーダの数え方も同じ。drv/encoder.c）。
 * KOGUMA.MkIIl で使い慣れた量なので、そのままにした */
#define UI_TIRE_COUNT_PER_STEP 300

void ui_init(void);

/* LED の表示 */
void ui_show_mode_number(int mode, int number);
void ui_show_value(int value);
void ui_all_on(void);
void ui_all_off(void);
void ui_set_outer_leds(bool on);
void ui_set_middle_leds(bool on);

/* 合図 */
void ui_blink_all(int times);
void ui_signal_ok(void);
void ui_signal_error(void);
void ui_show_loading(void);
void ui_play_finish(void);

/* 入力 */
void ui_forget_tire_rotation(void);
int ui_read_left_steps(void);
int ui_read_right_steps(void);
bool ui_switch_pushed(void);
void ui_wait_ms(uint32_t wait_ms);

/* 待つ */
bool ui_wait_start(int mode, int number);
void ui_wait_failsafe_reset(void);
void ui_stop_for_low_battery(void);

#endif /* SYS_UI_H */
