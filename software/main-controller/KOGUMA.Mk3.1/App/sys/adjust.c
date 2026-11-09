/*
 * adjust.c
 * 動きを 1 つずつ走らせて調整する（モード2・4・6。MODES.md）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/adjust.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - 距離・速さ・加速度を、クラシック（区画 180mm）と KOGUMA.MkIIl の値にした（下の各動きのコメント）
 *   - スラローム 90°（モード2 No.1）は、ctrl/run.c の探索の動きをそのまま使う（探索の値が 2 とおりあるため）。
 *     吸引探索のスラローム 90°（モード2 No.3）を足した
 *   - 加速度テスト（モード4）の加速度を KOGUMA の大きさにし、No.12 に、直線の加速の限界テスト
 *     （KOGUMA.MkIIl のモード6 No.0 と同じ走り。KOGUMA_PORTING.md の B16）を足した
 *   - モーターの向きの確かめ（モード6 No.2。ステップ5 の確かめで、app.c がスイッチを押すと回していたもの）を足した
 *   - 真ん中の LED は 4 個（LED3〜6）
 *
 * ■ モード4 No.1・4〜11 加速度テスト（adjust_accel_test。2026-11-03）
 * 決めた加速度で速くなり、同じ加速度で遅くなって止まる。タイヤが滑り始める加速度（加速度の限界）を見つける。
 *   - No.4〜11 は吸引あり（8000〜40000mm/s²。No. が大きいほど加速度が大きい。ToraNeko.Mk3 は 4000〜21000）
 *   - No.1 は、No.4（8000mm/s²）と同じ走り方を、吸引なしで走る（吸引あり・なしで比べるため）
 *   - スタートの区画に、お尻を後ろの壁に付けて置く。4 区画まっすぐの通路で、4 区画目のまん中で止まる（564mm = 24 + 3 × 180）
 *   - 吸引を上げて 0.5 秒待ち（No.1 は吸引しない）、ジャイロのゼロ点を測り直してから（約 0.5 秒）走り出す
 *   - 走行ログ（1ms ごと）の v（エンコーダの速さ）と、acc_y（IMU の前後の加速度）を見比べる。
 *     滑っていなければ、v の傾きと IMU の加速度が同じになる。滑ると、IMU の加速度が目標より小さくなり、
 *     止まったあとに本当に走った距離（ものさし）が 564mm からずれる
 *   - 速さの上限は決めず、距離の半分まで加速して、残りの半分で止まる（三角の形）。
 *     いちばん速いときの速さは √(加速度 × 564mm)（8000mm/s² で 2124mm/s、40000mm/s² で 4750mm/s）
 *   - No.12 は、KOGUMA.MkIIl のモード6 No.0 と同じ走り（B16）：吸引 8.1V（KOGUMA の Suction_Start(51)）、40000mm/s² で
 *     6000mm/s まで上げ、1080mm（6 区画）で止まる。スタートの区画から 7 区画まっすぐの通路が要る
 *
 * ■ モード6 No.1 直進（adjust_straight）
 * ゆっくり 564mm まっすぐ走って止まる。スタート区画に、お尻を後ろの壁に付けて置き、4 区画目のまん中で止まる
 * （24 + 3 × 180。加速度テストと同じ置き方・距離）。
 *   - 止まったら、後ろの壁からお尻までをものさしで測る（それが走った距離）。モード7 No.1 の RESULT の encoder
 *     （エンコーダで測った距離）より短ければタイヤの直径（config/robot_params.h の TIRE_DIAMETER_MM）を小さく、
 *     長ければ大きくする（新しい直径 = 今の直径 × ものさしの距離 ÷ encoder の距離。564mm ではなく encoder と比べる。
 *     止まるときに摩擦でずれが残ると、encoder は 564mm より短くなるため）
 *   - 走行ログ（2ms ごと）を残すので、モード7 No.1 で PC へ出し、KOGUMA のビューアで目標の速さと
 *     測った速さを見比べて、config/ctrl_gains.h の係数を合わせる
 *   - 走り終わったら、エンコーダで測った距離と、ジャイロで測った向きのずれを覚えておく
 *
 * ■ モード6 No.7 壁に沿って直進（adjust_straight_along_wall）
 * 564mm を、探索の速さ（吸引なし 500mm/s）で、横の壁に沿って走る補正を入れて走って止まる（置き方は直進と同じ）。
 *   - 左右に壁がある通路で、少し左か右に寄せて（または少し斜めに）置いて走らせ、まん中に戻るかを見る
 *   - 走行ログの wall_err（ずれ）、wall_p・wall_d（P・D の分）、w_target（補正を足した目標の角速度）で、
 *     config/ctrl_gains.h の WALL_KP・WALL_KD を合わせる
 *
 * ■ モード2 No.2 U ターン（adjust_uturn）
 * 区画のまん中に置き、前の区画が行き止まり（その区画の前に壁がある）になるようにして始める。
 * 前の区画へ進んでまん中で止まり、前壁合わせ（0.5 秒）→ 左に 90° → 前壁合わせ → 左に 90° で反対を向き、
 * 元の区画のまん中まで戻って止まる。
 * 探索の U ターンと同じ動き（ctrl/run.c の関数を使う）。元の区画のまん中で、反対を向いて止まれば合っている
 *
 * ■ モード2 No.5 壁合わせだけ（adjust_front_align）
 * 区画のまん中に、前に壁がある向きで置いて始める。前の壁で、位置と向きを 0.5 秒直して止まる。
 * 少し斜めに、または前後にずらして置いて、まっすぐ・まん中に直るかを見る
 *
 * ■ モード2 No.4 その場 180° 旋回（adjust_pivot_180）
 * その場で左に 180° 回って止まる（U ターンの部品）。
 *   - 止まったら、本当に 180° 回ったかを目で見る（ジャイロの値と合っているか）
 *   - 走行ログ（1ms ごと）を残すので、目標の角速度と測った角速度を見比べて、config/ctrl_gains.h の係数を合わせる
 *   - 走り終わったら、ジャイロで測った角度と、エンコーダで測った前後のずれを覚えておく
 *
 * ■ モード2 No.1 スラローム 90°（adjust_slalom_90）・No.3 吸引探索のスラローム 90°（adjust_suction_slalom_90）
 * 探索の速さ（No.1 は吸引なし 500mm/s、No.3 は吸引 1000mm/s）で、左に 90° のスラロームをする（探索の小回り）。
 * ctrl/run.c の探索の動きをそのまま使う（探索と同じ値・同じ動き）。
 *   1. 区画のまん中から、次の区画の入口の辺まで 90mm を、探索の速さまで速くしながら進む（run_search_go_from_center）
 *   2. 前距離、左に 90° 曲がる、後距離で、左の区画の入口の辺へ（run_search_turn_left。値は config/run_params.h）
 *   3. 入口の辺から 90mm 進みながら止まり、左の区画のまん中で止まる（run_search_stop_at_center）
 *   - 止まったら、左の区画のまん中で、左を向いて止まっているかを目で見る
 *     曲がりが足りない・行きすぎるなら、前距離・後距離（SLALOM_PRE_MM・SLALOM_POST_MM、吸引は SUCTION_SLALOM_*）を合わせる
 *   - 横の壁に沿う補正は入れない。走行ログ（2ms ごと）を残す
 *
 * ■ 走り出す前
 * 走り出す前に、ジャイロのゼロ点を測り直す（約 0.5 秒。スタートの合図のあと、機体は止まっている）。
 * 電源を入れた直後に測ったゼロ点は、機体を手で動かしていると大きくずれるため。
 *
 * ■ 結果の出し方
 * 走っているあいだはケーブルを挿していないので、結果は RAM に覚えておき、ケーブルを挿してから
 * モード7 No.1（ログ出力）で、走行ログの前に出す（adjust_print_last_result()）。電源を切ると消える。
 */
