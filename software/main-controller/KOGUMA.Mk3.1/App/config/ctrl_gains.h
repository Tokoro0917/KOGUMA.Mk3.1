/*
 * ctrl_gains.h
 * 速度と角度の制御の係数（ctrl/control.c が使う）
 *
 * ToraNeko.Mk3 の App/config/ctrl_gains.h と同じ形（同じ名前）。制御の仕組みは ToraNeko.Mk3 と同じにする（2026-11-08 に決定）。
 * 値は、KOGUMA.MkIIl の係数を、ToraNeko.Mk3 の式に換算して入れた（2026-11-08 のアンケートで決定）。
 * どれも、まず走らせてみるための最初の値。走行ログを見ながら合わせる（ステップ7）。
 *
 * モーターにかける電圧 [V] を、次の 2 つを足して決める（ctrl/control.c の先頭のコメント）。
 *   フィードフォワード：目標の速さ・加速度から「これくらいかければ、その速さになるはず」という電圧
 *   フィードバック    ：目標と、測った速さ・角度とのずれを小さくする電圧
 *
 * ■ KOGUMA.MkIIl の係数の換算のしかた
 * KOGUMA.MkIIl（Core/Src/motor.c）は、
 *   - 制御周期 1kHz（KOGUMA.Mk3.1 は 2kHz）
 *   - 出す値は PWM の数（200 で電池の電圧いっぱい。基準の電池 16V で 1 あたり 0.08V）
 *   - ずれを「1 周期ごとに足した数」（Sigma）で I をかけている（1kHz なので、速さのずれの合計 = 位置のずれ [mm] × 1000）
 *   - 向きの分は、角速度を cal_turnV（× トレッド幅 55mm × π ÷ 360 = × 0.48）でタイヤの速さにしてから足している
 * なので、
 *   速さの P [V/(mm/s)] = KOGUMA の Ksp × 0.08
 *   位置の I [V/mm]     = KOGUMA の Ksi × 1000 × 0.08
 *   角速度の P [V/(deg/s)] = KOGUMA の Ktp × 0.48 × 0.08
 *   角度の I [V/deg]       = KOGUMA の Kti × 1000 × 0.48 × 0.08
 * KOGUMA.MkIIl は、動きごとに 7 組の係数を使い分けていた。KOGUMA.Mk3.1 は 1 組にする（ToraNeko と同じ）：
 *   速さ：直進の組（Ksp 0.25・Ksi 0.0015）。吸引の組（Ksp_Fun 0.23・Ksi_Fun 0.0015）もほぼ同じ
 *   向き：探索のスラロームの組（Ktp_sula 0.7・Kti_sula 0.005）。その場旋回の組（Ktp 0.7・Kti 0.01）と P が同じ。
 *         直進の組（Ksp_AT 0.3・Ksi_AT 0.0045）、2000 の組（1.2・0.02）、2400 の組（1.4・0.02）は、下の換算の表に残す
 * KOGUMA.MkIIl の D（Ksd・Ktd）は、ToraNeko にないので使わない。
 *
 *   KOGUMA.MkIIl の組        P            I              換算した P [V/(deg/s)]  換算した I [V/deg]
 *   直進（Ksp_AT・Ksi_AT）    0.3          0.0045         0.0115                  0.173
 *   その場旋回（Ktp・Kti）    0.7          0.01           0.0269                  0.384
 *   スラローム（*_sula）      0.7          0.005          0.0269                  0.192   ← 使う
 *   2000（*_sula_20）         1.2          0.02           0.0461                  0.768
 *   2400（*_sula_24）         1.4          0.02           0.0538                  0.768
 */
#ifndef CONFIG_CTRL_GAINS_H
#define CONFIG_CTRL_GAINS_H

/* ---- フィードフォワード（前に進む分） ----
 * KOGUMA.MkIIl の走行ログから求めた 1 輪の電圧の式（KOGUMA.MkIIl の docs/noise_limits.md の「2」。± 20% くらいの見積もり）：
 *   前に進む分 V = 1.25 + 1.5v + 0.35a（v [m/s]、a [m/s²]）
 * KOGUMA.MkIIl の FF（KT・KE・Aff 0.65・Bff 0.50・FF_offset 15）から計算した値とも合う（Bff × KE で 1.47V/(m/s)） */

/* 速さ 1mm/s あたりの電圧 [V/(mm/s)]（上の式の 1.5V/(m/s)） */
#define FF_SPEED_V_PER_MM_S 0.0015f

/* 加速度 1mm/s² あたりの電圧 [V/(mm/s²)]（上の式の 0.35V/(m/s²)） */
#define FF_ACCEL_V_PER_MM_S2 0.00035f

