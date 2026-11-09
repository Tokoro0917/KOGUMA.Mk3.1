/*
 * shortest.c
 * 最短走行（モード1。MODES.md）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/shortest.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - No. を 1〜9 にした（config/run_params.h の SHORTEST1〜9。No.4〜9 は KOGUMA.MkIIl のモード1 No.4〜9 と同じ値）。
 *     No. ごとの値を 1 つの表（shortest_values_t、values_of()）にまとめた
 *   - 小回りだけで走るのは No.1 だけ（ToraNeko.Mk3 は No.1〜3）。No.2〜9 は大回り・斜め
 *   - ターンの組（組 2000・組 2400）と吸引の強さを、No. ごとに選ぶ
 *   - 経路を選ぶときのターンの速さを、その No. のターンの組の速さにした（ToraNeko.Mk3 は、どの No. も 500mm/s）
 *   - 未知壁探索（sys/explore.c）が、最短走行の組ごとの経路を求められるように、shortest_set_route_cost() を出した（B15）
 *   - 最短走行のあとにスタートへ帰れるように、止まった区画・向き・まん中を過ぎた距離を返す関数（shortest_get_end_*）を
 *     足した（モード0 No.11）
 *
 * RAM の迷路で、スタートからゴールまでの最短経路を作り、その経路のとおりに走って、ゴールの区画で止まる。
 * まだ見ていない壁は「ある」とみなす。No. で走り方と速さを選ぶ（config/run_params.h の SHORTEST1〜5）。
 *
 * ■ No.1：曲がるところは、探索と同じ小回りだけ
 *   1. 経路を作る（maze/path.c の path_make_shortest_small_turn()）
 *   2. 直進は最高速まで速くしてから、小回りの速さ（吸引なしの探索の速さ、500mm/s）まで遅くする。左・右は探索と同じスラローム 90°
 *   3. 最後の直進で、ゴールの最初の区画のまん中で止まる
 *
 * ■ No.2〜9：大回り・斜めのターンで曲がる（ToraNeko.Mk3 の No.4・5 と同じ走り方）
 *   1. 吸引を上げて（0.5 秒）、0.5 秒待つ（強さは No. ごと。組 2000 は 7.9V、組 2400 は 11.1V。KOGUMA.MkIIl と同じ）
 *   2. 経路を作る（maze/path.c の path_make_shortest()。大回り 90°・180°、斜め入り・斜め出 45°・135°、V90°、斜めの直進）
 *   3. ターンは、そのターンの速さ（No. のターンの組の *_SPEED_MM_S。組 2000 は 2000mm/s、組 2400 は 2400mm/s）で
 *      曲がる。直進・斜めの直進は、最高速まで速くしてから、次のターンの速さまで遅くする。
 *      ただし、次のことがあるときは、それより遅い速さで直進を終わり、次のターンをその速さで曲がる
 *      （同じ形のまま遅く曲がる。ctrl/run.c の fast_turn()）
 *        - 短くて上げきれないとき（スタートの直後など）
 *        - ターンが直進をはさまずに続くとき（ターンのあいだは速さを変えられないので、続くターンの遅いほうに合わせる）
 *        - その先の短い直進で、次の次のターンの速さまで遅くしきれないとき
 *      どこまで遅くしておくかは、経路の終わりまで先を見て決める（speed_limit_ahead_mm_s()）
 *   4. 最後が直進なら、ゴールの最初の区画のまん中で止まる。最後がターン（ゴールにターンで入る）なら、
 *      区画のまん中に着いたところから SHORTEST_STOP_ACCEL_MM_S2 で、SHORTEST_STOP_DISTANCE_MM 以内で止まる
 *      （止まれる速さより速く、最後のターンを曲がらないようにする）
 *   5. 吸引を止める
 *   小回り（MOTION_TURN_LEFT・RIGHT）は経路に残らない（PC で 500 個の迷路を調べて 0 個）が、残ったら走らずに「道がない」にする
 *
 * 経路は、その走り方と速さで本当にかかる時間が短いものを選ぶ（ダイクストラの走行時間のコスト。maze/dijkstra.c）。
 * No.2〜9 は「直線 ＋ ターン 1 つ」ごとの時間で数える（KOGUMA の #100）ので、ターンの種類ごとの時間の違いまで比べる。
 * 時間を数えるときは、どのターンも、その No. のターンの組の速さ（2000 か 2400mm/s）で曲がるとして数える（本当はターンごとに速さが違うが、
 * 直線の時間が前後のターンの速さで変わり、ダイクストラで数えにくいため。ターンの形はどれも同じなので、選ぶ道はあまり変わらない）。
 *
 * ■ スタートの位置
 * 経路は、スタート区画 (0,0) の南の辺から始まる。機体は、お尻を後ろの壁に付けて置くので、
 * タイヤの軸は南の辺より HALF_CELL_MM − START_OFFSET_MM（66mm）前にある。最初の直進は、その分だけ短くする。
 */
