/*
 * ui.c
 * 機体の操作：LED とブザーの表示と合図、タイヤ・スイッチ・前センサの読み取り（MODES.md の 1〜3）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/ui.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - LED が 8 個なので、並びを KOGUMA.MkIIl と同じにした（下の図。KOGUMA.MkIIl の UI.c の LED_program_mode・LED_program_number）
 *   - 合図にブザーの音を足した（ToraNeko.Mk3 にはブザーがない。音の高さは KOGUMA.MkIIl の UI.c と同じ）。
 *     音は、LED を点けている・待っているあいだに鳴らして止めるので、合図にかかる時間は ToraNeko.Mk3 と同じ
 *   - 「外側の 4 個」は LED1・2・7・8、「真ん中」は LED3〜6
 *   - タイヤ 1 段の回す量は、KOGUMA.MkIIl と同じ 300 カウント
 *
 * ■ LED の並び（左から LED1〜LED8。回路図・基板で確かめる）
 *
 *      LED1  LED2  LED3  LED4    LED5  LED6  LED7  LED8
 *      └──── モード（8・4・2・1）┘    └──── No.（8・4・2・1）──┘
 *
 *   モードは 0〜7（ToraNeko.Mk3 と同じ並び）なので、LED1 はモードの表示では点かない。
 *
 * ■ 入力の読み方
 *   - タイヤ：エンコーダの「回った量」の差を足していき、UI_TIRE_COUNT_PER_STEP ごとに 1 段とする
 *   - スイッチ：押した瞬間に 1 回だけ「押された」と答える。2 回続けて同じ値を読んだときだけ変わったとみなし、
 *               チャタリング（押した瞬間に入ったり切れたりすること）を取り除く
 *   - 前センサの手：HAND_STABLE_COUNT 回続けて同じ値を読んだときだけ、かざした／離したとみなす
 */
#include "sys/ui.h"

#include "ctrl/wall_ctrl.h"
#include "drv/buzzer.h"
#include "drv/encoder.h"
#include "drv/led.h"
#include "drv/switch.h"
#include "drv/timebase.h"

#define BLINK_MS           150   /* 全部を点滅させるときの、点けている時間・消している時間 [ms] */
#define WAIT_BLINK_MS      250   /* スタート待ちで、モードと No. を点滅させる間隔 [ms] */
#define SIGNAL_OK_MS       1000  /* 「うまくいった」で全部を点けておく時間 [ms] */
#define SIGNAL_ERROR_MS    80    /* 「できなかった」で外側の 4 個を速く点滅させる間隔 [ms] */
#define SIGNAL_ERROR_TIMES 5     /* 「できなかった」の点滅の回数 */
#define START_DELAY_MS     1000  /* 手を離してから走り出すまで [ms] */
#define HAND_STABLE_COUNT  3     /* 手をかざした／離したとみなすまでに、同じ値が続く回数（UI_POLL_MS ごと） */
#define SWITCH_START_MS    1000  /* スタート待ちで、スイッチをこれ以上押し続けたら、手のかわりにスタートする [ms] */
#define FAILSAFE_BLINK_MS  500   /* フェイルセーフで止まったときに、全部を点滅させる間隔 [ms] */
#define LOADING_STEP_MS    125   /* 「ロード中」で、LED を 1 個ずつ点けていく間隔 [ms]（8 個で 1 秒） */

/* ---- 音（KOGUMA.MkIIl の UI.c と同じ高さ。ブザーは drv/buzzer.c） ---- */
#define DECIDE_TONE_HZ      333   /* 決定（KOGUMA の Buzzer_Enter：AUTORELOAD 3000 → 1MHz ÷ 3000） */
#define START_TONE_LOW_HZ   333   /* 走り出す前の音の 1 つ目（KOGUMA の Buzzer_Start：3000 → 1500） */
#define START_TONE_HIGH_HZ  667   /* 2 つ目 */
#define START_TONE_MS       400   /* 1 つの音の長さ [ms]（2 つで 0.8 秒。走り出すまでの 1 秒の中で鳴らす） */
#define OK_TONE_HZ          2000  /* うまくいった（KOGUMA にはない。短く高い音） */
#define OK_TONE_MS          100
#define ERROR_TONE_HZ       167   /* できなかった（KOGUMA の LED_Path_Error：AUTORELOAD 6000。低い音） */
#define LOW_BATTERY_TONE_HZ 167   /* 電池が少ない（できなかったと同じ低い音を、点滅に合わせて鳴らし続ける） */