#include "sys/adjust.h"

#include <stdbool.h>
#include <stdio.h>

#include "config/run_params.h"
#include "ctrl/control.h"
#include "ctrl/odometry.h"
#include "ctrl/run.h"
#include "ctrl/trajectory.h"
#include "ctrl/wall_ctrl.h"
#include "drv/fan.h"
#include "drv/encoder.h"
#include "drv/imu.h"
#include "drv/motor.h"
#include "drv/timebase.h"
#include "drv/uart.h"
#include "sys/failsafe.h"
#include "sys/log.h"
#include "sys/suction.h"
#include "sys/ui.h"

#define RESULT_LINE_LENGTH_MAX 120  /* RESULT の 1 行の長さの上限 [文字]。出す前に、送信待ちバッファにこれだけの空きを待つ */

#define STRAIGHT_DISTANCE_MM  (START_OFFSET_MM + 3.0f * CELL_MM)  /* 走る距離 [mm]（スタートから 4 区画目のまん中まで：24 + 3 × 180 = 564） */
#define STRAIGHT_SPEED_MM_S   500.0f   /* 最高速 [mm/s]（吸引なしの探索と同じ） */
#define STRAIGHT_ACCEL_MM_S2  2000.0f  /* 加速度 [mm/s²]（ゆっくりめ） */
#define STRAIGHT_LOG_INTERVAL_MS 2     /* 走行ログの間隔 [ms]（止まるまで約 1.4 秒なので、1600 行に入るように） */
#define SETTLE_MS             300      /* 止まってから、制御を続けて待つ時間 [ms]（ぴたりと止めるため） */