#include "sys/shortest.h"

#include <math.h>

#include "config/run_params.h"
#include "config/turn_params.h"
#include "ctrl/run.h"
#include "drv/fan.h"
#include "maze/dijkstra.h"
#include "maze/path.h"
#include "sys/failsafe.h"
#include "sys/goal.h"
#include "sys/suction.h"
#include "sys/ui.h"

#define SHORTEST_SUCTION_WAIT_MS 500      /* 吸引を上げてから、走り出す前に待つ時間 [ms]（ファンの回り方を落ち着かせる） */
#define SPEED_NO_LIMIT_MM_S      100000.0f /* 「速さの上限なし」を表す、どの速さより大きい値 [mm/s] */

static path_t s_path;  /* 最短経路（大きいので、関数の中ではなくここに置く） */

/* 最後に止まったところが、ゴールの区画のまん中より何 mm 先か（ゴールにターンで入って止まったとき。直進で止まったら 0）。
 * 最短走行のあとにスタートへ帰るとき（モード0 No.11）に、区画のまん中へ戻すために使う */
static float s_end_overshoot_mm = 0.0f;

/* No. ごとの値（config/run_params.h の SHORTEST1〜9。KOGUMA.Mk3.1 で、ToraNeko.Mk3 の No. ごとの関数を 1 つの表にまとめた） */
typedef struct {
  int turn;                     /* ターンの組（0 なら小回りだけ、2000・2400 は大回り・斜めの組） */
  float max_speed_mm_s;         /* 縦横の直進の最高速 [mm/s] */
  float accel_mm_s2;            /* 縦横の直進の加速度 [mm/s²] */
  float diag_max_speed_mm_s;    /* 斜めの直進の最高速 [mm/s] */
  float diag_accel_mm_s2;       /* 斜めの直進の加速度 [mm/s²] */
  float suction_v;              /* 吸引の強さ [V]（0 なら吸わない） */
} shortest_values_t;

