/*
 * test_turn.c
 * 探索のスラローム（小回り 90°）と、最短走行のターン（大回り・斜め）の前距離・後距離を、PC で計算して確かめる。実機がなくても動く
 *
 *   ./test_turn          config/run_params.h・config/turn_params.h の値で、どのターンも決めた位置・向きで終わるかを調べる（make で実行）
 *   ./test_turn --calc   ターンごとの速さ・最高角速度・角加速度から、前距離・後距離を計算して、
 *                        config/run_params.h・config/turn_params.h に貼る行を出す
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の software/main-controller/test/test_turn.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - 区画の大きさ（180mm。終わりの位置の表 TURNS は config/run_params.h の CELL_MM・HALF_CELL_MM で書いた）
 *   - --calc は、最高角速度を config/turn_params.h の値（KOGUMA.MkIIl の値）のまま使い、前距離・後距離だけを計算する。
 *     ToraNeko.Mk3 は「前距離・後距離の短いほうが 3mm になる最高角速度」を探していたが、KOGUMA.MkIIl は最高角速度を
 *     人が決めて、前距離・後距離を合わせていたので、その形にした。大回り 180° だけは、横の位置がスラロームだけで決まるので、
 *     ToraNeko.Mk3 と同じく、横がちょうど 1 区画になる最高角速度を探す
 *   - 探索のスラローム（吸引なし・吸引あり）も計算する（ToraNeko.Mk3 は test_trajectory.c の [6] で確かめていた）
 *   - --slip（実機で足した分を「空回り」の式で説明する）は、まだ持ってきていない。実機でターンを合わせたあと（ステップ10）に持ってくる
 *
 * ■ 計算のしかた
 * ターンは「前距離（まっすぐ）→ スラローム（前に進む速さは一定、角速度は台形）→ 後距離（まっすぐ）」。
 * スラロームのあいだに進む位置は、本物の ctrl/trajectory.c を 0.5ms ずつ回して、目標の速さと角度から足していく。
 *
 * 終わりの位置（ターンを始めた位置と向きから見て、左へ何 mm・前へ何 mm）は、区画 180mm・半区画 90mm の迷路の形で決まる
 * （下の TURNS の表。左に曲がるとき。右はこれを左右に裏返したもの）。
 *   前距離 + スラローム + 後距離（後距離は曲がったあとの向き）で、ちょうどその位置になるように、前距離と後距離を決める。
 *
 * ■ KOGUMA.MkIIl の角速度の形との違い
 * KOGUMA.MkIIl のスラローム（motor.c の Motor_Sula_COS）は、角速度を cos の形で上げ下げする（w_ac は、いちばん大きい角加速度）。
 * ToraNeko.Mk3 の ctrl/trajectory.c は台形（角加速度は一定）。上げきるまでの時間が同じになるように、
 * 台形の角加速度 ＝ KOGUMA の w_ac × 2/π にした（config/run_params.h・config/turn_params.h のコメント）。
 * こうすると、上げているあいだに曲がる角度も KOGUMA と同じになり、曲がる形はほとんど同じになる。
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "config/robot_params.h"
#include "config/run_params.h"
#include "config/turn_params.h"
#include "ctrl/trajectory.h"

#define MAX_STEPS 100000      /* これだけ回しても終わらなければ失敗 */
#define PI_F 3.14159265f
#define SQRT2_F 1.41421356f

static int s_fail_count = 0;

/* ok が false なら失敗として数え、message を表示する */
static void check(bool ok, const char *message)
{
  if (!ok) {
    s_fail_count = s_fail_count + 1;
    printf("  失敗: %s\n", message);
  }
}

/* ターンの形（左に曲がるとき） */
typedef struct {
  const char *name;        /* 表示する名前 */
  const char *prefix;      /* config/run_params.h・turn_params.h の名前の頭 */
  float angle_deg;         /* 曲がる角度 */
  float target_left_mm;    /* 終わりの位置：始めた位置から左へ */
  float target_forward_mm; /* 終わりの位置：始めた位置から前へ */
  float speed_mm_s;        /* そのターンを曲がる速さ（*_SPEED_MM_S） */
  float omega_deg_s;       /* 最高角速度（*_OMEGA_DEG_S） */
  float alpha_deg_s2;      /* 角加速度（*_ALPHA_DEG_S2） */
} turn_shape_t;

