/*
 * turn_params.h
 * 最短走行のターン（大回り・斜め）の値
 *
 * ToraNeko.Mk3 の App/config/turn_params.h と同じ形（名前）にしてある。
 * ターンは「前距離（まっすぐ）→ スラローム（前に進む速さは一定、角速度は台形）→ 後距離（まっすぐ）」。
 *
 * ■ 今の値は仮（2026-11-08）
 * 速さ・最高角速度は、KOGUMA.MkIIl の 2000 のターンの値（Core/Src/ShortParam.c の G_Turn2000）。
 * 角加速度は、KOGUMA の w_ac × 2/π（KOGUMA は角速度を cos の形で上げ下げし、ToraNeko.Mk3 の ctrl/trajectory.c は台形なので、
 * 上げきるまでの時間が同じになるようにした。config/run_params.h の先頭のコメント）。
 * 前距離・後距離は、この形で PC で計算した値（test/test_turn.c --calc）。大回り 180° の最高角速度も、横がちょうど 1 区画に
 * なるように計算した。KOGUMA.MkIIl の前距離・後距離（st・end。実機で合わせた値）とは違う。
 * ステップ10 で最短走行を作るときに、実機で走らせて、ずれる分を下の「実機で足す前距離・後距離」に入れる。
 * 2400 のターンの値（G_Turn2400）も、そのときに入れる。
 *
 * ■ 値の決め方
 *   1. ターンごとの速さ・最高角速度・角加速度を決める
 *   2. software/main-controller/test で make test_turn && ./test_turn --calc を実行し、出た行を下に貼る
 *   3. 実機で走らせて、ずれる分を *_PRE_ADJUST_MM・*_POST_ADJUST_MM に入れる（2 の計算の値はそのままにしておく）
 *
 * ■ 決めた速さより速く・遅く曲がるとき
 * 時間の見積もりでは、決めた速さ（*_SPEED_MM_S）と違う速さで曲がるときは、同じ形のまま時間だけが
 *（決めた速さ ÷ 速さ）倍になるとする（maze/dijkstra.c の turn_time_s()）。
 */
#ifndef CONFIG_TURN_PARAMS_H
#define CONFIG_TURN_PARAMS_H

/* 最短走行の経路を選ぶとき（maze/dijkstra.c の走る時間の見積もり）に、どのターンも曲がるとする速さ [mm/s]。
 * KOGUMA.MkIIl のモード1 No.5（コーナー 2000）と同じ */
#define FAST_TURN_SPEED_MM_S 2000.0f

#define DIAG_STEP_MM 127.28f  /* 斜めの直進の 1 つぶん [mm]（壁の辺のまん中から、となりの辺のまん中まで = 90 × √2） */

/* 大回り 180° の幅を、1 区画（180mm）よりこれだけ広く計算する [mm]（ToraNeko.Mk3 と同じ。実機で内側を走るとき。今は 0） */
#define BIG180_WIDTH_ADJUST_MM 0.0f

/* ターンごとの、決めた速さ [mm/s] と角加速度 [deg/s²]（KOGUMA の G_Turn2000 の w_ac × 2/π。先頭のコメント） */
#define BIG90_SPEED_MM_S    2000.0f
#define BIG90_ALPHA_DEG_S2  25465.0f
#define BIG180_SPEED_MM_S   2000.0f
#define BIG180_ALPHA_DEG_S2 28648.0f
#define IN45_SPEED_MM_S     2000.0f
#define IN45_ALPHA_DEG_S2   76394.0f
#define IN135_SPEED_MM_S    2000.0f
#define IN135_ALPHA_DEG_S2  50930.0f
#define OUT45_SPEED_MM_S    2000.0f
#define OUT45_ALPHA_DEG_S2  31831.0f
#define OUT135_SPEED_MM_S   2000.0f
#define OUT135_ALPHA_DEG_S2 50930.0f
#define V90_SPEED_MM_S      2000.0f
#define V90_ALPHA_DEG_S2    82761.0f

/* ターンごとの最高角速度 [deg/s]（KOGUMA の G_Turn2000 の w）、前距離・後距離 [mm]（test/test_turn.c --calc） */
#define BIG90_OMEGA_DEG_S 2000.0f
#define BIG90_PRE_MM 39.6f
#define BIG90_POST_MM 40.2f

#define BIG180_OMEGA_DEG_S 1333.9f  /* 横がちょうど 180mm になる値（--calc。KOGUMA は 1280） */
#define BIG180_PRE_MM 3.0f
#define BIG180_POST_MM 4.0f

#define IN45_OMEGA_DEG_S 1900.0f
#define IN45_PRE_MM 40.7f
#define IN45_POST_MM 78.9f

#define IN135_OMEGA_DEG_S 1500.0f
#define IN135_PRE_MM 52.9f
#define IN135_POST_MM 38.3f

#define OUT45_OMEGA_DEG_S 1100.0f
#define OUT45_PRE_MM 50.0f
#define OUT45_POST_MM 13.5f

#define OUT135_OMEGA_DEG_S 1420.0f
#define OUT135_PRE_MM 29.2f
#define OUT135_POST_MM 45.5f

#define V90_OMEGA_DEG_S 2000.0f
#define V90_PRE_MM 45.4f
#define V90_POST_MM 45.6f

/* 実機で足す前距離・後距離 [mm]（ToraNeko.Mk3 と同じ考え方。ステップ10 で実機で合わせる。今は 0） */
#define BIG90_PRE_ADJUST_MM   0.0f
#define BIG90_POST_ADJUST_MM  0.0f
#define BIG180_PRE_ADJUST_MM  0.0f
#define BIG180_POST_ADJUST_MM 0.0f
#define IN45_PRE_ADJUST_MM    0.0f
#define IN45_POST_ADJUST_MM   0.0f
#define IN135_PRE_ADJUST_MM   0.0f
#define IN135_POST_ADJUST_MM  0.0f
#define OUT45_PRE_ADJUST_MM   0.0f
#define OUT45_POST_ADJUST_MM  0.0f
#define OUT135_PRE_ADJUST_MM  0.0f
#define OUT135_POST_ADJUST_MM 0.0f
#define V90_PRE_ADJUST_MM     0.0f
#define V90_POST_ADJUST_MM    0.0f

#endif /* CONFIG_TURN_PARAMS_H */
