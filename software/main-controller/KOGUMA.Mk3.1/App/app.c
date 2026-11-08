/*
 * app.c
 * App 全体の入口。main.c から app_init() と app_loop() が呼ばれる
 *
 * ToraNeko.Mk3 の App/app.c と同じ役目のファイル。ToraNeko.Mk3 と同じ順にステップを進め、そのたびに中身を足していく
 * （software/main-controller/README.md の「実装の進め方」）。
 *
 * ■ 今はステップ1〜6 の確かめ（実機で 1 つずつ確かめるための動き。ステップ9 でモードを選んで走らせる形にする）
 *   起動したとき：
 *     - 各部品を使えるようにして、エンコーダの磁石の状態・IMU の名前・電池の電圧をシリアルに出し、ブザーを短く鳴らす
 *     - 制御周期（2kHz）の割り込みを始める（ここから、エンコーダ・IMU・壁センサを 0.5ms ごとに読む）
 *     - ジャイロのゼロ点は、起動してから 0.5 秒で測るので、そのあいだは機体を動かさない
 *   そのあと：
 *     - 表示 LED と壁の表示の LED を、0.1 秒ごとに 1 個ずつ順に点けていく（ステップ1）
 *     - 1 秒ごとに、制御周期の記録（ステップ2）、エンコーダ（ステップ3）、IMU（ステップ4）、電池と壁センサ（ステップ6）を
 *       シリアルに出す
 *     - スイッチ S2 を押しているあいだだけ、左右のモーターに MOTOR_TEST_V をかける（ステップ5）。
 *       **タイヤを浮かせてから押す**（机の上で押すと走り出す）
 */
#include "app.h"

#include <stdbool.h>
#include <stdio.h>

#include "config/robot_params.h"
#include "drv/buzzer.h"
#include "drv/encoder.h"
#include "drv/fan.h"
#include "drv/imu.h"
#include "drv/led.h"
#include "drv/motor.h"
#include "drv/switch.h"
#include "drv/timebase.h"
#include "drv/uart.h"
#include "drv/wall.h"
#include "sys/scheduler.h"

#define LED_STEP_MS   100   /* LED を次に進める間隔 [ms] */
#define PRINT_STEP_MS 1000  /* シリアルに出す間隔 [ms] */
#define BOOT_BEEP_HZ  333   /* 起動したときに鳴らす音の高さ [Hz]（KOGUMA.MkIIl の決定の音と同じ高さ） */
#define BOOT_BEEP_MS  100   /* その長さ [ms] */
#define BATTERY_MEASURE_WAIT_MS 10  /* 電池の電圧を読む前に待つ時間 [ms]（制御周期 20 回ぶん。ToraNeko.Mk3 と同じ） */
#define MOTOR_TEST_V  1.0f  /* スイッチを押しているあいだモーターにかける電圧 [V]（ゆっくり回る。前に進む向きが +） */

/* シリアルに出す文字は英数字だけにする（日本語は、表示するソフトによって文字化けするため。ToraNeko.Mk3 と同じ） */

static uint32_t s_led_last_ms = 0;    /* 最後に LED を進めた時刻 [ms] */
static uint32_t s_print_last_ms = 0;  /* 最後にシリアルに出した時刻 [ms] */
static uint8_t s_led_num = 1;         /* 次に点ける表示 LED の番号（1〜8） */
static uint8_t s_wall_led_num = 1;    /* 次に点ける壁の表示の LED の番号（1〜4） */
static bool s_motor_on = false;       /* 今、モーターに電圧をかけているか */

/* 電池の電圧を読んで、シリアルに出す。下限より低ければ知らせる（ここでは止めない。止めるのはステップ9 で作る） */
static void check_battery(void)
{
  timebase_wait_ms(BATTERY_MEASURE_WAIT_MS);  /* 制御周期の割り込みで、電池の電圧を何回か測り終えるのを待つ */
  float battery_v = wall_get_battery_v();
  printf("Battery %.2f V\r\n", battery_v);
  if (battery_v < BATTERY_LOW_V) {
    printf("Battery low (below %.1f V)\r\n", BATTERY_LOW_V);
  }
}

