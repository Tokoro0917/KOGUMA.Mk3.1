/*
 * test_wall_dist.c
 * 壁センサの値を距離 [mm] に直す処理（ctrl/wall_dist.c）の PC 上でのテスト。実機がなくても動く
 *
 * ToraNeko.Mk3 の software/main-controller/test/test_wall_dist.c（ecd8e10）をもとにした。
 * KOGUMA.Mk3.1 で変えたところ：表の点の値を KOGUMA.MkIIl の表にした。会場ごとの倍率で割ってから表を引くことも調べる（[1]）。
 *
 * ビルドと実行（このフォルダで）:  make
 *
 * ■ テストの内容（表の中身が測った値に変わっても通るように、表の形だけを使って調べる）
 *   1. 表の点の値で、その点の距離になる（区画のまん中 84mm・85mm の点。倍率をかけた値で調べる）
 *   2. 表の 2 点のまん中の値で、2 点の距離のまん中になる（直線でつないでいる）
 *   3. 表よりも大きい値（近い）・小さい値（遠い）では、表の端の距離になる
 *   4. 値を 0〜4095 まで変えたとき、値が大きいほど距離は短い（遠くなる向きに戻らない）。距離は表の範囲の中
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "ctrl/wall_dist.h"

static int s_fail_count = 0;

/* ok が false なら失敗として数え、message を表示する */
static void check(bool ok, const char *message)
{
  if (!ok) {
    s_fail_count = s_fail_count + 1;
    printf("  失敗: %s\n", message);
  }
}

/* 2 つの距離がほぼ同じか（0.05mm まで） */
static bool near_mm(float a_mm, float b_mm)
{
  return fabsf(a_mm - b_mm) < 0.05f;
}

/* 1. 表の点の値で、その点の距離。会場ごとの倍率（ctrl/wall_dist.c の *_SCALE）をかけた値を渡すと、表の点の距離になる */
static void test_center(void)
{
  printf("[1] 表の点の値で、その点の距離（会場ごとの倍率をかけた値）\n");
  check(near_mm(wall_dist_front_left_mm(639), 84.0f), "左前 639（表の 645 × 倍率 0.99）→ 84mm");
  check(near_mm(wall_dist_left_mm(237), 85.0f), "左 237（表の 237 × 倍率 1.00）→ 85mm");
  check(near_mm(wall_dist_right_mm(193), 85.0f), "右 193（表の 201 × 倍率 0.96）→ 85mm");
  check(near_mm(wall_dist_front_right_mm(640), 84.0f), "右前 640（表の 627 × 倍率 1.02）→ 84mm");
}

/* 2. 表の 2 点のまん中の値で、距離もまん中 */
static void test_between(void)
{
  printf("[2] 表の点のあいだは直線でつなぐ\n");
  /* 左の表（倍率 1.00）の 49mm（603）と 61mm（415）のまん中の値 509 → 55mm */
  check(near_mm(wall_dist_left_mm(509), 55.0f), "左 509 → 55mm");
  /* 左の表の 73mm（308）と 85mm（237）の、73mm から 1/4 進んだ値（308 − 71 ÷ 4 ≒ 290）→ 約 76mm */
  check(fabsf(wall_dist_left_mm(290) - 76.0f) < 0.1f, "左 290 → 約 76mm");
}

/* 3. 表の外は、表の端の距離 */
static void test_outside(void)
{
  printf("[3] 表より近い・遠いときは表の端\n");
  float nearest_mm = wall_dist_front_left_mm(4095);
  float farthest_mm = wall_dist_front_left_mm(0);
  check(near_mm(wall_dist_front_left_mm(5000), nearest_mm), "左前：表よりずっと大きい値でも、いちばん近い距離のまま");
  check(near_mm(wall_dist_front_left_mm(-100), farthest_mm), "左前：0 より小さい値でも、いちばん遠い距離のまま");
  check(nearest_mm < farthest_mm, "左前：近い端のほうが距離が短い");
  check(near_mm(wall_dist_left_mm(0), wall_dist_left_mm(10)), "左：表より小さい値は、どれも同じ（いちばん遠い）距離");
}

/* センサの番号（0 左前、1 左、2 右、3 右前）の表で、値を距離に直す */
static float sensor_to_mm(int sensor, int32_t value)
{
  if (sensor == 0) {
    return wall_dist_front_left_mm(value);
  }
  if (sensor == 1) {
    return wall_dist_left_mm(value);
  }
  if (sensor == 2) {
    return wall_dist_right_mm(value);
  }
  return wall_dist_front_right_mm(value);
}

/* value を 0〜4095 まで変えて、距離が増えないこと（値が大きいほど近い）と、表の範囲の中にあることを調べる */
static void check_monotonic(const char *name, int sensor)
{
  float near_end_mm = sensor_to_mm(sensor, 4095);
  float far_end_mm = sensor_to_mm(sensor, 0);
  float previous_mm = far_end_mm;
  bool ok = true;
  for (int value = 0; value <= 4095; value++) {
    float mm = sensor_to_mm(sensor, value);
    if (mm > previous_mm + 0.0001f) {
      ok = false;
    }
    if (mm < near_end_mm - 0.0001f || mm > far_end_mm + 0.0001f) {
      ok = false;
    }
    previous_mm = mm;
  }
  if (!ok) {
    printf("  %s の表\n", name);
  }
  check(ok, "値が大きいほど距離が短く、表の範囲の中にある");
}

/* 4. 4 つのセンサとも、値が大きいほど近い */
static void test_monotonic(void)
{
  printf("[4] 値が大きいほど近い（4 つのセンサ）\n");
  check_monotonic("左前", 0);
  check_monotonic("左", 1);
  check_monotonic("右", 2);
  check_monotonic("右前", 3);
}

int main(void)
{
  test_center();
  test_between();
  test_outside();
  test_monotonic();

  if (s_fail_count == 0) {
    printf("すべて成功\n");
    return 0;
  }
  printf("失敗 %d 件\n", s_fail_count);
  return 1;
}
