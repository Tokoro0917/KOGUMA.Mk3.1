/*
 * mode.c
 * モードを選んで、走らせる（MODES.md の 2・4）
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/mode.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - 探索（モード0）は、KOGUMA.MkIIl のモード0 と同じ番号にした（No.1〜12。No.8〜12 は探索のあとに続けて最短走行を走る）
 *   - 最短走行（モード1）は No.1〜9（No.4〜9 は KOGUMA.MkIIl のモード1 No.4〜9 と同じ値。config/run_params.h）
 *   - ターンの調整（モード3）は、組 2000 が No.1〜7、組 2400 が No.9〜15。斜めの直進のテスト（ToraNeko.Mk3 のモード3 No.8・9）は
 *     モード4 No.2・3 に移した
 *   - KOGUMA.Mk3.1 で足したモード：モード2 No.3（吸引探索のスラローム 90°）、モード4 No.12（直線の加速の限界テスト）、
 *     モード5 No.7（部品の確かめ）、モード5 No.8（吸引のときのセンサのノイズ。B7）、モード6 No.2（モーターの向きの確かめ）
 *
 * ■ 流れ（mode_run_once() の 1 回ぶん）
 *   1. 左タイヤでモード、右タイヤで No. を選ぶ。スイッチで決定する
 *   2. スタート待ち（前センサに手をかざして離すと、1 秒後に始まる。スイッチで 1 に戻る）。
 *      センサ値の表示（モード5 No.1）・壁までの距離の表示（No.3）・部品の確かめ（No.7）は、前センサの値を見たいので、
 *      スタート待ちをせずにすぐ始める
 *   3. 選んだモードを走らせる。終わったら、同じモード・No. のまま 1 に戻る
 *      （最後まで走れたら「うまくいった」、フェイルセーフならスイッチで解除するまで待ち、道がなくなったら「できなかった」を出す）
 *   1 に戻るときは、どの道からでも（スタート待ちで取り消したとき、センサ値の表示を終えたとき、走り終わったとき、
 *   フェイルセーフを解除したとき）「ロード中」（約 1 秒）を出す。このあいだはスイッチを押しても何も起きないので、
 *   続けて押したスイッチが、次の決定にならない
 *
 * ■ 今あるモード（ほかのモードは、中身を作ったときにここに足す）
 *   モード0 No.1〜7  探索（No.1 往復、2 エセ全面探索、3 片道 は吸引なし 500mm/s。No.4 往復、5 エセ全面探索、
 *                    6 未知壁探索、7 未知壁探索＋既知区間加速 は吸引 1000mm/s。KOGUMA.MkIIl と同じ番号）
 *   モード0 No.8〜11 探索のあとに続けて最短走行（No.8 は No.1、No.9 は No.4、No.10・11 は No.7 の探索のあと。
 *                    最短走行はモード1 No.5 と同じ。No.11 は最短走行を 4 回、毎回ゴールから探索しながらスタートへ帰る）
 *   モード0 No.12    No.10 と同じだが、探索の既知区間を斜めで走る（KOGUMA.MkIIl の「お遊び」）
 *   モード1 No.1〜9  最短走行（No.1 は小回りだけ、No.2〜9 は大回り・斜め。決定したときに道がなければ「できなかった」）
 *   モード2 No.1     スラローム 90°（吸引なしの探索の速さで左に曲がり、左の区画のまん中で止まる）
 *   モード2 No.2     U ターン（行き止まりで止まり、前壁合わせ、180°、戻って止まる）
 *   モード2 No.3     吸引探索のスラローム 90°（吸引して、吸引探索の速さで。No.1 と同じ動き）
 *   モード2 No.4     その場 180° 旋回（左に回って止まる。旋回の制御の係数を合わせる）
 *   モード2 No.5     壁合わせだけ（前の壁で位置と向きを直す）
 *   モード2 No.9〜15 組 1000 のターンを 1 つずつ（探索の既知区間を斜めで走るときのターン。並びはモード3 と同じ）
 *   モード3 No.1〜7  最短走行のターンを 1 つずつ（組 2000。大回り 90°・180°、斜め入り 45°・135°、斜め出 45°・135°、V90°。左だけ）
 *   モード3 No.9〜15 同じく、組 2400
 *   モード4 No.1     吸引なしの加速度テスト
 *   モード4 No.2・3  斜めの直進のテスト（No.2 は斜めの補正なし、No.3 はあり）
 *   モード4 No.4〜11 吸引ありの加速度テスト
 *   モード4 No.12    直線の加速の限界テスト（KOGUMA.MkIIl のモード6 No.0 と同じ走り）
 *   モード5 No.1     センサ値の表示（スイッチを押すと終わる）
 *   モード5 No.2     センサ値の記録（区画のまん中で 1 秒測り、平均をモード7 No.1 で出す）
 *   モード5 No.3     壁までの距離の表示（センサの値と距離 [mm] を出し続ける。スイッチを押すと終わる）
 *   モード5 No.4〜6  壁切れ・柱の見え方テスト（500・1000・2000mm/s で 4 区画先まで走る）
 *   モード5 No.7     部品の確かめ（制御周期・エンコーダ・IMU・電池・壁センサの値を 1 秒ごとに出す。スイッチを押すと終わる）
 *   モード5 No.8     吸引のときのセンサのノイズ（止まったまま、吸引を 4 段に変えてばらつきを測る）
 *   モード6 No.1     直進（ゆっくり 564mm 走って止まる。タイヤの直径と、制御の係数を合わせる）
 *   モード6 No.2     モーターの向きの確かめ（タイヤを浮かせて、左右のモーターを 2 秒回す）
 *   モード6 No.3     吸引 ON（吸引ファンを回し、スイッチを押すまで吸い続ける）
 *   モード6 No.6     エンコーダの歪みを測る（タイヤを浮かせて回す）
 *   モード6 No.7     壁に沿って直進（横の壁に沿って走る補正を入れて、564mm 走って止まる）
 *   モード7 No.1     ログ出力（迷路と走行ログを PC へ出す）
 *   モード7 No.2     ゴールの位置を入れる
 *   モード7 No.15    迷路呼び出し（Flash に保存した迷路を RAM に戻す）
 * No.0 と、まだ何もない番号では、スイッチを押しても決定しない（何も起きない）。
 */
