/*
 * sensor_noise.c
 * 吸引のときに壁センサにのるノイズを調べる（モード5 No.8。MODES.md。KOGUMA_PORTING.md の B7）
 *
 * ■ もとにしたもの
 * KOGUMA.MkIIl の Core/Src/SensorNoise.c（モード3 No.10、fa64b58）。ToraNeko.Mk3 にはない（KOGUMA.Mk3.1 で新しく作った）。
 * KOGUMA.MkIIl では、吸引のときに左前・左のセンサの値が揺れていた（KOGUMA.MkIIl の docs/sensor_noise.md）。
 * KOGUMA.Mk3.1 は壁センサの読み方を ToraNeko.Mk3 の形（TIM8 → ADC → DMA。drv/wall.c）に変えたので、
 * 同じ揺れが出るかを確かめる。KOGUMA.MkIIl と違うところ：
 *   - 読み方の組み合わせ（吸引の PWM に合わせて読むか）は比べない。KOGUMA.Mk3.1 は、制御周期ごとに決まった時刻に読む
 *   - 生の値の並び（trace）は出さない（表だけ）
 *
 * ■ 流れ（止まったまま。モーターは使わない）
 *   1. 吸引の強さを NOISE_SUCTION_V の 4 段（0・5.5・7.9・11.1V。KOGUMA の duty 0・35・50・70 と同じ）の順に変える
 *   2. 変えるたびに NOISE_WAIT_MS 待ってから、1ms ごとに NOISE_COUNT 回、4 つの受光の値を読む
 *      点灯（その受光の組の LED を点けたとき）・消灯（全部消したとき）・差（点灯 − 消灯）のそれぞれで、
 *      平均・ばらつき（標準偏差）・いちばん小さい値・いちばん大きい値を求める。電池の電圧 [mV] も同じように求める
 *   3. 4 段とも測ったら吸引を止めて、表をシリアルに出し、「うまくいった」を出す
 * ケーブルを挿さずに測ったときは、ログ出力（モード7 No.1）で同じ表を出す（sensor_noise_print()）。
 *
 * ■ 見かた（KOGUMA.MkIIl の docs/sensor_noise.md と同じ）
 *   消灯の値も吸引で揺れる → 電気（GND や ADC）のノイズ
 *   点灯の値だけ揺れる     → LED の点け方（電源の揺れ、立ち上がりの途中を読んでいる）
 *   差のばらつきが、吸引 0V のときより大きくなる → 吸引で壁の読みが揺れる（壁のあり・なし、距離がぶれる）
 *
 * ■ 出す形（KOGUMA.MkIIl と同じ。tools/viewer.html の「センサのノイズ」で表になる）
 *   #BEGIN NOISE n=500 trace=0 settle_us=0 wait_ms=500 pwm_top=99
 *   stat,0,0,15,duty,センサ,種類,平均,ばらつき,いちばん小さい,いちばん大きい,0
 *   batt,0,0,15,duty,平均,ばらつき,いちばん小さい,いちばん大きい,0        （電池の電圧 [mV]）
 *   #END NOISE
 * 先頭の 0,0,15 は、KOGUMA の「読み方の番号、待ち時間 [µs]、取り込み時間 [サイクル]」の欄（KOGUMA.Mk3.1 は 1 通りなので、
 * 番号 0、待ち 0、取り込み 15 サイクル（Core/Src/adc.c）を入れる）。duty は吸引の強さを KOGUMA の duty [%] で表したもの。
 */
#include "sys/sensor_noise.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "drv/fan.h"
#include "drv/timebase.h"
#include "drv/uart.h"
#include "drv/wall.h"
#include "sys/suction.h"
#include "sys/ui.h"

#define NOISE_COUNT        500   /* 1 段で読む回数（1ms ごと） */
#define NOISE_INTERVAL_MS  1     /* 読む間隔 [ms]（壁センサは 0.5ms ごとに新しい値になる） */
#define NOISE_WAIT_MS      500   /* 吸引を変えてから、読み始めるまで待つ時間 [ms]（ファンの回り方を落ち着かせる） */
#define NOISE_STEP_COUNT   4     /* 吸引の段の数 */
#define NOISE_SENSOR_COUNT 4     /* 受光の数 */
#define NOISE_KIND_COUNT   3     /* 点灯・消灯・差 */
#define LINE_LENGTH_MAX    100   /* 1 行の長さの上限 [文字]。出す前に、送信待ちバッファにこれだけの空きを待つ */

