/*
 * run.c
 * 機体を動かす（探索の動き：発進・直進・スラローム・止まる・その場で向きを変えて発進）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/ctrl/run.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - 探索を 2 とおりにした（吸引なし 500mm/s・吸引あり 1000mm/s。KOGUMA.MkIIl のモード0 と同じ）。
 *     どちらの値で走るかを run_search_set_suction() で選ぶ（下の「■ 探索の 2 とおりの値」）。吸引のファンを回すのは呼んだ側（sys/explore.c）
 *   - 区画の大きさ（180mm）に合わせて、コメントの距離を直した。値はすべて config/run_params.h（KOGUMA.MkIIl の値）
 *   - 前壁補正と壁切れは、測るだけにしてある（config/run_params.h の FRONT_CORRECTION_ENABLE・WALL_EDGE_ENABLE が 0）
 *   - 大回り・斜めのターンの値を 2 組（組 2000・組 2400。KOGUMA.MkIIl の G_Turn2000・G_Turn2400）にした。
 *     どちらで曲がるかを run_fast_set_turn_set() で選ぶ（config/turn_params.h の先頭のコメント）
 * モードの番号（モード7 No.1 など）は ToraNeko.Mk3 のもの。KOGUMA.Mk3.1 もモードの並びは ToraNeko.Mk3 と同じにする（ステップ9 の MODES.md）。
 *
 * ■ 2 つの状態（run.h の先頭のコメント）
 *   走っている  ：区画の入口の辺を、探索の速さ（500mm/s か 1000mm/s）で通るところ。動きの関数は、ここに着いたら戻る
 *   止まっている：区画のまん中で止まっている
 * 呼んだ側（sys/explore.c）は、関数が戻ってから次の動きを頼むまでのあいだに、壁を記録して次の行き先を決める。
 * そのあいだも機体は探索の速さで前に進み続ける（目標の速さは、前の動きの終わりの速さのまま）。
 *
 * ■ 動きの作り方（値は config/run_params.h）
 *   発進       ：スタート区画（お尻を後ろの壁に付けて置く）から、北隣の区画の入口まで、速くしながら進む
 *   直進       ：1 区画（180mm）を探索の速さのまま進む
 *   左・右     ：前距離 → スラローム 90° → 後距離 で、左・右の区画の入口へ（前距離・後距離は test/test_turn.c --calc で計算した値）
 *   止まる     ：入口から 90mm 進みながら止まり、まん中で止まる。モーターの出力を 0 にしてから戻る
 *   向きを変えて発進：その場で 90° 回ってから（回らないこともある）、90mm 進みながら速くして、入口へ。
 *                    U ターンは 90° を 2 回（左に 90° → 左に 90°）
 *
 * ■ 最短走行の動き（モード1 は sys/shortest.c、モード3 は sys/turn_test.c が使う）
 *   発進       ：制御を始めるだけ（動かない）。すぐあとに直進を頼む
 *   直進       ：決めた距離を、最高速まで速くしてから、決めた終わりの速さ（次のターンの速さ）まで遅くして進む
 *   斜めの直進 ：同じ。斜めのあいだは、横の壁に沿って走る補正を入れない（横のセンサが柱や斜めの壁を見るので）
 *   左・右     ：探索と同じスラローム（前距離 → 90° → 後距離）
 *   大回り・斜めのターン：前距離 → スラローム → 後距離。値は config/turn_params.h（ターンの速さで曲がる）
 *   止まる     ：決めた距離を進みながら止まり、モーターの出力を 0 にする
 *
 * ■ 前壁補正（config/run_params.h の FRONT_CORRECTION_*）
 * 区画の入口で曲がる・止まるとき、その区画に前の壁があれば、前センサで壁までの距離を測る。
 * 入口の辺なら FRONT_WALL_AT_ENTRY_MM（174mm）のはずなので、ずれた分だけ、前距離と止まる距離を直す。
 * 前の壁までの距離は、左前と右前が FRONT_CORRECTION_SENSOR_GAP_MM 以上違うときは近いほう、そうでなければ平均を使う（front_wall_distance_mm()）。
 * センサを読んだときには、入口の辺から少し進んでいる（探索の計算のあいだも進む）ので、その分を足して比べる。
 * かけた量（かけないときは、かけるはずだった量）は数えておき、モード7 No.1 で出す（run_get_front_correction_*）。
 *
 * ■ 壁切れ（config/run_params.h の WALL_EDGE_*・FAST_WALL_EDGE_*。2026-11-04 に、柱も使えるように作り直した）
 * 横のセンサで見つけた壁の終わり・柱の端（ctrl/side_edge.c）から、区画の境目の位置を求め、思っていた境目とのずれだけ、
 * 今の直進の残りの距離を直す（trajectory_add_distance()）。横に壁のない区間でも、柱ごとに前後の位置が直る。
 *   探索：1 区画まっすぐ進むあいだ（go_straight_one_cell()）。1 区画で 1 回だけ
 *   最短走行：縦横の直進（run_fast_straight()）。1 本の直進で何回でもよい。直進を始める位置が、うしろの区画の境目から
 *            何 mm 先か（start_in_cell_mm）を教えてもらい、そこから区画の境目は 180mm ごとにあるとする。
 *            スタート区画の出口の境目は使わない（ToraNeko.Mk3 では、ほかの境目より 5mm ずれて見えた。ctrl/side_edge.c の値のコメント）
 * いちばん近い境目からのずれが WALL_EDGE_WINDOW_MM より大きければ、読み違いとして使わない。
 * 直したら、そのぶん「思っている位置」もずらす（1 本の直進で 2 つ目からは、直したあとの位置で比べる。2026-11-04 に直した。
 * 前は、1 つ目で直した分を差し引かずに 2 つ目でまた直していた）。同じ境目を左右で見つけたら、2 つ目はずれの半分だけ直し、
 * 2 つの平均にする（機体が横にずれたり、向きがずれたりすると、左右で見える位置が逆向きにずれるので、平均で打ち消す。
 * ToraNeko.Mk3 の 2026-11-04 の柱のテストで、まん中より 6mm 右・左へ 3° のとき、同じ境目で左 +4.6mm・右 −0.6mm だった）。
 * 見つけた端は 1 回ごとに覚えておき、モード7 No.1 で出す（使わなかったものも）。
 * KOGUMA.Mk3.1 では、探索も最短走行も、まだ測るだけ（WALL_EDGE_ENABLE・FAST_WALL_EDGE_ENABLE が 0）。
 *
 * ■ 壁切れの位置は、目標の位置ではなく、機体が本当にいる位置で数える（2026-11-04）
 * 加速しているあいだ、機体は目標の位置より少し遅れてついていく（1170mm/s に加速しているとき 3.4mm。ToraNeko.Mk3 の走行ログで確かめた）。
 * 目標の位置で数えると、その遅れの分だけ「壁切れが遅く見えた」ことになってしまうので、制御の位置のずれ
 * （目標 − 測った値。ctrl/control.c の control_get_position_error_mm()。制御を始めてからずっと足し合わせている）を引く。
 *
 * ■ 横壁合わせ（run_search_align_to_side()。2026-11-02）
 * 止まっている区画のまん中で、横の壁のほうへ 90° 回り、その壁で前壁合わせをして、元の向きに戻る。
 * 前壁合わせで、その壁までの距離（＝左右の位置）と、壁に対する向きが直る。戻るときは、直した向きから 90° 回るので、
 * 元の向きもまっすぐになる。いつ使うかは sys/explore.c が決める（横の壁が近すぎたとき）。
 *
 * ■ 横の壁に沿って走る補正
 * 発進のときに入れる（ctrl/control.c）。直進のあいだだけ効く（スラローム・その場旋回のあいだは効かない）。
 *
 * ■ 前壁合わせ
 * 止まっているところから発進するとき（U ターンや、止まって計算したあと）、前に壁があれば、
 * 前の壁を使って位置と向きを ALIGN_TIME_MS（0.5 秒）直してから、向きを変える（ctrl/control.c）。
 * U ターンでは、1 回目の 90° のあとにも、前（もとの横）に壁があれば、もう一度合わせる。
 * 行き止まりなら、前の壁で前後と向き、横の壁で左右と向きが直る
 *
 * ■ 探索の 2 とおりの値（KOGUMA.Mk3.1 で足した。config/run_params.h）
 *   吸引なし：SEARCH_*・KNOWN_*・SLALOM_*（500mm/s）
 *   吸引あり：SUCTION_SEARCH_*・SUCTION_KNOWN_*・SUCTION_SLALOM_*（1000mm/s）
 * run_search_set_suction() で選ぶと、s_search に入る。探索の動きは、どれも s_search の値で走る。
 * 選ばなければ、吸引なしの値。探索を始める前（run_search_start() の前）に選ぶ。
 *
 * ■ フェイルセーフ（run.h の約束 3）
 * 動きの途中でフェイルセーフで止まったら、すぐに戻る。止まっているあいだに呼ばれたら、何もせずにすぐ戻る。
 */
