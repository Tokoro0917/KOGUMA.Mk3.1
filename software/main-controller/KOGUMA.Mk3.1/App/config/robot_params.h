/*
 * robot_params.h
 * 機体の定数（制御周期、電池の下限、タイヤ径、トレッド幅、ギア比）
 *
 * ToraNeko.Mk3 の App/config/robot_params.h と同じ役目のファイル。値は KOGUMA.MkIIl の Core/Inc/Define.h から。
 * ほかの値は、使うときに足す（ToraNeko.Mk3 と同じ）。
 */
#ifndef CONFIG_ROBOT_PARAMS_H
#define CONFIG_ROBOT_PARAMS_H

/* 制御周期 [Hz]（README の決定事項。ToraNeko.Mk3 と同じ 2kHz。KOGUMA.MkIIl は 1kHz だった）。
 * 制御の処理（ログの記録も）は 1 秒にこの回数呼ばれる */
#define CTRL_FREQ_HZ 2000
/* 制御周期 [s]（1 ÷ 2000 = 0.0005 秒 = 0.5ms） */
#define CTRL_PERIOD_S (1.0f / CTRL_FREQ_HZ)

/* 電池の下限の電圧 [V]。電源を入れたときにこれより低ければ、何もせずに知らせる。
 * 4 セルのリポで、KOGUMA.MkIIl の LIMITBATT と同じ（1 セル 3.9V） */
#define BATTERY_LOW_V 15.6f

/* タイヤの直径 [mm]。KOGUMA.MkIIl の TIREDIAMETER（2026-10-21 にモード4 No.1 で実測した値） */
#define TIRE_DIAMETER_MM 23.42f

/* トレッド幅 [mm]（左右のタイヤのまん中どうしの間隔）。KOGUMA.MkIIl の TIREBETWEEN */
#define TREAD_MM 55.0f

/* ギア比（モーターの軸が何回まわるとタイヤが 1 回まわるか）。KOGUMA.MkIIl の GEARRATIO。
 * 走った距離の計算（ステップ7 の ctrl/odometry.c）で使う。
 * KOGUMA.MkIIl の距離の計算（PL_encoder.c）は、エンコーダの 14 ビット（0〜16383）を 8192 で割って角度にしていて、
 * エンコーダ 1 回転を 720° と数えている。そのぶん、タイヤの直径とギア比の値は実測で合わせてある。
 * どちらの軸に磁石が付いているか・本当のギア比は、機構（mechanical/）を見て、ステップ7 で確かめる */
#define GEAR_RATIO 1.7073f

#endif /* CONFIG_ROBOT_PARAMS_H */