/* No. ごとの値を取り出す（表にない No. は No.1 の値） */
static shortest_values_t values_of(int number)
{
  shortest_values_t values = {SHORTEST1_TURN, SHORTEST1_MAX_SPEED_MM_S, SHORTEST1_ACCEL_MM_S2,
                              SHORTEST1_DIAG_MAX_SPEED_MM_S, SHORTEST1_DIAG_ACCEL_MM_S2, SHORTEST1_SUCTION_V};
  if (number == 2) {
    shortest_values_t no2 = {SHORTEST2_TURN, SHORTEST2_MAX_SPEED_MM_S, SHORTEST2_ACCEL_MM_S2,
                             SHORTEST2_DIAG_MAX_SPEED_MM_S, SHORTEST2_DIAG_ACCEL_MM_S2, SHORTEST2_SUCTION_V};
    values = no2;
  } else if (number == 3) {
    shortest_values_t no3 = {SHORTEST3_TURN, SHORTEST3_MAX_SPEED_MM_S, SHORTEST3_ACCEL_MM_S2,
                             SHORTEST3_DIAG_MAX_SPEED_MM_S, SHORTEST3_DIAG_ACCEL_MM_S2, SHORTEST3_SUCTION_V};
    values = no3;
  } else if (number == 4) {
    shortest_values_t no4 = {SHORTEST4_TURN, SHORTEST4_MAX_SPEED_MM_S, SHORTEST4_ACCEL_MM_S2,
                             SHORTEST4_DIAG_MAX_SPEED_MM_S, SHORTEST4_DIAG_ACCEL_MM_S2, SHORTEST4_SUCTION_V};
    values = no4;
  } else if (number == 5) {
    shortest_values_t no5 = {SHORTEST5_TURN, SHORTEST5_MAX_SPEED_MM_S, SHORTEST5_ACCEL_MM_S2,
                             SHORTEST5_DIAG_MAX_SPEED_MM_S, SHORTEST5_DIAG_ACCEL_MM_S2, SHORTEST5_SUCTION_V};
    values = no5;
  } else if (number == 6) {
    shortest_values_t no6 = {SHORTEST6_TURN, SHORTEST6_MAX_SPEED_MM_S, SHORTEST6_ACCEL_MM_S2,
                             SHORTEST6_DIAG_MAX_SPEED_MM_S, SHORTEST6_DIAG_ACCEL_MM_S2, SHORTEST6_SUCTION_V};
    values = no6;
  } else if (number == 7) {
    shortest_values_t no7 = {SHORTEST7_TURN, SHORTEST7_MAX_SPEED_MM_S, SHORTEST7_ACCEL_MM_S2,
                             SHORTEST7_DIAG_MAX_SPEED_MM_S, SHORTEST7_DIAG_ACCEL_MM_S2, SHORTEST7_SUCTION_V};
    values = no7;
  } else if (number == 8) {
    shortest_values_t no8 = {SHORTEST8_TURN, SHORTEST8_MAX_SPEED_MM_S, SHORTEST8_ACCEL_MM_S2,
                             SHORTEST8_DIAG_MAX_SPEED_MM_S, SHORTEST8_DIAG_ACCEL_MM_S2, SHORTEST8_SUCTION_V};
    values = no8;
  } else if (number == 9) {
    shortest_values_t no9 = {SHORTEST9_TURN, SHORTEST9_MAX_SPEED_MM_S, SHORTEST9_ACCEL_MM_S2,
                             SHORTEST9_DIAG_MAX_SPEED_MM_S, SHORTEST9_DIAG_ACCEL_MM_S2, SHORTEST9_SUCTION_V};
    values = no9;
  }
  return values;
}

/* 大回り・斜めで走る No. か（No.2〜9。No.1 は小回りだけ） */
static bool uses_diagonal(int number)
{
  if (values_of(number).turn != 0) {
    return true;
  }
  return false;
}

/* No. ごとの、直進の最高速 [mm/s] */
static float max_speed_mm_s(int number)
{
  return values_of(number).max_speed_mm_s;
}

/* No. ごとの、直進の加速度 [mm/s²] */
static float accel_mm_s2(int number)
{
  return values_of(number).accel_mm_s2;
}

/* No. ごとの、斜めの直進の最高速 [mm/s]（大回り・斜めで走る No. だけ使う） */
static float diag_max_speed_mm_s(int number)
{
  return values_of(number).diag_max_speed_mm_s;
}

/* No. ごとの、斜めの直進の加速度 [mm/s²]（大回り・斜めで走る No. だけ使う） */
static float diag_accel_mm_s2(int number)
{
  return values_of(number).diag_accel_mm_s2;
}

/* No. ごとの、ターンの速さ [mm/s]（走る時間の見積もりで、どのターンもこの速さで曲がるとする。小回りだけなら探索の速さ） */
static float turn_speed_mm_s(int number)
{
  int turn = values_of(number).turn;
  if (turn == 2400) {
    return BIG90_2400_SPEED_MM_S;
  }
  if (turn == 2000) {
    return FAST_TURN_SPEED_MM_S;
  }
  return SEARCH_SPEED_MM_S;
}

/* No. ごとの、ターンの組を ctrl/run.c に選ばせる */
static void select_turn_set(int number)
{
  if (values_of(number).turn == 2400) {
    run_fast_set_turn_set(RUN_TURN_SET_2400);
  } else {
    run_fast_set_turn_set(RUN_TURN_SET_2000);
  }
}

