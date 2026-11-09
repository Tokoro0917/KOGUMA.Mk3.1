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
 * 実機で走らせて、ずれる分を下の「実機で足す前距離・後距離」に入れる（モード3 のターンの調整で）。
 *
 * ■ ターンの組は 3 つ（KOGUMA.Mk3.1 で足した。ToraNeko.Mk3 は 1 つ）
 * KOGUMA.MkIIl は、2000mm/s で曲がるターン（G_Turn2000）、2400mm/s で曲がるターン（G_Turn2400）、1000mm/s で曲がるターン（G_Turn1000）を持っていた。
 *   組 2000：下の BIG90_* など（ToraNeko.Mk3 と同じ名前）
 *   組 2400：下の BIG90_2400_* など（名前の頭の後ろに _2400 を付けた）
 *   組 1000：下の BIG90_1000_* など（KOGUMA.MkIIl の G_Turn1000。探索の既知区間を斜めで走るとき。モード0 No.12）
 * どちらの組で曲がるかは、最短走行の No. で決まる（ctrl/run.c の run_fast_set_turn_set()、sys/shortest.c）。
 * 走る時間の見積もり（maze/dijkstra.c）は、どちらの組でも組 2000 の形を、ターンの速さに合わせて縮めて数える
 * （組ごとの形の違いは、道の選び方にはあまり効かないため）。
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

/* ---- 組 2000 ----
 * ターンごとの、決めた速さ [mm/s] と角加速度 [deg/s²]（KOGUMA の G_Turn2000 の w_ac × 2/π。先頭のコメント） */
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

/* ---- 組 2400（KOGUMA の G_Turn2400。最高角速度は w、角加速度は w_ac × 2/π） ---- */
#define BIG90_2400_SPEED_MM_S     2400.0f
#define BIG90_2400_ALPHA_DEG_S2   50930.0f
#define BIG180_2400_SPEED_MM_S    2400.0f
#define BIG180_2400_ALPHA_DEG_S2  44563.0f
#define IN45_2400_SPEED_MM_S      2400.0f
#define IN45_2400_ALPHA_DEG_S2    101859.0f
#define IN135_2400_SPEED_MM_S     2400.0f
#define IN135_2400_ALPHA_DEG_S2   50930.0f  /* KOGUMA の 55000 × 2/π（35014）では後距離が − になるので、斜め出 135° と同じにした */
#define OUT45_2400_SPEED_MM_S     2400.0f
#define OUT45_2400_ALPHA_DEG_S2   45837.0f
#define OUT135_2400_SPEED_MM_S    2400.0f
#define OUT135_2400_ALPHA_DEG_S2  50930.0f
#define V90_2400_SPEED_MM_S       2400.0f
#define V90_2400_ALPHA_DEG_S2     95493.0f

#define BIG90_2400_OMEGA_DEG_S 1600.0f
#define BIG90_2400_PRE_MM 55.2f
#define BIG90_2400_POST_MM 55.4f

#define BIG180_2400_OMEGA_DEG_S 1588.2f  /* 横がちょうど 180mm になる値（--calc。KOGUMA は 1470） */
#define BIG180_2400_PRE_MM 3.0f
#define BIG180_2400_POST_MM 4.1f

#define IN45_2400_OMEGA_DEG_S 2500.0f
#define IN45_2400_PRE_MM 39.0f
#define IN45_2400_POST_MM 76.1f

#define IN135_2400_OMEGA_DEG_S 2000.0f
#define IN135_2400_PRE_MM 46.3f
#define IN135_2400_POST_MM 31.4f

#define OUT45_2400_OMEGA_DEG_S 1320.0f
#define OUT45_2400_PRE_MM 50.2f
#define OUT45_2400_POST_MM 13.7f

#define OUT135_2400_OMEGA_DEG_S 2100.0f
#define OUT135_2400_PRE_MM 34.7f
#define OUT135_2400_POST_MM 51.2f

#define V90_2400_OMEGA_DEG_S 2600.0f
#define V90_2400_PRE_MM 40.2f
#define V90_2400_POST_MM 40.8f

/* ---- 組 1000（KOGUMA の Move.c の G_Turn1000。最高角速度は w、角加速度は w_ac × 2/π） ----
 * 探索の既知区間を斜めで走るとき（モード0 No.12）に、斜め入り・斜め出・V90 で使う（吸引探索の速さ 1000mm/s で曲がる）。
 * 大回り 90°・180° は、既知区間では使わない（既知区間は小回りで走る。maze/path.c の convert_known_to_diagonal()）が、
 * 組 2000・組 2400 と同じ形にするために置いてある */
