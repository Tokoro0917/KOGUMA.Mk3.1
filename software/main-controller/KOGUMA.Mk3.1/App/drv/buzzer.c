/*
 * buzzer.c
 * ブザー（TIM4 CH4 = PB9 の PWM）で音を鳴らす
 *
 * ToraNeko.Mk3 にはない（ToraNeko はブザーがない）。KOGUMA.MkIIl の UI.c（Buzzer_Enter、Buzzer_Finish など）の
 * 鳴らし方を、ToraNeko の書き方にしたもの。決定・スタート・走り終わりの曲などは、ステップ9で sys/ui.c に作る。
 *
 * ■ しくみ（KOGUMA.MkIIl と同じ）
 * TIM4 は CubeMX で 1MHz（1 カウント = 1µs）で数えるように設定してある（TIM4 のクロック 90MHz を 90 で割る）。
 *   - 周期（ARR）：周波数 f [Hz] の音は、1 周期が 1000000 ÷ f [µs]。数えるのは 0 からなので、ARR = 1000000 ÷ f − 1
 *   - High の長さ（CCR）：BUZZER_HIGH_US [µs]。音の大きさが決まる（KOGUMA.MkIIl と同じ 20µs）
 * 周期だけを変えて音の高さを変える。
 */
#include "drv/buzzer.h"

#include "main.h"
#include "tim.h"

#define BUZZER_TIMER_HZ 1000000  /* TIM4 が 1 秒に数える数（CubeMX の設定。PSC 90-1） */
#define BUZZER_HIGH_US  20       /* 1 周期のうち High にする長さ [µs]（音の大きさ。KOGUMA.MkIIl と同じ） */
#define BUZZER_MIN_HZ   20       /* これより低い音は鳴らさない（周期が 16 ビットのタイマーに入りきらないため） */

/* 起動時に呼ぶ。鳴らさずにおく */
void buzzer_init(void)
{
  buzzer_off();
}

/* 周波数 freq_hz [Hz] の音を鳴らし始める（止めるまで鳴り続ける） */
void buzzer_on(uint32_t freq_hz)
{
  if (freq_hz < BUZZER_MIN_HZ) {
    buzzer_off();
    return;
  }
  uint32_t period_count = BUZZER_TIMER_HZ / freq_hz;  /* 1 周期のカウント数 */

  /* 周期を変えるときは、カウンタも 0 に戻す（新しい周期より先まで数えていると、
   * 16 ビットの最後（65535）まで数えて 0 に戻るまで、音が途切れるため） */
  __HAL_TIM_SET_AUTORELOAD(&htim4, period_count - 1);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, BUZZER_HIGH_US);
  __HAL_TIM_SET_COUNTER(&htim4, 0);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);
}

/* 音を止める */
void buzzer_off(void)
{
  HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_4);
}

/* 周波数 freq_hz [Hz] の音を length_ms [ms] 鳴らして止める。鳴っているあいだは戻らない（待つ） */
void buzzer_beep(uint32_t freq_hz, uint32_t length_ms)
{
  buzzer_on(freq_hz);
  HAL_Delay(length_ms);
  buzzer_off();
}
