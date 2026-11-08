/*
 * turn_params.h
 * 最短走行のターン（大回り・斜め）の値
 *
 * ToraNeko.Mk3 の App/config/turn_params.h と同じ形（名前）にしてある。
 * ターンは「前距離（まっすぐ）→ スラローム（前に進む速さは一定、角速度は台形）→ 後距離（まっすぐ）」。
 *
 * ■ 今の値は仮（ステップ8）
 * 今は、迷路の計算（maze/dijkstra.c の走る時間の見積もり）が使うだけ。
 * KOGUMA.MkIIl の 2000 のターンの値（Core/Src/ShortParam.c の G_Turn2000）を、そのまま入れた：
 *   前距離 = st、後距離 = end、最高角速度 = w、角加速度 = w_ac、速さ = 2000mm/s。
 * KOGUMA の角速度は cos の形で上げ下げし、ToraNeko.Mk3 は台形なので、同じ値でも曲がり方は少し違う。
 * 道の選び方（どちらの道が速いか）に使うだけなら、この違いは小さい。
 * ステップ10 で最短走行を作るときに、ToraNeko.Mk3 と同じく test/test_turn.c --calc で計算し直し、
 * 2400 のターンの値（G_Turn2400）も入れる。
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

/* ターンごとの、決めた速さ [mm/s] と角加速度 [deg/s²]（KOGUMA の G_Turn2000 の w_ac） */
#define BIG90_SPEED_MM_S    2000.0f
#define BIG90_ALPHA_DEG_S2  40000.0f
#define BIG180_SPEED_MM_S   2000.0f
#define BIG180_ALPHA_DEG_S2 45000.0f
#define IN45_SPEED_MM_S     2000.0f
#define IN45_ALPHA_DEG_S2   120000.0f
#define IN135_SPEED_MM_S    2000.0f
#define IN135_ALPHA_DEG_S2  80000.0f
#define OUT45_SPEED_MM_S    2000.0f
#define OUT45_ALPHA_DEG_S2  50000.0f
#define OUT135_SPEED_MM_S   2000.0f
#define OUT135_ALPHA_DEG_S2 80000.0f
#define V90_SPEED_MM_S      2000.0f
#define V90_ALPHA_DEG_S2    130000.0f

/* ターンごとの最高角速度 [deg/s]、前距離・後距離 [mm]（KOGUMA の G_Turn2000 の w、st、end） */
#define BIG90_OMEGA_DEG_S 2000.0f
#define BIG90_PRE_MM 14.0f
#define BIG90_POST_MM 75.0f

#define BIG180_OMEGA_DEG_S 1280.0f
#define BIG180_PRE_MM 5.0f
#define BIG180_POST_MM 67.0f

#define IN45_OMEGA_DEG_S 1900.0f
#define IN45_PRE_MM 20.0f
#define IN45_POST_MM 107.0f

#define IN135_OMEGA_DEG_S 1500.0f
#define IN135_PRE_MM 37.0f
#define IN135_POST_MM 80.0f

#define OUT45_OMEGA_DEG_S 1100.0f
#define OUT45_PRE_MM 23.0f
#define OUT45_POST_MM 45.0f

#define OUT135_OMEGA_DEG_S 1420.0f
#define OUT135_PRE_MM 5.0f
#define OUT135_POST_MM 85.0f

#define V90_OMEGA_DEG_S 2000.0f
#define V90_PRE_MM 24.0f
#define V90_POST_MM 92.0f

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