#include "sys/mode.h"

#include "maze/maze_map.h"
#include "config/run_params.h"
#include "ctrl/run.h"
#include "sys/adjust.h"
#include "sys/edge_test.h"
#include "sys/encoder_cal.h"
#include "sys/explore.h"
#include "sys/failsafe.h"
#include "sys/flash.h"
#include "sys/goal.h"
#include "sys/output.h"
#include "sys/sensor_noise.h"
#include "sys/sensor_record.h"
#include "sys/sensor_view.h"
#include "sys/shortest.h"
#include "sys/turn_test.h"
#include "sys/ui.h"

/* 選んでいるモードと No.（走り終わっても、そのまま残す） */
static int s_mode = 0;
static int s_number = 0;

/* 起動時に 1 回呼ぶ。迷路は空にし、ゴールは Flash から読む。モード0 の No.0 から始める */
void mode_init(void)
{
  ui_init();
  maze_map_init();
  goal_init();
  s_mode = 0;
  s_number = 0;
}

/* 今選んでいるモード */
int mode_get_mode(void)
{
  return s_mode;
}

/* 今選んでいる No. */
int mode_get_number(void)
{
  return s_number;
}

/* モード mode の No. number に、することがあるか（決定できるか） */
bool mode_has_action(int mode, int number)
{
  if (mode == 0) {
    if (number >= 1 && number <= 12) {
      return true;  /* 探索（No.8〜12 は探索のあとに続けて最短走行） */
    }
    return false;
  }
  if (mode == 1) {
    if (number >= 1 && number <= 9) {
      return true;  /* 最短走行 */
    }
    return false;
  }
  if (mode == 3) {
    if (number >= 1 && number <= 7) {
      return true;  /* 最短走行のターン（組 2000） */
    }
    if (number >= 9 && number <= 15) {
      return true;  /* 最短走行のターン（組 2400） */
    }
    return false;
  }
  if (mode == 2) {
    if (number >= 9 && number <= 15) {
      return true;  /* 組 1000 のターン（探索の既知区間を斜めで走るとき） */
    }
    if (number >= 1 && number <= 5) {
      return true;  /* スラローム 90°、U ターン、吸引探索のスラローム 90°、その場 180° 旋回、壁合わせだけ */
    }
    return false;
  }
  if (mode == 4) {
    if (number >= 1 && number <= 3) {
      return true;  /* 吸引なしの加速度テスト、斜めの直進のテスト（補正なし・あり） */
    }
    if (number >= 4 && number <= 12) {
      return true;  /* 吸引ありの加速度テスト（No.12 は直線の加速の限界テスト） */
    }
    return false;
  }
  if (mode == 5) {
    if (number >= 1 && number <= 3) {
      return true;  /* センサ値の表示、センサ値の記録、壁までの距離の表示 */
    }
    if (number >= 4 && number <= 6) {
      return true;  /* 壁切れ・柱の見え方テスト */
    }
    if (number == 7 || number == 8) {
      return true;  /* 部品の確かめ、吸引のときのセンサのノイズ */
    }
    return false;
  }
  if (mode == 6) {
    if (number >= 1 && number <= 3) {
      return true;  /* 直進、モーターの向きの確かめ、吸引 ON */
    }
    if (number == 6) {
      return true;  /* エンコーダの歪みを測る */
    }
    if (number == 7) {
      return true;  /* 壁に沿って直進 */
    }
    return false;
  }
  if (mode == 7) {
    if (number == 1 || number == 2) {
      return true;  /* ログ出力、ゴールの位置 */
    }
    if (number == 15) {
      return true;  /* 迷路呼び出し */
    }
    return false;
  }
  return false;
}