/* ダイクストラを、No. number の速さで走る時間で比べるようにする（未知壁探索の組ごとの経路を求めるときにも、
 * sys/explore.c が使う。KOGUMA_PORTING.md の B15）。使い終わったら dijkstra_set_step_cost() で戻す */
void shortest_set_route_cost(int number)
{
  if (uses_diagonal(number)) {
    dijkstra_set_time_cost_diagonal(turn_speed_mm_s(number), max_speed_mm_s(number), accel_mm_s2(number),
                                    diag_max_speed_mm_s(number), diag_accel_mm_s2(number));
  } else {
    dijkstra_set_time_cost_small_turn(SEARCH_SPEED_MM_S, max_speed_mm_s(number), accel_mm_s2(number));
  }
}

/* 経路の中に、小回り（探索用の左右のターン）があるか（No.2〜9 では走れない） */
static bool has_small_turn(void)
{
  for (int i = 0; i < s_path.count; i++) {
    if (s_path.steps[i].motion == MOTION_TURN_LEFT || s_path.steps[i].motion == MOTION_TURN_RIGHT) {
      return true;
    }
  }
  return false;
}

/* 最短経路を作って s_path に入れる。number は No.（直進の速さで、選ぶ経路が変わる）。走れる経路ができなければ false。
 * 最初の動きは直進でないといけない（スタート区画の中で曲がる動きは作っていない） */
static bool make_path(int number, int goal_x, int goal_y)
{
  /* ダイクストラを、この No. の速さで走る時間で比べるようにしてから経路を作り、終わったら元に戻す
   * （未知壁探索など、ほかでダイクストラを使うときに影響しないように） */
  bool found = false;
  shortest_set_route_cost(number);
  if (uses_diagonal(number)) {
    found = path_make_shortest(goal_x, goal_y, GOAL_WIDTH, GOAL_HEIGHT, &s_path);
  } else {
    found = path_make_shortest_small_turn(goal_x, goal_y, GOAL_WIDTH, GOAL_HEIGHT, &s_path);
  }
  dijkstra_set_step_cost();
  if (!found) {
    return false;
  }
  if (uses_diagonal(number) && has_small_turn()) {
    return false;  /* 小回りが残った（先頭のコメント。普通は起きない） */
  }
  if (s_path.count == 0) {
    return false;
  }
  if (s_path.steps[0].motion != MOTION_STRAIGHT) {
    return false;
  }
  return true;
}

/* スタートからゴールへの道があるか（モードを決めたときに調べる。number は No.） */
bool shortest_has_route(int number, int goal_x, int goal_y)
{
  return make_path(number, goal_x, goal_y);
}

/* スタート区画の南の辺から、タイヤの軸までの距離 [mm]（先頭のコメントの「■ スタートの位置」。66mm） */
static float straight_start_offset_mm(void)
{
  return HALF_CELL_MM - START_OFFSET_MM;
}

/* 経路の i 番目の直進の長さ [mm]。最初の直進は、スタートの置き方の分だけ短くする（先頭のコメント） */
static float straight_distance_mm(int i)
{
  float distance_mm = s_path.steps[i].half_count * HALF_CELL_MM;
  if (i == 0) {
    distance_mm = distance_mm - straight_start_offset_mm();
  }
  return distance_mm;
}

/* 経路の i 番目の直進を始める位置が、うしろの区画の境目から何 mm 先か（壁切れに使う。ctrl/run.c の run_fast_straight）。
 *   最初の直進：スタート区画の南の辺から、タイヤの軸まで（先頭のコメントの「■ スタートの位置」。66mm）
 *   小回りのあと（No.1）：小回りは区画の入口から次の区画の入口まで曲がるので、境目の上（0mm）
 *   大回り・斜め出のあと（No.2〜9）：区画のまん中（90mm） */
static float straight_start_in_cell_mm(int i)
{
  if (i == 0) {
    return straight_start_offset_mm();
  }
  motion_t before = s_path.steps[i - 1].motion;
  if (before == MOTION_TURN_LEFT || before == MOTION_TURN_RIGHT) {
    return 0.0f;
  }
  return HALF_CELL_MM;
}

