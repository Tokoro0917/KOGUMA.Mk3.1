/*
 * run_params.h
 * 区画の大きさと、走る動きの値
 *
 * ToraNeko.Mk3 の App/config/run_params.h（ecd8e10）と同じ役目・同じ名前のファイル。値は KOGUMA.MkIIl のもの。
 * 使うのは、探索・最短走行の動き（ctrl/run.c）、迷路の計算（maze/dijkstra.c の走る時間の見積もり）、壁センサ（ctrl/wall_ctrl.c）。
 *
 * ■ 探索は 2 とおり（KOGUMA.MkIIl のモード0 と同じ。どちらを使うかは run_search_set_suction()。ctrl/run.c）
 *   吸引なし（スラローム探索。モード0 No.1〜3）：500mm/s（KOGUMA.MkIIl の Move.c の Robot_Maze_Sula_Action）
 *   吸引あり（吸引探索。モード0 No.4〜7）    ：1000mm/s（同じく Robot_Maze_Suction_Action）
 *
 * ■ KOGUMA.MkIIl の角速度の形との違い
 * KOGUMA.MkIIl のスラローム（motor.c の Motor_Sula_COS）は、角速度を cos の形で上げ下げする（w_ac は、いちばん大きい角加速度）。
 * ToraNeko.Mk3 の ctrl/trajectory.c は台形（角加速度は一定）。上げきるまでの時間が同じになるように、
 * 台形の角加速度 ＝ KOGUMA の w_ac × 2/π にした。こうすると、上げているあいだに曲がる角度も同じになる。
 * 前距離・後距離は、この形で PC で計算した値（test/test_turn.c --calc）。KOGUMA.MkIIl の前距離・後距離（実機で合わせた値）とは少し違う。
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

/* ---- 探索（吸引なし）の直進 ---- */

#define SEARCH_SPEED_MM_S  500.0f   /* 探索の速さ [mm/s]（KOGUMA.MkIIl のスラローム探索） */
/* 探索の加速度 [mm/s²]（止まる・止まったところから発進するとき。KOGUMA.MkIIl の Motor_trapezoid(500, 500, 0, 5000, 70)。
 * 止まったところから 90mm で 949mm/s まで上げられるので、半区画あれば 500mm/s になる） */
#define SEARCH_ACCEL_MM_S2 5000.0f

/* ---- 探索（吸引なし）の既知区間加速（仮） ----
 * KOGUMA.MkIIl は、既知区間加速を吸引探索（モード0 No.7）でしか使っていない。吸引なしで使うときのための仮の値 */
#define KNOWN_MAX_SPEED_MM_S 1000.0f  /* 最高速 [mm/s] */
#define KNOWN_ACCEL_MM_S2    5000.0f  /* 加速度 [mm/s²]（500 → 1000mm/s に 75mm。1 区画あれば上げて戻せる） */

/* ---- 探索（吸引なし）のスラローム（小回り 90°） ---- */

#define SLALOM_ANGLE_DEG    90.0f    /* 曲がる角度 [deg] */
#define SLALOM_OMEGA_DEG_S  700.0f   /* 最高の角速度 [deg/s]（KOGUMA.MkIIl の Motor_Sula_COS(500, 90, 700, 10000)） */
#define SLALOM_ALPHA_DEG_S2 6366.0f  /* 角加速度 [deg/s²]（KOGUMA の 10000 × 2/π。先頭のコメント） */
/* 前距離・後距離 [mm]（曲がる前・後の直進。区画の入口の辺から、左・右の区画の入口の辺まで）。
 * test/test_turn.c --calc で計算した値。KOGUMA.MkIIl は前 20mm・後 25mm（実機で合わせた値）で、近い。実機で確かめる（README の「確かめ方」の 11） */
#define SLALOM_PRE_MM  19.3f
#define SLALOM_POST_MM 19.5f

/* ---- 吸引探索の直進・既知区間加速・スラローム（KOGUMA.MkIIl の Robot_Maze_Suction_Action・Robot_Maze_Pass_Action） ---- */

