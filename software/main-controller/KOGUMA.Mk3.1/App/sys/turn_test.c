/*
 * turn_test.c
 * 最短走行のターン（大回り・斜め）を 1 つずつ走らせて調整する（モード3 No.1〜7・No.9〜15。MODES.md）。
 * 斜めの直進だけを走らせて、斜めの補正を調整する（モード4 No.2・3。下の「■ 斜めの直進のテスト」）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/turn_test.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - ターンの組が 2 つ（config/turn_params.h）なので、モード3 No.1〜7 は組 2000、No.9〜15 は組 2400 の同じ並びのターン
 *     （No.9 大回り 90°〜No.15 V90°。KOGUMA.MkIIl のモード6 No.1〜7・モード7 No.1〜7 と同じ並び）
 *   - 斜めの直進のテスト（ToraNeko.Mk3 のモード3 No.8・9）は、モード4 No.2・3 に移した（turn_test_run_diag()）
 *   - 組 1000（探索の既知区間を斜めで走るときのターン。モード0 No.12）を、モード2 No.9〜15 で走らせる（turn_test_run_1000()）
 *   - 速くする・止まる加速度を 25000mm/s² にした（2400mm/s まで、斜め 1 つぶん 127mm で上げるため。下の TURN_TEST_ACCEL_MM_S2）
 *   - 吸引の強さを、組 2000 は 7.9V、組 2400 は 11.1V にした（最短走行と同じ。config/run_params.h の SHORTEST*_SUCTION_V）
 *   - 走行ログは 1ms ごとで 1600 行（1.6 秒）
 * 下の説明の距離は、ToraNeko.Mk3 の区画（90mm）のときのもの。KOGUMA.Mk3.1 は区画 180mm なので、2 倍になる
 * （例：スタート区画から次の区画のまん中まで 24 + 180 = 204mm、斜め 1 つぶん 127.3mm）。
 *
 * KOGUMA のターン単体テスト（Move.c の Turn1000_Test など）と同じやり方（2026-11-03 に作り直した）：
 *   - ターンは 1 つだけ走る（ほかのターンをつなげない。つなげると、2 つのターンのずれが混ざってしまう）
 *   - 区画のまん中から入るターン（No.1〜4）は、スタート区画に置いて始める（お尻を後ろの壁に付ける。置き方がそろう）
 *   - 斜めから入るターン（No.5〜7）は、機体を斜めに置いて始め、斜め 1 つぶん進んでから曲がる
 * 曲がるのは左だけ（KOGUMA と同じ）。
 *
 * ■ 流れ
 *   1. 吸引を上げて（0.5 秒）、0.5 秒待つ（最短走行は吸引して走るので、ターンも吸引して合わせる。
 *      吸引なしでは 4000mm/s² で前後に滑っていて、500mm/s のターンの横向きの加速度 3000〜5800mm/s² では滑る）
 *   2. ジャイロのゼロ点を測り直す（約 0.5 秒。吸ったまま測る）
 *   3. そのターンの速さ（config/turn_params.h の *_SPEED_MM_S。ゆるいターンは 800mm/s、きついターンは 600mm/s）まで
 *      速くしながら、ターンの始まりまで進む
 *        No.1〜4：スタート区画から、次の区画のまん中まで（START_OFFSET_MM + 1 区画 = 100mm）
 *        No.5〜7：区画の辺のまん中から、斜めに 1 つぶん（63.6mm）進んで、次の辺のまん中まで
 *   4. 調べるターンを 1 つ走る
 *   5. 止まる：区画のまん中で終わるターンのあとは 1 区画（90mm）、斜めで終わるターンのあとは
 *      斜めの直進 1 つぶん（63.6mm）進みながら止まる
 *   6. 吸引を止める
 * 走行ログは 1ms ごと（1 秒ぶん）。モード7 No.1 で出す。
 *
 * ■ 置き方と、どこで止まれば合っているか
 * 始めた区画を (0, 0)、北を前、東を x の + とする。単位は区画（区画のまん中が整数）。PC で計算した位置。
 *   No.1〜4：(0, 0) のスタート区画に、北向きに、お尻を後ろの壁に付けて置く
 *     No.1 大回り 90°  ：(0, 1) のまん中から曲がり、(−1, 2) のまん中で西を向く → 1 区画進んで (−2, 2) のまん中で止まる
 *     No.2 大回り 180° ：(0, 1) のまん中から曲がり、(−1, 1) のまん中で南を向く → 1 区画進んで (−1, 0) のまん中で止まる
 *     No.3 斜め入り 45°：(0, 2) の西の辺のまん中で北西を向く → 斜めに 1 つ進んで (−1, 2) の北の辺のまん中で止まる
 *     No.4 斜め入り 135°：(−1, 2) の南の辺のまん中で南西を向く → 斜めに 1 つ進んで (−1, 1) の西の辺のまん中で止まる
 *   No.5〜7：(0, 0) の南の辺のまん中に、タイヤの軸を置き、北東（右斜め前 45°）を向けて置く
 *     斜めに 1 つ進んで (0, 0) の東の辺のまん中で、曲がり始める
 *     No.5 斜め出 45°  ：(1, 1) のまん中で北を向く → 1 区画進んで (1, 2) のまん中で止まる
 *     No.6 斜め出 135° ：(0, 1) のまん中で西を向く → 1 区画進んで (−1, 1) のまん中で止まる
 *     No.7 V90°        ：(0, 1) の東の辺のまん中で北西を向く → 斜めに 1 つ進んで (0, 1) の北の辺のまん中で止まる
 * 通るところに壁や柱があるとぶつかるので、迷路の中ではなく、柱だけ（または何もない）の広いところで走らせる。
 * 止まった位置・向きがずれていたら、そのターンの前距離・後距離（config/turn_params.h）を直す。
 * ターンの調整（No.1〜7）では、斜めの補正（ctrl/control.c）を入れない（補正で曲がると、ターンのずれと混ざるため）。
 *
 * ■ 斜めの直進のテスト（モード4 No.2・3。ToraNeko.Mk3 のモード3 No.8・9）
 * 斜めの通り道（階段のような形に壁を並べたもの）で、斜めに DIAG_TEST_STEPS つぶん（6 つ、764mm）走って止まる。
 * 速さは 2000mm/s（組 2000 のターンの速さ）。吸引する（組 2000 と同じ 7.9V）。
 *   No.2：斜めの補正を入れない。走行ログに前のセンサの値が残るので、まん中を走ったときの値から、しきい値を決める
 *   No.3：斜めの補正を入れる。柱に近づいたら離れるかを見る
 * 置き方は No.5〜7 と同じ（(0, 0) の南の辺のまん中にタイヤの軸を置き、北東を向ける）。通り道は、
 * (0, 0)・(1, 0)・(1, 1)・(2, 1)・(2, 2)・(3, 2)・(3, 3) の 7 区画。通り道が通る辺（(0, 0) の南（置くところ）・
 * (0, 0) の東・(1, 0) の北・(1, 1) の東・(2, 1) の北・(2, 2) の東・(3, 2) の北）には壁を置かず、7 区画のほかの辺には
 * 全部壁を置く。止まるのは (3, 2) の北の辺のまん中。
 * 迷路の外の壁（南の端）にお尻が当たるので、迷路の角ではなく、まん中のあいているところに作る。
 */