/* No.1：経路の i 番目の動きを走る。最後の動き（ゴールの区画のまん中まで進む直進）では止まる */
static void run_small_turn_step(int i, float max_speed, float accel)
{
  motion_t motion = s_path.steps[i].motion;
  bool is_last = (i == s_path.count - 1);

  if (motion == MOTION_STRAIGHT && is_last) {
    run_fast_stop(straight_distance_mm(i), max_speed, accel);
  } else if (motion == MOTION_STRAIGHT) {
    /* 次の小回りの速さで終わる */
    run_fast_straight(straight_distance_mm(i), max_speed, SEARCH_SPEED_MM_S, accel, straight_start_in_cell_mm(i));
  } else if (motion == MOTION_TURN_LEFT) {
    run_fast_turn_left();
  } else if (motion == MOTION_TURN_RIGHT) {
    run_fast_turn_right();
  }
}

/* No.2〜9：動き motion が、どの大回り・斜めのターンか */
static run_fast_turn_t fast_turn_kind(motion_t motion)
{
  if (motion == MOTION_BIG_TURN_LEFT_90 || motion == MOTION_BIG_TURN_RIGHT_90) {
    return RUN_FAST_TURN_BIG90;
  }
  if (motion == MOTION_BIG_TURN_LEFT_180 || motion == MOTION_BIG_TURN_RIGHT_180) {
    return RUN_FAST_TURN_BIG180;
  }
  if (motion == MOTION_DIAG_IN45_LEFT || motion == MOTION_DIAG_IN45_RIGHT) {
    return RUN_FAST_TURN_IN45;
  }
  if (motion == MOTION_DIAG_IN135_LEFT || motion == MOTION_DIAG_IN135_RIGHT) {
    return RUN_FAST_TURN_IN135;
  }
  if (motion == MOTION_DIAG_OUT45_LEFT || motion == MOTION_DIAG_OUT45_RIGHT) {
    return RUN_FAST_TURN_OUT45;
  }
  if (motion == MOTION_DIAG_OUT135_LEFT || motion == MOTION_DIAG_OUT135_RIGHT) {
    return RUN_FAST_TURN_OUT135;
  }
  return RUN_FAST_TURN_V90;
}

/* No.2〜9：動き motion が左に曲がるターンか */
static bool is_left_turn(motion_t motion)
{
  if (motion == MOTION_BIG_TURN_LEFT_90 || motion == MOTION_BIG_TURN_LEFT_180
      || motion == MOTION_DIAG_IN45_LEFT || motion == MOTION_DIAG_IN135_LEFT
      || motion == MOTION_DIAG_OUT45_LEFT || motion == MOTION_DIAG_OUT135_LEFT
      || motion == MOTION_DIAG_V90_LEFT) {
    return true;
  }
  return false;
}

/* 2 つの速さの、遅いほう [mm/s] */
static float slower_mm_s(float a_mm_s, float b_mm_s)
{
  if (a_mm_s < b_mm_s) {
    return a_mm_s;
  }
  return b_mm_s;
}

/* 経路の i 番目の斜めの直進の長さ [mm] */
static float diag_distance_mm(int i)
{
  return s_path.steps[i].half_count * DIAG_STEP_MM;
}

/* No.2〜9：経路の first 番目の動きを始めるときに、出していてよい速さの上限 [mm/s]（先頭のコメントの 3）。
 * その先のどのターンも、そのターンの速さ以下で曲がれて、最後に止まれる速さにする。
 * 直進で遅くできる分は「速さの 2 乗」で 2 × 加速度 × 距離（速さ v から、距離 d で遅くできるのは √(v² − 2ad) まで）。
 * なので、あるターンの手前までに遅くできる分を slow_down とすると、今の速さは √(ターンの速さ² ＋ slow_down) までよい。
 * これを経路の終わりまで、ターンごとに計算して、いちばん遅いものにする */
