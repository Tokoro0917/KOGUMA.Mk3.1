/*
 * run_params.h
 * 区画の大きさと、走る動きの値
 *
 * ToraNeko.Mk3 の App/config/run_params.h と同じ役目のファイル。
 * 今は、迷路の計算（maze/dijkstra.c の走る時間の見積もり）が使う値だけを置いている（ステップ8）。
 * 探索の速さ・スラローム・その場旋回などの値は、ステップ7 で探索の動きを作るときに、KOGUMA.MkIIl の値から足す。
 */
#ifndef CONFIG_RUN_PARAMS_H
#define CONFIG_RUN_PARAMS_H

/* ---- 区画と、スタートの位置 ---- */

#define HALF_CELL_MM 90.0f   /* 区画の半分 [mm]（クラシックの 1 区画は 180mm） */
#define CELL_MM      180.0f  /* 1 区画 [mm] */

/* スタート区画で、機体のお尻を後ろの壁に付けて置いたとき、タイヤの軸が区画のまん中から何 mm 後ろにあるか。
 * KOGUMA.MkIIl のダイクストラ（Maze.c の Maze_Dijkstra_Set_Run）は、スタートから区画の境目までを 114mm としている。
 * 区画のまん中から境目までは 90mm なので、114 − 90 = 24mm。ステップ7 で実機で合わせる */
#define START_OFFSET_MM 24.0f

/* ---- 最短走行の止まり方 ---- */

/* ゴールにターンで入って終わるときに止まる加速度 [mm/s²]。
 * KOGUMA.MkIIl のダイクストラの見積もり（30000mm/s² で止まる）と同じ値。ステップ10 で、本当に走る値に合わせる */
#define SHORTEST_STOP_ACCEL_MM_S2 30000.0f

#endif /* CONFIG_RUN_PARAMS_H */