/* タイヤ：前に読んだときの回った量と、まだ 1 段にならない回転量 [カウント] */
static int32_t s_left_last_count = 0;
static int32_t s_left_rest = 0;
static int32_t s_right_last_count = 0;
static int32_t s_right_rest = 0;

/* スイッチ：前に読んだ値と、チャタリングを取り除いたあとの状態（true なら押されている） */
static bool s_switch_last_read = false;
static bool s_switch_state = false;

/* 起動時に 1 回呼ぶ。LED を消し、今のタイヤとスイッチの状態を覚える */
void ui_init(void)
{
  led_all_off();
  ui_forget_tire_rotation();
  s_switch_last_read = switch_is_pressed();
  s_switch_state = s_switch_last_read;
}

/* ---------------------------------------------------------------------------
 * LED の表示
 * ------------------------------------------------------------------------- */

/* 2 進数の 1 桁を LED に出す。残りの値 rest がその桁の重み weight 以上なら LED を点けて、
 * 重みを引いた残りを返す。小さければ LED を消して、rest をそのまま返す（上の桁から順に呼ぶ） */
static int show_digit(uint8_t led_num, int rest, int weight)
{
  if (rest >= weight) {
    led_set(led_num, true);
    return rest - weight;
  }
  led_set(led_num, false);
  return rest;
}

/* モード（0〜7）を LED1〜4 に、No.（0〜15）を LED5〜8 に、2 進数で出す（KOGUMA.MkIIl と同じ） */
void ui_show_mode_number(int mode, int number)
{
  int rest = mode;
  rest = show_digit(1, rest, 8);
  rest = show_digit(2, rest, 4);
  rest = show_digit(3, rest, 2);
  show_digit(4, rest, 1);

  rest = number;
  rest = show_digit(5, rest, 8);
  rest = show_digit(6, rest, 4);
  rest = show_digit(7, rest, 2);
  show_digit(8, rest, 1);
}

/* 0〜15 の値を、LED5〜8 に 2 進数で出す（No. と同じ場所）。ゴールの位置を入れるときに使う。LED1〜4 は消す
 * （迷路は 16×16 なので、ゴールの左下の x・y は 0〜14。ToraNeko.Mk3 は 32×32 なので 0〜31 を 5 個で出していた） */
void ui_show_value(int value)
{
  led_set(1, false);
  led_set(2, false);
  led_set(3, false);
  led_set(4, false);
  int rest = value;
  rest = show_digit(5, rest, 8);
  rest = show_digit(6, rest, 4);
  rest = show_digit(7, rest, 2);
  show_digit(8, rest, 1);
}

/* 8 個全部を点ける */
void ui_all_on(void)
{
  for (uint8_t num = 1; num <= LED_COUNT; num++) {
    led_set(num, true);
  }
}

/* 8 個全部を消す */
void ui_all_off(void)
{
  led_all_off();
}

/* 外側の 4 個（LED1・LED2・LED7・LED8）を点ける／消す */
void ui_set_outer_leds(bool on)
{
  led_set(1, on);
  led_set(2, on);
  led_set(7, on);
  led_set(8, on);
}

/* 真ん中の 4 個（LED3〜LED6）を点ける／消す（センサ値の記録・吸引中の表示） */
void ui_set_middle_leds(bool on)
{
  led_set(3, on);
  led_set(4, on);
  led_set(5, on);
  led_set(6, on);
}

