/*
 * sensor_view.c
 * センサ値の表示（モード5 No.1）、壁までの距離の表示（モード5 No.3）、部品の確かめ（モード5 No.7）。MODES.md
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/sensor_view.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - 壁の有無を出す LED を、8 個の並びに合わせた（左の壁 LED1・2、前の壁 LED3〜6、右の壁 LED7・8）。
 *     壁の表示の LED（LEDF1〜4。ToraNeko.Mk3 にはない）にも出す（KOGUMA.MkIIl の Wall_search_LED と同じ並び）
 *   - 部品の確かめ（モード5 No.7）を足した。ステップ1〜7 の確かめで app.c が 1 秒ごとに出していたものと同じ
 *     （制御周期の記録、エンコーダ、IMU、電池、壁センサ、壁までの距離）
 *
 * どれも、スイッチが押されるまで続ける。
 *   - センサ値の表示：0.2 秒ごとに、壁センサ 4 つ（ON−OFF）、電池の電圧、角速度をシリアルに出す
 *     書式は KOGUMA のモード4 No.0 と同じ（ビューアの流れるグラフで見られる）
 *   - 壁までの距離の表示：0.2 秒ごとに、センサの値と壁までの距離 [mm] を出す（KOGUMA のモード3 No.4 と同じ書式）
 *   - 部品の確かめ：1 秒ごとに、部品ごとの値を 5 行出す
 */
#include "sys/sensor_view.h"

#include <stdio.h>

#include "ctrl/wall_ctrl.h"
#include "drv/encoder.h"
#include "drv/imu.h"
#include "drv/led.h"
#include "drv/timebase.h"
#include "drv/wall.h"
#include "sys/scheduler.h"
#include "sys/ui.h"

#define PRINT_INTERVAL_MS 200        /* センサ値・距離をシリアルに出す間隔 [ms] */
#define PARTS_PRINT_INTERVAL_MS 1000 /* 部品の確かめを出す間隔 [ms] */

/* 壁の有無を LED に出す。左前・右前の壁の表示の LED は、前の壁があるかで点ける */
static void show_walls(void)
{
  bool left = wall_ctrl_has_wall_left();
  bool front = wall_ctrl_has_wall_front();
  bool right = wall_ctrl_has_wall_right();
  led_set(1, left);
  led_set(2, left);
  led_set(3, front);
  led_set(4, front);
  led_set(5, front);
  led_set(6, front);
  led_set(7, right);
  led_set(8, right);
  led_set_wall(1, front);  /* LEDF1：左前 */
  led_set_wall(2, right);  /* LEDF2：右 */
  led_set_wall(3, left);   /* LEDF3：左 */
  led_set_wall(4, front);  /* LEDF4：右前 */
}

/* 壁の表示の LED（LEDF1〜4）を全部消す */
static void wall_leds_off(void)
{
  for (uint8_t num = 1; num <= WALL_LED_COUNT; num++) {
    led_set_wall(num, false);
  }
}

/* センサの値を 1 行出す（KOGUMA と同じ書式） */
static void print_sensors(void)
{
  printf("FL = %4ld // L = %4ld // R = %4ld //FR = %4ld //BATT = %.2f//GyroZ = %.3f\n\r",
         wall_ctrl_get_front_left(), wall_ctrl_get_left(),
         wall_ctrl_get_right(), wall_ctrl_get_front_right(),
         wall_get_battery_v(), imu_get_gyro_z_dps());
}

/* センサの値と、壁までの距離 [mm] を 1 行出す。
 * 書式は KOGUMA のモード3 No.4 と同じ CAL,左前,左,右,右前,左前mm,左mm,右mm,右前mm（ビューアの流れるグラフで見られる） */
static void print_distances(void)
{
  printf("CAL,%ld,%ld,%ld,%ld,%.1f,%.1f,%.1f,%.1f\r\n",
         wall_ctrl_get_front_left(), wall_ctrl_get_left(),
         wall_ctrl_get_right(), wall_ctrl_get_front_right(),
         wall_ctrl_get_front_left_mm(), wall_ctrl_get_left_mm(),
         wall_ctrl_get_right_mm(), wall_ctrl_get_front_right_mm());
}

/* 部品ごとの値を 5 行出す（部品の確かめ。software/main-controller/README.md の「確かめ方」） */
static void print_parts(void)
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
  /* 壁センサは、点けたときの値 − 全部消したときの値。左前（1）と右（2）は組A、左（3）と右前（4）は組B で点ける（drv/wall.h） */
  printf("batt=%.2f V wall FL=%ld L=%ld R=%ld FR=%ld overrun=%lu\r\n", wall_get_battery_v(),
         wall_get_a_minus_dark(1), wall_get_b_minus_dark(3), wall_get_a_minus_dark(2), wall_get_b_minus_dark(4),
         wall_get_overrun_count());
  /* 壁までの距離 [mm]（区画のまん中で 84mm）と、壁があるか（ctrl/wall_ctrl.c のしきい値） */
  printf("mm FL=%.0f L=%.0f R=%.0f FR=%.0f wall front=%d left=%d right=%d hand=%d\r\n", wall_ctrl_get_front_left_mm(),
         wall_ctrl_get_left_mm(), wall_ctrl_get_right_mm(), wall_ctrl_get_front_right_mm(), wall_ctrl_has_wall_front(),
         wall_ctrl_has_wall_left(), wall_ctrl_has_wall_right(), wall_ctrl_is_hand_front());
}

/* 表示を終えるときに、LED を全部消す */
static void view_end(void)
{
  ui_all_off();
  wall_leds_off();
}

/* モード5 No.1：スイッチが押されるまで、センサの値をシリアルと LED に出し続ける */
void sensor_view_run(void)
{
  uint32_t next_print_ms = timebase_get_ms();
  while (!ui_switch_pushed()) {
    show_walls();
    if (timebase_get_ms() >= next_print_ms) {
      print_sensors();
      next_print_ms = next_print_ms + PRINT_INTERVAL_MS;
    }
    ui_wait_ms(UI_POLL_MS);
  }
  view_end();
}

/* モード5 No.3：スイッチが押されるまで、センサの値と壁までの距離 [mm] をシリアルに、壁の有無を LED に出し続ける。
 * 表（ctrl/wall_dist.c）を入れたあと、ものさしで測った距離と同じになるかを確かめるのに使う */
void sensor_view_distance_run(void)
{
  uint32_t next_print_ms = timebase_get_ms();
  while (!ui_switch_pushed()) {
    show_walls();
    if (timebase_get_ms() >= next_print_ms) {
      print_distances();
      next_print_ms = next_print_ms + PRINT_INTERVAL_MS;
    }
    ui_wait_ms(UI_POLL_MS);
  }
  view_end();
}

/* モード5 No.7：スイッチが押されるまで、部品ごとの値を 1 秒ごとに出し続ける（エンコーダ・IMU・電池・壁センサの確かめ） */
void sensor_view_parts_run(void)
{
  uint32_t next_print_ms = timebase_get_ms();
  while (!ui_switch_pushed()) {
    show_walls();
    if (timebase_get_ms() >= next_print_ms) {
      print_parts();
      next_print_ms = next_print_ms + PARTS_PRINT_INTERVAL_MS;
    }
    ui_wait_ms(UI_POLL_MS);
  }
  view_end();
}