/* 動き出すのに要る電圧 [V]（床との摩擦の分）。KOGUMA.MkIIl の FF_offset 15（PWM の数。× 0.08 = 1.2V）。
 * 上の式の 1.25V とも合う。KOGUMA.MkIIl のログは吸引しているものが多いので、吸引の分も入っているかもしれない。
 * 吸引なしで、止まる位置が先に行きすぎるなら下げる */
#define FF_FRICTION_V 1.2f

/* 吸引しているときに、FF_FRICTION_V に足す電圧 [V]（ToraNeko.Mk3 にある値）。
 * KOGUMA.MkIIl の FF_offset は吸引に関係なく同じだったので、0 にする */
#define FF_SUCTION_FRICTION_V 0.0f

/* その場旋回で、摩擦に負けないために足す電圧 [V]（タイヤ 1 つあたり。ToraNeko.Mk3 にある値）。
 * KOGUMA.MkIIl のその場旋回には摩擦の分がなかった（フィードバックだけ）ので、0 から始める。
 * 回りきらずに止まるようなら足す（ToraNeko.Mk3 は 0.2V） */
#define FF_TURN_FRICTION_V 0.0f

/* 吸引しながらスラロームで曲がるときに、向きの分に足す電圧 [V/(deg/s)]（角速度 1deg/s あたり。ToraNeko.Mk3 にある値）。
 * KOGUMA.MkIIl のログの式の向きの分は ΔV = 0.063ω（ω [rad/s]）。ToraNeko の式の向きのフィードフォワード
 * （タイヤ 1 つの速さ × FF_SPEED_V_PER_MM_S）では 27.5mm × 0.0015 = 0.041V/(rad/s) にしかならないので、
 * 足りない 0.022V/(rad/s) = 0.00038V/(deg/s) を足す（KOGUMA.MkIIl のログは吸引しているものが多いので、吸引のときだけ） */
#define FF_SUCTION_SLALOM_V_PER_DEG_S 0.00038f

/* ---- フィードバック（速さ） ---- */

/* 速さのずれ 1mm/s あたりの電圧 [V/(mm/s)]（P）。KOGUMA.MkIIl の Ksp 0.25 × 0.08 */
#define SPEED_KP 0.02f

/* 位置のずれ（速さのずれを足し合わせたもの）1mm あたりの電圧 [V/mm]（I）。KOGUMA.MkIIl の Ksi 0.0015 × 1000 × 0.08 */
#define SPEED_KI 0.12f

/* ---- フィードバック（向き） ---- */

/* 角速度のずれ 1deg/s あたりの電圧 [V/(deg/s)]（P）。左右のモーターに、+ と - で逆向きにかける。
 * KOGUMA.MkIIl のスラロームの Ktp_sula 0.7 × 0.48 × 0.08（先頭の表） */
#define OMEGA_KP 0.0269f

/* 角度のずれ（角速度のずれを足し合わせたもの）1deg あたりの電圧 [V/deg]（I）。
 * KOGUMA.MkIIl のスラロームの Kti_sula 0.005 × 1000 × 0.48 × 0.08（先頭の表） */
#define ANGLE_KI 0.192f

/* ---- 横の壁に沿って走る補正（直進のときだけ） ----
 * KOGUMA.MkIIl は、横の壁のずれで電圧を直接足していた（Wallsensor.c）。ToraNeko.Mk3 は、目標の角速度を曲げる。
 * 仕組みが違うので、KOGUMA の値はそのまま換算できない。ToraNeko の値を、壁から 1mm 寄ったときに同じだけ曲げるように、
 * センサの値の変わり方の比（区画のまん中の近くで 1mm あたり ToraNeko 約 290・KOGUMA 約 6）でかけ直した（仮） */

/* 横の壁からのずれ 1 あたり（センサの値の差）、目標の角速度をどれだけ曲げるか [deg/s]（P）。
 * ToraNeko 0.05 × 290 ÷ 6 ≒ 2.4（1mm で約 15deg/s 曲げる） */
#define WALL_KP 2.4f

/* ずれの変わる速さ 1/s あたり、目標の角速度をどれだけ曲げるか [deg/s]（D）。ToraNeko 0.008 × 290 ÷ 6 ≒ 0.39 */
#define WALL_KD 0.39f

/* 補正で曲げる角速度の上限 [deg/s]（ToraNeko と同じ） */
#define WALL_OMEGA_LIMIT_DEG_S 45.0f

/* ---- 斜めの補正（斜めの直進のとき） ---- */

/* 前のセンサがしきい値を超えた分 1 あたり、1000mm/s のときに目標の角速度をどれだけ曲げるか [deg/s]（速さに比例させる）。
 * ToraNeko 0.05 を、前のセンサの値の大きさの比（区画のまん中で ToraNeko 約 3360・KOGUMA 約 636）でかけ直した（仮） */
#define DIAG_KP 0.26f

/* ---- 向きを戻す（縦横の直進で、使える横の壁がないとき。ToraNeko と同じ） ---- */

