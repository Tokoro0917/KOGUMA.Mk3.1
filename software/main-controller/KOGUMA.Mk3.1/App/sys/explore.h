/*
 * explore.h
 * 探索走行（モード0）：迷路の処理（maze/）に次の行き先を聞き、機体を動かす処理（ctrl/run）で 1 区画ずつ動かす
 *
 * ToraNeko.Mk3 の App/sys/explore.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で変えたところ：
 * エセ全面探索（EXPLORE_ALL_CELLS。KOGUMA.MkIIl のモード0 No.2・5）を足し、吸引するか（suction）を引数で選べるようにした。
 *
 * ■ 走り方（MODES.md のモード0）
 *   EXPLORE_ROUND_TRIP    往復      ：スタート → ゴール → スタート（No.1・4）
 *   EXPLORE_ALL_CELLS     エセ全面探索：スタート → ゴール → まだ入っていない区画を、行き止まり潰しで外しながら
 *                                     全部見に行く → スタート（No.2・5）
 *   EXPLORE_ONE_WAY       片道      ：スタート → ゴール。ゴールの区画の真ん中で止まる（No.3）
 *   EXPLORE_UNKNOWN_WALLS 未知壁探索：スタート → ゴール → 最短経路の上の見ていない壁を見に行く → スタート（No.6）
 *   EXPLORE_UNKNOWN_WALLS_KNOWN 未知壁探索＋既知区間加速：No.6 と同じ道を通り、行きと帰りで、見たことのある区画が
 *                                 まっすぐ 2 区画以上続くところは、まとめて速く走る（No.7）
 *   EXPLORE_UNKNOWN_WALLS_KNOWN_DIAG    No.7 と同じだが、見たことのある区画の経路にジグザグがあれば、
 *                                 斜めで走る（KOGUMA.MkIIl のモード0 No.12。吸引探索で使う）
 *   suction が true なら吸引探索（1000mm/s、吸引 5.5V）、false ならスラローム探索（500mm/s、吸引なし）
 *
 * ■ 使い方
 * 機体をスタート区画に北向きに置いてから explore_run() を呼ぶ。走り終わるまで戻ってこない。
 * 区画のまん中で止まっているところから、スタートへ帰るときは explore_return_from()（最短走行のあと。モード0 No.11）。
 * 迷路は、今 RAM にある迷路の続きから使う（電源を入れた直後なら空の迷路）。
 */
#ifndef SYS_EXPLORE_H
#define SYS_EXPLORE_H

#include <stdbool.h>

#include "maze/maze_map.h"

/* 探索の走り方 */
typedef enum {
  EXPLORE_ROUND_TRIP,     /* 往復 */
  EXPLORE_ALL_CELLS,      /* エセ全面探索（KOGUMA.Mk3.1 で足した） */
  EXPLORE_ONE_WAY,        /* 片道 */
  EXPLORE_UNKNOWN_WALLS,  /* 未知壁探索 */
  EXPLORE_UNKNOWN_WALLS_KNOWN,  /* 未知壁探索＋既知区間加速 */
  EXPLORE_UNKNOWN_WALLS_KNOWN_DIAG,  /* 未知壁探索＋既知区間加速、既知区間を斜めで（KOGUMA.Mk3.1 で足した。モード0 No.12） */
} explore_type_t;

/* 探索の結果 */
typedef enum {
  EXPLORE_RESULT_DONE,      /* 最後まで走った（往復・未知壁探索はスタート、片道はゴールの区画の真ん中で止まっている） */
  EXPLORE_RESULT_FAILSAFE,  /* フェイルセーフで止まった（最後に壁を記録した 5 区画ぶんを戻してから、迷路を保存した） */
  EXPLORE_RESULT_NO_ROUTE,  /* 目的地（行きはゴール、帰りはスタート）へ行く道がなくなった（壁の読み間違いなど）、
                             * または動きすぎた。その場で止まっている。迷路は保存しない */
} explore_result_t;

explore_result_t explore_run(explore_type_t type, bool suction, int goal_x, int goal_y, int goal_width,
                             int goal_height);
explore_result_t explore_return_from(explore_type_t type, bool suction, int x, int y, direction_t dir, int goal_x,
                                     int goal_y, int goal_width, int goal_height);

#endif /* SYS_EXPLORE_H */
