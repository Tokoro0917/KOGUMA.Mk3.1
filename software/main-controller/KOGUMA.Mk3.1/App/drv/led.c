/*
 * led.c
 * LED の点灯・消灯
 *   - 表示 LED（LED1〜LED8）：モードの番号や合図を出す
 *   - 壁の表示の LED（LEDF1〜LEDF4）：壁があるかを出す（KOGUMA.MkIIl の Wallsensor.c で使っていたもの）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/drv/led.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 * 表示 LED を 7 個から 8 個にした。壁の表示の LED を点ける led_set_wall() を足した。
 *
 * ピン名（LED1_GPIO_Port、LED1_Pin など）は CubeMX で付けたラベルで、main.h に定義されている。
 * どの番号の LED が基板のどこにあるかは、ステップ1で実機を見て確かめる（回路図で確認）。
 */
#include "drv/led.h"

#include "main.h"

/* on が true なら点灯（High）、false なら消灯（Low） */
static GPIO_PinState to_pin_state(bool on)
{
  if (on) {
    return GPIO_PIN_SET;
  }
  return GPIO_PIN_RESET;
}

/* 起動時に呼ぶ。全部消しておく */
void led_init(void)
{
  led_all_off();
}

/* 表示 LED を 1 個点ける／消す。num は基板の LED 番号（1〜8）。on が true なら点灯 */
void led_set(uint8_t num, bool on)
{
  GPIO_PinState state = to_pin_state(on);

  if (num == 1) {
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, state);
  } else if (num == 2) {
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, state);
  } else if (num == 3) {
    HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, state);
  } else if (num == 4) {
    HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, state);
  } else if (num == 5) {
    HAL_GPIO_WritePin(LED5_GPIO_Port, LED5_Pin, state);
  } else if (num == 6) {
    HAL_GPIO_WritePin(LED6_GPIO_Port, LED6_Pin, state);
  } else if (num == 7) {
    HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, state);
  } else if (num == 8) {
    HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, state);
  }
  /* 1〜8 以外は何もしない */
}

/* 壁の表示の LED を 1 個点ける／消す。num は LEDF の番号（1〜4）。on が true なら点灯 */
void led_set_wall(uint8_t num, bool on)
{
  GPIO_PinState state = to_pin_state(on);

  if (num == 1) {
    HAL_GPIO_WritePin(LEDF1_GPIO_Port, LEDF1_Pin, state);
  } else if (num == 2) {
    HAL_GPIO_WritePin(LEDF2_GPIO_Port, LEDF2_Pin, state);
  } else if (num == 3) {
    HAL_GPIO_WritePin(LEDF3_GPIO_Port, LEDF3_Pin, state);
  } else if (num == 4) {
    HAL_GPIO_WritePin(LEDF4_GPIO_Port, LEDF4_Pin, state);
  }
  /* 1〜4 以外は何もしない */
}

/* 全部の LED（表示 LED と、壁の表示の LED）を消す */
void led_all_off(void)
{
  for (uint8_t num = 1; num <= LED_COUNT; num++) {
    led_set(num, false);
  }
  for (uint8_t num = 1; num <= WALL_LED_COUNT; num++) {
    led_set_wall(num, false);
  }
}