/* value に steps を足して、0〜(count - 1) の中で一周させる（count - 1 の次は 0、0 の前は count - 1） */
static int add_and_wrap(int value, int steps, int count)
{
  value = value + steps;
  while (value >= count) {
    value = value - count;
  }
  while (value < 0) {
    value = value + count;
  }
  return value;
}

/* ---------------------------------------------------------------------------
 * モードを選ぶ
 * ------------------------------------------------------------------------- */

/* 左タイヤでモード、右タイヤで No. を選び、スイッチで決定するまで待つ。選んでいるものは LED に出す。
 * することのない番号（No.0 など）では、スイッチを押しても決定しない */
static void select_mode(void)
{
  ui_forget_tire_rotation();
  while (true) {
    s_mode = add_and_wrap(s_mode, ui_read_left_steps(), MODE_COUNT);
    s_number = add_and_wrap(s_number, ui_read_right_steps(), NUMBER_COUNT);
    ui_show_mode_number(s_mode, s_number);

    /* ui_switch_pushed() は、押されたかを毎回読む必要がある（チャタリングを取り除くため）ので、&& の左に置く。
     * && は左が false なら右を調べないので、左右を入れ替えると、することのない番号のあいだスイッチを読まなくなる */
    if (ui_switch_pushed() && mode_has_action(s_mode, s_number)) {
      ui_blink_all(1);  /* 決定の合図（音も鳴る） */
      return;
    }
    ui_wait_ms(UI_POLL_MS);
  }
}

/* ---------------------------------------------------------------------------
 * モード0：探索
 * ------------------------------------------------------------------------- */

/* No. から、探索の走り方を決める（KOGUMA.MkIIl のモード0 と同じ番号。MODES.md）。
 * No.8〜12 は、探索のあとに続けて最短走行を走る。そのときの探索は、No.8 が No.1、No.9 が No.4、No.10・11 が No.7 と同じ。
 * No.12 は No.7 の探索の既知区間を斜めで走る */