/* 起動時に 1 回だけ呼ばれる（周辺機能の初期化 MX_*_Init が終わったあと） */
void app_init(void)
{
  led_init();
  uart_init();
  timebase_init();
  buzzer_init();
  printf("\r\nKOGUMA.Mk3.1 boot (SYSCLK %lu Hz)\r\n", timebase_get_sysclk_hz());
  wall_init();
  encoder_init();  /* 左右のエンコーダの磁石の状態をシリアルに出す */
  motor_init();
  fan_init();
  if (imu_init()) {
    printf("IMU OK (LSM6DSR)\r\n");
  } else {
    printf("IMU NG: WHO_AM_I=0x%02X (expected 0x6B)\r\n", imu_get_who_am_i());
  }
  scheduler_start();  /* ここから、制御周期ごとにエンコーダ・IMU・壁センサを読む。ジャイロのゼロ点は最初の 0.5 秒で測る */
  check_battery();
  buzzer_beep(BOOT_BEEP_HZ, BOOT_BEEP_MS);
}

/* 表示 LED と壁の表示の LED を、1 個ずつ次に進める（ステップ1） */
static void step_leds(void)
{
  led_all_off();
  led_set(s_led_num, true);
  led_set_wall(s_wall_led_num, true);

  s_led_num = s_led_num + 1;
  if (s_led_num > LED_COUNT) {
    s_led_num = 1;
  }
  s_wall_led_num = s_wall_led_num + 1;
  if (s_wall_led_num > WALL_LED_COUNT) {
    s_wall_led_num = 1;
  }
}

/* スイッチを押しているあいだだけ、モーターに電圧をかける（ステップ5） */
static void update_motor_test(void)
{
  bool pressed = switch_is_pressed();
  if (pressed && !s_motor_on) {
    motor_set_voltage(MOTOR_TEST_V, MOTOR_TEST_V);
    s_motor_on = true;
  }
  if (!pressed && s_motor_on) {
    motor_stop();
    s_motor_on = false;
  }
}

/* 1 秒ごとに、今の値をシリアルに出す（ステップ2〜6） */
static void print_status(void)
{
  scheduler_stats_t stats;
  scheduler_take_stats(&stats);
  printf("tick=%lu period=%lu-%lu us busy=%lu us\r\n", stats.tick_count, stats.period_min_us, stats.period_max_us,
         stats.busy_max_us);
  printf("enc total L=%ld R=%ld angle L=%u R=%u err L=%lu R=%lu\r\n", encoder_get_left_total(),
         encoder_get_right_total(), encoder_get_left_angle(), encoder_get_right_angle(),
         encoder_get_left_error_count(), encoder_get_right_error_count());
  printf("imu ready=%d gyro=%.1f dps angle=%.1f deg offset=%.2f dps\r\n", imu_is_offset_ready(), imu_get_gyro_z_dps(),
         imu_get_angle_deg(), imu_get_gyro_offset_dps());
  /* 壁センサは、点けたときの値 − 全部消したときの値。左前（1）と左（3）は組A、右（2）と右前（4）は組B で点ける（drv/wall.h） */
  printf("batt=%.2f V wall FL=%ld L=%ld R=%ld FR=%ld overrun=%lu motor=%d\r\n", wall_get_battery_v(),
         wall_get_a_minus_dark(1), wall_get_a_minus_dark(3), wall_get_b_minus_dark(2), wall_get_b_minus_dark(4),
         wall_get_overrun_count(), s_motor_on);
}

/* main() の while(1) から繰り返し呼ばれる */
void app_loop(void)
{
  uint32_t now_ms = timebase_get_ms();

  if (now_ms - s_led_last_ms >= LED_STEP_MS) {
    s_led_last_ms = now_ms;
    step_leds();
  }

  update_motor_test();

  if (now_ms - s_print_last_ms >= PRINT_STEP_MS) {
    s_print_last_ms = now_ms;
    print_status();
  }
}