#define SUCTION_SEARCH_SPEED_MM_S  1000.0f   /* 吸引探索の速さ [mm/s] */
#define SUCTION_SEARCH_ACCEL_MM_S2 10000.0f  /* 吸引探索の加速度 [mm/s²]（止まる・発進するとき。止まるのに 50mm） */
#define SUCTION_KNOWN_MAX_SPEED_MM_S 4000.0f   /* 既知区間加速の最高速 [mm/s]（KOGUMA.MkIIl の Motor_trapezoid_PID(1000, 4000, 1000, 15000, …)） */
#define SUCTION_KNOWN_ACCEL_MM_S2    15000.0f  /* 既知区間加速の加速度 [mm/s²]（1000 → 4000mm/s に 500mm） */
#define SUCTION_SLALOM_OMEGA_DEG_S  1640.0f   /* 最高の角速度 [deg/s]（KOGUMA.MkIIl の Motor_Sula_COS(1000, 90, 1640, 60000)） */
#define SUCTION_SLALOM_ALPHA_DEG_S2 38197.0f  /* 角加速度 [deg/s²]（KOGUMA の 60000 × 2/π） */
/* 前距離・後距離 [mm]（test/test_turn.c --calc）。KOGUMA.MkIIl は前 15mm・後 50mm（実機で合わせた値）で、計算とかなり違う。
 * 横向きの加速度が 28600mm/s² と大きく、曲がっているあいだにタイヤが滑って、計算より外へふくらむのだと思う。
 * 吸引探索（ステップ9）を実機で走らせて、合わなければ KOGUMA の値に近づける */
#define SUCTION_SLALOM_PRE_MM  32.3f
#define SUCTION_SLALOM_POST_MM 32.5f

/* ---- 吸引の強さ（sys/suction.c・sys/explore.c・sys/adjust.c） ----
 * KOGUMA.MkIIl の吸引探索は Suction_Start(35)：duty 35%（電池 15.8V のときの割合）なので、15.8 × 0.35 ＝ 5.5V。
 * 調整の動き（モード2・4・6）で吸うときも、この強さ（ToraNeko.Mk3 と同じく 1 つの値） */
#define SUCTION_VOLTAGE_V 5.5f
#define SUCTION_RAMP_MS   500   /* 0 から SUCTION_VOLTAGE_V まで上げるのにかける時間 [ms]（急に回して電池の電圧が落ちないように） */

/* ---- 吸引探索の既知区間を斜めで走る（モード0 No.12。KOGUMA.MkIIl の Robot_Maze_Pass_Action の G_Known_Naname） ----
 * 縦横の直進は既知区間加速と同じ（SUCTION_KNOWN_*）。斜めの直進は KOGUMA.MkIIl の Motor_NANAME_PID(1000, 3000, 1000, 10000)。
 * 斜め入り・斜め出・V90 は、ターンの組 1000（config/turn_params.h）で、吸引探索の速さ（1000mm/s）で曲がる */
#define KNOWN_DIAG_MAX_SPEED_MM_S 3000.0f   /* 斜めの直進の最高速 [mm/s] */
#define KNOWN_DIAG_ACCEL_MM_S2    10000.0f  /* 斜めの直進の加速度 [mm/s²] */

/* ---- その場旋回・前壁合わせ・止まる ---- */

#define PIVOT_OMEGA_DEG_S  600.0f    /* その場旋回の最高の角速度 [deg/s]（KOGUMA.MkIIl の Motor_trapezoid_Turn(90, 600, 20000)） */
#define PIVOT_ALPHA_DEG_S2 20000.0f  /* その場旋回の角加速度 [deg/s²]（同じ） */
#define ALIGN_TIME_MS      500       /* 前壁合わせの時間 [ms]（KOGUMA.MkIIl の Motor_Robot_Alignment も 0.5 秒） */
#define STOP_SETTLE_MS     100       /* 止まったあと、目標の速さ 0 のまま制御を続ける時間 [ms]（ToraNeko.Mk3 と同じ） */

/* ---- 最短走行（モード1。sys/shortest.c）----
 * No. は遅い順（ToraNeko.Mk3 と同じ考え方）。No.4〜9 は KOGUMA.MkIIl のモード1 No.4〜9（ダイクストラ）と同じ値。
 * KOGUMA.MkIIl の No.1〜3（BFS の経路）は持ち込まない（KOGUMA_PORTING.md の B4）ので、No.1〜3 は確かめ用にした：
 *   No.1：小回りだけ（吸引なし。曲がるところは探索と同じ 500mm/s のスラローム）。道を確かめる
 *   No.2・3：大回り・斜め、直進の最高速 ＝ ターンの速さ（区間ごとの加減速がない。KOGUMA.MkIIl のモード3 No.1・2）
 * KOGUMA.MkIIl の直進は「減速は加速の 2 倍」だったが、ToraNeko.Mk3 の台形は加速と減速が同じ。なので同じ加速度でも、
 * 止まる・遅くするのに KOGUMA の 2 倍の距離を使う（実機で走らせて、足りなければ上げる）。
 * TURN は使うターンの組（0 なら小回りだけ、2000・2400 は config/turn_params.h の組）、SUCTION_V は吸引の強さ
 * （KOGUMA.MkIIl の Suction_Start の duty × 15.8V。組 2000 は 50 → 7.9V、組 2400 は 70 → 11.1V） */

