/*
 * app.c
 * App 全体の入口。main.c から app_init() と app_loop() が呼ばれる
 *
 * ToraNeko.Mk3 の App/app.c と同じ役目のファイル。ToraNeko.Mk3 と同じ順にステップを進め、そのたびに中身を足していく
 * （software/main-controller/README.md の「実装の進め方」）。
 *
 * ■ 今はステップ1 の確かめ（ToraNeko.Mk3 のステップ1 と同じ）
 *   - 起動したら、ブザーを短く鳴らし、シリアルに起動の文とマイコンのクロック（180MHz のはず）を出す
 *   - 表示 LED を LED1 → LED8 の順に 1 個ずつ点けていく（0.1 秒ごと）。壁の表示の LED も LEDF1 → LEDF4 の順に点ける
 *   - 1 秒ごとに、起動してからの時間と、送りきれずに捨てた文字の数（0 のはず）をシリアルに出す
 */
#include "app.h"

#include <stdio.h>

#include "drv/buzzer.h"
#include "drv/led.h"
#include "drv/timebase.h"
#include "drv/uart.h"

#define LED_STEP_MS   100   /* LED を次に進める間隔 [ms] */
#define PRINT_STEP_MS 1000  /* シリアルに時間を出す間隔 [ms] */
#define BOOT_BEEP_HZ  333   /* 起動したときに鳴らす音の高さ [Hz]（KOGUMA.MkIIl の決定の音と同じ高さ） */
#define BOOT_BEEP_MS  100   /* その長さ [ms] */

/* シリアルに出す文字は英数字だけにする（日本語は、表示するソフトによって文字化けするため。ToraNeko.Mk3 と同じ） */

static uint32_t s_led_last_ms = 0;    /* 最後に LED を進めた時刻 [ms] */
static uint32_t s_print_last_ms = 0;  /* 最後にシリアルに出した時刻 [ms] */
static uint8_t s_led_num = 1;         /* 次に点ける表示 LED の番号（1〜8） */
static uint8_t s_wall_led_num = 1;    /* 次に点ける壁の表示の LED の番号（1〜4） */

/* 起動時に 1 回だけ呼ばれる（周辺機能の初期化 MX_*_Init が終わったあと） */
void app_init(void)
{
  led_init();
  uart_init();
  timebase_init();
  buzzer_init();
  printf("\r\nKOGUMA.Mk3.1 boot (SYSCLK %lu Hz)\r\n", timebase_get_sysclk_hz());
  buzzer_beep(BOOT_BEEP_HZ, BOOT_BEEP_MS);
}

/* 表示 LED と壁の表示の LED を、1 個ずつ次に進める */
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

/* main() の while(1) から繰り返し呼ばれる */
void app_loop(void)
{
  uint32_t now_ms = timebase_get_ms();

  if (now_ms - s_led_last_ms >= LED_STEP_MS) {
    s_led_last_ms = now_ms;
    step_leds();
  }

  if (now_ms - s_print_last_ms >= PRINT_STEP_MS) {
    s_print_last_ms = now_ms;
    printf("t=%lu ms us=%lu dropped=%lu\r\n", now_ms, timebase_get_us(), uart_get_dropped_count());
  }
}