static explore_type_t explore_type_of(int number)
{
  if (number == 8 || number == 9) {
    return EXPLORE_ROUND_TRIP;
  }
  if (number == 10 || number == 11) {
    return EXPLORE_UNKNOWN_WALLS_KNOWN;
  }
  if (number == 12) {
    return EXPLORE_UNKNOWN_WALLS_KNOWN_DIAG;
  }
  if (number == 2 || number == 5) {
    return EXPLORE_ALL_CELLS;  /* エセ全面探索 */
  }
  if (number == 3) {
    return EXPLORE_ONE_WAY;  /* 片道 */
  }
  if (number == 6) {
    return EXPLORE_UNKNOWN_WALLS;  /* 未知壁探索 */
  }
  if (number == 7) {
    return EXPLORE_UNKNOWN_WALLS_KNOWN;  /* 未知壁探索＋既知区間加速 */
  }
  return EXPLORE_ROUND_TRIP;  /* No.1・4 往復 */
}

/* No. から、吸引探索かを決める（No.4〜7・9〜12 は吸引探索、No.1〜3・8 はスラローム探索） */
static bool explore_uses_suction(int number)
{
  if (number == 8) {
    return false;
  }
  if (number >= 4) {
    return true;
  }
  return false;
}

/* 探索の結果が「最後まで走った」でないときの合図を出す（フェイルセーフなら、スイッチで解除するまで待つ） */
static void signal_explore_trouble(explore_result_t result)
{
  if (result == EXPLORE_RESULT_FAILSAFE) {
    /* 8 個全部を点滅させて、スイッチが押されるまで待ち、フェイルセーフを解除する（「ロード中」は mode_run_once() が出す） */
    ui_wait_failsafe_reset();
    failsafe_clear();
  } else if (result == EXPLORE_RESULT_NO_ROUTE) {
    ui_signal_error();  /* 道がなくなった（行き・帰りのどちらか）か、動きすぎた。迷路は保存しない */
  }
}

/* フェイルセーフで止まっていたら、8 個全部を点滅させて、スイッチが押されるまで待ち、解除して true を返す */
static bool reset_if_failsafe(void)
{
  if (!failsafe_is_stopped()) {
    return false;
  }
  ui_wait_failsafe_reset();
  failsafe_clear();
  return true;
}

#define SHORTEST_WAIT_MS   2000  /* 探索・帰りが終わってから、最短走行を始めるまで待つ時間 [ms]（KOGUMA.MkIIl と同じ 2 秒） */
#define SHORTEST_REPEAT_COUNT 4  /* No.11 で、最短走行を走る回数（KOGUMA.MkIIl の SEARCH_REPEAT_N） */

/* スタートの区画のまん中で止まっているところから、反対（北）を向き、後ろの壁まで下がってから、最短走行をする（モード0 No.8〜11）。
 * 探索でスタートに戻ると、来た向き（南）を向いて止まっているので、まず反対を向く。下がると、ふつうに置いたときと同じ位置
 * （お尻を後ろの壁に付けたところ）になる（KOGUMA.MkIIl と同じ）。うまくゴールに着いたら true */
static bool shortest_after_search(void)
{
  ui_wait_ms(SHORTEST_WAIT_MS);
  run_search_turn_back_at_center(0.0f);
  run_search_back_to_start_wall();
  if (reset_if_failsafe()) {
    return false;
  }
  shortest_result_t result = shortest_run(SHORTEST_AFTER_SEARCH_NUMBER, goal_get_x(), goal_get_y());
  if (result == SHORTEST_RESULT_FAILSAFE) {
    reset_if_failsafe();
    return false;
  }
  if (result != SHORTEST_RESULT_DONE) {
    ui_signal_error();  /* 道がない（探索のあとなので、ふつうは起きない） */
    return false;
  }
  return true;
}

/* 最短走行でゴールに着いたところから、反対を向いて区画のまん中に戻り、探索（No.7 と同じ未知壁探索＋既知区間加速、吸引）を
 * しながらスタートへ帰る（モード0 No.11。KOGUMA.MkIIl の Maze_Return_From_Goal）。スタートに着いたら true */
static bool return_from_goal(void)
{
  ui_wait_ms(SHORTEST_WAIT_MS);
  run_search_turn_back_at_center(shortest_get_end_overshoot_mm());
  if (reset_if_failsafe()) {
    return false;
  }
  direction_t dir = maze_turn_back(shortest_get_end_dir());  /* 反対を向いた */
  explore_result_t result = explore_return_from(EXPLORE_UNKNOWN_WALLS_KNOWN, true, shortest_get_end_x(),
                                                shortest_get_end_y(), dir, goal_get_x(), goal_get_y(), GOAL_WIDTH,
                                                GOAL_HEIGHT);
  if (result != EXPLORE_RESULT_DONE) {
    signal_explore_trouble(result);
    return false;
  }
  return true;
}

