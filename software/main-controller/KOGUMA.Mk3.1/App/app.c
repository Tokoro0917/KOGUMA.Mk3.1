/*
 * app.c
 * App 全体の入口。main.c から app_init() と app_loop() が呼ばれる
 *
 * ToraNeko.Mk3 の App/app.c と同じ役目のファイル。ToraNeko.Mk3 と同じ順にステップを進め、そのたびに中身を足していく
 * （software/main-controller/README.md の「実装の進め方」）。
 *
 * ■ 今はステップ1〜7 の確かめ（実機で 1 つずつ確かめるための動き。ステップ9 でモードを選んで走らせる形にする）
 *   起動したとき：
 *     - 各部品を使えるようにして、エンコーダの磁石の状態・IMU の名前・電池の電圧をシリアルに出し、ブザーを短く鳴らす
 *     - 制御周期（2kHz）の割り込みを始める（ここから、エンコーダ・IMU・壁センサを 0.5ms ごとに読む）
 *     - ジャイロのゼロ点は、起動してから 0.5 秒で測るので、そのあいだは機体を動かさない
 *   そのあと：
 *     - 表示 LED を、0.1 秒ごとに 1 個ずつ順に点けていく（ステップ1）
 *     - 壁の表示の LED（LEDF1〜4）で、壁があるかを出す（ステップ6。KOGUMA.MkIIl の Wall_search_LED と同じ並び：
 *       LEDF1 = 左前、LEDF2 = 右、LEDF3 = 左、LEDF4 = 右前）
 *     - 1 秒ごとに、制御周期の記録（ステップ2）、エンコーダ（ステップ3）、IMU（ステップ4）、電池と壁センサ（ステップ6）を
 *       シリアルに出す
 *     - スイッチ S2 を押しているあいだだけ、左右のモーターに MOTOR_TEST_V をかける（ステップ5）。
 *       **タイヤを浮かせてから押す**（机の上で押すと走り出す）
 *     - 前センサに手をかざして離すと、1 秒後に直進のテストをする（ステップ7）：STRAIGHT_DISTANCE_MM を
 *       STRAIGHT_SPEED_MM_S で走って止まる（横の壁の補正は入れない）。走り終わったら、走った距離・向きのずれ・
 *       フェイルセーフで止まったわけを覚えておき、次に S2 を押したときに、それと走行ログをシリアルに出す
 *       （走るときはケーブルを挿していないため。ToraNeko.Mk3 のモード6 No.1 と同じ考え方）
 *     - 前センサに手を LONG_HAND_MS（2 秒）以上かざすと、ブザーが鳴る。そこで離すと、1 秒後にスラロームのテストをする
 *       （ステップ7。ctrl/run.c の探索の動き、吸引なし 500mm/s）：スタート区画から北へ 1 区画まっすぐ進み、
 *       右にスラロームで曲がって、その区画のまん中で止まる（スタート区画を (0, 0) として (1, 2) のまん中）。
 *       結果の出し方は直進のテストと同じ
 * ステップ9 でモード（sys/mode.c）を持ってきたら、この確かめの動きは、ToraNeko と同じくモードの中に移す。
 */
#include "app.h"

#include <stdbool.h>
#include <stdio.h>

#include "config/robot_params.h"
#include "ctrl/control.h"
#include "ctrl/odometry.h"
#include "ctrl/run.h"
#include "ctrl/trajectory.h"
#include "ctrl/wall_ctrl.h"
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
#include "sys/failsafe.h"
#include "sys/log.h"
#include "sys/scheduler.h"

#define LED_STEP_MS   100   /* LED を次に進める間隔 [ms] */
#define PRINT_STEP_MS 1000  /* シリアルに出す間隔 [ms] */
#define BOOT_BEEP_HZ  333   /* 起動したときに鳴らす音の高さ [Hz]（KOGUMA.MkIIl の決定の音と同じ高さ） */
#define BOOT_BEEP_MS  100   /* その長さ [ms] */
#define BATTERY_MEASURE_WAIT_MS 10  /* 電池の電圧を読む前に待つ時間 [ms]（制御周期 20 回ぶん。ToraNeko.Mk3 と同じ） */
#define MOTOR_TEST_V  1.0f  /* スイッチを押しているあいだモーターにかける電圧 [V]（ゆっくり回る。前に進む向きが +） */

/* 直進のテスト（ステップ7。ToraNeko.Mk3 のモード6 No.1 と同じ形。値は KOGUMA の探索の速さ） */
#define STRAIGHT_DISTANCE_MM    540.0f   /* 走る距離 [mm]（3 区画。スタート区画に置けば、4 区画目のまん中で止まる） */
#define STRAIGHT_SPEED_MM_S     500.0f   /* 最高速 [mm/s]（KOGUMA.MkIIl のスラローム探索の速さ） */
#define STRAIGHT_ACCEL_MM_S2    2000.0f  /* 加速度 [mm/s²]（仮。ゆっくりめ） */
#define STRAIGHT_LOG_INTERVAL_MS 2       /* 走行ログを残す間隔 [ms] */
#define START_DELAY_MS          1000     /* 手を離してから走り出すまで待つ時間 [ms]（ToraNeko と同じ） */
#define SETTLE_MS               100      /* 止まってから制御を続けて待つ時間 [ms]（ぴたりと止めるため） */
#define LONG_HAND_MS            2000     /* 前センサにこれより長く手をかざしたら、スラロームのテストにする [ms] */
#define LONG_HAND_BEEP_HZ       1000     /* スラロームのテストになったことを知らせる音の高さ [Hz] */

