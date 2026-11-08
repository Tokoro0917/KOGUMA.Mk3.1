/*
 * wall_ctrl.c
 * 壁のあり・なしの判定と、壁に沿って走るための補正に使う値
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/ctrl/wall_ctrl.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 * しきい値・区画のまん中の値を、KOGUMA.MkIIl の Core/Src/Wallsensor.c の値にした（下の表）。
 * 考え方と関数は ToraNeko.Mk3 のまま（ToraNeko と同じ制御方法にする。2026-11-08 に決定）。
 *
 * ■ 受光の番号とセンサ（drv/wall.h。ToraNeko.Mk3 と同じ）
 *   左前 = 受光 1（組A で光らせる）、右の壁 = 受光 2（組A）、左の壁 = 受光 3（組B）、右前 = 受光 4（組B）
 *
 * ■ 値の出どころ（KOGUMA.MkIIl の Wallsensor.c。センサの値は KOGUMA.MkIIl の大きさ）
 *   KOGUMA.MkIIl                                KOGUMA.Mk3.1
 *   Wall_threshold = {60, 90, 90, 60}           壁があるかのしきい値（左前 60、右 90、左 90、右前 60）
 *   Wall_L = 243、Wall_R = 206                  区画のまん中の横の値（LEFT/RIGHT_WALL_REFERENCE）
 *   FL 645・FR 627（区画のまん中、2026-10-06）  区画のまん中の前の値（FRONT_*_REFERENCE。前壁合わせの目標）
 *   calWallConrol_NANAME の 110                 斜めの補正のしきい値（DIAG_*_THRESHOLD）
 *   Sensor_Enter の左前 > 1400                  手のしきい値（FRONT_*_HAND_THRESHOLD）
 * KOGUMA.MkIIl は、壁があるかを 10 回の平均（g_sensor_av10）で見ていた。KOGUMA.Mk3.1 は ToraNeko.Mk3 と同じく、
 * 制御周期ごとの値をそのまま見る。ならさない分だけノイズでぶれるので、ステップ6 で実機の値を見て、
 * しきい値と、ならすかどうかを決め直す（KOGUMA は左前・左にノイズがのっていた。KOGUMA_PORTING.md の B7）。
 * ToraNeko.Mk3 にしかない値（壁の終わりの見分け、横の壁のずれの上限、補正に使う下限）は、
 * ToraNeko の値を、センサの値の大きさの比（区画のまん中の横の値 ToraNeko 1460 : KOGUMA 243 ≒ 6 : 1）で割って入れた（仮）。
 */
#include "ctrl/wall_ctrl.h"

#include "config/run_params.h"
#include "ctrl/wall_dist.h"
#include "drv/wall.h"

#define FRONT_LEFT_WALL_THRESHOLD  60   /* 左前：これより大きければ前に壁（KOGUMA.MkIIl の Wall_threshold[0]） */
#define FRONT_RIGHT_WALL_THRESHOLD 60   /* 右前：同じく（Wall_threshold[3]） */
#define LEFT_WALL_THRESHOLD        90   /* 左：これより大きければ左に壁（Wall_threshold[2]） */
#define RIGHT_WALL_THRESHOLD       90   /* 右：これより大きければ右に壁（Wall_threshold[1]） */

/* 区画のまん中にいるときの、横のセンサの値。壁に沿って走る補正の目標（KOGUMA.MkIIl の Wall_L・Wall_R） */
#define LEFT_WALL_REFERENCE  243
#define RIGHT_WALL_REFERENCE 206

/* 壁に沿って走る補正に、その壁を使う下限。まん中の値のおよそ 7 割より大きいときだけ使う（ToraNeko.Mk3 と同じ考え方）。
 * 壁の端（柱の近く）では、壁があっても値がまだ上がりきっておらず、補正が「壁から遠い」と読み違えるため */
#define LEFT_WALL_CONTROL_MIN  170  /* 左のまん中の値 243 の約 7 割 */
#define RIGHT_WALL_CONTROL_MIN 144  /* 右のまん中の値 206 の約 7 割 */

/* 壁の終わりの見分け方（ToraNeko.Mk3 の 2026-11-04）。WALL_END_WINDOW_MM 進むあいだに WALL_END_DROP より下がったら、
 * 壁の終わりとみなし、そこから WALL_END_HOLD_MM 進むまで、その壁を補正に使わない。
 * KOGUMA は区画が 2 倍なので、距離も 2 倍にした。下がり方は、ToraNeko の 60 をセンサの値の大きさの比（約 6 分の 1）で割った（仮） */
#define WALL_END_WINDOW_MM 10    /* 下がり方を見る距離 [mm]（1mm ごとに値を覚えておく） */
#define WALL_END_DROP      10    /* WALL_END_WINDOW_MM 進むあいだに、これより下がったら壁の終わり（仮） */
#define WALL_END_HOLD_MM   30.0f /* 壁の終わりとみなしてから、その壁を使わない距離 [mm] */