/* 探索のスラローム：区画の入口の辺から、左の区画の入口の辺へ（左へ半区画・前へ半区画） */
#define SEARCH_TURN_COUNT 2
static const turn_shape_t SEARCH_TURNS[SEARCH_TURN_COUNT] = {
  {"探索 小回り", "SLALOM", 90.0f, HALF_CELL_MM, HALF_CELL_MM, SEARCH_SPEED_MM_S, SLALOM_OMEGA_DEG_S, SLALOM_ALPHA_DEG_S2},
  {"吸引探索 小回り", "SUCTION_SLALOM", 90.0f, HALF_CELL_MM, HALF_CELL_MM, SUCTION_SEARCH_SPEED_MM_S,
   SUCTION_SLALOM_OMEGA_DEG_S, SUCTION_SLALOM_ALPHA_DEG_S2},
};

/* 最短走行のターン。始めた位置は、大回り・斜め入りは区画のまん中、斜め出・V90 は壁の辺のまん中（斜めの通り道の上）。
 * 終わりの位置は、大回り・斜め出は区画のまん中、斜め入り・V90 は壁の辺のまん中 */
#define TURN_COUNT 7
static const turn_shape_t TURNS[TURN_COUNT] = {
  {"大回り 90°", "BIG90", 90.0f, CELL_MM, CELL_MM, BIG90_SPEED_MM_S, BIG90_OMEGA_DEG_S, BIG90_ALPHA_DEG_S2},
  /* 大回り 180° の幅は、実機で内側を走る分（BIG180_WIDTH_ADJUST_MM）だけ広く計算する（config/turn_params.h） */
  {"大回り 180°", "BIG180", 180.0f, CELL_MM + BIG180_WIDTH_ADJUST_MM, 0.0f, BIG180_SPEED_MM_S, BIG180_OMEGA_DEG_S,
   BIG180_ALPHA_DEG_S2},
  {"斜め入り 45°", "IN45", 45.0f, HALF_CELL_MM, CELL_MM, IN45_SPEED_MM_S, IN45_OMEGA_DEG_S, IN45_ALPHA_DEG_S2},
  {"斜め入り 135°", "IN135", 135.0f, CELL_MM, HALF_CELL_MM, IN135_SPEED_MM_S, IN135_OMEGA_DEG_S, IN135_ALPHA_DEG_S2},
  {"斜め出 45°", "OUT45", 45.0f, HALF_CELL_MM / SQRT2_F, 3.0f * HALF_CELL_MM / SQRT2_F, OUT45_SPEED_MM_S,
   OUT45_OMEGA_DEG_S, OUT45_ALPHA_DEG_S2},
  {"斜め出 135°", "OUT135", 135.0f, 3.0f * HALF_CELL_MM / SQRT2_F, HALF_CELL_MM / SQRT2_F, OUT135_SPEED_MM_S,
   OUT135_OMEGA_DEG_S, OUT135_ALPHA_DEG_S2},
  {"V90°", "V90", 90.0f, CELL_MM / SQRT2_F, CELL_MM / SQRT2_F, V90_SPEED_MM_S, V90_OMEGA_DEG_S, V90_ALPHA_DEG_S2},
};

/* スラロームだけで進む位置（始めた向きから見て、左へ・前へ） */
typedef struct {
  float left_mm;
  float forward_mm;
} arc_t;

/* trajectory.c でスラロームを 1 回走らせて、進んだ位置を足していく */
static arc_t simulate_arc(float angle_deg, float speed_mm_s, float omega_deg_s, float alpha_deg_s2)
{
  arc_t arc = {0.0f, 0.0f};
  trajectory_reset();
  trajectory_start_slalom(angle_deg, speed_mm_s, omega_deg_s, alpha_deg_s2);
  int steps = 0;
  do {
    trajectory_update();
    float angle_rad = trajectory_get_angle_deg() * PI_F / 180.0f;
    float speed_now_mm_s = trajectory_get_speed_mm_s();
    arc.forward_mm = arc.forward_mm + speed_now_mm_s * cosf(angle_rad) * CTRL_PERIOD_S;
    arc.left_mm = arc.left_mm + speed_now_mm_s * sinf(angle_rad) * CTRL_PERIOD_S;
    steps = steps + 1;
  } while (!trajectory_is_done() && steps < MAX_STEPS);
  return arc;
}

/* スラロームで進む位置から、前距離と後距離を決める。
 *   左：スラロームの左 + 後距離 × sin(角度) = 目標の左
 *   前：前距離 + スラロームの前 + 後距離 × cos(角度) = 目標の前
 * 大回り 180° は sin(角度) が 0 なので、前距離を 3mm にし、後距離（後ろ向きに進む）は、前距離にスラロームの前を足した長さにする
 * （前と後ろで打ち消して、前の位置は変わらない。ToraNeko.Mk3 は前距離と後距離を同じにしていたが、2000mm/s では
 * 0.5ms ごとに 1mm 進むので、スラロームの前が 0 にならず 1mm ほど残る。その分を後距離で打ち消す） */