/* No.1：小回りだけ（吸引なし） */
#define SHORTEST1_TURN               0
#define SHORTEST1_MAX_SPEED_MM_S     1000.0f
#define SHORTEST1_ACCEL_MM_S2        5000.0f
#define SHORTEST1_DIAG_MAX_SPEED_MM_S 1000.0f  /* 小回りだけなので使わない */
#define SHORTEST1_DIAG_ACCEL_MM_S2   5000.0f
#define SHORTEST1_SUCTION_V          0.0f
/* No.2：ターン 2000、直進も 2000mm/s まで（KOGUMA.MkIIl のモード3 No.1） */
#define SHORTEST2_TURN               2000
#define SHORTEST2_MAX_SPEED_MM_S     2000.0f
#define SHORTEST2_ACCEL_MM_S2        15000.0f
#define SHORTEST2_DIAG_MAX_SPEED_MM_S 2000.0f
#define SHORTEST2_DIAG_ACCEL_MM_S2   10000.0f
#define SHORTEST2_SUCTION_V          7.9f
/* No.3：ターン 2400、直進も 2400mm/s まで（KOGUMA.MkIIl のモード3 No.2） */
#define SHORTEST3_TURN               2400
#define SHORTEST3_MAX_SPEED_MM_S     2400.0f
#define SHORTEST3_ACCEL_MM_S2        20000.0f
#define SHORTEST3_DIAG_MAX_SPEED_MM_S 2400.0f
#define SHORTEST3_DIAG_ACCEL_MM_S2   15000.0f
#define SHORTEST3_SUCTION_V          11.1f
/* No.4：ターン 2000、直線 6000mm/s・15000mm/s²（斜め 10000）（KOGUMA.MkIIl のモード1 No.4） */
#define SHORTEST4_TURN               2000
#define SHORTEST4_MAX_SPEED_MM_S     6000.0f
#define SHORTEST4_ACCEL_MM_S2        15000.0f
#define SHORTEST4_DIAG_MAX_SPEED_MM_S 6000.0f
#define SHORTEST4_DIAG_ACCEL_MM_S2   10000.0f
#define SHORTEST4_SUCTION_V          7.9f
/* No.5：ターン 2000、直線 6000mm/s・20000mm/s²（斜め 15000）（No.5。探索のあとの最短と、未知壁探索の組 1） */
#define SHORTEST5_TURN               2000
#define SHORTEST5_MAX_SPEED_MM_S     6000.0f
#define SHORTEST5_ACCEL_MM_S2        20000.0f
#define SHORTEST5_DIAG_MAX_SPEED_MM_S 6000.0f
#define SHORTEST5_DIAG_ACCEL_MM_S2   15000.0f
#define SHORTEST5_SUCTION_V          7.9f
/* No.6：ターン 2000、直線 6000mm/s・23000mm/s²（斜め 18000）（No.6） */
#define SHORTEST6_TURN               2000
#define SHORTEST6_MAX_SPEED_MM_S     6000.0f
#define SHORTEST6_ACCEL_MM_S2        23000.0f
#define SHORTEST6_DIAG_MAX_SPEED_MM_S 6000.0f
#define SHORTEST6_DIAG_ACCEL_MM_S2   18000.0f
#define SHORTEST6_SUCTION_V          7.9f
/* No.7：ターン 2000、直線 6000mm/s・25000mm/s²（斜め 20000）（No.7） */
#define SHORTEST7_TURN               2000
#define SHORTEST7_MAX_SPEED_MM_S     6000.0f
#define SHORTEST7_ACCEL_MM_S2        25000.0f
#define SHORTEST7_DIAG_MAX_SPEED_MM_S 6000.0f
#define SHORTEST7_DIAG_ACCEL_MM_S2   20000.0f
#define SHORTEST7_SUCTION_V          7.9f
/* No.8：ターン 2400、直線 6000mm/s・20000mm/s²（斜め 15000）（No.8。未知壁探索の組 2） */
#define SHORTEST8_TURN               2400
#define SHORTEST8_MAX_SPEED_MM_S     6000.0f
#define SHORTEST8_ACCEL_MM_S2        20000.0f
#define SHORTEST8_DIAG_MAX_SPEED_MM_S 6000.0f
#define SHORTEST8_DIAG_ACCEL_MM_S2   15000.0f
#define SHORTEST8_SUCTION_V          11.1f
/* No.9：ターン 2400、直線 6000mm/s・23000mm/s²（斜め 18000）（No.9） */
#define SHORTEST9_TURN               2400
#define SHORTEST9_MAX_SPEED_MM_S     6000.0f
#define SHORTEST9_ACCEL_MM_S2        23000.0f
#define SHORTEST9_DIAG_MAX_SPEED_MM_S 6000.0f
#define SHORTEST9_DIAG_ACCEL_MM_S2   18000.0f
#define SHORTEST9_SUCTION_V          11.1f