/* 区画のまん中にいるときの、前のセンサの値。前壁合わせの目標（KOGUMA.MkIIl の Wallsensor.c のコメント：
 * 区画中心（前の壁まで 84mm）で FL = 645、FR = 627。2026-10-06 実測） */
#define FRONT_LEFT_REFERENCE  645
#define FRONT_RIGHT_REFERENCE 627

/* 横の壁からのずれの上限（センサの値の差）。急に大きく曲げないようにする。
 * KOGUMA.MkIIl の距離版の上限（WallCtrl_mm_ErrMax）20mm を、区画のまん中の近くの値の変わり方（約 6 / mm）で値にした */
#define SIDE_ERROR_LIMIT 120.0f

/* 斜めの直進で、柱や壁に近づいたとみなす前のセンサの値（KOGUMA.MkIIl の calWallConrol_NANAME の 110） */
#define DIAG_FRONT_LEFT_THRESHOLD  110
#define DIAG_FRONT_RIGHT_THRESHOLD 110

/* 前センサに手がかざされているか（KOGUMA.MkIIl の Sensor_Enter は左前 > 1400 だけ。ToraNeko.Mk3 と同じく両方で見る。仮） */
#define FRONT_LEFT_HAND_THRESHOLD  1400
#define FRONT_RIGHT_HAND_THRESHOLD 1400

/* 左前のセンサの値（受光 1、組A） */
int32_t wall_ctrl_get_front_left(void)
{
  return wall_get_a_minus_dark(1);
}

/* 左の壁のセンサの値（受光 3、組B） */
int32_t wall_ctrl_get_left(void)
{
  return wall_get_b_minus_dark(3);
}

/* 右の壁のセンサの値（受光 2、組A） */
int32_t wall_ctrl_get_right(void)
{
  return wall_get_a_minus_dark(2);
}

/* 右前のセンサの値（受光 4、組B） */
int32_t wall_ctrl_get_front_right(void)
{
  return wall_get_b_minus_dark(4);
}

/* 横の壁が近すぎた記録（探索の横壁合わせ。探索を始めるたびに 0 に戻す） */
static int s_too_close_count = 0;        /* 近すぎた回数 */
static float s_too_close_min_mm = 0.0f;  /* いちばん近かった距離 [mm] */

/* 横の壁があって、その距離 distance_mm が SIDE_TOO_CLOSE_MM より近いか。近ければ記録する。
 * SIDE_ALIGN_ENABLE が 0 なら、記録だけして false を返す（探索は止まらない） */
static bool check_too_close(bool has_wall, float distance_mm)
{
  if (!has_wall || distance_mm >= SIDE_TOO_CLOSE_MM) {
    return false;
  }
  if (s_too_close_count == 0 || distance_mm < s_too_close_min_mm) {
    s_too_close_min_mm = distance_mm;
  }
  s_too_close_count = s_too_close_count + 1;
  if (SIDE_ALIGN_ENABLE == 0) {
    return false;
  }
  return true;
}

/* 左の壁が近すぎるか（区画の入口で呼ぶ。探索の横壁合わせ） */
bool wall_ctrl_is_left_too_close(void)
{
  return check_too_close(wall_ctrl_has_wall_left(), wall_ctrl_get_left_mm());
}

/* 右の壁が近すぎるか */
bool wall_ctrl_is_right_too_close(void)
{
  return check_too_close(wall_ctrl_has_wall_right(), wall_ctrl_get_right_mm());
}

/* 横の壁が近すぎた記録を 0 に戻す（探索を始めるときに呼ぶ） */
void wall_ctrl_clear_too_close(void)
{
  s_too_close_count = 0;
  s_too_close_min_mm = 0.0f;
}

/* 横の壁が近すぎた回数と、いちばん近かった距離 [mm]（モード7 No.1 で出す） */
int wall_ctrl_get_too_close_count(void)
{
  return s_too_close_count;
}

float wall_ctrl_get_too_close_min_mm(void)
{
  return s_too_close_min_mm;
}

/* 前の壁までの距離 [mm]（左前と右前の平均。区画のまん中で 84mm） */
float wall_ctrl_get_front_distance_mm(void)
{
  return (wall_ctrl_get_front_left_mm() + wall_ctrl_get_front_right_mm()) / 2.0f;
}

/* 前の壁までの距離 [mm]（左前のセンサから。表は ctrl/wall_dist.c） */
float wall_ctrl_get_front_left_mm(void)
{
  return wall_dist_front_left_mm(wall_ctrl_get_front_left());
}