#define ACCEL_TEST_DISTANCE_MM     (START_OFFSET_MM + 3.0f * CELL_MM)  /* 加速度テストで走る距離 [mm]（4 区画目のまん中まで：564） */
#define ACCEL_TEST_MAX_SPEED_MM_S  8000.0f  /* 速さの上限 [mm/s]（届かない大きさにして、三角の形で走る） */
/* No.12：直線の加速の限界テスト（KOGUMA.MkIIl のモード6 No.0 の Motor_trapezoid_PID(0, 6000, 0, 40000, 180 * 6)） */
#define LIMIT_TEST_DISTANCE_MM     1080.0f  /* 走る距離 [mm]（6 区画） */
#define LIMIT_TEST_MAX_SPEED_MM_S  6000.0f  /* 最高速 [mm/s]（40000mm/s² では、上げきる前に止まり始める） */
#define LIMIT_TEST_ACCEL_MM_S2     40000.0f /* 加速度 [mm/s²] */
#define LIMIT_TEST_SUCTION_V       8.1f     /* 吸引の強さ [V]（KOGUMA の Suction_Start(51)：15.8V × 0.51） */
#define ACCEL_TEST_SUCTION_WAIT_MS 500      /* 吸引を上げてから、走り出す前に待つ時間 [ms]（ファンの回り方を落ち着かせる） */
#define ACCEL_TEST_LOG_INTERVAL_MS 1        /* 走行ログの間隔 [ms]（8000mm/s² でも走るのは約 0.53 秒なので、1600 行に入る） */

#define PIVOT_ANGLE_DEG       180.0f   /* 回る角度 [deg]（左回り） */
#define PIVOT_LOG_INTERVAL_MS 1        /* 走行ログの間隔 [ms]（約 0.6 秒なので、1600 行に入る） */

#define SLALOM_LOG_INTERVAL_MS 2       /* 走行ログの間隔 [ms]（約 1.5 秒なので、1600 行に入る） */
#define UTURN_LOG_INTERVAL_MS  2       /* 走行ログの間隔 [ms]（約 3 秒なので、1600 行に入る） */
#define ALIGN_LOG_INTERVAL_MS  1       /* 走行ログの間隔 [ms]（0.5 秒） */
/* 探索の速さ・スラローム・その場旋回・前壁合わせの値は config/run_params.h（探索と同じ値を使う） */

/* 最後に走らせた動きの結果（走り終わったときはケーブルを挿していないので、覚えておいて、あとで出す） */
typedef enum {
  RESULT_NONE = 0,   /* まだ何も走らせていない */
  RESULT_STRAIGHT,   /* 直進 */
  RESULT_PIVOT,      /* その場旋回 */
  RESULT_SLALOM,     /* スラローム */
  RESULT_ACCEL,      /* 加速度テスト（吸引あり・なし） */
} result_kind_t;

typedef struct {
  result_kind_t kind;
  float target;             /* 目標（直進は距離 [mm]、旋回は角度 [deg]、加速度テストは加速度 [mm/s²]） */
  float moved_mm;           /* エンコーダで測った、進んだ距離 [mm] */
  float turned_deg;         /* ジャイロで測った、回った角度 [deg] */
  float position_error_mm;  /* 終わったときの位置のずれ [mm] */
  float angle_error_deg;    /* 終わったときの角度のずれ [deg] */
  bool failsafe;            /* フェイルセーフで止まったか */
  float suction_v;          /* 走ったときの吸引の電圧 [V]（加速度テストだけ。吸引なしなら 0） */
  float distance_mm;        /* 走る距離 [mm]（加速度テストだけ。No.12 は 1080mm） */
} run_result_t;

static run_result_t s_result = {RESULT_NONE, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, 0.0f, 0.0f};

/* 決めた距離・角度を走り終えるか、フェイルセーフで止まるまで待つ */
static void wait_until_done(void)
{
  while (!trajectory_is_done() && !failsafe_is_stopped()) {
    ui_wait_ms(UI_POLL_MS);
  }
}