#include "sys/turn_test.h"

#include <stdbool.h>

#include "config/run_params.h"
#include "config/turn_params.h"
#include "ctrl/run.h"
#include "drv/fan.h"
#include "drv/timebase.h"
#include "sys/failsafe.h"
#include "sys/log.h"
#include "sys/suction.h"
#include "sys/ui.h"

#define TURN_TEST_LOG_INTERVAL_MS 1         /* 走行ログの間隔 [ms]（1600 行で 1.6 秒ぶん） */
#define TURN_TEST_ACCEL_MM_S2     25000.0f  /* 速くする・止まるときの加速度 [mm/s²]（2400mm/s まで 115mm。斜め 1 つぶん 127mm で
                                               2400mm/s に上げて止めるため。KOGUMA.MkIIl の直線の限界テストは 40000mm/s²） */
#define TURN_TEST_SUCTION_WAIT_MS 500       /* 吸引を上げてから、走り出す前に待つ時間 [ms]（ファンの回り方を落ち着かせる） */
#define DIAG_TEST_STEPS           6         /* 斜めの直進のテスト（モード4 No.2・3）で進む、斜めの直進の数（6 つで 764mm） */
#define DIAG_TEST_SPEED_MM_S      2000.0f   /* 同じく、速さ [mm/s]（組 2000 のターンの速さと同じ） */