/* 左の壁までの距離 [mm] */
float wall_ctrl_get_left_mm(void)
{
  return wall_dist_left_mm(wall_ctrl_get_left());
}

/* 右の壁までの距離 [mm] */
float wall_ctrl_get_right_mm(void)
{
  return wall_dist_right_mm(wall_ctrl_get_right());
}

/* 前の壁までの距離 [mm]（右前のセンサから） */
float wall_ctrl_get_front_right_mm(void)
{
  return wall_dist_front_right_mm(wall_ctrl_get_front_right());
}

/* 前に壁があるか（左前と右前の両方が、しきい値より大きいとき。KOGUMA と同じ） */
bool wall_ctrl_has_wall_front(void)
{
  if (wall_ctrl_get_front_left() > FRONT_LEFT_WALL_THRESHOLD
      && wall_ctrl_get_front_right() > FRONT_RIGHT_WALL_THRESHOLD) {
    return true;
  }
  return false;
}

/* 左に壁があるか */
bool wall_ctrl_has_wall_left(void)
{
  if (wall_ctrl_get_left() > LEFT_WALL_THRESHOLD) {
    return true;
  }
  return false;
}

/* 右に壁があるか */
bool wall_ctrl_has_wall_right(void)
{
  if (wall_ctrl_get_right() > RIGHT_WALL_THRESHOLD) {
    return true;
  }
  return false;
}

/* ---- 壁の終わりの見分け（上の WALL_END_* のコメント） ----
 * 1mm 進むごとに、左右のセンサの値を覚える（いちばん新しい WALL_END_WINDOW_MM + 1 個）。
 * 覚えておく場所は、s_end_index の場所に書いたら 1 つ進め、端まで来たら 0 に戻す（いちばん古い値は、次に書く場所にある） */
#define END_HISTORY_COUNT (WALL_END_WINDOW_MM + 1)
static int32_t s_left_history[END_HISTORY_COUNT];
static int32_t s_right_history[END_HISTORY_COUNT];
static int s_end_index = 0;                  /* 次に書く場所 */
static int s_end_history_filled = 0;         /* 覚えた数（END_HISTORY_COUNT まで） */
static float s_end_last_mm = 0.0f;           /* 最後に覚えたときの、走った距離 [mm] */
static float s_now_mm = 0.0f;                /* 今の、走った距離 [mm] */
static float s_left_unusable_until_mm = 0.0f;   /* 左の壁を、この距離まで補正に使わない */
static float s_right_unusable_until_mm = 0.0f;  /* 右の壁を、この距離まで補正に使わない */

/* 壁の終わりの見分けを、最初からやり直す（制御を始めるとき。ctrl/control.c） */
void wall_ctrl_reset_end_check(float distance_mm)
{
  s_end_index = 0;
  s_end_history_filled = 0;
  s_end_last_mm = distance_mm;
  s_now_mm = distance_mm;
  s_left_unusable_until_mm = distance_mm;
  s_right_unusable_until_mm = distance_mm;
}

/* 制御周期ごとに呼ぶ（ctrl/control.c）。distance_mm は、今の走った距離（エンコーダで測った値）。
 * 1mm 進んでいたら左右の値を覚え、WALL_END_WINDOW_MM 前の値から WALL_END_DROP より下がっていたら、壁の終わりとみなす */
void wall_ctrl_update_end_check(float distance_mm)
{
  s_now_mm = distance_mm;
  if (distance_mm < s_end_last_mm + 1.0f) {
    return;  /* まだ 1mm 進んでいない（止まっている、下がっているときも、覚えない） */
  }
  s_end_last_mm = distance_mm;

  int32_t left = wall_ctrl_get_left();
  int32_t right = wall_ctrl_get_right();
  if (s_end_history_filled >= END_HISTORY_COUNT) {
    /* いちばん古い値（WALL_END_WINDOW_MM 前）は、これから書く場所にある */
    int32_t left_before = s_left_history[s_end_index];
    int32_t right_before = s_right_history[s_end_index];
    if (left_before - left > WALL_END_DROP) {
      s_left_unusable_until_mm = distance_mm + WALL_END_HOLD_MM;
    }
    if (right_before - right > WALL_END_DROP) {
      s_right_unusable_until_mm = distance_mm + WALL_END_HOLD_MM;
    }
  } else {
    s_end_history_filled = s_end_history_filled + 1;
  }
  s_left_history[s_end_index] = left;
  s_right_history[s_end_index] = right;
  s_end_index = s_end_index + 1;
  if (s_end_index >= END_HISTORY_COUNT) {
    s_end_index = 0;
  }
}