/* ---------------------------------------------------------------------------
 * 合図
 * ------------------------------------------------------------------------- */

/* 8 個全部を times 回点滅させる（決定したとき 1 回、ゴールの y の入力に移るとき 2 回）。
 * 点滅するあいだ（1 回 0.3 秒）、決定の音を鳴らす（KOGUMA の Buzzer_Enter は 0.3 秒） */
void ui_blink_all(int times)
{
  for (int i = 0; i < times; i++) {
    buzzer_on(DECIDE_TONE_HZ);
    ui_all_on();
    ui_wait_ms(BLINK_MS);
    ui_all_off();
    ui_wait_ms(BLINK_MS);
    buzzer_off();
  }
}

/* 「うまくいった」：8 個全部を 1 秒点ける。はじめに短く高い音を鳴らす */
void ui_signal_ok(void)
{
  ui_all_on();
  buzzer_on(OK_TONE_HZ);
  ui_wait_ms(OK_TONE_MS);
  buzzer_off();
  ui_wait_ms(SIGNAL_OK_MS - OK_TONE_MS);
  ui_all_off();
}

/* 「できなかった」：外側の 4 個を速く 5 回点滅させる。点けているあいだ、低い音を鳴らす */
void ui_signal_error(void)
{
  ui_all_off();
  for (int i = 0; i < SIGNAL_ERROR_TIMES; i++) {
    ui_set_outer_leds(true);
    buzzer_on(ERROR_TONE_HZ);
    ui_wait_ms(SIGNAL_ERROR_MS);
    ui_set_outer_leds(false);
    buzzer_off();
    ui_wait_ms(SIGNAL_ERROR_MS);
  }
}

/* 時刻 until_ms [ms] になるまで待つ。待っているあいだに押されたスイッチは読み捨てる（あとで「押された」と答えないようにする）。
 * 待つ回数ではなく時刻で決めるのは、実機の 1 回の待ち（HAL_Delay）が少し長くなることがあり、回数で数えるとずれがたまるため */
static void wait_until_ignoring_switch(uint32_t until_ms)
{
  while (timebase_get_ms() < until_ms) {
    ui_switch_pushed();  /* 読むだけで、答えは使わない */
    ui_wait_ms(UI_POLL_MS);
  }
}

/* 「ロード中」：LED1 から LED8 へ 1 個ずつ順に点けていき、全部点いたら消す（1 秒。KOGUMA.MkIIl の LED_Loading と同じ）。
 * このあいだはスイッチを押しても何も起きない（続けて押したときに、次の決定にならないようにする） */
void ui_show_loading(void)
{
  ui_all_off();
  uint32_t next_ms = timebase_get_ms();
  for (uint8_t num = 1; num <= LED_COUNT; num++) {
    led_set(num, true);
    next_ms = next_ms + LOADING_STEP_MS;
    wait_until_ignoring_switch(next_ms);
  }
  ui_all_off();
}

/* ---------------------------------------------------------------------------
 * 入力
 * ------------------------------------------------------------------------- */

/* これまでにタイヤを回した量を忘れる。モードを選び始めるときに呼ぶ
 * （走っているあいだにタイヤが回った量で、モードや No. が変わらないようにするため） */
void ui_forget_tire_rotation(void)
{
  s_left_last_count = encoder_get_left_total();
  s_left_rest = 0;
  s_right_last_count = encoder_get_right_total();
  s_right_rest = 0;
}

/* 回転量 rest [カウント] が何段ぶんか。前に回したら正、後ろに回したら負。
 * 1 段に足りない分は数えない（C の割り算は、0 の方へ切り捨てる。例：-350 / 300 = -1） */
static int steps_in(int32_t rest)
{
  return rest / UI_TIRE_COUNT_PER_STEP;
}