#include "ctrl/run.h"

#include <math.h>
#include <stdbool.h>

#include "config/run_params.h"
#include "config/turn_params.h"
#include "ctrl/control.h"
#include "ctrl/odometry.h"
#include "ctrl/side_edge.h"
#include "ctrl/trajectory.h"
#include "ctrl/wall_ctrl.h"
#include "drv/imu.h"
#include "drv/timebase.h"
#include "sys/failsafe.h"
#include "sys/log.h"

#define SEARCH_LOG_INTERVAL_MS 8  /* 探索の走行ログの間隔 [ms]（1600 行で 12.8 秒ぶん） */

/* ---- 探索の 2 とおりの値（先頭のコメントの「■ 探索の 2 とおりの値」） ---- */
typedef struct {
  float speed_mm_s;            /* 探索の速さ [mm/s] */
  float accel_mm_s2;           /* 止まる・発進するときの加速度 [mm/s²] */
  float known_max_speed_mm_s;  /* 既知区間加速の最高速 [mm/s] */
  float known_accel_mm_s2;     /* 既知区間加速の加速度 [mm/s²] */
  float slalom_omega_deg_s;    /* スラロームの最高の角速度 [deg/s] */
  float slalom_alpha_deg_s2;   /* スラロームの角加速度 [deg/s²] */
  float slalom_pre_mm;         /* スラロームの前距離 [mm] */
  float slalom_post_mm;        /* スラロームの後距離 [mm] */
} search_values_t;

/* 今の探索の値（はじめは吸引なし） */
static search_values_t s_search = {
  SEARCH_SPEED_MM_S, SEARCH_ACCEL_MM_S2, KNOWN_MAX_SPEED_MM_S, KNOWN_ACCEL_MM_S2,
  SLALOM_OMEGA_DEG_S, SLALOM_ALPHA_DEG_S2, SLALOM_PRE_MM, SLALOM_POST_MM,
};

/* 探索の値を選ぶ（on が true なら吸引探索の値、false なら吸引なしの値）。吸引のファンは回さない（呼んだ側が回す） */
void run_search_set_suction(bool on)
{
  if (on) {
    search_values_t suction = {
      SUCTION_SEARCH_SPEED_MM_S, SUCTION_SEARCH_ACCEL_MM_S2, SUCTION_KNOWN_MAX_SPEED_MM_S, SUCTION_KNOWN_ACCEL_MM_S2,
      SUCTION_SLALOM_OMEGA_DEG_S, SUCTION_SLALOM_ALPHA_DEG_S2, SUCTION_SLALOM_PRE_MM, SUCTION_SLALOM_POST_MM,
    };
    s_search = suction;
  } else {
    search_values_t normal = {
      SEARCH_SPEED_MM_S, SEARCH_ACCEL_MM_S2, KNOWN_MAX_SPEED_MM_S, KNOWN_ACCEL_MM_S2,
      SLALOM_OMEGA_DEG_S, SLALOM_ALPHA_DEG_S2, SLALOM_PRE_MM, SLALOM_POST_MM,
    };
    s_search = normal;
  }
}

/* 今の探索の速さ [mm/s]（run_search_set_suction() で選んだほう） */
float run_get_search_speed_mm_s(void)
{
  return s_search.speed_mm_s;
}

/* 動きが戻ってから次の動きを頼まれるまでの時間を測る（探索の計算の時間） */
static bool s_waiting = false;          /* 走ったまま戻って、次の動きを待っているところか */
static uint32_t s_return_us = 0;        /* 戻った時刻 [µs] */
static uint32_t s_longest_wait_us = 0;  /* いちばん長かった待ち時間 [µs] */

/* 走ったまま戻るときに呼ぶ。戻った時刻を覚える */
static void mark_return(void)
{
  s_return_us = timebase_get_us();
  s_waiting = true;
}

/* 次の動きを頼まれたときに呼ぶ。戻ってからの時間が、いちばん長ければ覚える */
static void measure_wait(void)
{
  if (!s_waiting) {
    return;
  }
  uint32_t wait_us = timebase_get_us() - s_return_us;
  if (wait_us > s_longest_wait_us) {
    s_longest_wait_us = wait_us;
  }
  s_waiting = false;
}

/* 前壁補正の記録（探索を始めるたびに 0 に戻す） */
static int s_front_correction_count = 0;            /* 測った回数 */
static float s_front_correction_sum_mm = 0.0f;      /* 測った量の合計（平均を出すため） [mm] */
static float s_front_correction_min_mm = 0.0f;      /* いちばん小さかった量 [mm] */
static float s_front_correction_max_mm = 0.0f;      /* いちばん大きかった量 [mm] */

/* 前壁補正と壁切れを 1 回ごとに覚えておく数（それより多いときは、最初のこれだけを覚える） */
#define EVENT_LIST_MAX 40

/* 前壁補正の 1 回ごとの記録（2026-11-03 に足した。ずれがいつも同じか、走るほど増えるかを見るため） */
static float s_front_list_mm[EVENT_LIST_MAX];         /* 測った量 [mm] */
static float s_front_list_traveled_mm[EVENT_LIST_MAX];  /* そのときまでに走った距離 [mm]（探索を始めてから） */
static float s_search_start_mm = 0.0f;                 /* 探索を始めたときの、走った距離の合計（odometry） [mm] */
static float s_search_start_angle_deg = 0.0f;          /* 探索を始めたときの、ジャイロの角度 [deg]（区画の向きの基準） */
static run_front_detail_t s_front_details[EVENT_LIST_MAX];  /* 1 回ごとの、くわしい様子（run.h） */