/* 吸引の強さ [V] と、それを KOGUMA の duty [%]（電池 15.8V のときの割合）で表したもの（KOGUMA の noise_duty と同じ段） */
static const float NOISE_SUCTION_V[NOISE_STEP_COUNT] = {0.0f, 5.5f, 7.9f, 11.1f};
static const int NOISE_DUTY[NOISE_STEP_COUNT] = {0, 35, 50, 70};

/* 受光の名前（受光 1〜4 の順。drv/wall.h：1 左前、2 右、3 左、4 右前）と、種類の名前 */
static const char *SENSOR_NAME[NOISE_SENSOR_COUNT] = {"FL", "R", "L", "FR"};
static const char *KIND_NAME[NOISE_KIND_COUNT] = {"on", "off", "diff"};

/* 集めている途中の値（1 つのセンサの 1 つの種類） */
typedef struct {
  double sum;         /* 値の合計 */
  double square_sum;  /* 値の 2 乗の合計（ばらつきを求めるため） */
  int32_t min;        /* いちばん小さい値 */
  int32_t max;        /* いちばん大きい値 */
} noise_sum_t;

/* 求めた結果 */
typedef struct {
  float mean;  /* 平均 */
  float sd;    /* ばらつき（標準偏差） */
  int32_t min;
  int32_t max;
} noise_stat_t;

static noise_sum_t s_sums[NOISE_SENSOR_COUNT][NOISE_KIND_COUNT];
static noise_sum_t s_battery_sum;
static noise_stat_t s_stats[NOISE_STEP_COUNT][NOISE_SENSOR_COUNT][NOISE_KIND_COUNT];
static noise_stat_t s_battery_stats[NOISE_STEP_COUNT];
static bool s_measured = false;  /* 一度でも測ったか */

/* ここからの関数は、集めている途中の値（noise_sum_t）の場所を受け取って、その中身を書きかえる・読む。
 * noise_sum_t *sum は「集めている値の場所」で、&s_sums[0][0] のように & を付けて渡す。中身は sum->min のように -> で使う。
 * 4 つのセンサ × 3 つの種類で同じ関数を使い回すために、この書き方にした（sys/sensor_record.c と同じ） */

/* 空にする（min は大きな値、max は小さな値から始めて、最初の値で置きかわるようにする） */
static void clear_sum(noise_sum_t *sum)
{
  sum->sum = 0.0;
  sum->square_sum = 0.0;
  sum->min = 100000;
  sum->max = -100000;
}

/* 値を 1 つ足す */
static void add_value(noise_sum_t *sum, int32_t value)
{
  sum->sum = sum->sum + value;
  sum->square_sum = sum->square_sum + (double)value * value;  /* (double) で小数にしてからかける（大きな数でもあふれない） */
  if (value < sum->min) {
    sum->min = value;
  }
  if (value > sum->max) {
    sum->max = value;
  }
}

/* 集めた値から、平均・ばらつきなどを求める。ばらつき ＝ √（2 乗の平均 − 平均の 2 乗） */
static noise_stat_t make_stat(const noise_sum_t *sum)
{
  noise_stat_t stat;
  double mean = sum->sum / NOISE_COUNT;
  double variance = sum->square_sum / NOISE_COUNT - mean * mean;
  if (variance < 0.0) {
    variance = 0.0;  /* 計算の誤差で、ほんの少し − になることがある */
  }
  stat.mean = mean;
  stat.sd = sqrt(variance);
  stat.min = sum->min;
  stat.max = sum->max;
  return stat;
}

/* 受光 num（1〜4）の、点灯したときの値（左前・右は組A、左・右前は組B の LED。drv/wall.h） */
static int32_t lit_value(int num)
{
  if (num == 1 || num == 2) {
    return wall_get_lit_a(num);
  }
  return wall_get_lit_b(num);
}