static void solve_pre_post(const turn_shape_t *turn, arc_t arc, float *pre_mm, float *post_mm)
{
  float angle_rad = turn->angle_deg * PI_F / 180.0f;
  float sin_value = sinf(angle_rad);
  if (fabsf(sin_value) < 0.01f) {
    *pre_mm = 3.0f;
    *post_mm = 3.0f + arc.forward_mm;
    return;
  }
  *post_mm = (turn->target_left_mm - arc.left_mm) / sin_value;
  *pre_mm = turn->target_forward_mm - arc.forward_mm - *post_mm * cosf(angle_rad);
}

/* 大回り 180°：最高角速度 omega_deg_s で曲がったときの、スラロームの左 − 目標の幅。
 * 角速度を上げると小さく回るので、この値は増える（引く順を逆にしてある） */
static float big180_width_criterion(const turn_shape_t *turn, float omega_deg_s)
{
  arc_t arc = simulate_arc(turn->angle_deg, turn->speed_mm_s, omega_deg_s, turn->alpha_deg_s2);
  return turn->target_left_mm - arc.left_mm;
}

/* 大回り 180° の、横がちょうど目標の幅になる最高角速度を探す（二分法。見つからなければ 0） */
static float find_big180_omega(const turn_shape_t *turn)
{
  float low_deg_s = 100.0f;
  float high_deg_s = 5000.0f;
  if (big180_width_criterion(turn, high_deg_s) < 0.0f || big180_width_criterion(turn, low_deg_s) > 0.0f) {
    return 0.0f;
  }
  for (int n = 0; n < 40; n++) {
    float middle_deg_s = (low_deg_s + high_deg_s) / 2.0f;
    if (big180_width_criterion(turn, middle_deg_s) < 0.0f) {
      low_deg_s = middle_deg_s;
    } else {
      high_deg_s = middle_deg_s;
    }
  }
  return high_deg_s;
}

/* ターン 1 つの、前距離・後距離を計算して、貼る行を出す。omega_deg_s はそのターンの最高角速度 */
static void print_turn_lines(const turn_shape_t *turn, float omega_deg_s)
{
  arc_t arc = simulate_arc(turn->angle_deg, turn->speed_mm_s, omega_deg_s, turn->alpha_deg_s2);
  float pre_mm;
  float post_mm;
  solve_pre_post(turn, arc, &pre_mm, &post_mm);
  printf("/* %s（%.0fmm/s、最高 %.0fdeg/s、角加速度 %.0fdeg/s²。横向きの加速度はいちばん大きいとき %.0fmm/s²） */\n",
         turn->name, turn->speed_mm_s, omega_deg_s, turn->alpha_deg_s2,
         turn->speed_mm_s * omega_deg_s * PI_F / 180.0f);
  if (pre_mm < 0.0f || post_mm < 0.0f) {
    printf("/* 注意：前距離か後距離が 0 より短い。最高角速度を上げるか、角加速度を上げる */\n");
  }
  printf("#define %s_PRE_MM %.1ff\n", turn->prefix, pre_mm);
  printf("#define %s_POST_MM %.1ff\n", turn->prefix, post_mm);
}

/* --calc：探索のスラロームと、最短走行のターンの前距離・後距離を計算して、貼る行を出す */
static int calculate(void)
{
  printf("/* ---- config/run_params.h に貼る（探索のスラローム。test/test_turn.c --calc） ---- */\n");
  for (int i = 0; i < SEARCH_TURN_COUNT; i++) {
    print_turn_lines(&SEARCH_TURNS[i], SEARCH_TURNS[i].omega_deg_s);
  }
  printf("\n/* ---- config/turn_params.h に貼る（最短走行のターン。test/test_turn.c --calc） ---- */\n");
  for (int i = 0; i < TURN_COUNT; i++) {
    const turn_shape_t *turn = &TURNS[i];
    float omega_deg_s = turn->omega_deg_s;
    if (turn->angle_deg > 179.0f) {
      omega_deg_s = find_big180_omega(turn);
      if (omega_deg_s <= 0.0f) {
        printf("/* %s：角速度 100〜5000deg/s では幅を合わせられない（角加速度を変える） */\n", turn->name);
        continue;
      }
      printf("#define %s_OMEGA_DEG_S %.1ff  /* 横がちょうど %.0fmm になる最高角速度 */\n", turn->prefix, omega_deg_s,
             turn->target_left_mm);
    }
    print_turn_lines(turn, omega_deg_s);
  }
  return 0;
}