/* 壁切れ（探索・最短走行）の 1 回ごとの記録（走り始めるたびに 0 に戻す。最初の EVENT_LIST_MAX 回まで） */
static int s_edge_count = 0;                       /* 見つけた回数 */
static float s_edge_error_mm[EVENT_LIST_MAX];      /* いちばん近い境目からのずれ [mm]（+ なら、思っているより手前にいる） */
static float s_edge_speed_mm_s[EVENT_LIST_MAX];    /* そのときの目標の速さ [mm/s] */
static float s_edge_traveled_mm[EVENT_LIST_MAX];   /* そのときまでに走った距離 [mm]（走り始めてから） */
static float s_edge_lag_mm[EVENT_LIST_MAX];        /* そのときの、目標の位置からの遅れ [mm]（制御の位置のずれ） */
static bool s_edge_is_left[EVENT_LIST_MAX];        /* 左のセンサで見つけたか */
static bool s_edge_is_pillar[EVENT_LIST_MAX];      /* 柱だけか（false なら壁の終わり） */
static bool s_edge_used[EVENT_LIST_MAX];           /* 位置を直すのに使えたか（範囲の外・スタート区画の出口なら false） */

/* いちばん長かった待ち時間 [µs] */
uint32_t run_get_longest_wait_us(void)
{
  return s_longest_wait_us;
}

/* 今の動きが終わるまで、休まずに待つ。フェイルセーフで止まったら、すぐに戻る。
 * 少しずつ休みながら待つと、そのあいだに機体が進んで次の動きの距離がずれるので、休まない
 * （trajectory_is_done() は割り込みが書き換える） */
static void wait_until_done(void)
{
  while (!trajectory_is_done() && !failsafe_is_stopped()) {
  }
}

/* 区画の向き（探索を始めた向きから 90° ごと）に対する、今の機体の向きのずれ [deg]。
 * 探索を始めてから回った角度を 90° で割って、いちばん近い整数の 90° 倍を引く（-45〜+45° になる） */
static float yaw_from_grid_deg(void)
{
  float turned_deg = imu_get_angle_deg() - s_search_start_angle_deg;
  float quarter_count = roundf(turned_deg / 90.0f);  /* roundf は、いちばん近い整数にする（math.h） */
  return turned_deg - quarter_count * 90.0f;
}

/* 前壁補正を測ったときの様子を、count 回目として覚える */
static void record_front_detail(int count)
{
  s_front_details[count].front_left_mm = wall_ctrl_get_front_left_mm();
  s_front_details[count].front_right_mm = wall_ctrl_get_front_right_mm();
  s_front_details[count].side_left_mm = wall_ctrl_get_left_mm();
  s_front_details[count].side_right_mm = wall_ctrl_get_right_mm();
  s_front_details[count].overrun_mm = trajectory_get_overrun_mm();
  s_front_details[count].speed_mm_s = odometry_get_speed_mm_s();
  s_front_details[count].yaw_deg = yaw_from_grid_deg();
}

/* 前壁補正の量（上限をかける前の、測ったままの量）を記録する。平均・いちばん小さい・大きいをモード7 No.1 で出す */
static void record_front_correction(float measured_mm)
{
  if (s_front_correction_count == 0 || measured_mm < s_front_correction_min_mm) {
    s_front_correction_min_mm = measured_mm;
  }
  if (s_front_correction_count == 0 || measured_mm > s_front_correction_max_mm) {
    s_front_correction_max_mm = measured_mm;
  }
  s_front_correction_sum_mm = s_front_correction_sum_mm + measured_mm;
  if (s_front_correction_count < EVENT_LIST_MAX) {
    s_front_list_mm[s_front_correction_count] = measured_mm;
    s_front_list_traveled_mm[s_front_correction_count] = odometry_get_distance_mm() - s_search_start_mm;
    record_front_detail(s_front_correction_count);
  }
  s_front_correction_count = s_front_correction_count + 1;
}

/* 前壁補正に使う、前の壁までの距離 [mm]。左前と右前が FRONT_CORRECTION_SENSOR_GAP_MM より違うときは近いほう、
 * そうでなければ 2 つの平均（わけは config/run_params.h の FRONT_CORRECTION_SENSOR_GAP_MM） */
static float front_wall_distance_mm(void)
{
  float left_mm = wall_ctrl_get_front_left_mm();
  float right_mm = wall_ctrl_get_front_right_mm();
  float gap_mm = fabsf(left_mm - right_mm);  /* fabsf は、- を取った大きさ（math.h） */
  if (gap_mm < FRONT_CORRECTION_SENSOR_GAP_MM) {
    return (left_mm + right_mm) / 2.0f;
  }
  if (left_mm < right_mm) {
    return left_mm;
  }
  return right_mm;
}

/* 前壁補正の量 [mm] を決めて記録する（+ なら、壁が思ったより遠い＝まだ手前にいるので、そのぶん長く進む）。
 * 前に壁がないときは 0。FRONT_CORRECTION_ENABLE が 0 なら、記録だけして 0 を返す。
 * 記録するのは上限をかける前の量（2026-11-03 に変えた。上限で止めた値だと、本当はいくつずれていたかわからないため） */
static float front_correction_mm(void)
{
  if (!wall_ctrl_has_wall_front()) {
    return 0.0f;
  }
  /* 今いる位置は、入口の辺から、まだ頼んでいない分（trajectory_get_overrun_mm()）だけ進んだところ */
  float measured_mm = front_wall_distance_mm() + trajectory_get_overrun_mm() - FRONT_WALL_AT_ENTRY_MM;
  record_front_correction(measured_mm);

  float correction_mm = measured_mm;
  if (correction_mm > FRONT_CORRECTION_LIMIT_MM) {
    correction_mm = FRONT_CORRECTION_LIMIT_MM;
  }
  if (correction_mm < -FRONT_CORRECTION_LIMIT_MM) {
    correction_mm = -FRONT_CORRECTION_LIMIT_MM;
  }

  if (FRONT_CORRECTION_ENABLE == 0) {
    return 0.0f;
  }
  return correction_mm;
}

/* 前壁補正を測った回数・平均・いちばん小さかった量・いちばん大きかった量 [mm]（モード7 No.1 で出す） */
int run_get_front_correction_count(void)
{
  return s_front_correction_count;
}

float run_get_front_correction_average_mm(void)
{
  if (s_front_correction_count == 0) {
    return 0.0f;
  }
  return s_front_correction_sum_mm / s_front_correction_count;
}

float run_get_front_correction_min_mm(void)
{
  return s_front_correction_min_mm;
}

float run_get_front_correction_max_mm(void)
{
  return s_front_correction_max_mm;
}

/* 1 回ごとの記録の数（EVENT_LIST_MAX より多くは覚えない） */
static int list_count(int count)
{
  if (count > EVENT_LIST_MAX) {
    return EVENT_LIST_MAX;
  }
  return count;
}

/* 前壁補正の 1 回ごとの記録：覚えている数と、index 番目の量・そのときまでに走った距離 [mm]（モード7 No.1 で出す） */
int run_get_front_correction_list_count(void)
{
  return list_count(s_front_correction_count);
}

float run_get_front_correction_list_mm(int index)
{
  return s_front_list_mm[index];
}

float run_get_front_correction_list_traveled_mm(int index)
{
  return s_front_list_traveled_mm[index];
}