/* 受光 num（1〜4）の、点灯 − 消灯 */
static int32_t diff_value(int num)
{
  if (num == 1 || num == 2) {
    return wall_get_a_minus_dark(num);
  }
  return wall_get_b_minus_dark(num);
}

/* 今の吸引の強さで、NOISE_COUNT 回読んで、step 段目の結果にする */
static void measure_step(int step)
{
  for (int i = 0; i < NOISE_SENSOR_COUNT; i++) {
    for (int kind = 0; kind < NOISE_KIND_COUNT; kind++) {
      clear_sum(&s_sums[i][kind]);
    }
  }
  clear_sum(&s_battery_sum);

  for (int n = 0; n < NOISE_COUNT; n++) {
    for (int i = 0; i < NOISE_SENSOR_COUNT; i++) {
      int num = i + 1;  /* 受光の番号は 1〜4 */
      add_value(&s_sums[i][0], lit_value(num));
      add_value(&s_sums[i][1], wall_get_dark(num));
      add_value(&s_sums[i][2], diff_value(num));
    }
    int32_t battery_mv = wall_get_battery_v() * 1000.0f;  /* [mV] にする（int32_t に入れると、小数は切り捨てられる） */
    add_value(&s_battery_sum, battery_mv);
    timebase_wait_ms(NOISE_INTERVAL_MS);
  }

  for (int i = 0; i < NOISE_SENSOR_COUNT; i++) {
    for (int kind = 0; kind < NOISE_KIND_COUNT; kind++) {
      s_stats[step][i][kind] = make_stat(&s_sums[i][kind]);
    }
  }
  s_battery_stats[step] = make_stat(&s_battery_sum);
}

/* 吸引の強さを step 段目にする（0V なら止める。上げるときは、少しずつ上げる） */
static void set_suction(int step)
{
  if (NOISE_SUCTION_V[step] <= 0.0f) {
    fan_stop();
    return;
  }
  suction_ramp_up_to(NOISE_SUCTION_V[step]);
}

/* モード5 No.8：止まったまま、吸引を 4 段に変えて、壁センサのばらつきを測る（流れは先頭のコメント） */
void sensor_noise_run(void)
{
  ui_set_middle_leds(true);  /* 測っているあいだは、真ん中の 4 個を点ける（センサ値の記録と同じ） */
  for (int step = 0; step < NOISE_STEP_COUNT; step++) {
    set_suction(step);
    timebase_wait_ms(NOISE_WAIT_MS);
    measure_step(step);
  }
  fan_stop();
  ui_all_off();

  s_measured = true;
  sensor_noise_print();  /* ケーブルを挿したまま測ったときのために、すぐにも出す */
  ui_signal_ok();
}

/* 最後に測った表をシリアルに出す（形は先頭のコメント）。モード7 No.1（ログ出力）でも呼ぶ。まだ測っていなければ何も出さない */
void sensor_noise_print(void)
{
  if (!s_measured) {
    return;
  }
  uart_wait_free(LINE_LENGTH_MAX);
  printf("\r\n#BEGIN NOISE n=%d trace=0 settle_us=0 wait_ms=%d pwm_top=99\r\n", NOISE_COUNT, NOISE_WAIT_MS);
  for (int step = 0; step < NOISE_STEP_COUNT; step++) {
    for (int i = 0; i < NOISE_SENSOR_COUNT; i++) {
      for (int kind = 0; kind < NOISE_KIND_COUNT; kind++) {
        noise_stat_t stat = s_stats[step][i][kind];
        uart_wait_free(LINE_LENGTH_MAX);
        printf("stat,0,0,15,%d,%s,%s,%.1f,%.2f,%ld,%ld,0\r\n", NOISE_DUTY[step], SENSOR_NAME[i], KIND_NAME[kind],
               stat.mean, stat.sd, stat.min, stat.max);
      }
    }
    noise_stat_t battery = s_battery_stats[step];
    uart_wait_free(LINE_LENGTH_MAX);
    printf("batt,0,0,15,%d,%.1f,%.2f,%ld,%ld,0\r\n", NOISE_DUTY[step], battery.mean, battery.sd, battery.min, battery.max);
  }
  uart_wait_free(LINE_LENGTH_MAX);
  printf("#END NOISE\r\n");
}
