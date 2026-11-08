/*
 * wall_dist.c
 * 壁センサの値を、壁までの距離 [mm] に直す（表を引いて、表の点のあいだは直線でつなぐ）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/ctrl/wall_dist.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 * 表を KOGUMA.MkIIl の Core/Src/WallDistance.c の表（実測）にした。会場ごとの倍率（WallDist_Scale）を足した。
 * HAL を使わないので、PC のテスト（test/test_wall_dist.c）でも動く。
 *
 * ■ 距離の決め方（区画のまん中にいるとき、どれも 84mm。KOGUMA.MkIIl と同じ）
 *   横（左・右）：機体の中心線から、横の壁の表面まで
 *   前（左前・右前）：機体の中心から、前の壁の表面まで
 *   区画 180mm から壁の厚さ 12mm を引いて 2 で割ると 84mm（WALL_DIST_CENTER_MM）。
 *
 * ■ 今の表（KOGUMA.MkIIl の WallDistance.c の表をそのまま写した。2026-10-06 にモード3 No.4 で実測）
 *   横：機体の側面を反対側の壁に付け、壁の板（12mm）を n 枚はさんで、測る側の壁までを 133 − 12n mm（n = 0〜8）にして測った
 *       （機体の幅 70mm、中心線から側面まで 35mm として）
 *   前：お尻を後ろの壁に付け、壁の板を n 枚はさんで、前の壁までを 108 − 12n mm（n = −8〜4。60〜204mm）にして測った
 *       （全長 120mm、中心からお尻まで 60mm として）。60mm より近い点は測っていない
 * KOGUMA.MkIIl のセンサの値は、制御周期ごとの値をならしたもの（PL_sensor.c で 0.85・0.15 の重みでならし、5 回の平均）。
 * KOGUMA.Mk3.1 では、ToraNeko.Mk3 と同じく、ならさずに使う。ならした分だけ値が少しずれるかもしれないので、
 * ステップ6 で実機で測り直す（ToraNeko.Mk3 のモード5 No.2 と tools/wall_table.py を持ってきてから）。
 *
 * ■ 会場ごとの倍率（KOGUMA.MkIIl の WallDist_Scale。モード3 No.5 の会場ごとの感度合わせで出す）
 * 会場の壁の色・光の具合で、センサの値が全体に何割か変わる。測った値 ÷ 倍率 を表で距離に直す。
 * 今は KOGUMA.MkIIl の値をそのまま入れた（2026-10-10 に合わせた値）。
 * KOGUMA.Mk3.1 では、会場で測った倍率を Flash に保存する（KOGUMA_PORTING.md の B5。ステップ9 で作る）。
 *
 * ■ 表の書き方
 *   - 距離の近い順に並べる。センサの値は、距離が遠くなるほど小さくなること（同じ値が続いてもよい）
 *   - 表より近い（値が大きい）ときは表のいちばん近い距離、表より遠い（値が小さい）ときはいちばん遠い距離を返す
 */
#include "ctrl/wall_dist.h"

/* ---- 会場ごとの倍率（上の「会場ごとの倍率」。KOGUMA.MkIIl の WallDist_Scale = { 0.99, 0.96, 1.00, 1.02 }） ---- */
#define FRONT_LEFT_SCALE  0.99f
#define RIGHT_SCALE       0.96f
#define LEFT_SCALE        1.00f
#define FRONT_RIGHT_SCALE 1.02f

/* ---- 左前（FL）の表（KOGUMA.MkIIl の実測） ---- */
#define FRONT_LEFT_POINT_COUNT 13
static const float FRONT_LEFT_MM[FRONT_LEFT_POINT_COUNT] = {60, 72, 84, 96, 108, 120, 132, 144, 156, 168, 180, 192, 204};
static const int32_t FRONT_LEFT_VALUE[FRONT_LEFT_POINT_COUNT] = {1366, 902, 645, 470, 356, 295, 230, 195, 167, 146, 132, 123, 112};

/* ---- 左（L）の表（KOGUMA.MkIIl の実測） ---- */
#define LEFT_POINT_COUNT 9
static const float LEFT_MM[LEFT_POINT_COUNT] = {37, 49, 61, 73, 85, 97, 109, 121, 133};
static const int32_t LEFT_VALUE[LEFT_POINT_COUNT] = {910, 603, 415, 308, 237, 183, 150, 125, 105};