static float speed_limit_ahead_mm_s(int first, float accel, float diag_accel)
{
  float limit_mm_s = SPEED_NO_LIMIT_MM_S;
  float slow_down_mm2_s2 = 0.0f;  /* first 番目から、今見ている動きの手前までの直進で遅くできる分（速さの 2 乗） */

  for (int i = first; i < s_path.count; i++) {
    motion_t motion = s_path.steps[i].motion;
    bool is_last = (i == s_path.count - 1);

    if (motion == MOTION_STRAIGHT && is_last) {
      /* 最後の直進：その終わり（ゴールの区画のまん中）で止まれる速さ */
      float stop_mm2_s2 = slow_down_mm2_s2 + 2.0f * accel * straight_distance_mm(i);
      return slower_mm_s(limit_mm_s, sqrtf(stop_mm2_s2));
    }
    if (motion == MOTION_STRAIGHT) {
      slow_down_mm2_s2 = slow_down_mm2_s2 + 2.0f * accel * straight_distance_mm(i);
    } else if (motion == MOTION_DIAG_STRAIGHT) {
      slow_down_mm2_s2 = slow_down_mm2_s2 + 2.0f * diag_accel * diag_distance_mm(i);
    } else {
      /* ターン：そのターンの速さで曲がれる速さ */
      float turn_mm_s = run_fast_turn_speed_mm_s(fast_turn_kind(motion));
      float turn_limit_mm_s = sqrtf(turn_mm_s * turn_mm_s + slow_down_mm2_s2);
      limit_mm_s = slower_mm_s(limit_mm_s, turn_limit_mm_s);
    }
  }

  /* 最後がターン（ゴールにターンで入る）：区画のまん中に着いてから SHORTEST_STOP_DISTANCE_MM で止まれる速さ */
  float stop_mm_s = sqrtf(2.0f * SHORTEST_STOP_ACCEL_MM_S2 * SHORTEST_STOP_DISTANCE_MM);
  float stop_limit_mm_s = sqrtf(stop_mm_s * stop_mm_s + slow_down_mm2_s2);
  return slower_mm_s(limit_mm_s, stop_limit_mm_s);
}

/* No.2〜9：直進の終わりの速さ [mm/s]。速さ start_mm_s から distance_mm で上げられる速さ、最高速 max_speed、
 * その先を見た上限 limit_mm_s の、いちばん遅いもの（上げきれない速さを頼むと、着いたときに速さが飛んでしまうので） */
static float straight_end_speed_mm_s(float start_mm_s, float distance_mm, float accel, float max_speed, float limit_mm_s)
{
  float reachable_mm_s = sqrtf(start_mm_s * start_mm_s + 2.0f * accel * distance_mm);
  float end_mm_s = slower_mm_s(reachable_mm_s, max_speed);
  end_mm_s = slower_mm_s(end_mm_s, limit_mm_s);
  return end_mm_s;
}