/* 前壁補正を測ったときの、くわしい様子（index 番目。index は 0 から run_get_front_correction_list_count() − 1 まで） */
run_front_detail_t run_get_front_correction_detail(int index)
{
  return s_front_details[index];
}

/* 壁切れの 1 回ごとの記録（モード7 No.1 で出す）：覚えている数と、index 番目のずれ・速さ・走った距離・遅れ・左か・柱か・使えたか */
int run_get_edge_count(void)
{
  return list_count(s_edge_count);
}

float run_get_edge_error_mm(int index)
{
  return s_edge_error_mm[index];
}

float run_get_edge_speed_mm_s(int index)
{
  return s_edge_speed_mm_s[index];
}

float run_get_edge_traveled_mm(int index)
{
  return s_edge_traveled_mm[index];
}

float run_get_edge_lag_mm(int index)
{
  return s_edge_lag_mm[index];
}

bool run_get_edge_is_left(int index)
{
  return s_edge_is_left[index];
}

bool run_get_edge_is_pillar(int index)
{
  return s_edge_is_pillar[index];
}

bool run_get_edge_used(int index)
{
  return s_edge_used[index];
}

/* 機体が本当にいる位置 [mm]：目標の位置 target_mm から、制御の位置のずれ（目標より遅れている分）を引く（先頭のコメント） */
static float actual_position_mm(float target_mm)
{
  return target_mm - control_get_position_error_mm();
}

/* 壁切れを 1 回記録する（最初の EVENT_LIST_MAX 回まで） */
static void record_edge(float error_mm, bool is_left, bool is_pillar, bool used)
{
  if (s_edge_count >= EVENT_LIST_MAX) {
    s_edge_count = s_edge_count + 1;
    return;
  }
  s_edge_error_mm[s_edge_count] = error_mm;
  s_edge_speed_mm_s[s_edge_count] = trajectory_get_speed_mm_s();
  s_edge_traveled_mm[s_edge_count] = odometry_get_distance_mm() - s_search_start_mm;
  s_edge_lag_mm[s_edge_count] = control_get_position_error_mm();
  s_edge_is_left[s_edge_count] = is_left;
  s_edge_is_pillar[s_edge_count] = is_pillar;
  s_edge_used[s_edge_count] = used;
  s_edge_count = s_edge_count + 1;
}

/* ---- 直進のあいだに、新しい端を見つけたかを見る（ctrl/side_edge.c の記録を読む） ----
 * 直進を始めるときに watch_edges_start() を呼び、そのあと new_edge_found() で左右を見る。
 * 道の上の位置（うしろの区画の境目から [mm]）＝ s_watch_origin_mm ＋ 走った距離（エンコーダ）。
 * 機体が本当にいる位置で数えるので、始めたときの目標の位置から、制御の位置のずれ（遅れ）を引いておく */
static int s_watch_seen_left = 0;        /* 直進を始めてから読んだ、左の端の数 */
static int s_watch_seen_right = 0;       /* 同じく、右 */
static float s_watch_origin_mm = 0.0f;   /* 道の上の位置 ＝ これ ＋ 走った距離 */
static float s_found_boundary_mm = 0.0f; /* new_edge_found() で見つけた端から求めた、境目の道の上の位置 [mm] */
static int s_last_used_cell = -1;        /* この直進で最後に使った境目が、何番目の境目か（道の上の位置 ÷ 90。まだなければ −1） */
static bool s_last_used_is_left = false; /* その端が、左のセンサか */
static bool s_found_is_pillar = false;   /* new_edge_found() で見つけた端が、柱だけか */

/* 直進を始めるときに呼ぶ。start_mm は、直進を始める目標の位置（うしろの区画の境目から [mm]） */
static void watch_edges_start(float start_mm)
{
  s_watch_seen_left = side_edge_get_count(true);
  s_watch_seen_right = side_edge_get_count(false);
  s_watch_origin_mm = actual_position_mm(start_mm) - odometry_get_distance_mm();
  s_last_used_cell = -1;
}

/* is_left 側（左なら true）に、新しい端があれば、その端から求めた境目の道の上の位置を s_found_boundary_mm に、
 * 柱だけかを s_found_is_pillar に入れて true を返す。なければ false */
static bool new_edge_found(bool is_left)
{
  int seen = s_watch_seen_right;
  if (is_left) {
    seen = s_watch_seen_left;
  }
  int count = side_edge_get_count(is_left);
  if (count == seen) {
    return false;
  }
  /* 端の記録は割り込みが書くので、読んでいるあいだに次の端で書き換わっていないかを、数を読み直して確かめる
   * （書き換わっていたら、もう一度読む。ctrl/side_edge.c の先頭のコメント） */
  float boundary_mm = side_edge_get_boundary_mm(is_left);
  bool is_pillar = side_edge_is_pillar(is_left);
  while (side_edge_get_count(is_left) != count) {
    count = side_edge_get_count(is_left);
    boundary_mm = side_edge_get_boundary_mm(is_left);
    is_pillar = side_edge_is_pillar(is_left);
  }
  if (is_left) {
    s_watch_seen_left = count;
  } else {
    s_watch_seen_right = count;
  }
  s_found_boundary_mm = s_watch_origin_mm + boundary_mm;
  s_found_is_pillar = is_pillar;
  return true;
}

/* 境目の道の上の位置 boundary_mm の、いちばん近い区画の境目（180mm ごと）からのずれ [mm]（+ なら、思っているより手前にいる） */
static float boundary_error_mm(float boundary_mm)
{
  float cell_index = roundf(boundary_mm / CELL_MM);  /* roundf は、いちばん近い整数にする（math.h） */
  return boundary_mm - cell_index * CELL_MM;
}

/* 見つけた端（s_found_*）を、壁切れとして使う。ずれが範囲の中なら記録して、enable が 1 なら今の直進の残りを直す。
 * skip_start_cell が true なら、スタート区画の出口の境目（道の上の 180mm）は使わない（先頭のコメント）。使ったら true */
static bool use_found_edge(bool is_left, int enable, bool skip_start_cell)
{
  float error_mm = boundary_error_mm(s_found_boundary_mm);
  int cell = (int)roundf(s_found_boundary_mm / CELL_MM);  /* 何番目の境目か（整数にする） */
  bool used = true;
  if (error_mm > WALL_EDGE_WINDOW_MM || error_mm < -WALL_EDGE_WINDOW_MM) {
    used = false;  /* 離れすぎている：読み違いとして使わない */
  }
  if (skip_start_cell && cell <= 1) {
    used = false;  /* スタート区画の出口の境目 */
  }
  record_edge(error_mm, is_left, s_found_is_pillar, used);
  if (!used || enable != 1) {
    return used;
  }
  /* 同じ境目を、反対側のセンサでもう見つけて直していたら、2 つ目はずれの半分だけ直す（2 つの平均にする。先頭のコメント） */
  float correction_mm = error_mm;
  if (cell == s_last_used_cell && is_left != s_last_used_is_left) {
    correction_mm = error_mm / 2.0f;
  }
  trajectory_add_distance(correction_mm);
  /* 直したぶん、思っている位置もずらす（本当の位置は、思っていた位置より correction_mm 手前だった） */
  s_watch_origin_mm = s_watch_origin_mm - correction_mm;
  s_last_used_cell = cell;
  s_last_used_is_left = is_left;
  return used;
}