/* 前に読んでから、左タイヤが何段回ったか（前に回したら正、後ろに回したら負） */
int ui_read_left_steps(void)
{
  int32_t count = encoder_get_left_total();
  s_left_rest = s_left_rest + (count - s_left_last_count);
  s_left_last_count = count;

  int steps = steps_in(s_left_rest);
  s_left_rest = s_left_rest - steps * UI_TIRE_COUNT_PER_STEP;
  return steps;
}

/* 前に読んでから、右タイヤが何段回ったか（前に回したら正、後ろに回したら負） */
int ui_read_right_steps(void)
{
  int32_t count = encoder_get_right_total();
  s_right_rest = s_right_rest + (count - s_right_last_count);
  s_right_last_count = count;

  int steps = steps_in(s_right_rest);
  s_right_rest = s_right_rest - steps * UI_TIRE_COUNT_PER_STEP;
  return steps;
}

/* スイッチが押された瞬間なら true（押しっぱなしでも、1 回押すごとに 1 回だけ true）。
 * UI_POLL_MS ごとに呼ぶ。2 回続けて同じ値を読んだときだけ、状態が変わったとみなす（チャタリングを取り除く） */
bool ui_switch_pushed(void)
{
  bool now_read = switch_is_pressed();
  bool pushed = false;

  if (now_read == s_switch_last_read && now_read != s_switch_state) {
    s_switch_state = now_read;
    if (s_switch_state) {
      pushed = true;  /* 離れた状態から、押された状態に変わった */
    }
  }
  s_switch_last_read = now_read;
  return pushed;
}

/* wait_ms [ms] 待つ */
void ui_wait_ms(uint32_t wait_ms)
{
  timebase_wait_ms(wait_ms);
}

/* ---------------------------------------------------------------------------
 * 待つ
 * ------------------------------------------------------------------------- */

/* 前センサに手がかざされる（want_hand が true）、または手が離れる（false）まで待つ。
 * HAND_STABLE_COUNT 回続けて同じ値を読んだら、そうなったとみなす */
static void wait_hand(bool want_hand)
{
  int stable_count = 0;
  while (stable_count < HAND_STABLE_COUNT) {
    if (wall_ctrl_is_hand_front() == want_hand) {
      stable_count = stable_count + 1;
    } else {
      stable_count = 0;
    }
    ui_wait_ms(UI_POLL_MS);
  }
}

/* スイッチが離されるまで待つ（離されたかは ui_switch_pushed() が覚えている s_switch_state で見る） */
static void wait_switch_released(void)
{
  while (s_switch_state) {
    ui_switch_pushed();  /* 読むと、押されているか（s_switch_state）が新しくなる */
    ui_wait_ms(UI_POLL_MS);
  }
}

/* 全部消して、走り出す前の音（KOGUMA の Buzzer_Start：低い音 → 高い音）を鳴らしながら、1 秒待つ */
static void wait_before_start(void)
{
  ui_all_off();
  buzzer_on(START_TONE_LOW_HZ);
  ui_wait_ms(START_TONE_MS);
  buzzer_on(START_TONE_HIGH_HZ);
  ui_wait_ms(START_TONE_MS);
  buzzer_off();
  ui_wait_ms(START_DELAY_MS - 2 * START_TONE_MS);
}

/* スタート待ち（MODES.md の 2 の 3〜5）。走り出してよくなったら true、選び直すなら false。
 *   1. 選んだモードと No. を点滅させて、前センサに手がかざされるのを待つ。
 *      スイッチを短く押して離したら false（選び直し）。SWITCH_START_MS 以上押し続けたら、手のかわりにスタートする
 *      （区画のまん中で前に壁があると、前センサは壁を見ていて、手をかざして離したのがわからない）
 *   2. 手がかざされたら（スイッチを押し続けたら）8 個全部を点けて、手が離れる（スイッチが離される）のを待つ
 *   3. 全部消して、走り出す前の音を鳴らしながら 1 秒待ってから true（このあいだに、走り出す処理がジャイロの 0 点を測り直す） */