/* No.2〜9：経路を前から順に走る。speed_mm_s は今の速さ（直進が終わった速さで、次のターンを曲がる） */
static void run_diagonal_path(int number)
{
  float max_speed = max_speed_mm_s(number);
  float accel = accel_mm_s2(number);
  float diag_max_speed = diag_max_speed_mm_s(number);
  float diag_accel = diag_accel_mm_s2(number);
  float speed_mm_s = 0.0f;
  bool last_was_turn = false;

  for (int i = 0; i < s_path.count; i++) {
    motion_t motion = s_path.steps[i].motion;
    bool is_last = (i == s_path.count - 1);
    last_was_turn = false;

    if (motion == MOTION_STRAIGHT && is_last) {
      run_fast_stop(straight_distance_mm(i), max_speed, accel);  /* ゴールの区画のまん中で止まる */
      speed_mm_s = 0.0f;
    } else if (motion == MOTION_STRAIGHT) {
      float distance_mm = straight_distance_mm(i);
      float limit_mm_s = speed_limit_ahead_mm_s(i + 1, accel, diag_accel);
      float end_mm_s = straight_end_speed_mm_s(speed_mm_s, distance_mm, accel, max_speed, limit_mm_s);
      run_fast_straight(distance_mm, max_speed, end_mm_s, accel, straight_start_in_cell_mm(i));
      speed_mm_s = end_mm_s;
    } else if (motion == MOTION_DIAG_STRAIGHT) {
      float distance_mm = diag_distance_mm(i);
      float limit_mm_s = speed_limit_ahead_mm_s(i + 1, accel, diag_accel);
      float end_mm_s = straight_end_speed_mm_s(speed_mm_s, distance_mm, diag_accel, diag_max_speed, limit_mm_s);
      run_fast_diag_straight(distance_mm, diag_max_speed, end_mm_s, diag_accel);
      speed_mm_s = end_mm_s;
    } else {
      /* ターン：今の速さで曲がる（speed_limit_ahead_mm_s() で、そのターンの速さ以下にしてある） */
      run_fast_turn(fast_turn_kind(motion), is_left_turn(motion), speed_mm_s);
      last_was_turn = true;
    }
    if (failsafe_is_stopped()) {
      return;
    }
  }

  if (last_was_turn) {
    /* ゴールにターンで入って、区画のまん中に今の速さで着いた：そこから止まる（SHORTEST_STOP_DISTANCE_MM 以内） */
    float stop_mm = speed_mm_s * speed_mm_s / (2.0f * SHORTEST_STOP_ACCEL_MM_S2);
    run_fast_stop(stop_mm, speed_mm_s, SHORTEST_STOP_ACCEL_MM_S2);
    s_end_overshoot_mm = stop_mm;
  }
}

/* 大回り・斜めで走る No.：吸引を上げて、少し待つ（先頭のコメントの 1）。強さは No. ごと（config/run_params.h の SHORTEST*_SUCTION_V） */
static void start_suction(int number)
{
  suction_ramp_up_to(values_of(number).suction_v);  /* 0.5 秒かけて吸引を上げる（sys/suction.c） */
  ui_wait_ms(SHORTEST_SUCTION_WAIT_MS);
}

/* 最短走行をする（流れは先頭のコメント）。number は No.（走り方と速さを選ぶ） */
shortest_result_t shortest_run(int number, int goal_x, int goal_y)
{
  s_end_overshoot_mm = 0.0f;
  if (!make_path(number, goal_x, goal_y)) {
    return SHORTEST_RESULT_NO_ROUTE;
  }
  float max_speed = max_speed_mm_s(number);
  float accel = accel_mm_s2(number);

  run_search_set_suction(false);  /* 小回り（No.1）は、吸引なしの探索と同じスラロームで曲がる */
  select_turn_set(number);        /* 大回り・斜めのターンの組（No. ごと） */
  if (uses_diagonal(number)) {
    start_suction(number);
  }
  run_fast_start();  /* ジャイロのゼロ点を測り直してから、制御を始める（No.2〜9 は、吸ったまま測る） */
  if (uses_diagonal(number)) {
    run_diagonal_path(number);
  } else {
    for (int i = 0; i < s_path.count; i++) {
      run_small_turn_step(i, max_speed, accel);
      if (failsafe_is_stopped()) {
        break;
      }
    }
  }

  fan_stop();  /* 吸引を止める（No.1 は回していないので、何も変わらない） */

  if (failsafe_is_stopped()) {
    run_fast_stop(0.0f, 0.0f, accel);  /* 制御とログを止める（フェイルセーフのときは、動かさずに止めるだけ） */
    return SHORTEST_RESULT_FAILSAFE;
  }
  return SHORTEST_RESULT_DONE;
}

/* 最後の最短走行で止まった区画と向き（経路の終わり。maze/path.h の end_x・end_y・end_dir） */
int shortest_get_end_x(void)
{
  return s_path.end_x;
}

int shortest_get_end_y(void)
{
  return s_path.end_y;
}

direction_t shortest_get_end_dir(void)
{
  return s_path.end_dir;
}

/* 最後の最短走行で止まったところが、その区画のまん中より何 mm 先か（ゴールにターンで入ったとき。直進で止まったら 0） */
float shortest_get_end_overshoot_mm(void)
{
  return s_end_overshoot_mm;
}
