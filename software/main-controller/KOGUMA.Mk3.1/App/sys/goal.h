/*
 * goal.h
 * ゴールの位置（MODES.md のモード7 No.2）
 *
 * ToraNeko.Mk3 の App/sys/goal.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で変えたところ：コメントだけ（迷路は 16×16）。
 *
 * ゴールは「左下の区画 (x, y)」と大きさ（幅・高さ）で表す。
 *   - 大きさは 2×2 で決まり（下の GOAL_WIDTH、GOAL_HEIGHT）。違う大きさの大会では、ここを書き換えて書き込み直す
 *   - 位置は機体で入れて Flash に保存し、電源を入れたときに読む。一度も入れていなければ迷路の中央の 2×2
 */
#ifndef SYS_GOAL_H
#define SYS_GOAL_H

#include <stdbool.h>

#include "maze/maze_map.h"

#define GOAL_WIDTH  2  /* ゴールの幅 [区画] */
#define GOAL_HEIGHT 2  /* ゴールの高さ [区画] */

/* 入れられる位置の最大（ゴールが迷路からはみ出さないように） */
#define GOAL_X_MAX (MAZE_SIZE - GOAL_WIDTH)
#define GOAL_Y_MAX (MAZE_SIZE - GOAL_HEIGHT)

/* 一度も入れていないとき（Flash にゴールがないとき）の位置。16×16 の迷路の中央の 2×2（クラシックの大会のゴール）。
 * ToraNeko.Mk3 も同じ (7, 7) */
#define GOAL_DEFAULT_X 7
#define GOAL_DEFAULT_Y 7

void goal_init(void);
int goal_get_x(void);
int goal_get_y(void);
bool goal_is_valid(int goal_x, int goal_y);
bool goal_set(int goal_x, int goal_y);

#endif /* SYS_GOAL_H */