/* 探索走行をする。No.8〜12 は、探索のあとに続けて最短走行を走る（No.11 は 4 回。毎回ゴールから帰る） */
static void run_explore(int number)
{
  explore_result_t result = explore_run(explore_type_of(number), explore_uses_suction(number), goal_get_x(), goal_get_y(),
                                        GOAL_WIDTH, GOAL_HEIGHT);
  if (result != EXPLORE_RESULT_DONE) {
    signal_explore_trouble(result);
    return;
  }
  if (number <= 7) {
    ui_signal_ok();  /* 走り終わった（迷路の保存は explore_run() の中で済んでいる） */
    return;
  }

  /* No.8〜12：続けて最短走行 */
  int run_count = 1;
  if (number == 11) {
    run_count = SHORTEST_REPEAT_COUNT;
  }
  for (int i = 0; i < run_count; i++) {
    if (i > 0 && !return_from_goal()) {
      return;  /* 帰れなかった（合図は return_from_goal() の中で出した） */
    }
    if (!shortest_after_search()) {
      return;  /* 最短走行がうまくいかなかった（合図は shortest_after_search() の中で出した） */
    }
  }
  if (number == 11) {
    /* 最後の最短走行のあとも、スタートへ帰る（自律賞は、最後にスタートへ戻るまでノータッチが条件。KOGUMA.MkIIl と同じ） */
    if (!return_from_goal()) {
      return;
    }
    ui_play_finish();  /* 全部走り終わった曲 */
    return;
  }
  ui_signal_ok();  /* 最短走行でゴールに着いた。スタートへは手で戻す */
}

/* ---------------------------------------------------------------------------
 * モード1：最短走行
 * ------------------------------------------------------------------------- */

/* 最短走行をする（No. で走り方と速さを選ぶ） */
static void run_shortest(int number)
{
  shortest_result_t result = shortest_run(number, goal_get_x(), goal_get_y());

  if (result == SHORTEST_RESULT_DONE) {
    ui_signal_ok();  /* ゴールで止まった。スタートへは手で戻す */
  } else if (result == SHORTEST_RESULT_FAILSAFE) {
    /* 8 個全部を点滅させて、スイッチが押されるまで待ち、フェイルセーフを解除する */
    ui_wait_failsafe_reset();
    failsafe_clear();
  } else {
    ui_signal_error();  /* 道がない（決定したときに調べているので、ふつうは起きない） */
  }
}

/* ---------------------------------------------------------------------------
 * モード7：設定
 * ------------------------------------------------------------------------- */

/* ゴールの x か y を 1 つ入れる（0〜value_max）。右タイヤで 1 ずつ変え、スイッチで決める。
 * 値は LED5〜LED8 に 2 進数で出す。value は最初の値 */
static int input_goal_value(int value, int value_max)
{
  ui_forget_tire_rotation();
  while (true) {
    value = add_and_wrap(value, ui_read_right_steps(), value_max + 1);
    ui_show_value(value);

    if (ui_switch_pushed()) {
      return value;
    }
    ui_wait_ms(UI_POLL_MS);
  }
}

/* ゴールの位置（左下の区画の x・y）を入れて、Flash に保存する（MODES.md のモード7 No.2）。
 * 使えない位置（スタートの区画を含む）なら、保存せずに x の入力に戻る */
static void input_goal(void)
{
  while (true) {
    ui_blink_all(1);  /* x の入力 */
    int goal_x = input_goal_value(goal_get_x(), GOAL_X_MAX);
    ui_blink_all(2);  /* y の入力 */
    int goal_y = input_goal_value(goal_get_y(), GOAL_Y_MAX);

    if (goal_set(goal_x, goal_y)) {
      ui_signal_ok();
      return;
    }
    ui_signal_error();
  }
}