/* 止まるまで制御を続けてから、制御とログを止める（フェイルセーフで止まったときは、すぐ止める） */
static void settle_and_stop(void)
{
  if (!failsafe_is_stopped()) {
    ui_wait_ms(SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  }
  control_stop();
  log_stop();
}

/* 終わったことを LED で知らせる。フェイルセーフで止まったときは、スイッチが押されるまで待って解除する */
static void signal_end(void)
{
  if (failsafe_is_stopped()) {
    /* 8 個全部を点滅させて、スイッチが押されるまで待ち、フェイルセーフを解除する */
    ui_wait_failsafe_reset();
    failsafe_clear();
  } else {
    ui_signal_ok();
  }
}

/* 直進の結果を覚える（ケーブルは走り終わってから挿すので、あとでモード7 No.1 で出す） */
static void save_straight_result(float start_distance_mm, float start_angle_deg)
{
  s_result.kind = RESULT_STRAIGHT;
  s_result.target = STRAIGHT_DISTANCE_MM;
  s_result.moved_mm = odometry_get_distance_mm() - start_distance_mm;
  s_result.turned_deg = imu_get_angle_deg() - start_angle_deg;
  s_result.position_error_mm = control_get_position_error_mm();
  s_result.angle_error_deg = control_get_angle_error_deg();
  s_result.failsafe = failsafe_is_stopped();
}

/* 前壁補正の 1 回ごとの量を、走った距離といっしょに 1 行ずつ出す（ずれがいつも同じか、走るほど増えるかを見る） */
static void print_front_correction_list(void)
{
  for (int i = 0; i < run_get_front_correction_list_count(); i++) {
    uart_wait_free(RESULT_LINE_LENGTH_MAX);
    printf("  front %d: traveled %.0f mm, correction %.1f mm\r\n", i + 1, run_get_front_correction_list_traveled_mm(i),
           run_get_front_correction_list_mm(i));
    /* そのときの様子：前の壁までの距離（左前・右前）、横の壁までの距離、入口から進んでいた距離、速さ、向きのずれ */
    run_front_detail_t detail = run_get_front_correction_detail(i);
    uart_wait_free(RESULT_LINE_LENGTH_MAX);
    printf("    FL %.1f FR %.1f mm, side L %.1f R %.1f mm, overrun %.1f mm, speed %.0f mm/s, yaw %.1f deg\r\n",
           detail.front_left_mm, detail.front_right_mm, detail.side_left_mm, detail.side_right_mm,
           detail.overrun_mm, detail.speed_mm_s, detail.yaw_deg);
  }
}

/* 壁切れ（探索・最短走行）の 1 回ごとのずれを、左右・柱か壁の終わりか・速さ・走った距離・遅れ・使えたかといっしょに 1 行ずつ出す。
 * 1 行の形：  edge 1: left, pillar, traveled 110 mm, speed 800 mm/s, error 1.2 mm, lag 0.8 mm, used */
static void print_edge_list(void)
{
  int count = run_get_edge_count();
  if (count == 0) {
    return;
  }
  printf("Wall edge: %d times (search applied=%d, shortest applied=%d)\r\n", count, WALL_EDGE_ENABLE, FAST_WALL_EDGE_ENABLE);
  for (int i = 0; i < count; i++) {
    const char *side = "right";
    if (run_get_edge_is_left(i)) {
      side = "left";
    }
    const char *kind = "wall end";
    if (run_get_edge_is_pillar(i)) {
      kind = "pillar";
    }
    const char *used = "not used";
    if (run_get_edge_used(i)) {
      used = "used";
    }
    uart_wait_free(RESULT_LINE_LENGTH_MAX);
    printf("  edge %d: %s, %s, traveled %.0f mm, speed %.0f mm/s, error %.1f mm, lag %.1f mm, %s\r\n", i + 1, side, kind,
           run_get_edge_traveled_mm(i), run_get_edge_speed_mm_s(i), run_get_edge_error_mm(i), run_get_edge_lag_mm(i), used);
  }
}

/* 探索で、区画の入口に着いてから次の動きを頼まれるまでの、いちばん長かった時間と、前壁補正の量・壁切れの位置を出す（探索を走らせていれば） */
static void print_search_wait(void)
{
  uint32_t wait_us = run_get_longest_wait_us();
  if (wait_us > 0) {
    printf("Search: longest wait between moves %lu us\r\n", wait_us);
  }
  if (run_get_front_correction_count() > 0) {
    printf("Search: front correction %d times, average %.1f mm, min %.1f mm, max %.1f mm (applied=%d)\r\n",
           run_get_front_correction_count(), run_get_front_correction_average_mm(), run_get_front_correction_min_mm(),
           run_get_front_correction_max_mm(), FRONT_CORRECTION_ENABLE);
    print_front_correction_list();
  }
  if (wall_ctrl_get_too_close_count() > 0) {
    printf("Search: side wall too close %d times, nearest %.1f mm (aligned=%d)\r\n",
           wall_ctrl_get_too_close_count(), wall_ctrl_get_too_close_min_mm(), SIDE_ALIGN_ENABLE);
  }
  print_edge_list();
}

/* 最後に走らせた動き（直進・その場旋回・スラローム）の結果を出す。まだ何も走らせていなければ何も出さない */
static void print_move_result(void)
{
  if (s_result.kind == RESULT_STRAIGHT) {
    printf("Straight: target %.1f mm, encoder %.1f mm, angle %.2f deg\r\n",
           s_result.target, s_result.moved_mm, s_result.turned_deg);
  } else if (s_result.kind == RESULT_PIVOT) {
    printf("Pivot: target %.1f deg, gyro %.2f deg, moved %.2f mm\r\n",
           s_result.target, s_result.turned_deg, s_result.moved_mm);
  } else if (s_result.kind == RESULT_SLALOM) {
    printf("Slalom: target %.1f deg, gyro %.2f deg, encoder %.1f mm\r\n",
           s_result.target, s_result.turned_deg, s_result.moved_mm);
  } else if (s_result.kind == RESULT_ACCEL) {
    printf("Accel test (suction %.1f V): accel %.0f mm/s2, target %.1f mm, encoder %.1f mm, angle %.2f deg\r\n",
           s_result.suction_v, s_result.target, s_result.distance_mm, s_result.moved_mm, s_result.turned_deg);
  } else {
    return;  /* 調整の動きは、まだ何も走らせていない */
  }
  printf("  position error %.2f mm, angle error %.2f deg\r\n",
         s_result.position_error_mm, s_result.angle_error_deg);
  if (s_result.failsafe) {
    printf("  stopped by failsafe\r\n");
  }
}

/* 最後にフェイルセーフで止めたわけを出す（止めていなければ何も出さない） */
static void print_failsafe_reason(void)
{
  control_failsafe_reason_t reason = control_get_failsafe_reason();
  if (reason == CONTROL_FAILSAFE_POSITION) {
    printf("Failsafe: position error too large\r\n");
  } else if (reason == CONTROL_FAILSAFE_ANGLE) {
    printf("Failsafe: angle error too large\r\n");
  } else if (reason == CONTROL_FAILSAFE_SLOW) {
    printf("Failsafe: much slower than target (hit something)\r\n");
  } else if (reason == CONTROL_FAILSAFE_STALL) {
    printf("Failsafe: not moving while running (pushing a wall)\r\n");
  }
}

/* 最後に走らせた動きの結果と、探索の待ち時間・前壁補正の量をシリアルに出す。モード7 No.1（ログ出力）でも呼ぶ。
 * ビューア（tools/viewer.html）が 1 つのデータとして受け取って保存できるように、
 * #BEGIN RESULT と #END RESULT ではさむ。出すものがなければ何も出さない。電源を切ると消える（走行ログと同じ） */
void adjust_print_last_result(void)
{
  if (s_result.kind == RESULT_NONE && run_get_longest_wait_us() == 0 && run_get_front_correction_count() == 0
      && run_get_edge_count() == 0 && wall_ctrl_get_too_close_count() == 0
      && control_get_failsafe_reason() == CONTROL_FAILSAFE_NONE) {
    return;
  }
  printf("\r\n#BEGIN RESULT\r\n");
  print_move_result();
  print_search_wait();
  print_failsafe_reason();
  printf("#END RESULT\r\n");
}

/* モード6 No.1 直進：ゆっくり 280mm まっすぐ走って止まる（流れは先頭のコメント） */
void adjust_straight(void)
{
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒） */
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();

  log_start(STRAIGHT_LOG_INTERVAL_MS);
  control_set_wall_control(false);  /* 横の壁に沿って走る補正は入れない */
  control_start();
  trajectory_start_straight(STRAIGHT_DISTANCE_MM, STRAIGHT_SPEED_MM_S, 0.0f, STRAIGHT_ACCEL_MM_S2);
  wait_until_done();
  settle_and_stop();

  save_straight_result(start_distance_mm, start_angle_deg);
  adjust_print_last_result();  /* ケーブルを挿したまま走らせたときのために、すぐにも出す */
  signal_end();
}

/* その場旋回の結果を覚える（あとでモード7 No.1 で出す） */
static void save_pivot_result(float start_distance_mm, float start_angle_deg)
{
  s_result.kind = RESULT_PIVOT;
  s_result.target = PIVOT_ANGLE_DEG;
  s_result.moved_mm = odometry_get_distance_mm() - start_distance_mm;
  s_result.turned_deg = imu_get_angle_deg() - start_angle_deg;
  s_result.position_error_mm = control_get_position_error_mm();
  s_result.angle_error_deg = control_get_angle_error_deg();
  s_result.failsafe = failsafe_is_stopped();
}

/* モード2 No.4 その場 180° 旋回：その場で左に 180° 回って止まる（流れは先頭のコメント） */
void adjust_pivot_180(void)
{
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒） */
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();

  log_start(PIVOT_LOG_INTERVAL_MS);
  control_set_wall_control(false);  /* 横の壁に沿って走る補正は入れない */
  control_start();
  trajectory_start_pivot(PIVOT_ANGLE_DEG, PIVOT_OMEGA_DEG_S, PIVOT_ALPHA_DEG_S2);
  wait_until_done();
  settle_and_stop();

  save_pivot_result(start_distance_mm, start_angle_deg);
  adjust_print_last_result();
  signal_end();
}

/* スラロームの結果を覚える（あとでモード7 No.1 で出す）。target は角度、moved はスラローム全体の距離 */
static void save_slalom_result(float start_distance_mm, float start_angle_deg)
{
  s_result.kind = RESULT_SLALOM;
  s_result.target = SLALOM_ANGLE_DEG;
  s_result.moved_mm = odometry_get_distance_mm() - start_distance_mm;
  s_result.turned_deg = imu_get_angle_deg() - start_angle_deg;
  s_result.position_error_mm = control_get_position_error_mm();
  s_result.angle_error_deg = control_get_angle_error_deg();
  s_result.failsafe = failsafe_is_stopped();
}

/* スラローム 90° の中身（モード2 No.1・No.3）：区画のまん中から走り出し、次の区画で左に 90° 曲がり、
 * 左の区画のまん中で止まる（流れは先頭のコメント）。探索の値（吸引なし・吸引あり）は、呼ぶ前に選んでおく */
static void slalom_90(void)
{
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒。吸引ありなら、吸ったまま測る） */
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();

  log_start(SLALOM_LOG_INTERVAL_MS);
  control_set_wall_control(false);  /* 横の壁に沿って走る補正は入れない */
  control_start();
  run_search_go_from_center(RUN_TURN_NONE);  /* 1. 次の区画の入口の辺まで、速くしながら進む */
  run_search_turn_left();                    /* 2. 前距離、左に 90°、後距離で、左の区画の入口の辺へ */
  run_search_stop_at_center();               /* 3. 左の区画のまん中まで進んで止まる */
  settle_and_stop();

  save_slalom_result(start_distance_mm, start_angle_deg);
}

/* モード2 No.1 スラローム 90°（吸引なし、500mm/s） */
void adjust_slalom_90(void)
{
  run_search_set_suction(false);
  slalom_90();
  adjust_print_last_result();  /* ケーブルを挿したまま走らせたときのために、すぐにも出す */
  signal_end();
}

/* モード2 No.3 吸引探索のスラローム 90°（吸引あり、1000mm/s）。吸引を上げてから走り、止まったら吸引を止める */
void adjust_suction_slalom_90(void)
{
  suction_ramp_up();  /* 0.5 秒かけて吸引を上げる（sys/suction.c） */
  ui_wait_ms(ACCEL_TEST_SUCTION_WAIT_MS);
  run_search_set_suction(true);
  slalom_90();
  fan_stop();
  run_search_set_suction(false);  /* ほかの動きが、吸引探索の値で走らないように戻す */
  adjust_print_last_result();
  signal_end();
}

/* モード6 No.7 壁に沿って直進：スタート区画から 564mm を、横の壁に沿って走る補正を入れて、探索の速さ（吸引なし）で走って止まる */
void adjust_straight_along_wall(void)
{
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒） */
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();

  log_start(STRAIGHT_LOG_INTERVAL_MS);
  control_set_wall_control(true);  /* 横の壁に沿って走る補正を入れる */
  control_start();
  trajectory_start_straight(STRAIGHT_DISTANCE_MM, SEARCH_SPEED_MM_S, 0.0f, SEARCH_ACCEL_MM_S2);
  wait_until_done();
  settle_and_stop();
  control_set_wall_control(false);

  save_straight_result(start_distance_mm, start_angle_deg);
  adjust_print_last_result();
  signal_end();
}

/* モード2 No.2 U ターン：区画のまん中から、行き止まりの次の区画へ進んで止まり、前壁合わせをして、
 * その場で 180° 回って、元の区画のまん中まで戻って止まる（探索の U ターンと同じ動き。ctrl/run.c を使う） */
void adjust_uturn(void)
{
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒） */
  run_search_set_suction(false);  /* 吸引なしの探索の U ターン */
  log_start(UTURN_LOG_INTERVAL_MS);
  control_set_wall_control(true);  /* 探索と同じく、横の壁に沿って走る補正を入れる */
  run_search_go_from_center(RUN_TURN_NONE);   /* 次の区画の入口へ（前に壁がなければ、そのまま発進） */
  run_search_stop_at_center();                /* 行き止まりの区画のまん中で止まる */
  run_search_go_from_center(RUN_TURN_BACK);   /* 前壁合わせ → 90° → 前壁合わせ → 90° → 元の区画の入口へ */
  run_search_stop_at_center();                /* 元の区画のまん中で止まる */
  control_stop();
  control_set_wall_control(false);
  log_stop();
  signal_end();
}

/* モード2 No.5 壁合わせだけ：区画のまん中で、前の壁で位置と向きを直して止まる */
void adjust_front_align(void)
{
  imu_remeasure_offset();
  log_start(ALIGN_LOG_INTERVAL_MS);
  control_set_wall_control(false);
  control_start();
  control_set_front_align(true);
  ui_wait_ms(ALIGN_TIME_MS);
  control_set_front_align(false);
  control_stop();
  log_stop();
  signal_end();
}

/* ---------------------------------------------------------------------------
 * モード6 No.3 吸引 ON（2026-10-28）
 * ------------------------------------------------------------------------- */

#define SUCTION_BLINK_MS   500  /* 吸っているあいだ、真ん中の 4 個を点滅させる間隔 [ms]（ジャイロの角度も、この間隔で出す） */
#define SUCTION_SETTLE_MS  500  /* 吸引を上げてから、ジャイロのゼロ点を測る前に待つ時間 [ms]（最短走行と同じ） */

/* モード6 No.3 吸引 ON：吸引ファンを回し、スイッチが押されるまで吸い続ける（機体は動かない）。
 * 机に置いて吸い付くか、手で持ち上げてどれくらいの力が要るかを見て、SUCTION_VOLTAGE_V を決める。
 * 2026-11-04 から、吸っているあいだ、ジャイロの角度を 0.5 秒ごとにシリアルに出す（`SUCTION_GYRO,時間[s],角度[deg]`）。
 * 機体は止まっているので、角度が変わった分は、吸引ファンの振動などでジャイロが流れた分（向きを戻す補正が、
 * ジャイロの角度を使うので、どれくらい流れるかを見る。ctrl/control.c の「向きを戻す」） */
void adjust_suction_on(void)
{
  suction_ramp_up();  /* 0.5 秒かけて吸引を上げる（sys/suction.c） */
  ui_wait_ms(SUCTION_SETTLE_MS);
  imu_remeasure_offset();  /* 吸ったまま、ジャイロのゼロ点を測り直す（最短走行と同じ。約 0.5 秒） */
  uint32_t start_ms = timebase_get_ms();
  bool lit = true;
  uint32_t next_toggle_ms = timebase_get_ms() + SUCTION_BLINK_MS;
  while (!ui_switch_pushed()) {
    if (timebase_get_ms() >= next_toggle_ms) {
      lit = !lit;
      next_toggle_ms = next_toggle_ms + SUCTION_BLINK_MS;
      /* LED を切り替えるたび（0.5 秒ごと）に、ジャイロの角度を出す */
      float elapsed_s = (timebase_get_ms() - start_ms) / 1000.0f;
      printf("SUCTION_GYRO,%.1f,%.2f\r\n", elapsed_s, imu_get_angle_deg());
    }
    ui_set_middle_leds(lit);  /* 真ん中の 4 個（LED3〜6） */
    ui_wait_ms(UI_POLL_MS);
  }
  fan_stop();
  ui_all_off();
  ui_signal_ok();
}

/* ---------------------------------------------------------------------------
 * モード4 No.1・4〜12 加速度テスト（ToraNeko.Mk3 の 2026-11-03。流れは先頭のコメント）
 * ------------------------------------------------------------------------- */

/* No. から、加速度テストの加速度 [mm/s²] を決める。表にない No. なら 0 を返す */
static float accel_test_accel_mm_s2(int number)
{
  if (number == 1) {
    return 8000.0f;  /* 吸引なし（No.4 と同じ加速度） */
  }
  if (number == 4) {
    return 8000.0f;
  }
  if (number == 5) {
    return 12000.0f;
  }
  if (number == 6) {
    return 16000.0f;
  }
  if (number == 7) {
    return 20000.0f;
  }
  if (number == 8) {
    return 25000.0f;
  }
  if (number == 9) {
    return 30000.0f;
  }
  if (number == 10) {
    return 35000.0f;
  }
  if (number == 11) {
    return 40000.0f;
  }
  if (number == 12) {
    return LIMIT_TEST_ACCEL_MM_S2;  /* 直線の加速の限界テスト（B16） */
  }
  return 0.0f;
}

/* No. で、吸引するかを決める。No.1 だけ吸引しない */
static bool accel_test_uses_suction(int number)
{
  if (number == 1) {
    return false;
  }
  return true;
}

/* 加速度テストの結果を覚える（あとでモード7 No.1 で出す） */
static void save_accel_result(float accel_mm_s2, float suction_v, float distance_mm, float start_distance_mm,
                              float start_angle_deg)
{
  s_result.kind = RESULT_ACCEL;
  s_result.distance_mm = distance_mm;
  s_result.target = accel_mm_s2;
  s_result.moved_mm = odometry_get_distance_mm() - start_distance_mm;
  s_result.turned_deg = imu_get_angle_deg() - start_angle_deg;
  s_result.position_error_mm = control_get_position_error_mm();
  s_result.angle_error_deg = control_get_angle_error_deg();
  s_result.failsafe = failsafe_is_stopped();
  s_result.suction_v = suction_v;
}

/* No.12（直線の加速の限界テスト）だけは、距離・最高速・吸引の強さが違う。No. から走る距離 [mm] を決める */
static float accel_test_distance_mm(int number)
{
  if (number == 12) {
    return LIMIT_TEST_DISTANCE_MM;
  }
  return ACCEL_TEST_DISTANCE_MM;
}

/* No. から最高速 [mm/s] を決める */
static float accel_test_max_speed_mm_s(int number)
{
  if (number == 12) {
    return LIMIT_TEST_MAX_SPEED_MM_S;
  }
  return ACCEL_TEST_MAX_SPEED_MM_S;
}

/* No. から吸引の強さ [V] を決める */
static float accel_test_suction_v(int number)
{
  if (number == 12) {
    return LIMIT_TEST_SUCTION_V;
  }
  return SUCTION_VOLTAGE_V;
}

/* モード4 No.1・4〜12 加速度テスト：No. で決まる加速度で 564mm（No.12 は 1080mm）走って止まる（No.1 は吸引なし、ほかは吸引あり） */
void adjust_accel_test(int number)
{
  float accel_mm_s2 = accel_test_accel_mm_s2(number);
  if (accel_mm_s2 <= 0.0f) {
    return;  /* 表にない No.（mode.c で選べないようにしてあるので、ふつうは来ない） */
  }

  /* 1. 吸引を上げて、少し待つ（吸引なしの No.1 では、何もしない） */
  if (accel_test_uses_suction(number)) {
    suction_ramp_up_to(accel_test_suction_v(number));  /* 0.5 秒かけて吸引を上げる（sys/suction.c） */
    ui_wait_ms(ACCEL_TEST_SUCTION_WAIT_MS);
  }
  float suction_v = fan_get_voltage();

  /* 2. 走るときと同じ状態で、ジャイロのゼロ点を測り直す（吸引ありなら、ファンの揺れがあっても吸ったまま測る） */
  imu_remeasure_offset();
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();

  /* 3. 走る（横の壁に沿う補正は入れない） */
  log_start(ACCEL_TEST_LOG_INTERVAL_MS);
  control_set_wall_control(false);
  control_start();
  trajectory_start_straight(accel_test_distance_mm(number), accel_test_max_speed_mm_s(number), 0.0f, accel_mm_s2);
  wait_until_done();
  settle_and_stop();

  /* 4. 吸引を止めて、結果を覚える（フェイルセーフで止まったときは、もう止まっている） */
  fan_stop();
  save_accel_result(accel_mm_s2, suction_v, accel_test_distance_mm(number), start_distance_mm, start_angle_deg);
  adjust_print_last_result();  /* ケーブルを挿したまま走らせたときのために、すぐにも出す */
  signal_end();
}

/* ---------------------------------------------------------------------------
 * モード6 No.2 モーターの向きの確かめ（KOGUMA.Mk3.1 で足した。ステップ5 の確かめ）
 * ------------------------------------------------------------------------- */

#define MOTOR_CHECK_V  1.0f   /* モーターにかける電圧 [V]（ゆっくり回る。前に進む向きが +） */
#define MOTOR_CHECK_MS 2000   /* 回す時間 [ms] */
#define MOTOR_PRINT_MS 500    /* エンコーダの回った量を出す間隔 [ms] */

/* モード6 No.2：タイヤを浮かせて置き、左右のモーターに MOTOR_CHECK_V を 2 秒かけて止める（制御はしない）。
 * 左右とも、機体が前に進む向きに回るかを目で見る。回っているあいだ、エンコーダの回った量を 0.5 秒ごとに出す
 * （前に回っていれば、L も R も増える） */
void adjust_motor_check(void)
{
  int32_t start_left = encoder_get_left_total();
  int32_t start_right = encoder_get_right_total();
  motor_set_voltage(MOTOR_CHECK_V, MOTOR_CHECK_V);
  for (int i = 0; i < MOTOR_CHECK_MS / MOTOR_PRINT_MS; i++) {
    ui_wait_ms(MOTOR_PRINT_MS);
    printf("MOTOR enc L=%ld R=%ld\r\n", encoder_get_left_total() - start_left, encoder_get_right_total() - start_right);
  }
  motor_stop();
  ui_signal_ok();
}