/* 探索のあとに続けて走る最短走行（モード0 No.8〜12）と、未知壁探索で見る最短走行の組（B15）の No.
 * KOGUMA.MkIIl の G_Short_Run_Set（Core/Src/ShortParam.c）と同じ：組 1 = No.5、組 2 = No.8 */
#define SHORTEST_AFTER_SEARCH_NUMBER 5
#define SHORTEST_PLAN_SET1_NUMBER    5
#define SHORTEST_PLAN_SET2_NUMBER    8

/* ---- 最短走行の止まり方 ---- */

/* ゴールにターンで入って終わるときに止まる加速度 [mm/s²] と、止まるまでの距離の上限 [mm]。
 * 加速度は KOGUMA.MkIIl のダイクストラの見積もり（30000mm/s² で止まる）と同じ値。
 * ゴールの区画のまん中から壁までは 84mm なので、80mm 以内で止まる（ToraNeko.Mk3 は 45mm の区画で 40mm）。
 * 30000mm/s² で 80mm なら 2191mm/s から止まれるので、ゴールに入る最後のターンは、組 2400 でも 2191mm/s で曲がる */
#define SHORTEST_STOP_ACCEL_MM_S2 30000.0f
#define SHORTEST_STOP_DISTANCE_MM 80.0f

/* ---- 探索の前壁補正（ctrl/run.c。ToraNeko.Mk3 の 2026-10-28） ----
 * 区画の入口で、その区画に前の壁があるとき、前センサで測った壁までの距離が FRONT_WALL_AT_ENTRY_MM からずれていれば、
 * そのぶん、曲がり始め（スラロームの前距離）と、区画のまん中で止まる位置を直す。KOGUMA.MkIIl にはない動き。 */

/* 補正を実際にかけるか（1 かける、0 かけない）。0 でも、かけるはずだった量は測って、出力のモードで出す。
 * KOGUMA の前センサは、区画の入口（壁まで 174mm）では値が小さく（約 150）、12mm で 20 ほどしか変わらないので、
 * 探索で測った量を見てから 1 にするかを決める */
#define FRONT_CORRECTION_ENABLE 0
#define FRONT_WALL_AT_ENTRY_MM  174.0f  /* 区画の入口の辺にいるときの、前の壁までの距離 [mm]（90 + 84） */
/* 左前と右前の距離がこれ以上違うときは、近いほうだけを使う [mm]（ToraNeko.Mk3 の 6mm の 2 倍。仮） */
#define FRONT_CORRECTION_SENSOR_GAP_MM 12.0f
#define FRONT_CORRECTION_LIMIT_MM 10.0f  /* 直す量の上限 [mm]（ToraNeko.Mk3 の 5mm の 2 倍。仮） */

/* ---- 壁切れ（ctrl/run.c・ctrl/side_edge.c。ToraNeko.Mk3 の 2026-11-04） ----
 * 横のセンサで見つけた壁の終わり・柱の端から区画の境目を求め、思っていた境目とのずれだけ、今の直進の残りの距離を直す。
 * KOGUMA.MkIIl の Search_WallCut・Motor_Wallcut_ST と同じ役目。ctrl/side_edge.c の値が仮なので、
 * 壁切れのテスト（ステップ9 のモード）で測ってから 1 にする */
#define WALL_EDGE_ENABLE      0      /* 探索で、直進の残りを直すか（1 直す、0 測るだけ） */
#define FAST_WALL_EDGE_ENABLE 0      /* 最短走行で、直進の残りを直すか（1 直す、0 測るだけ） */
#define WALL_EDGE_WINDOW_MM   40.0f  /* いちばん近い境目から、これ以上ずれていたら、読み違いとして使わない [mm]（ToraNeko.Mk3 の 2 倍。仮） */

/* ---- 探索の横壁合わせ（ToraNeko.Mk3 の 2026-11-02。ctrl/wall_ctrl.c、のちに sys/explore.c・ctrl/run.c） ----
 * 走っていて、区画の入口で横の壁が SIDE_TOO_CLOSE_MM より近いとわかったら、その区画のまん中で止まり、
 * 近すぎた側の壁のほうへ 90° 回って前壁合わせをし、元の向きに戻ってから探索を続ける（左右のずれと向きを直す）。
 * KOGUMA.MkIIl にはない動き。 */

/* 横壁合わせをするか（1 する、0 しない）。0 でも、近すぎた回数と、いちばん近かった距離は測る。
 * KOGUMA.Mk3.1 では、壁までの距離の表を実機で確かめるまで 0 にしておく */
#define SIDE_ALIGN_ENABLE 0
/* 横の壁がこれより近ければ「近すぎる」[mm]（区画のまん中で 84mm。20mm 寄っている。
 * ToraNeko.Mk3 の 32mm（まん中 42mm から 10mm）と同じ割合にした。仮） */
#define SIDE_TOO_CLOSE_MM 64.0f

#endif /* CONFIG_RUN_PARAMS_H */