/* 吸引を suction_v [V] まで上げ、制御とログを始める（まだ動かない） */
static void prepare(float suction_v)
{
  run_fast_set_diag_correction(false);  /* ターンの調整では、斜めの補正を入れない（先頭のコメント） */
  suction_ramp_up_to(suction_v);  /* 0.5 秒かけて吸引を上げる（sys/suction.c） */
  ui_wait_ms(TURN_TEST_SUCTION_WAIT_MS);
  run_fast_start();   /* ジャイロのゼロ点を測り直してから、制御を始める */
  log_start(TURN_TEST_LOG_INTERVAL_MS);  /* ターンを細かく見たいので、ログは 1ms ごとにし直す */
}

/* No.1〜4：スタート区画から、次の区画のまん中まで進みながら、速さ speed_mm_s にする。
 * 迷路の外の広いところで走るので、横の壁に沿う補正は入れない（run_fast_diag_straight は、横の壁に沿う補正を入れない直進。
 * 斜めの補正も、prepare() で切ってある。走行ログでは、この直進も「斜めの直線」（phase 2）になり、前のセンサの値が入る）。
 * 2026-11-04 に、補正を入れる run_fast_straight から変えた（止まるときに、横のセンサが何かを見て補正が入り、0.5° 回った） */
static void approach_from_start(float speed_mm_s)
{
  run_fast_diag_straight(START_OFFSET_MM + CELL_MM, speed_mm_s, speed_mm_s, TURN_TEST_ACCEL_MM_S2);
}

/* No.5〜7：区画の辺のまん中から、斜めに 1 つぶん進みながら、速さ speed_mm_s にする */
static void approach_on_diag(float speed_mm_s)
{
  run_fast_diag_straight(DIAG_STEP_MM, speed_mm_s, speed_mm_s, TURN_TEST_ACCEL_MM_S2);
}

/* 区画のまん中から、1 区画進みながら止まる（速さ speed_mm_s から）。横の壁に沿う補正は入れない（approach_from_start と同じ） */
static void stop_after_cell(float speed_mm_s)
{
  run_fast_diag_stop(CELL_MM, speed_mm_s, TURN_TEST_ACCEL_MM_S2);
}

/* 斜めの上（壁の辺のまん中）から、斜めの直進 1 つぶん進みながら止まる（速さ speed_mm_s から） */
static void stop_after_diag(float speed_mm_s)
{
  run_fast_diag_stop(DIAG_STEP_MM, speed_mm_s, TURN_TEST_ACCEL_MM_S2);
}

/* ターン turn を、そのターンの速さで、左に 1 つ走る */
static void run_one_turn(run_fast_turn_t turn)
{
  run_fast_turn(turn, true, run_fast_turn_speed_mm_s(turn));
}

/* 終わったことを LED で知らせる。フェイルセーフで止まったときは、スイッチが押されるまで待って解除する */
static void signal_end(void)
{
  if (failsafe_is_stopped()) {
    ui_wait_failsafe_reset();
    failsafe_clear();
  } else {
    ui_signal_ok();
  }
}

/* No. で、調べるターンを決める（1 大回り 90°、2 大回り 180°、3 斜め入り 45°、4 斜め入り 135°、
 * 5 斜め出 45°、6 斜め出 135°、7 V90°。組 2400 の No.9〜15 は、8 を引いた番号で呼ぶ） */