/* 左の壁を、壁に沿って走る補正に使えるか（値がまん中の値に近く、壁の終わりではない） */
static bool can_use_left_wall(void)
{
  if (s_now_mm < s_left_unusable_until_mm) {
    return false;  /* 壁の終わり */
  }
  if (wall_ctrl_get_left() > LEFT_WALL_CONTROL_MIN) {
    return true;
  }
  return false;
}

/* 右の壁を、壁に沿って走る補正に使えるか */
static bool can_use_right_wall(void)
{
  if (s_now_mm < s_right_unusable_until_mm) {
    return false;  /* 壁の終わり */
  }
  if (wall_ctrl_get_right() > RIGHT_WALL_CONTROL_MIN) {
    return true;
  }
  return false;
}

/* 横の壁からのずれ（+ なら左の壁に近い）。上限は ±SIDE_ERROR_LIMIT。
 * 両方の壁が使えれば (左 − 左の目標) − (右 − 右の目標)、片方だけなら、その壁のずれを 2 倍にする
 * （両方のときと、同じずれで同じくらいの大きさになるように）。どちらも使えなければ 0 */
float wall_ctrl_get_side_error(void)
{
  float left_error = wall_ctrl_get_left() - LEFT_WALL_REFERENCE;
  float right_error = wall_ctrl_get_right() - RIGHT_WALL_REFERENCE;
  bool left = can_use_left_wall();
  bool right = can_use_right_wall();

  float error = 0.0f;
  if (left && right) {
    error = left_error - right_error;
  } else if (left) {
    error = 2.0f * left_error;
  } else if (right) {
    error = -2.0f * right_error;
  }

  if (error > SIDE_ERROR_LIMIT) {
    error = SIDE_ERROR_LIMIT;
  }
  if (error < -SIDE_ERROR_LIMIT) {
    error = -SIDE_ERROR_LIMIT;
  }
  return error;
}

/* 横の壁からのずれを求めるのに使う壁（0 なし、1 左、2 右、3 両方。ログに残す） */
int wall_ctrl_get_side_walls_used(void)
{
  int used = 0;
  if (can_use_left_wall()) {
    used = used + 1;
  }
  if (can_use_right_wall()) {
    used = used + 2;
  }
  return used;
}

/* 斜めの直進で、柱や壁に近づいたずれ（+ なら左の柱・壁に近い。斜めの補正で使う）。KOGUMA の calWallConrol_NANAME と同じく、
 * 左前がしきい値を超えていれば（左前 − しきい値）、そうでなく右前が超えていれば −（右前 − しきい値）、どちらでもなければ 0 */
float wall_ctrl_get_diag_error(void)
{
  int32_t front_left = wall_ctrl_get_front_left();
  int32_t front_right = wall_ctrl_get_front_right();
  if (front_left > DIAG_FRONT_LEFT_THRESHOLD) {
    return front_left - DIAG_FRONT_LEFT_THRESHOLD;
  }
  if (front_right > DIAG_FRONT_RIGHT_THRESHOLD) {
    return -(front_right - DIAG_FRONT_RIGHT_THRESHOLD);
  }
  return 0.0f;
}

/* 斜めの補正で、どちらの前のセンサがしきい値を超えたか（0 なし、1 左前、2 右前。ログに残す） */
int wall_ctrl_get_diag_used(void)
{
  if (wall_ctrl_get_front_left() > DIAG_FRONT_LEFT_THRESHOLD) {
    return 1;
  }
  if (wall_ctrl_get_front_right() > DIAG_FRONT_RIGHT_THRESHOLD) {
    return 2;
  }
  return 0;
}

/* 前の壁との距離のずれ（+ なら近すぎる）。左前と右前の、目標との差の平均 */
float wall_ctrl_get_front_distance_error(void)
{
  float left_error = wall_ctrl_get_front_left() - FRONT_LEFT_REFERENCE;
  float right_error = wall_ctrl_get_front_right() - FRONT_RIGHT_REFERENCE;
  return (left_error + right_error) / 2.0f;
}

/* 前の壁に対する向きのずれ（+ なら左前のほうが近い）。
 * 機体が右を向くと、左前のセンサが壁に近づき、右前のセンサが遠ざかるので、左前の差 − 右前の差 が + になる */
float wall_ctrl_get_front_angle_error(void)
{
  float left_error = wall_ctrl_get_front_left() - FRONT_LEFT_REFERENCE;
  float right_error = wall_ctrl_get_front_right() - FRONT_RIGHT_REFERENCE;
  return left_error - right_error;
}

/* 前センサに手がかざされているか（左前と右前の両方が、壁よりずっと近いときの値を超えている） */
bool wall_ctrl_is_hand_front(void)
{
  if (wall_ctrl_get_front_left() > FRONT_LEFT_HAND_THRESHOLD
      && wall_ctrl_get_front_right() > FRONT_RIGHT_HAND_THRESHOLD) {
    return true;
  }
  return false;
}