/* 探索で 1 区画まっすぐ進む（走っている → 次の区画の入口の辺へ）。進むあいだ、壁切れを探す（先頭のコメント）。
 * 使うのは 1 区画で 1 回だけ（左右どちらか先に見つかったほう） */
static void go_straight_one_cell(void)
{
  if (failsafe_is_stopped()) {
    return;
  }
  /* 区画の入口の辺から、もう進んでしまった分（この直進では引かれる分）。位置は「入口から」で数えるので足す */
  watch_edges_start(trajectory_get_overrun_mm());
  bool edge_used = false;

  trajectory_start_straight(CELL_MM, s_search.speed_mm_s, s_search.speed_mm_s, s_search.accel_mm_s2);
  while (!trajectory_is_done() && !failsafe_is_stopped()) {
    if (edge_used) {
      continue;  /* もう使った。あとは着くのを待つ（休まずに待つ理由は wait_until_done() と同じ） */
    }
    if (new_edge_found(true)) {
      edge_used = use_found_edge(true, WALL_EDGE_ENABLE, false);
    }
    if (!edge_used && new_edge_found(false)) {
      edge_used = use_found_edge(false, WALL_EDGE_ENABLE, false);
    }
  }
}

/* distance_mm まっすぐ進んで、探索の速さで戻る。走っているときは速さを変えずに進み、
 * 止まっているところからなら、進みながら探索の速さまで速くする */
static void go_straight(float distance_mm)
{
  if (failsafe_is_stopped()) {
    return;
  }
  trajectory_start_straight(distance_mm, s_search.speed_mm_s, s_search.speed_mm_s, s_search.accel_mm_s2);
  wait_until_done();
}

/* 前距離 → スラローム angle_deg（左が +、右が -）→ 後距離 で、横の区画の入口へ。
 * 前に壁があれば、前距離を前壁補正で直す（前距離は 0 より短くしない） */
static void slalom(float angle_deg)
{
  float pre_mm = s_search.slalom_pre_mm + front_correction_mm();
  if (pre_mm < 0.0f) {
    pre_mm = 0.0f;
  }
  go_straight(pre_mm);
  if (failsafe_is_stopped()) {
    return;
  }
  trajectory_start_slalom(angle_deg, s_search.speed_mm_s, s_search.slalom_omega_deg_s, s_search.slalom_alpha_deg_s2);
  wait_until_done();
  go_straight(s_search.slalom_post_mm);
}

/* その場で angle_deg 回って止まる（左が +） */
static void pivot(float angle_deg)
{
  if (failsafe_is_stopped()) {
    return;
  }
  trajectory_start_pivot(angle_deg, PIVOT_OMEGA_DEG_S, PIVOT_ALPHA_DEG_S2);
  wait_until_done();
}

/* 前の壁で、位置と向きを ALIGN_TIME_MS 直す（止まっているときに呼ぶ） */
static void align_to_front_wall(void)
{
  if (failsafe_is_stopped()) {
    return;
  }
  control_set_front_align(true);
  timebase_wait_ms(ALIGN_TIME_MS);
  control_set_front_align(false);
}

/* 前に壁があれば前壁合わせをして、直した向きを、まっすぐの向きにし直す（止まっているときに呼ぶ） */
static void align_if_front_wall(void)
{
  if (wall_ctrl_has_wall_front()) {
    align_to_front_wall();
    control_restart();                   /* 直した向きを、まっすぐの向きにし直す */
    control_reset_heading_reference();   /* 前の壁で向きを合わせたので、今の向きを迷路の向きとみなす（ctrl/control.c） */
  }
}

/* 制御とログを始める（探索と最短走行の発進で使う）。横の壁に沿って走る補正も入れる */
static void start_control(void)
{
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒。電源を入れたときに手で動かしていてもよいように） */
  s_waiting = false;
  s_longest_wait_us = 0;
  s_front_correction_count = 0;
  s_front_correction_sum_mm = 0.0f;
  s_search_start_mm = odometry_get_distance_mm();
  s_search_start_angle_deg = imu_get_angle_deg();
  wall_ctrl_clear_too_close();
  s_edge_count = 0;
  log_start(SEARCH_LOG_INTERVAL_MS);
  control_set_wall_control(true);  /* 横の壁に沿って走る補正を入れる */
  control_start();                 /* 今の向き（スタート区画で、お尻を後ろの壁に付けた向き）を、迷路の向きの基準にする */
  control_set_heading_hold(true);  /* 使える横の壁がないときは、向きを迷路の向きに戻す（ctrl/control.c） */
}

/* スタート区画から走り出し、北隣の区画 (0, 1) の入口の辺まで進む。
 * スタート区画では、機体のお尻を後ろの壁に付けて置く（タイヤの軸は区画のまん中より START_OFFSET_MM 後ろ） */
void run_search_start(void)
{
  if (failsafe_is_stopped()) {
    return;
  }
  start_control();
  control_set_heading_auto_reset(true);  /* 探索は長く走るので、両側の壁が続いたら、基準の向きをリセットする（ctrl/control.c） */
  go_straight(START_OFFSET_MM + HALF_CELL_MM);
  mark_return();
}

/* 走っている → 1 区画まっすぐ進み、次の区画の入口の辺へ（壁切れで前後の位置を直す） */
void run_search_straight(void)
{
  measure_wait();
  go_straight_one_cell();
  mark_return();
}

/* 走っている → 見たことのある区画を cells 区画まっすぐ進み、その先の区画の入口の辺へ（既知区間加速。sys/explore.c）。
 * 既知区間加速の最高速（吸引なしは KNOWN_MAX_SPEED_MM_S、吸引ありは SUCTION_KNOWN_MAX_SPEED_MM_S）まで速くしてから、探索の速さに戻して着く */
void run_search_known_straight(int cells)
{
  measure_wait();
  if (!failsafe_is_stopped()) {
    trajectory_start_straight(cells * CELL_MM, s_search.known_max_speed_mm_s, s_search.speed_mm_s, s_search.known_accel_mm_s2);
    wait_until_done();
  }
  mark_return();
}

/* 走っている → スラロームで左 90° */
void run_search_turn_left(void)
{
  measure_wait();
  slalom(SLALOM_ANGLE_DEG);
  mark_return();
}

/* 走っている → スラロームで右 90° */
void run_search_turn_right(void)
{
  measure_wait();
  slalom(-SLALOM_ANGLE_DEG);
  mark_return();
}

/* 走っている → 今入った区画のまん中で止まる。止まったら、モーターの出力を 0 にしてから戻る
 * （このあと Flash に保存して CPU が止まっても、機体が動かないように。run.h の約束 2） */