/* 迷路の向きからのずれ 1° あたり、1000mm/s のときに戻す角速度 [deg/s]（速さに比例させる）。
 * 20 なら、ずれは 1000 ÷ 20 = 50mm 進むあいだに戻る。KOGUMA は区画が 2 倍なので、同じ区画の割合で戻るように 10 にした（仮） */
#define HEADING_HOLD_GAIN 10.0f
/* 戻す角速度の上限 [deg/s]（横の壁の補正と同じ） */
#define HEADING_HOLD_LIMIT_DEG_S 45.0f

/* 両側の壁が続いたら、基準の向きをリセットする条件（探索と最短走行）。ToraNeko の値を、両側の壁で 1mm 寄ったときの
 * センサの値の差（ToraNeko 約 440・KOGUMA 約 12）でかけ直した（仮） */
#define HEADING_RESET_WALL_ERROR_MAX 6.0f     /* 横の壁からのずれ（センサの値の差。ToraNeko 200） */
#define HEADING_RESET_WALL_RATE_MAX  41.0f    /* ずれの変わる速さ [1/s]（ToraNeko 1500） */
#define HEADING_RESET_DISTANCE_MM    180.0f   /* この距離続いたらリセットする [mm]（1 区画） */

/* 斜めの補正で曲げる角速度の上限 [deg/s]（横の壁の補正と同じ） */
#define DIAG_OMEGA_LIMIT_DEG_S 45.0f

/* これより遅いときは補正しない [mm/s]（止まる直前・走り出しは、センサの値が目標から離れていても気にしない） */
#define WALL_CONTROL_MIN_SPEED_MM_S 100.0f

/* ---- 前壁合わせ（区画のまん中で止まっているとき、前の壁で位置と向きを直す） ----
 * KOGUMA.MkIIl の前壁合わせ（Kp_FW_mm 1.8PWM/mm など）も電圧を直接出していたので、ToraNeko の値を、
 * 前のセンサの値の変わり方（区画のまん中の近くで 1mm あたり ToraNeko 約 100・KOGUMA 約 21）でかけ直した（仮） */

/* 前の壁との距離のずれ 1 あたりの、目標の速さ [mm/s]（近すぎれば下がる）。ToraNeko 0.3 × 100 ÷ 21（1mm で 30mm/s） */
#define ALIGN_SPEED_GAIN 1.4f
#define ALIGN_SPEED_LIMIT_MM_S 50.0f  /* 目標の速さの上限 [mm/s] */

/* 前壁合わせで、合わせ始めた位置から前後に動いてよい距離 [mm]（KOGUMA.MkIIl の Align_Err_Max_mm 20mm に合わせた） */
#define ALIGN_DISTANCE_LIMIT_MM 20.0f

/* 前の壁に対する向きのずれ 1 あたりの、目標の角速度 [deg/s]（右を向いていれば左へ回す）。ToraNeko 0.2 × 100 ÷ 21（仮） */
#define ALIGN_OMEGA_GAIN 0.95f
#define ALIGN_OMEGA_LIMIT_DEG_S 90.0f  /* 目標の角速度の上限 [deg/s] */

/* ---- 止める条件（フェイルセーフ。KOGUMA_PORTING.md の E3） ---- */

/* 位置のずれがこれより大きくなったら止める [mm]（ぶつかった、タイヤが空回りした、など）。
 * KOGUMA.MkIIl にはない（ToraNeko 30mm）。KOGUMA は速いので、区画の大きさに合わせて 2 倍にした（仮） */
#define POSITION_ERROR_LIMIT_MM 60.0f

/* 角度のずれがこれより大きくなったら止める [deg]（ToraNeko と同じ。KOGUMA.MkIIl にはない） */
#define ANGLE_ERROR_LIMIT_DEG 30.0f

/* 速さで見つける異常（KOGUMA.MkIIl の Failsafe_MotorTagetdiff の値。2026-11-08 のアンケートで「KOGUMA の値から」に決定）。
 * どちらも、当てはまった状態が FAILSAFE_HOLD_MS 続いたら止める（ノイズ 1 回では止めない。ToraNeko と同じ） */
#define SPEED_DROP_LIMIT_MM_S   2000.0f /* 目標の速さより、測った速さがこれ以上遅い（ぶつかった）[mm/s]（KOGUMA の Vdiff） */
#define STALL_TARGET_MIN_MM_S   500.0f  /* 加速も減速もせず、目標の速さがこれ以上で走っているはずなのに…（KOGUMA の 500mm/s から） */
#define STALL_SPEED_MAX_MM_S    100.0f  /*   測った速さがこれより遅い（壁を押し続けている）[mm/s]（KOGUMA の 100mm/s） */
#define FAILSAFE_HOLD_MS        10      /* 当てはまった状態がこれだけ続いたら止める [ms]（ToraNeko と同じ） */

#endif /* CONFIG_CTRL_GAINS_H */