#define BIG90_1000_SPEED_MM_S     1000.0f
#define BIG90_1000_ALPHA_DEG_S2   12732.0f
#define BIG180_1000_SPEED_MM_S    1000.0f
#define BIG180_1000_ALPHA_DEG_S2  8276.0f
#define IN45_1000_SPEED_MM_S      1000.0f
#define IN45_1000_ALPHA_DEG_S2    12732.0f
#define IN135_1000_SPEED_MM_S     1000.0f
#define IN135_1000_ALPHA_DEG_S2   12732.0f
#define OUT45_1000_SPEED_MM_S     1000.0f
#define OUT45_1000_ALPHA_DEG_S2   12732.0f
#define OUT135_1000_SPEED_MM_S    1000.0f
#define OUT135_1000_ALPHA_DEG_S2  12732.0f
#define V90_1000_SPEED_MM_S       1000.0f
#define V90_1000_ALPHA_DEG_S2     15915.0f

#define BIG90_1000_OMEGA_DEG_S 750.0f
#define BIG90_1000_PRE_MM 73.0f
#define BIG90_1000_POST_MM 73.3f

#define BIG180_1000_OMEGA_DEG_S 658.4f  /* 横がちょうど 180mm になる値（--calc。KOGUMA は 680） */
#define BIG180_1000_PRE_MM 3.0f
#define BIG180_1000_POST_MM 3.4f

#define IN45_1000_OMEGA_DEG_S 800.0f
#define IN45_1000_PRE_MM 28.8f
#define IN45_1000_POST_MM 66.4f

#define IN135_1000_OMEGA_DEG_S 800.0f
#define IN135_1000_PRE_MM 60.9f
#define IN135_1000_POST_MM 46.1f

#define OUT45_1000_OMEGA_DEG_S 700.0f
#define OUT45_1000_PRE_MM 65.9f
#define OUT45_1000_POST_MM 28.8f

#define OUT135_1000_OMEGA_DEG_S 800.0f
#define OUT135_1000_PRE_MM 45.5f
#define OUT135_1000_POST_MM 61.5f

#define V90_1000_OMEGA_DEG_S 1000.0f
#define V90_1000_PRE_MM 36.6f
#define V90_1000_POST_MM 36.9f

/* 実機で足す前距離・後距離 [mm]（ToraNeko.Mk3 と同じ考え方。モード3 で実機で合わせる。今は 0）。組 2000 */
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
#define V90_POST_ADJUST_MM     0.0f

/* 同じく、組 2400 */
#define BIG90_2400_PRE_ADJUST_MM    0.0f
#define BIG90_2400_POST_ADJUST_MM   0.0f
#define BIG180_2400_PRE_ADJUST_MM   0.0f
#define BIG180_2400_POST_ADJUST_MM  0.0f
#define IN45_2400_PRE_ADJUST_MM     0.0f
#define IN45_2400_POST_ADJUST_MM    0.0f
#define IN135_2400_PRE_ADJUST_MM    0.0f
#define IN135_2400_POST_ADJUST_MM   0.0f
#define OUT45_2400_PRE_ADJUST_MM    0.0f
#define OUT45_2400_POST_ADJUST_MM   0.0f
#define OUT135_2400_PRE_ADJUST_MM   0.0f
#define OUT135_2400_POST_ADJUST_MM  0.0f
#define V90_2400_PRE_ADJUST_MM      0.0f
#define V90_2400_POST_ADJUST_MM     0.0f

/* 同じく、組 1000 */
#define BIG90_1000_PRE_ADJUST_MM    0.0f
#define BIG90_1000_POST_ADJUST_MM   0.0f
#define BIG180_1000_PRE_ADJUST_MM   0.0f
#define BIG180_1000_POST_ADJUST_MM  0.0f
#define IN45_1000_PRE_ADJUST_MM     0.0f
#define IN45_1000_POST_ADJUST_MM    0.0f
#define IN135_1000_PRE_ADJUST_MM    0.0f
#define IN135_1000_POST_ADJUST_MM   0.0f
#define OUT45_1000_PRE_ADJUST_MM    0.0f
#define OUT45_1000_POST_ADJUST_MM   0.0f
#define OUT135_1000_PRE_ADJUST_MM   0.0f
#define OUT135_1000_POST_ADJUST_MM  0.0f
#define V90_1000_PRE_ADJUST_MM      0.0f
#define V90_1000_POST_ADJUST_MM     0.0f

#endif /* CONFIG_TURN_PARAMS_H */