void run_search_stop_at_center(void)
{
  measure_wait();
  if (failsafe_is_stopped()) {
    return;
  }
  /* 前に壁があれば（行き止まりなど）、止まる距離を前壁補正で直す */
  float stop_mm = HALF_CELL_MM + front_correction_mm();
  trajectory_start_straight(stop_mm, s_search.speed_mm_s, 0.0f, s_search.accel_mm_s2);
  wait_until_done();
  if (failsafe_is_stopped()) {
    return;
  }
  timebase_wait_ms(STOP_SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  control_stop();                    /* 制御をやめて、モーターを止める */
}

/* 止まっている → 横の壁（is_left が true なら左）のほうへ 90° 回って前壁合わせをし、元の向きに戻る（先頭のコメント）。
 * 終わったら、モーターの出力を 0 にして止まっている（run.h の約束 2） */
void run_search_align_to_side(bool is_left)
{
  if (failsafe_is_stopped()) {
    return;
  }
  float angle_deg = -90.0f;
  if (is_left) {
    angle_deg = 90.0f;
  }
  control_restart();  /* 止まっているので、制御を始め直す（迷路の向きの基準は、そのまま） */
  pivot(angle_deg);
  align_if_front_wall();
  pivot(-angle_deg);
  if (failsafe_is_stopped()) {
    return;
  }
  timebase_wait_ms(STOP_SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  control_stop();
}

/* 区画のまん中で止まっているところから、探索を始める（KOGUMA.Mk3.1 で足した。モード0 No.11 で、最短走行のあと
 * ゴールからスタートへ帰るとき）。制御とログを始めるだけで、機体は動かない（このあと run_search_go_from_center() で発進する）。
 * 迷路の向きの基準は、今の向き（最短走行で止まったときの向き。区画の向きにそろっているとする） */
void run_search_start_at_center(void)
{
  if (failsafe_is_stopped()) {
    return;
  }
  start_control();
  control_set_heading_auto_reset(true);
  control_stop();  /* 止まっている状態（run.h の約束 2）にする。発進するときに run_search_go_from_center() が始め直す */
}

#define TURN_BACK_FORWARD_SPEED_MM_S 200.0f  /* 反対を向いたあと、まん中まで戻るときの速さ [mm/s]（ゆっくり） */
#define BACK_TO_WALL_SPEED_MM_S      100.0f  /* 後ろの壁まで下がるときの速さ [mm/s]（ゆっくり。KOGUMA.MkIIl の Motor_Back も遅い） */
#define BACK_TO_WALL_ACCEL_MM_S2     1000.0f /* 同じく、加速度 [mm/s²] */

/* 止まっている → その場で反対を向いて（左に 90° を 2 回）、区画のまん中で止まる（KOGUMA.Mk3.1 で足した。モード0 No.8〜11）。
 * forward_mm は、反対を向いたあとに、まん中まで進む距離 [mm]（区画のまん中より forward_mm 先で止まっていたとき。
 * 最短走行がゴールにターンで入って、まん中を過ぎて止まったとき）。
 * 回る前に前に壁があれば前壁合わせをする（U ターンと同じ）。そのときは前の壁でまん中に直るので、forward_mm は使わない。
 * 終わったら、モーターの出力を 0 にして止まっている（run.h の約束 2） */
void run_search_turn_back_at_center(float forward_mm)
{
  if (failsafe_is_stopped()) {
    return;
  }
  control_restart();  /* 止まっているので、制御を始め直す（迷路の向きの基準は、そのまま） */
  if (wall_ctrl_has_wall_front()) {
    forward_mm = 0.0f;  /* 前の壁で、区画のまん中に直る */
  }
  align_if_front_wall();
  pivot(90.0f);           /* 1 回目の 90°。前には、もとの右の壁が来る */
  align_if_front_wall();
  pivot(90.0f);           /* 2 回目の 90°。これで反対を向く */
  if (forward_mm > 1.0f && !failsafe_is_stopped()) {
    trajectory_start_straight(forward_mm, TURN_BACK_FORWARD_SPEED_MM_S, 0.0f, s_search.accel_mm_s2);
    wait_until_done();
  }
  if (failsafe_is_stopped()) {
    return;
  }
  timebase_wait_ms(STOP_SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  control_stop();
}

/* 止まっている（スタート区画のまん中で、北を向いて）→ 後ろの壁まで下がって、お尻を後ろの壁に付けたところで止まる
 * （KOGUMA.Mk3.1 で足した。KOGUMA.MkIIl の Motor_Back）。探索でスタートに戻ったあと、続けて最短走行をするときに、
 * ふつうに置いたときと同じ位置（タイヤの軸が区画のまん中より START_OFFSET_MM 後ろ）にする。
 * 下がる距離は、区画のまん中からその位置まで（START_OFFSET_MM）。終わったら、モーターの出力を 0 にして止まっている */
void run_search_back_to_start_wall(void)
{
  if (failsafe_is_stopped()) {
    return;
  }
  control_restart();  /* 止まっているので、制御を始め直す（迷路の向きの基準は、そのまま） */
  control_set_wall_control(false);  /* 下がるあいだは、横の壁に沿って走る補正を入れない */
  trajectory_start_straight_back(START_OFFSET_MM, BACK_TO_WALL_SPEED_MM_S, BACK_TO_WALL_ACCEL_MM_S2);
  wait_until_done();
  if (failsafe_is_stopped()) {
    return;
  }
  timebase_wait_ms(STOP_SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  control_stop();
}

/* 止まっている → 前に壁があれば前壁合わせをして、その場で向きを変えて（turn）、
 * 次の区画の入口の辺まで、速くしながら進む。U ターンは、左に 90° 回るたびに、前に壁があれば前壁合わせをする */
void run_search_go_from_center(run_turn_t turn)
{
  if (failsafe_is_stopped()) {
    return;
  }
  control_restart();  /* 止まっているので、制御を始め直す（迷路の向きの基準は、そのまま） */
  align_if_front_wall();
  if (turn == RUN_TURN_LEFT) {
    pivot(90.0f);
  } else if (turn == RUN_TURN_RIGHT) {
    pivot(-90.0f);
  } else if (turn == RUN_TURN_BACK) {
    pivot(90.0f);           /* 1 回目の 90°。前には、もとの右の壁が来る */
    align_if_front_wall();
    pivot(90.0f);           /* 2 回目の 90°。これで反対を向く */
  }
  go_straight(HALF_CELL_MM);
  mark_return();
}

/* ---------------------------------------------------------------------------
 * 最短走行の動き（モード1。sys/shortest.c が使う）
 * ------------------------------------------------------------------------- */

static bool s_diag_correction = true;  /* 斜めの直進で、斜めの補正を入れるか（run_fast_set_diag_correction()） */

/* 斜めの直進（run_fast_diag_straight・run_fast_diag_stop）で、斜めの補正を入れる（true。ふつうはこちら）・入れない（false）。
 * ターンの調整（sys/turn_test.c）では、補正が混ざらないように入れない。斜めの補正の値を測るとき（モード3 No.8）も入れない */
void run_fast_set_diag_correction(bool on)
{
  s_diag_correction = on;
}

/* 斜めの直進を始める前・終わったあとに、斜めの直進のしるしと、斜めの補正を入れる・切る（ctrl/control.c） */
static void set_on_diagonal(bool on)
{
  control_set_wall_control(false);  /* 横の壁に沿う補正は、斜めでは使えない */
  control_set_on_diagonal(on);
  if (on) {
    control_set_diag_control(s_diag_correction);
  } else {
    control_set_diag_control(false);
  }
}

/* 最短走行を始める。ジャイロのゼロ点を測り直して、制御とログを始める（機体はまだ動かない） */
void run_fast_start(void)
{
  if (failsafe_is_stopped()) {
    return;
  }
  start_control();
  /* 両側の壁が続いたら、基準の向きをリセットする（ctrl/control.c）。2026-11-04 に、吸引したまま止めておくと、
   * ジャイロが 1 秒に約 0.14° 流れた（モード6 No.3。30 秒で 4°）ので、数秒の最短走行でも入れる */
  control_set_heading_auto_reset(true);
}

/* distance_mm まっすぐ進む。max_speed_mm_s まで速くしてから、end_speed_mm_s（次のターンの速さ）まで遅くして戻る。
 * 横の壁に沿って走る補正を入れる。進むあいだ、壁切れを探す（先頭のコメントの「■ 壁切れ」）。
 * start_in_cell_mm は、直進を始める位置が、うしろの区画の境目から何 mm 先か（区画のまん中なら 90、境目なら 0。
 * スタート区画から走り出すときは HALF_CELL_MM − START_OFFSET_MM で、そのときはスタート区画の出口の境目を使わない） */
void run_fast_straight(float distance_mm, float max_speed_mm_s, float end_speed_mm_s, float accel_mm_s2,
                       float start_in_cell_mm)
{
  if (failsafe_is_stopped()) {
    return;
  }
  control_set_wall_control(true);
  bool from_start_cell = start_in_cell_mm == HALF_CELL_MM - START_OFFSET_MM;
  /* 前の動きが終わってから、もう進んでしまった分（この直進では引かれる分）。位置は「始める位置から」で数えるので足す */
  watch_edges_start(start_in_cell_mm + trajectory_get_overrun_mm());

  trajectory_start_straight(distance_mm, max_speed_mm_s, end_speed_mm_s, accel_mm_s2);
  /* 着くまで、休まずに端を見る（休まずに待つ理由は wait_until_done() と同じ） */
  while (!trajectory_is_done() && !failsafe_is_stopped()) {
    if (new_edge_found(true)) {
      use_found_edge(true, FAST_WALL_EDGE_ENABLE, from_start_cell);
    }
    if (new_edge_found(false)) {
      use_found_edge(false, FAST_WALL_EDGE_ENABLE, from_start_cell);
    }
  }
}

/* 斜めに distance_mm まっすぐ進む（run_fast_straight と同じ。横の壁に沿って走る補正のかわりに、斜めの補正を入れる。
 * 斜めの補正は、柱や壁に近づいたら離れるように曲げる。ctrl/control.c の先頭のコメント） */
void run_fast_diag_straight(float distance_mm, float max_speed_mm_s, float end_speed_mm_s, float accel_mm_s2)
{
  if (failsafe_is_stopped()) {
    return;
  }
  set_on_diagonal(true);
  trajectory_start_straight(distance_mm, max_speed_mm_s, end_speed_mm_s, accel_mm_s2);
  wait_until_done();
  set_on_diagonal(false);
}

/* ターン 1 つぶんの値（config/turn_params.h の、そのターンの行） */
typedef struct {
  float angle_deg;          /* 曲がる角度 [deg]（左に曲がるとき。+） */
  float design_speed_mm_s;  /* そのターンを曲がる速さ（*_SPEED_MM_S） */
  float omega_deg_s;        /* その速さでの最高角速度（*_OMEGA_DEG_S） */
  float alpha_deg_s2;       /* その速さでの角加速度（*_ALPHA_DEG_S2） */
  float pre_mm;             /* 前距離（計算の値 + 実機で足す分） */
  float post_mm;            /* 後距離（計算の値 + 実機で足す分） */
} fast_turn_values_t;

/* 速さ speed_mm_s で、前距離 → 角度 turn.angle_deg のスラローム（is_left が false なら右）→ 後距離 と走る。
 * 前距離・後距離はまっすぐなので、加速度は何でもよい（速さは変わらない）。横の壁に沿って走る補正は入れない。
 * そのターンの速さ（turn.design_speed_mm_s）より遅いとき（最短走行で 500mm/s で曲がるとき、スタートの直後で
 * 速さが足りないときなど）は、最高角速度を k 倍、角加速度を k² 倍にする（k = 速さ ÷ そのターンの速さ）。
 * こうすると、曲がる形はまったく同じで、かかる時間だけが 1/k 倍になるので、前距離・後距離はそのまま使える
 * （KOGUMA の最初のターンと同じ考え方） */
static void fast_turn(float speed_mm_s, fast_turn_values_t turn, bool is_left)
{
  if (failsafe_is_stopped()) {
    return;
  }
  float ratio = speed_mm_s / turn.design_speed_mm_s;  /* k */
  float angle_deg = turn.angle_deg;
  if (!is_left) {
    angle_deg = -turn.angle_deg;
  }
  float pre_mm = turn.pre_mm;
  float post_mm = turn.post_mm;
  control_set_wall_control(false);
  trajectory_start_straight(pre_mm, speed_mm_s, speed_mm_s, s_search.accel_mm_s2);
  wait_until_done();
  if (failsafe_is_stopped()) {
    return;
  }
  trajectory_start_slalom(angle_deg, speed_mm_s, turn.omega_deg_s * ratio, turn.alpha_deg_s2 * ratio * ratio);
  wait_until_done();
  if (failsafe_is_stopped()) {
    return;
  }
  trajectory_start_straight(post_mm, speed_mm_s, speed_mm_s, s_search.accel_mm_s2);
  wait_until_done();
}

/* どちらのターンの組で曲がるか（run_fast_set_turn_set()。はじめは組 2000） */
static run_turn_set_t s_turn_set = RUN_TURN_SET_2000;

/* 大回り・斜めのターンを、どちらの組の値（config/turn_params.h の先頭のコメント）で曲がるかを決める（KOGUMA.Mk3.1 で足した） */
void run_fast_set_turn_set(run_turn_set_t set)
{
  s_turn_set = set;
}

/* ターン turn の値を、組 2000 の表から取り出す（前距離・後距離は、計算の値に実機で足す分を足したもの） */
static fast_turn_values_t fast_turn_values_2000(run_fast_turn_t turn)
{
  fast_turn_values_t values = {90.0f, BIG90_SPEED_MM_S, BIG90_OMEGA_DEG_S, BIG90_ALPHA_DEG_S2, BIG90_PRE_MM + BIG90_PRE_ADJUST_MM, BIG90_POST_MM + BIG90_POST_ADJUST_MM};
  if (turn == RUN_FAST_TURN_BIG180) {
    fast_turn_values_t big180 = {180.0f, BIG180_SPEED_MM_S, BIG180_OMEGA_DEG_S, BIG180_ALPHA_DEG_S2, BIG180_PRE_MM + BIG180_PRE_ADJUST_MM, BIG180_POST_MM + BIG180_POST_ADJUST_MM};
    values = big180;
  } else if (turn == RUN_FAST_TURN_IN45) {
    fast_turn_values_t in45 = {45.0f, IN45_SPEED_MM_S, IN45_OMEGA_DEG_S, IN45_ALPHA_DEG_S2, IN45_PRE_MM + IN45_PRE_ADJUST_MM, IN45_POST_MM + IN45_POST_ADJUST_MM};
    values = in45;
  } else if (turn == RUN_FAST_TURN_IN135) {
    fast_turn_values_t in135 = {135.0f, IN135_SPEED_MM_S, IN135_OMEGA_DEG_S, IN135_ALPHA_DEG_S2, IN135_PRE_MM + IN135_PRE_ADJUST_MM, IN135_POST_MM + IN135_POST_ADJUST_MM};
    values = in135;
  } else if (turn == RUN_FAST_TURN_OUT45) {
    fast_turn_values_t out45 = {45.0f, OUT45_SPEED_MM_S, OUT45_OMEGA_DEG_S, OUT45_ALPHA_DEG_S2, OUT45_PRE_MM + OUT45_PRE_ADJUST_MM, OUT45_POST_MM + OUT45_POST_ADJUST_MM};
    values = out45;
  } else if (turn == RUN_FAST_TURN_OUT135) {
    fast_turn_values_t out135 = {135.0f, OUT135_SPEED_MM_S, OUT135_OMEGA_DEG_S, OUT135_ALPHA_DEG_S2, OUT135_PRE_MM + OUT135_PRE_ADJUST_MM, OUT135_POST_MM + OUT135_POST_ADJUST_MM};
    values = out135;
  } else if (turn == RUN_FAST_TURN_V90) {
    fast_turn_values_t v90 = {90.0f, V90_SPEED_MM_S, V90_OMEGA_DEG_S, V90_ALPHA_DEG_S2, V90_PRE_MM + V90_PRE_ADJUST_MM, V90_POST_MM + V90_POST_ADJUST_MM};
    values = v90;
  }
  return values;
}

/* ターン turn の値を、組 2400 の表から取り出す（fast_turn_values_2000() と同じ形。名前に _2400 が付いた値） */
static fast_turn_values_t fast_turn_values_2400(run_fast_turn_t turn)
{
  fast_turn_values_t values = {90.0f, BIG90_2400_SPEED_MM_S, BIG90_2400_OMEGA_DEG_S, BIG90_2400_ALPHA_DEG_S2, BIG90_2400_PRE_MM + BIG90_2400_PRE_ADJUST_MM, BIG90_2400_POST_MM + BIG90_2400_POST_ADJUST_MM};
  if (turn == RUN_FAST_TURN_BIG180) {
    fast_turn_values_t big180 = {180.0f, BIG180_2400_SPEED_MM_S, BIG180_2400_OMEGA_DEG_S, BIG180_2400_ALPHA_DEG_S2, BIG180_2400_PRE_MM + BIG180_2400_PRE_ADJUST_MM, BIG180_2400_POST_MM + BIG180_2400_POST_ADJUST_MM};
    values = big180;
  } else if (turn == RUN_FAST_TURN_IN45) {
    fast_turn_values_t in45 = {45.0f, IN45_2400_SPEED_MM_S, IN45_2400_OMEGA_DEG_S, IN45_2400_ALPHA_DEG_S2, IN45_2400_PRE_MM + IN45_2400_PRE_ADJUST_MM, IN45_2400_POST_MM + IN45_2400_POST_ADJUST_MM};
    values = in45;
  } else if (turn == RUN_FAST_TURN_IN135) {
    fast_turn_values_t in135 = {135.0f, IN135_2400_SPEED_MM_S, IN135_2400_OMEGA_DEG_S, IN135_2400_ALPHA_DEG_S2, IN135_2400_PRE_MM + IN135_2400_PRE_ADJUST_MM, IN135_2400_POST_MM + IN135_2400_POST_ADJUST_MM};
    values = in135;
  } else if (turn == RUN_FAST_TURN_OUT45) {
    fast_turn_values_t out45 = {45.0f, OUT45_2400_SPEED_MM_S, OUT45_2400_OMEGA_DEG_S, OUT45_2400_ALPHA_DEG_S2, OUT45_2400_PRE_MM + OUT45_2400_PRE_ADJUST_MM, OUT45_2400_POST_MM + OUT45_2400_POST_ADJUST_MM};
    values = out45;
  } else if (turn == RUN_FAST_TURN_OUT135) {
    fast_turn_values_t out135 = {135.0f, OUT135_2400_SPEED_MM_S, OUT135_2400_OMEGA_DEG_S, OUT135_2400_ALPHA_DEG_S2, OUT135_2400_PRE_MM + OUT135_2400_PRE_ADJUST_MM, OUT135_2400_POST_MM + OUT135_2400_POST_ADJUST_MM};
    values = out135;
  } else if (turn == RUN_FAST_TURN_V90) {
    fast_turn_values_t v90 = {90.0f, V90_2400_SPEED_MM_S, V90_2400_OMEGA_DEG_S, V90_2400_ALPHA_DEG_S2, V90_2400_PRE_MM + V90_2400_PRE_ADJUST_MM, V90_2400_POST_MM + V90_2400_POST_ADJUST_MM};
    values = v90;
  }
  return values;
}

/* ターン turn の値を、今の組（s_turn_set）の表から取り出す */
static fast_turn_values_t fast_turn_values(run_fast_turn_t turn)
{
  if (s_turn_set == RUN_TURN_SET_2400) {
    return fast_turn_values_2400(turn);
  }
  return fast_turn_values_2000(turn);
}

/* ターン turn を曲がる速さ [mm/s]（config/turn_params.h の *_SPEED_MM_S。ターンの調整（モード3）で使う） */
float run_fast_turn_speed_mm_s(run_fast_turn_t turn)
{
  return fast_turn_values(turn).design_speed_mm_s;
}

/* 大回り・斜めのターンを 1 つ、速さ speed_mm_s で走る（is_left が true なら左）。その速さで走っているところから呼ぶ。
 * 速さは、そのターンの速さ（run_fast_turn_speed_mm_s()）以下にする。遅いときは同じ形で曲がる（fast_turn() のコメント）。
 * 始める位置と終わる位置は、config/turn_params.h の先頭のコメントと test/test_turn.c の表 */
void run_fast_turn(run_fast_turn_t turn, bool is_left, float speed_mm_s)
{
  fast_turn(speed_mm_s, fast_turn_values(turn), is_left);
}

/* ターンの速さで走っている → 探索と同じスラロームで左 90° */
void run_fast_turn_left(void)
{
  slalom(SLALOM_ANGLE_DEG);
}

/* ターンの速さで走っている → 探索と同じスラロームで右 90° */
void run_fast_turn_right(void)
{
  slalom(-SLALOM_ANGLE_DEG);
}

/* distance_mm 進みながら止まり、モーターの出力を 0 にして、ログを止める（run_fast_stop・run_fast_diag_stop の中身） */
static void stop_after(float distance_mm, float max_speed_mm_s, float accel_mm_s2)
{
  if (!failsafe_is_stopped()) {
    trajectory_start_straight(distance_mm, max_speed_mm_s, 0.0f, accel_mm_s2);
    wait_until_done();
  }
  if (!failsafe_is_stopped()) {
    timebase_wait_ms(STOP_SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  }
  control_stop();
  control_set_wall_control(false);
  log_stop();
}

/* distance_mm 進みながら止まり（max_speed_mm_s まで速くしてもよい）、モーターの出力を 0 にして、ログを止める。
 * 横の壁に沿って走る補正は入れる */
void run_fast_stop(float distance_mm, float max_speed_mm_s, float accel_mm_s2)
{
  control_set_wall_control(true);
  stop_after(distance_mm, max_speed_mm_s, accel_mm_s2);
}

/* 斜めに distance_mm 進みながら止まる（run_fast_stop と同じ。横の壁に沿って走る補正のかわりに、斜めの補正を入れる） */
void run_fast_diag_stop(float distance_mm, float max_speed_mm_s, float accel_mm_s2)
{
  set_on_diagonal(true);
  stop_after(distance_mm, max_speed_mm_s, accel_mm_s2);
  set_on_diagonal(false);
}