/* シリアルに出す文字は英数字だけにする（日本語は、表示するソフトによって文字化けするため。ToraNeko.Mk3 と同じ） */

static uint32_t s_led_last_ms = 0;    /* 最後に LED を進めた時刻 [ms] */
static uint32_t s_print_last_ms = 0;  /* 最後にシリアルに出した時刻 [ms] */
static uint8_t s_led_num = 1;         /* 次に点ける表示 LED の番号（1〜8） */
static bool s_motor_on = false;       /* 今、モーターに電圧をかけているか */
static bool s_hand_seen = false;      /* 前センサに手がかざされたのを見たか（離したら走る） */
static uint32_t s_hand_start_ms = 0;  /* 手がかざされた時刻 [ms] */
static bool s_long_hand = false;      /* 手が LONG_HAND_MS より長くかざされたか（離したらスラロームのテスト） */
static bool s_result_ready = false;   /* テストの結果があって、まだ出していないか */
static bool s_result_is_slalom = false;    /* その結果がスラロームのテストか（false なら直進のテスト） */
static float s_result_distance_mm = 0.0f;  /* テストで、エンコーダで測った走った距離 [mm] */
static float s_result_angle_deg = 0.0f;    /* テストで、ジャイロで測った向きの変わった量 [deg] */
static int s_result_failsafe = 0;          /* テストで、フェイルセーフで止まったわけ（ctrl/control.h の番号。0 なら止まっていない） */

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
  odometry_init();
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

/* 表示 LED を 1 個ずつ次に進める（ステップ1） */
static void step_leds(void)
{
  for (uint8_t num = 1; num <= LED_COUNT; num++) {
    led_set(num, false);
  }
  led_set(s_led_num, true);

  s_led_num = s_led_num + 1;
  if (s_led_num > LED_COUNT) {
    s_led_num = 1;
  }
}

/* 壁の表示の LED で、壁があるかを出す（ステップ6）。前の壁は左前・右前の両方のしきい値で決まるので、
 * 左前・右前の LED は、それぞれのセンサが前の壁を見ているか（距離が区画のまん中の前の壁より近いか）で点ける */
static void show_walls(void)
{
  bool front = wall_ctrl_has_wall_front();
  led_set_wall(1, front);                       /* 左前 */
  led_set_wall(2, wall_ctrl_has_wall_right());  /* 右 */
  led_set_wall(3, wall_ctrl_has_wall_left());   /* 左 */
  led_set_wall(4, front);                       /* 右前 */
}

/* 直進のテストをする（ステップ7）。走り終わるか、フェイルセーフで止まるまで戻らない */
static void run_straight_test(void)
{
  timebase_wait_ms(START_DELAY_MS);
  imu_remeasure_offset();  /* ジャイロのゼロ点を測り直す（約 0.5 秒。ToraNeko と同じ） */
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();
  failsafe_clear();

  log_start(STRAIGHT_LOG_INTERVAL_MS);
  control_set_wall_control(false);  /* 横の壁に沿って走る補正は入れない */
  control_start();
  trajectory_start_straight(STRAIGHT_DISTANCE_MM, STRAIGHT_SPEED_MM_S, 0.0f, STRAIGHT_ACCEL_MM_S2);
  while (!trajectory_is_done() && !failsafe_is_stopped()) {
    /* 走り終わるのを待つ（制御は割り込みの中で進む） */
  }
  if (!failsafe_is_stopped()) {
    timebase_wait_ms(SETTLE_MS);  /* 目標の速さ 0 のまま制御を続けて、ぴたりと止める */
  }
  control_stop();
  log_stop();
  motor_stop();

  s_result_distance_mm = odometry_get_distance_mm() - start_distance_mm;
  s_result_angle_deg = imu_get_angle_deg() - start_angle_deg;
  s_result_failsafe = control_get_failsafe_reason();
  s_result_is_slalom = false;
  s_result_ready = true;
  buzzer_beep(BOOT_BEEP_HZ, BOOT_BEEP_MS);  /* 走り終わった合図 */
}

/* スラロームのテストをする（ステップ7）。探索の動き（ctrl/run.c）で、北へ 1 区画進んでから右に曲がり、
 * その区画のまん中で止まる。走り終わるか、フェイルセーフで止まるまで戻らない */