/* Flash に保存した迷路を RAM に戻す（MODES.md のモード7 No.15 迷路呼び出し） */
static void load_maze(void)
{
  if (flash_load_maze()) {
    ui_signal_ok();
  } else {
    ui_signal_error();  /* 保存した迷路がない */
  }
}

/* 出力と設定（No.1 ログ出力、No.2 ゴールの位置、No.15 迷路呼び出し） */
static void run_setting(int number)
{
  if (number == 1) {
    adjust_print_last_result();  /* 最後に走らせた調整の動きの結果（走ったときはケーブルを挿していないので、ここで出す） */
    sensor_record_print();       /* 最後に測ったセンサ値の記録（モード5 No.2） */
    sensor_noise_print();        /* 最後に測った吸引のときのセンサのノイズ（モード5 No.8） */
    output_all(goal_get_x(), goal_get_y());
    ui_signal_ok();  /* 出し終わった */
  } else if (number == 2) {
    input_goal();
  } else if (number == 15) {
    load_maze();
  }
}

/* ---------------------------------------------------------------------------
 * 1 回ぶん
 * ------------------------------------------------------------------------- */

/* 選んだモードを走らせる（スタートしたあと） */
static void run_selected(void)
{
  if (s_mode == 0) {
    run_explore(s_number);
  } else if (s_mode == 1) {
    run_shortest(s_number);
  } else if (s_mode == 3) {
    turn_test_run(s_number);
  } else if (s_mode == 4 && s_number == 2) {
    turn_test_run_diag(false);
  } else if (s_mode == 4 && s_number == 3) {
    turn_test_run_diag(true);
  } else if (s_mode == 2 && s_number >= 9) {
    turn_test_run_1000(s_number);
  } else if (s_mode == 2 && s_number == 1) {
    adjust_slalom_90();
  } else if (s_mode == 2 && s_number == 2) {
    adjust_uturn();
  } else if (s_mode == 2 && s_number == 3) {
    adjust_suction_slalom_90();
  } else if (s_mode == 2 && s_number == 4) {
    adjust_pivot_180();
  } else if (s_mode == 2 && s_number == 5) {
    adjust_front_align();
  } else if (s_mode == 4) {
    adjust_accel_test(s_number);
  } else if (s_mode == 5 && s_number == 2) {
    sensor_record_run();
  } else if (s_mode == 5 && s_number >= 4 && s_number <= 6) {
    edge_test_run(s_number);
  } else if (s_mode == 5 && s_number == 8) {
    sensor_noise_run();
  } else if (s_mode == 6 && s_number == 1) {
    adjust_straight();
  } else if (s_mode == 6 && s_number == 2) {
    adjust_motor_check();
  } else if (s_mode == 6 && s_number == 3) {
    adjust_suction_on();
  } else if (s_mode == 6 && s_number == 6) {
    encoder_cal_run();
  } else if (s_mode == 6 && s_number == 7) {
    adjust_straight_along_wall();
  } else if (s_mode == 7) {
    run_setting(s_number);
  }
}

/* モードを選んで、スタートして、走らせる。走り終わったら戻る（起動後、繰り返し呼ぶ）。
 * スタート待ちでスイッチが押されたときは、走らずに戻る（次に呼ばれたときに選び直す）。
 * どの道から戻るときも、最後に「ロード中」を出す */
void mode_run_once(void)
{
  select_mode();

  if (s_mode == 1 && !shortest_has_route(s_number, goal_get_x(), goal_get_y())) {
    ui_signal_error();  /* 最短走行で、スタートからゴールへの道がない。走らずに選び直す */
  } else if (s_mode == 5 && s_number == 1) {
    sensor_view_run();  /* スタート待ちをしない */
  } else if (s_mode == 5 && s_number == 3) {
    sensor_view_distance_run();  /* スタート待ちをしない（前センサの値を見たいので） */
  } else if (s_mode == 5 && s_number == 7) {
    sensor_view_parts_run();  /* スタート待ちをしない（前センサの手の読みも見たいので） */
  } else if (ui_wait_start(s_mode, s_number)) {
    run_selected();
  }

  ui_show_loading();
}