/* ---- 右（R）の表（KOGUMA.MkIIl の実測） ---- */
#define RIGHT_POINT_COUNT 9
static const float RIGHT_MM[RIGHT_POINT_COUNT] = {37, 49, 61, 73, 85, 97, 109, 121, 133};
static const int32_t RIGHT_VALUE[RIGHT_POINT_COUNT] = {780, 522, 356, 262, 201, 160, 131, 110, 93};

/* ---- 右前（FR）の表（KOGUMA.MkIIl の実測） ---- */
#define FRONT_RIGHT_POINT_COUNT 13
static const float FRONT_RIGHT_MM[FRONT_RIGHT_POINT_COUNT] = {60, 72, 84, 96, 108, 120, 132, 144, 156, 168, 180, 192, 204};
static const int32_t FRONT_RIGHT_VALUE[FRONT_RIGHT_POINT_COUNT] = {1377, 940, 627, 461, 358, 281, 248, 204, 175, 155, 129, 120, 112};

/* センサの値 value を、会場ごとの倍率 scale で割って、いちばん近い整数にする（KOGUMA.MkIIl の WallDist_mm と同じ） */
static int32_t apply_scale(int32_t value, float scale)
{
  if (scale <= 0.0f) {
    return value;  /* 倍率がおかしいときは、そのまま使う */
  }
  float scaled = value / scale;
  /* (int32_t) は小数点より下を切り捨てるので、0.5 を足して（マイナスなら引いて）から切り捨てる */
  if (scaled >= 0.0f) {
    return (int32_t)(scaled + 0.5f);
  }
  return (int32_t)(scaled - 0.5f);
}

/* センサの値 value を、表（距離 mm_table と値 value_table、point_count 点）で距離 [mm] に直す。
 * 表の i 番目と i+1 番目のあいだに value があれば、その 2 点を直線でつないだところの距離を返す */
static float value_to_mm(int32_t value, const float mm_table[], const int32_t value_table[], int point_count)
{
  int last = point_count - 1;
  if (value >= value_table[0]) {
    return mm_table[0];  /* 表よりも近い */
  }
  if (value <= value_table[last]) {
    return mm_table[last];  /* 表よりも遠い */
  }
  for (int i = 0; i < last; i++) {
    if (value >= value_table[i + 1]) {
      /* value は、i 番目（近いほう）と i+1 番目（遠いほう）のあいだ。
       * ratio は、i+1 番目から i 番目へ、どれだけ近づいたか（0〜1） */
      float value_span = value_table[i] - value_table[i + 1];
      if (value_span <= 0.0f) {
        return mm_table[i];  /* 同じ値が続いている（割り算ができない）ので、近いほうの距離にする */
      }
      float ratio = (value - value_table[i + 1]) / value_span;
      return mm_table[i + 1] + ratio * (mm_table[i] - mm_table[i + 1]);
    }
  }
  return mm_table[last];  /* ここには来ない（表が近い順に並んでいれば、上のどれかで戻る） */
}

/* 左前のセンサの値 → 前の壁までの距離 [mm] */
float wall_dist_front_left_mm(int32_t value)
{
  return value_to_mm(apply_scale(value, FRONT_LEFT_SCALE), FRONT_LEFT_MM, FRONT_LEFT_VALUE, FRONT_LEFT_POINT_COUNT);
}

/* 左のセンサの値 → 左の壁までの距離 [mm] */
float wall_dist_left_mm(int32_t value)
{
  return value_to_mm(apply_scale(value, LEFT_SCALE), LEFT_MM, LEFT_VALUE, LEFT_POINT_COUNT);
}

/* 右のセンサの値 → 右の壁までの距離 [mm] */
float wall_dist_right_mm(int32_t value)
{
  return value_to_mm(apply_scale(value, RIGHT_SCALE), RIGHT_MM, RIGHT_VALUE, RIGHT_POINT_COUNT);
}

/* 右前のセンサの値 → 前の壁までの距離 [mm] */
float wall_dist_front_right_mm(int32_t value)
{
  return value_to_mm(apply_scale(value, FRONT_RIGHT_SCALE), FRONT_RIGHT_MM, FRONT_RIGHT_VALUE, FRONT_RIGHT_POINT_COUNT);
}