/* 値の表のとおりに曲がったとき、決めた位置・向きで終わるかを調べる（ずれ 0.5mm まで） */
static void check_turn(const turn_shape_t *turn, float pre_mm, float post_mm)
{
  arc_t arc = simulate_arc(turn->angle_deg, turn->speed_mm_s, turn->omega_deg_s, turn->alpha_deg_s2);
  float angle_rad = turn->angle_deg * PI_F / 180.0f;
  float end_left_mm = arc.left_mm + post_mm * sinf(angle_rad);
  float end_forward_mm = pre_mm + arc.forward_mm + post_mm * cosf(angle_rad);
  float error_left_mm = end_left_mm - turn->target_left_mm;
  float error_forward_mm = end_forward_mm - turn->target_forward_mm;
  printf("  %-18s 最高 %6.1fdeg/s、前 %5.1fmm、後 %5.1fmm → 左 %6.1f・前 %6.1fmm（目標 %5.1f・%5.1f）\n",
         turn->name, turn->omega_deg_s, pre_mm, post_mm, end_left_mm, end_forward_mm,
         turn->target_left_mm, turn->target_forward_mm);
  check(fabsf(error_left_mm) < 0.5f && fabsf(error_forward_mm) < 0.5f, "決めた位置で終わる（±0.5mm）");
  check(pre_mm >= 0.0f && post_mm >= 0.0f, "前距離・後距離が 0 以上");
  check(fabsf(trajectory_get_angle_deg() - turn->angle_deg) < 0.01f, "ちょうど決めた角度だけ曲がる");
}

/* 速さを k 倍、最高角速度を k 倍、角加速度を k² 倍にしても、同じ位置で終わるか（遅く入ったターン。ctrl/run.c の fast_turn()） */
static void check_slow_turn(const turn_shape_t *turn, float ratio)
{
  arc_t normal = simulate_arc(turn->angle_deg, turn->speed_mm_s, turn->omega_deg_s, turn->alpha_deg_s2);
  arc_t slow = simulate_arc(turn->angle_deg, turn->speed_mm_s * ratio, turn->omega_deg_s * ratio,
                            turn->alpha_deg_s2 * ratio * ratio);
  printf("  %-18s 速さ %.1f 倍：左 %6.2f → %6.2f、前 %6.2f → %6.2f mm\n", turn->name, ratio,
         normal.left_mm, slow.left_mm, normal.forward_mm, slow.forward_mm);
  /* 0.5ms ごとに足していくので、速いほど 1 回で進む量が大きく、少しずれる（2000mm/s では 1 回 1mm。
   * 曲がり始めと終わりで 1 回ずつずれると 2mm）。ToraNeko.Mk3 は ±0.5mm（最高 800mm/s）。KOGUMA は 2000mm/s で曲がるので ±2.0mm にした */
  check(fabsf(normal.left_mm - slow.left_mm) < 2.0f && fabsf(normal.forward_mm - slow.forward_mm) < 2.0f,
        "遅く曲がっても、スラロームで進む位置が同じ（±2.0mm）");
}

int main(int argc, char *argv[])
{
  if (argc >= 2 && strcmp(argv[1], "--calc") == 0) {
    return calculate();
  }

  printf("[1] 探索のスラローム（config/run_params.h の値）\n");
  check_turn(&SEARCH_TURNS[0], SLALOM_PRE_MM, SLALOM_POST_MM);
  check_turn(&SEARCH_TURNS[1], SUCTION_SLALOM_PRE_MM, SUCTION_SLALOM_POST_MM);

  printf("[2] 最短走行のターン（config/turn_params.h の値。ターンごとの速さで）\n");
  check_turn(&TURNS[0], BIG90_PRE_MM, BIG90_POST_MM);
  check_turn(&TURNS[1], BIG180_PRE_MM, BIG180_POST_MM);
  check_turn(&TURNS[2], IN45_PRE_MM, IN45_POST_MM);
  check_turn(&TURNS[3], IN135_PRE_MM, IN135_POST_MM);
  check_turn(&TURNS[4], OUT45_PRE_MM, OUT45_POST_MM);
  check_turn(&TURNS[5], OUT135_PRE_MM, OUT135_POST_MM);
  check_turn(&TURNS[6], V90_PRE_MM, V90_POST_MM);

  printf("[3] 遅く入ったターン（速さ k 倍、最高角速度 k 倍、角加速度 k² 倍）\n");
  check_slow_turn(&TURNS[0], 0.5f);
  check_slow_turn(&TURNS[2], 0.4f);
  check_slow_turn(&TURNS[3], 0.6f);

  if (s_fail_count == 0) {
    printf("すべて成功\n");
    return 0;
  }
  printf("失敗 %d 件\n", s_fail_count);
  return 1;
}