static void run_slalom_test(void)
{
  timebase_wait_ms(START_DELAY_MS);
  float start_distance_mm = odometry_get_distance_mm();
  float start_angle_deg = imu_get_angle_deg();
  failsafe_clear();

  run_search_set_suction(false);  /* 吸引なしの探索の値（500mm/s） */
  run_search_start();             /* ジャイロのゼロ点を測り直し、制御と走行ログを始めて、(0, 1) の入口へ */
  run_search_straight();          /* (0, 2) の入口へ */
  run_search_turn_right();        /* 右に曲がって (1, 2) の入口へ */
  run_search_stop_at_center();    /* (1, 2) のまん中で止まる */
  control_stop();                 /* フェイルセーフで止まったときも、制御を止める */
  log_stop();
  motor_stop();

  s_result_distance_mm = odometry_get_distance_mm() - start_distance_mm;
  s_result_angle_deg = imu_get_angle_deg() - start_angle_deg;
  s_result_failsafe = control_get_failsafe_reason();
  s_result_is_slalom = true;
  s_result_ready = true;
  buzzer_beep(BOOT_BEEP_HZ, BOOT_BEEP_MS);  /* 走り終わった合図 */
}

/* 前センサに手をかざして離したら、テストをする。LONG_HAND_MS より長くかざしていたらスラロームのテスト、
 * 短ければ直進のテスト。長くなったところでブザーを鳴らして知らせる */
static void update_hand_tests(void)
{
  if (wall_ctrl_is_hand_front()) {
    if (!s_hand_seen) {
      s_hand_seen = true;
      s_hand_start_ms = timebase_get_ms();
      s_long_hand = false;
    }
    if (!s_long_hand && timebase_get_ms() - s_hand_start_ms >= LONG_HAND_MS) {
      s_long_hand = true;
      buzzer_beep(LONG_HAND_BEEP_HZ, BOOT_BEEP_MS);
    }
    return;
  }
  if (s_hand_seen) {
    s_hand_seen = false;
    if (s_long_hand) {
      run_slalom_test();
    } else {
      run_straight_test();
    }
  }
}

/* テストの結果と走行ログをシリアルに出す（S2 を押したとき）。
 * スラロームのテストの距離の目安：発進 114mm（24 + 90）+ 1 区画 180mm + 前距離・スラローム・後距離 + まん中まで 90mm で、約 542mm */
static void print_test_result(void)
{
  if (s_result_is_slalom) {
    printf("RESULT slalom encoder=%.1f mm (about 542) angle=%.2f deg (-90) failsafe=%d (0 none, 1 position, 2 angle, 3 slow, 4 stall)\r\n",
           s_result_distance_mm, s_result_angle_deg, s_result_failsafe);
  } else {
    printf("RESULT straight target=%.1f mm encoder=%.1f mm angle=%.2f deg failsafe=%d (0 none, 1 position, 2 angle, 3 slow, 4 stall)\r\n",
           STRAIGHT_DISTANCE_MM, s_result_distance_mm, s_result_angle_deg, s_result_failsafe);
  }
  log_print();
  s_result_ready = false;
}

/* スイッチを押しているあいだだけ、モーターに電圧をかける（ステップ5）。
 * テストの結果がまだ出ていなければ、押したときに結果を出す（そのときはモーターは回さない） */
static void update_motor_test(void)
{
  bool pressed = switch_is_pressed();
  if (pressed && s_result_ready) {
    print_test_result();
    while (switch_is_pressed()) {
      /* 離すまで待つ（続けてモーターが回らないように） */
    }
    return;
  }
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
  /* 壁センサは、点けたときの値 − 全部消したときの値。左前（1）と右（2）は組A、左（3）と右前（4）は組B で点ける（drv/wall.h） */
  printf("batt=%.2f V wall FL=%ld L=%ld R=%ld FR=%ld overrun=%lu motor=%d\r\n", wall_get_battery_v(),
         wall_get_a_minus_dark(1), wall_get_b_minus_dark(3), wall_get_a_minus_dark(2), wall_get_b_minus_dark(4),
         wall_get_overrun_count(), s_motor_on);
  /* 壁までの距離 [mm]（区画のまん中で 84mm）と、壁があるか（ctrl/wall_ctrl.c のしきい値。ステップ6） */
  printf("mm FL=%.0f L=%.0f R=%.0f FR=%.0f wall front=%d left=%d right=%d hand=%d\r\n", wall_ctrl_get_front_left_mm(),
         wall_ctrl_get_left_mm(), wall_ctrl_get_right_mm(), wall_ctrl_get_front_right_mm(), wall_ctrl_has_wall_front(),
         wall_ctrl_has_wall_left(), wall_ctrl_has_wall_right(), wall_ctrl_is_hand_front());
}

/* main() の while(1) から繰り返し呼ばれる */
void app_loop(void)
{
  uint32_t now_ms = timebase_get_ms();

  if (now_ms - s_led_last_ms >= LED_STEP_MS) {
    s_led_last_ms = now_ms;
    step_leds();
    show_walls();
  }

  update_motor_test();
  update_hand_tests();

  if (now_ms - s_print_last_ms >= PRINT_STEP_MS) {
    s_print_last_ms = now_ms;
    print_status();
  }
}