static run_fast_turn_t turn_of_number(int number)
{
  if (number == 1) {
    return RUN_FAST_TURN_BIG90;
  }
  if (number == 2) {
    return RUN_FAST_TURN_BIG180;
  }
  if (number == 3) {
    return RUN_FAST_TURN_IN45;
  }
  if (number == 4) {
    return RUN_FAST_TURN_IN135;
  }
  if (number == 5) {
    return RUN_FAST_TURN_OUT45;
  }
  if (number == 6) {
    return RUN_FAST_TURN_OUT135;
  }
  return RUN_FAST_TURN_V90;
}

/* ターン turn が、斜めの上（壁の辺のまん中）で終わるか（斜め入り 45°・135°、V90°）。
 * そうでなければ、区画のまん中で終わる（大回り、斜め出） */
static bool ends_on_diag(run_fast_turn_t turn)
{
  if (turn == RUN_FAST_TURN_IN45 || turn == RUN_FAST_TURN_IN135 || turn == RUN_FAST_TURN_V90) {
    return true;
  }
  return false;
}

/* モード4 No.2・3：斜めの直進のテストを走らせる（先頭のコメントの「■ 斜めの直進のテスト」）。
 * use_correction が true なら斜めの補正を入れる（No.3）、false なら入れない（No.2）。組 2000 の吸引で走る */
void turn_test_run_diag(bool use_correction)
{
  prepare(SHORTEST5_SUCTION_V);
  run_fast_set_diag_correction(use_correction);
  run_fast_diag_stop(DIAG_TEST_STEPS * DIAG_STEP_MM, DIAG_TEST_SPEED_MM_S, TURN_TEST_ACCEL_MM_S2);
  run_fast_set_diag_correction(true);  /* ふつうの「入れる」に戻す（最短走行のため） */
  fan_stop();
  signal_end();
}

static void run_turn_test(int turn_number, run_turn_set_t set, float suction_v);  /* 下にある（名前だけ先に書く） */

/* モード3 No.number のターンを走らせる（流れは先頭のコメント）。No.1〜7 は組 2000、No.9〜15 は組 2400 */
void turn_test_run(int number)
{
  if (number >= 9) {
    run_turn_test(number - 8, RUN_TURN_SET_2400, SHORTEST8_SUCTION_V);  /* 組 2400 は、組 2000 と同じ並び */
  } else {
    run_turn_test(number, RUN_TURN_SET_2000, SHORTEST5_SUCTION_V);
  }
}

/* モード2 No.number（9〜15）：組 1000 のターンを走らせる（KOGUMA.Mk3.1 で足した。探索の既知区間を斜めで走るときのターン。
 * KOGUMA.MkIIl のモード5 No.1〜7（Turn1000_Test）。並びはモード3 と同じ。吸引は探索と同じ 5.5V） */
void turn_test_run_1000(int number)
{
  run_turn_test(number - 8, RUN_TURN_SET_1000, SUCTION_VOLTAGE_V);
}

/* ターン turn_number（1〜7。turn_of_number() の並び）を、組 set で、吸引 suction_v [V] で走らせる（流れは先頭のコメント） */
static void run_turn_test(int turn_number, run_turn_set_t set, float suction_v)
{
  run_fast_set_turn_set(set);
  run_fast_turn_t turn = turn_of_number(turn_number);
  float speed_mm_s = run_fast_turn_speed_mm_s(turn);

  prepare(suction_v);

  /* ターンの始まりまで進む（No.1〜4 は区画のまん中から、No.5〜7 は斜めから入る） */
  if (turn_number <= 4) {
    approach_from_start(speed_mm_s);
  } else {
    approach_on_diag(speed_mm_s);
  }

  run_one_turn(turn);

  /* 止まる */
  if (ends_on_diag(turn)) {
    stop_after_diag(speed_mm_s);
  } else {
    stop_after_cell(speed_mm_s);
  }

  fan_stop();  /* 吸引を止める（フェイルセーフで止まったときは、もう止まっている） */
  run_fast_set_diag_correction(true);  /* ふつうの「入れる」に戻す（最短走行のため） */
  run_fast_set_turn_set(RUN_TURN_SET_2000);  /* 組 2000 に戻す（最短走行は、走るたびに No. の組を選ぶ） */
  signal_end();
}