bool ui_wait_start(int mode, int number)
{
  bool lit = true;
  uint32_t next_toggle_ms = timebase_get_ms() + WAIT_BLINK_MS;
  int hand_count = 0;
  bool switch_held = false;      /* スイッチが押されているあいだ true */
  uint32_t switch_pushed_ms = 0; /* スイッチが押された時刻 [ms] */

  /* 1. 手がかざされるのを待つ */
  while (hand_count < HAND_STABLE_COUNT) {
    if (ui_switch_pushed()) {
      switch_held = true;
      switch_pushed_ms = timebase_get_ms();
    }
    if (switch_held && !s_switch_state) {
      ui_all_off();
      return false;  /* 短く押して離した：選び直し */
    }
    if (switch_held && timebase_get_ms() - switch_pushed_ms >= SWITCH_START_MS) {
      /* 押し続けた：スタートする。全部点けて、スイッチが離されるのを待つ */
      ui_all_on();
      wait_switch_released();
      wait_before_start();
      return true;
    }
    if (wall_ctrl_is_hand_front()) {
      hand_count = hand_count + 1;
    } else {
      hand_count = 0;
    }

    /* WAIT_BLINK_MS ごとに、点ける・消すを入れ替える */
    if (timebase_get_ms() >= next_toggle_ms) {
      lit = !lit;
      next_toggle_ms = next_toggle_ms + WAIT_BLINK_MS;
    }
    if (lit) {
      ui_show_mode_number(mode, number);
    } else {
      ui_all_off();
    }
    ui_wait_ms(UI_POLL_MS);
  }

  /* 2. 全部点けて、手が離れるのを待つ */
  ui_all_on();
  wait_hand(false);

  /* 3. 全部消して、1 秒待つ */
  wait_before_start();
  return true;
}

/* フェイルセーフで止まったあと（MODES.md の 3）。
 *   8 個全部を FAILSAFE_BLINK_MS ごとに点滅させて、スイッチが押されるのを待つ。押されたら全部消して戻る。
 *   止まったときに 1 回だけ、できなかったと同じ低い音を短く鳴らす（KOGUMA にはない。止まったことに気づくように）。
 *   「ロード中」は、モード選択に戻るときに sys/mode.c が出す（どの道から戻るときも同じように出すため） */
void ui_wait_failsafe_reset(void)
{
  bool lit = true;
  uint32_t next_toggle_ms = timebase_get_ms() + FAILSAFE_BLINK_MS;
  bool tone_on = true;
  buzzer_on(ERROR_TONE_HZ);

  /* 1. スイッチが押されるのを待つ */
  while (!ui_switch_pushed()) {
    /* FAILSAFE_BLINK_MS ごとに、点ける・消すを入れ替える。最初に消すときに、音も止める */
    if (timebase_get_ms() >= next_toggle_ms) {
      lit = !lit;
      next_toggle_ms = next_toggle_ms + FAILSAFE_BLINK_MS;
      if (tone_on) {
        buzzer_off();
        tone_on = false;
      }
    }
    if (lit) {
      ui_all_on();
    } else {
      ui_all_off();
    }
    ui_wait_ms(UI_POLL_MS);
  }

  buzzer_off();
  ui_all_off();
}

/* 電池が少ないとき（MODES.md の 2-1・3）。外側の 4 個（LED1・2・7・8）を FAILSAFE_BLINK_MS ごとに点滅させ続け、
 * 点けているあいだは低い音を鳴らす（2026-11-08 の作者の決定：外側の 4 個の点滅とブザー）。
 * 何もできないようにするため、戻らない。電池を替えて電源を入れ直す */
void ui_stop_for_low_battery(void)
{
  bool lit = true;
  while (true) {
    ui_all_off();
    if (lit) {
      ui_set_outer_leds(true);
      buzzer_on(LOW_BATTERY_TONE_HZ);
    } else {
      buzzer_off();
    }
    lit = !lit;
    ui_wait_ms(FAILSAFE_BLINK_MS);
  }
}
