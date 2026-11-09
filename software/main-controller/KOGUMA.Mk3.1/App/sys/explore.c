/*
 * explore.c
 * 探索走行（モード0）：迷路の処理（maze/）に次の行き先を聞き、機体を動かす処理（ctrl/run）で 1 区画ずつ動かす。
 * 迷路の処理と機体を動かす処理をつなぐ、ただ 1 つの場所。
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の App/sys/explore.c（ecd8e10）。KOGUMA.Mk3.1 で変えたところ：
 *   - 吸引探索（KOGUMA.MkIIl のモード0 No.4〜7）：suction が true なら、走り出す前に吸引を上げ、探索の値を
 *     吸引探索の値（ctrl/run.c の run_search_set_suction()）にする。終わったら吸引を止める（下の「■ 吸引」）
 *   - エセ全面探索（KOGUMA.MkIIl のモード0 No.2・5）：ゴールに着いたら、まだ入っていない区画を全部見に行ってから帰る
 *     （下の「■ エセ全面探索」）
 *   - 既知区間加速は、No.7（未知壁探索＋既知区間加速）だけ（ToraNeko.Mk3 の No.4 と同じ）
 *   - 迷路は 16×16 なので、MOVE_LIMIT のコメントを直した
 *   - 最短走行でゴールに着いたあと、そこからスタートへ帰る探索 explore_return_from() を足した（モード0 No.11）
 *   - 未知壁探索で、最短走行の値の組（組 1・組 2）ごとの経路の壁を全部見る（KOGUMA.MkIIl と同じ。B15。下の plan_unknown_walls()）
 *
 * ■ 機体の位置
 * 位置は s_x, s_y, s_dir で覚える。機体は次のどちらかの状態にいる（ctrl/run.h、maze/search.c と同じ考え方）。
 *   走っている  （s_stopped が false）：区画 (s_x, s_y) の入口の辺にいて、向き s_dir でその区画に入るところ。
 *                                       ここでセンサがその区画の前・左・右の壁を見て、記録する
 *   止まっている（s_stopped が true） ：区画 (s_x, s_y) の真ん中で、向き s_dir を向いて止まっている
 *
 * ■ 止まるところ
 *   - U ターン（止まって、後ろを向いて発進する）
 *   - 未知壁探索で計算するとき（ダイクストラは重いので、止まっているときだけ計算する。KOGUMA と同じ）
 *   - 終わり（スタートに戻ったとき、片道でゴールに着いたとき）
 *
 * ■ 既知区間加速（No.7 だけ。ToraNeko.Mk3 の No.4。2026-10-27 に追加）
 * 走っているとき（行き・未知壁を見に行くあいだ・帰り）、今いる区画から先に「見たことのある区画をまっすぐ進む」が 2 区画以上続くなら、
 * 1 区画ずつではなく、まとめて速く走る（run_search_known_straight()。速さは config/run_params.h の KNOWN_*）。
 * 見たことのある区画では、壁がもうわかっているので、1 区画ずつ止まらずに考えても同じ道になる（maze/path.c の
 * path_make_known と同じ考え方）。曲がるところは、ふだんどおり 1 区画ずつ小回りで走る。
 * まっすぐ進む区画を数えるだけにして、経路（path_t。約 4KB）は作らない（RAM が残り少ないため）。
 *
 * ■ 横壁合わせ（2026-11-02 に追加）
 * 走っているとき、今入った区画で横の壁が近すぎたら（wall_ctrl_is_left_too_close() など。config/run_params.h の SIDE_*）、
 * 動く前に、その区画のまん中で止まり、近すぎた側の壁で位置と向きを直す（run_search_align_to_side()）。
 * 直したあとは止まっているので、次の動きは、まん中からの発進になる（行き先の決め方は変わらない）。
 *
 * ■ 既知区間を斜めで（KOGUMA.MkIIl のモード0 No.12。KOGUMA.Mk3.1 で足した。try_known_diagonal()）
 * 既知区間加速と同じく、走っているときに、今いる区画から先の見たことのある区画の経路を作る（maze/path.c の
 * path_make_known_diagonal()。ジグザグのところを、斜め入り・斜めの直進・V90・斜め出にしたもの）。斜めの動きがあれば、
 * その経路を最短走行の動き（ctrl/run.c の run_fast_*）でまとめて走る。曲がるところは、斜めは組 1000 のターン（1000mm/s）、
 * 小回りは探索と同じスラローム。縦横の直進は既知区間加速と同じ速さ（4000mm/s まで）、斜めの直進は 3000mm/s まで。
 * 経路は、まだ見ていない区画の入口の辺で、縦横の向きで終わる（斜めのまま終わる区間は、path.c が小回りに戻してある）。
 * 吸引探索（1000mm/s）で使う（組 1000 のターンの速さと、探索の速さが同じでないといけない）。
 *
 * ■ エセ全面探索（KOGUMA.MkIIl のモード0 No.2・5。KOGUMA.Mk3.1 で足した）
 * ゴールに着いたら、目的地を「まだ入っていない区画すべて」にして（maze/search.c の search_set_target_unvisited()）、
 * いちばん近い区画へ足立法で進むのを繰り返す。見た壁で袋小路とわかった区画は、行き止まり潰しで目的地から外す。
 * 見に行く区画がなくなるか、残りの区画にたどり着けなくなったら、止まらずにスタートへ帰る。
 *
 * ■ 吸引（KOGUMA.Mk3.1 で足した）
 * 吸引探索では、スタート区画で吸引を上げてから走り出す（sys/suction.c。0.5 秒かけて 5.5V、そのあと 0.5 秒待つ）。
 * 走り出す処理（run_search_start()）が、吸ったままジャイロのゼロ点を測り直す。
 * 終わったら（止まってから、迷路を Flash に保存する前に）吸引を止める。フェイルセーフで止まったときは、
 * sys/failsafe.c が吸引も止めている。KOGUMA.MkIIl は U ターンのあいだだけ吸引を弱めていた（Robot_adjustment_180）が、
 * それはまだ入れていない（実機で U ターンを見てから決める）。
 *
 * ■ Flash への保存（MODES.md の 5）
 *   - スタートに戻ったとき、片道でゴールに止まったとき
 *   - フェイルセーフで止まったとき（最後に壁を記録した 5 区画ぶんを、記録する前の状態に戻してから）
 */
#include "sys/explore.h"

#include <stdbool.h>

#include "config/run_params.h"
#include "ctrl/run.h"
#include "ctrl/wall_ctrl.h"
#include "drv/fan.h"
#include "maze/dijkstra.h"
#include "config/turn_params.h"
#include "maze/maze_map.h"
#include "maze/path.h"
#include "maze/search.h"
#include "sys/failsafe.h"
#include "sys/flash.h"
#include "sys/shortest.h"
#include "sys/suction.h"
#include "sys/ui.h"

/* 1 つの区間（行き、未知壁探索、帰り）で、これだけ動いても終わらなければ、おかしいとみなしてやめる。
 * 壁の読み間違いなどで、同じところを回り続けないようにするため。
 * 16×16 の迷路の区画は全部で 256 個なので、十分に大きい（エセ全面探索で全部の区画を回っても届かない） */
#define MOVE_LIMIT 5000

#define SUCTION_SETTLE_MS 500  /* 吸引を上げてから、走り出す前に待つ時間 [ms]（ファンの回り方を落ち着かせる） */

/* 機体の位置と向き（上の「機体の位置」） */
static int s_x = 0;
static int s_y = 0;
static direction_t s_dir = DIR_NORTH;
static bool s_stopped = true;

static bool s_use_known = false;  /* 既知区間加速をするか（No.7・No.12 のとき true） */
static bool s_use_known_diagonal = false;  /* 既知区間を斜めで走るか（No.12 のときだけ true） */
static path_t s_known_path;  /* 既知区間を斜めで走るときの経路（大きいので、関数の中ではなくここに置く） */

/* ---------------------------------------------------------------------------
 * 1 区画ぶんの動き
 * ------------------------------------------------------------------------- */

/* 今入った区画の前・左・右の壁を、センサで見て記録する。区画の入口の辺に着いたときに呼ぶ */
static void record_walls_here(void)
{
  bool wall_front = wall_ctrl_has_wall_front();
  bool wall_left = wall_ctrl_has_wall_left();
  bool wall_right = wall_ctrl_has_wall_right();
  maze_map_record_walls(s_x, s_y, s_dir, wall_front, wall_left, wall_right);
}

/* 今入った区画の真ん中で止まる。もう止まっていれば何もしない */
static void stop_here(void)
{
  if (s_stopped) {
    return;
  }
  run_search_stop_at_center();
  s_stopped = true;
}

/* 止まったあとに返す結果。止まる途中でぶつかって（フェイルセーフで止まって）いたら、
 * result の代わりに EXPLORE_RESULT_FAILSAFE を返す（5 区画ぶんを戻して保存するため） */
static explore_result_t result_after_stop(explore_result_t result)
{
  if (failsafe_is_stopped()) {
    return EXPLORE_RESULT_FAILSAFE;
  }
  return result;
}

/* 動き action をしたあとの向き */
static direction_t direction_after(direction_t dir, action_t action)
{
  if (action == ACTION_TURN_LEFT) {
    return maze_turn_left(dir);
  }
  if (action == ACTION_TURN_RIGHT) {
    return maze_turn_right(dir);
  }
  if (action == ACTION_UTURN) {
    return maze_turn_back(dir);
  }
  return dir;  /* ACTION_FORWARD */
}

/* 止まっているところから発進するときの、向きの変え方 */
static run_turn_t turn_from_center(action_t action)
{
  if (action == ACTION_TURN_LEFT) {
    return RUN_TURN_LEFT;
  }
  if (action == ACTION_TURN_RIGHT) {
    return RUN_TURN_RIGHT;
  }
  if (action == ACTION_UTURN) {
    return RUN_TURN_BACK;
  }
  return RUN_TURN_NONE;  /* ACTION_FORWARD */
}

/* 動き action で、今の区画を出て、次の区画の入口の辺まで動く。位置と向きも進める。
 *   止まっているとき：その場で向きを変えて発進する
 *   走っているとき  ：直進とスラロームはそのまま走る。U ターンは、止まってから後ろを向いて発進する
 * action に ACTION_STOP を渡してはいけない（呼ぶ側で調べる） */
static void move(action_t action)
{
  if (s_stopped) {
    run_search_go_from_center(turn_from_center(action));
  } else if (action == ACTION_FORWARD) {
    run_search_straight();
  } else if (action == ACTION_TURN_LEFT) {
    run_search_turn_left();
  } else if (action == ACTION_TURN_RIGHT) {
    run_search_turn_right();
  } else {
    /* ACTION_UTURN：止まってから、後ろを向いて発進する（止まる途中でぶつかったら、発進しない） */
    run_search_stop_at_center();
    if (!failsafe_is_stopped()) {
      run_search_go_from_center(RUN_TURN_BACK);
    }
  }

  s_stopped = false;
  s_dir = direction_after(s_dir, action);
  s_x = maze_next_x(s_x, s_dir);
  s_y = maze_next_y(s_y, s_dir);
}

/* 今いる区画（入口の辺にいる）から、見たことのある区画をまっすぐ何区画続けて進めるかを数える（既知区間加速）。
 * 見たことのない区画、目的地、まっすぐ以外の動きになる区画の手前で数えるのをやめる。
 * 歩数マップ（search_update_step_map()）は、呼ぶ前に作っておくこと */
static int count_known_straight(void)
{
  int x = s_x;
  int y = s_y;
  int count = 0;
  while (count < MAZE_SIZE) {
    if (!maze_map_is_visited(x, y)) {
      break;
    }
    if (search_is_in_target(x, y)) {
      break;
    }
    if (search_decide_action(x, y, s_dir) != ACTION_FORWARD) {
      break;
    }
    count = count + 1;
    x = maze_next_x(x, s_dir);
    y = maze_next_y(y, s_dir);
  }
  return count;
}

/* 既知区間加速：走っているときに、まっすぐ進む見たことのある区画が 2 区画以上続くなら、まとめて速く走る。
 * 走ったら true（位置を進め、着いた区画の壁を記録してある）。走らなかったら false */
static bool try_known_straight(void)
{
  if (!s_use_known || s_stopped) {
    return false;
  }
  int count = count_known_straight();
  if (count < 2) {
    return false;  /* 1 区画だけなら、ふだんの直進と同じ */
  }
  run_search_known_straight(count);
  for (int i = 0; i < count; i++) {
    s_x = maze_next_x(s_x, s_dir);
    s_y = maze_next_y(s_y, s_dir);
  }
  if (!failsafe_is_stopped()) {
    record_walls_here();
  }
  return true;
}

/* 経路 path に、斜めの動き（斜め入り・斜め出・V90・斜めの直進）があるか */
static bool path_has_diagonal(const path_t *path)
{
  for (int i = 0; i < path->count; i++) {
    motion_t motion = path->steps[i].motion;
    if (motion != MOTION_STRAIGHT && motion != MOTION_TURN_LEFT && motion != MOTION_TURN_RIGHT && motion != MOTION_UTURN) {
      return true;
    }
  }
  return false;
}

/* 経路 path に、U ターンがあるか（U ターンは止まらないとできないので、斜めの経路では走らない） */
static bool path_has_uturn(const path_t *path)
{
  for (int i = 0; i < path->count; i++) {
    if (path->steps[i].motion == MOTION_UTURN) {
      return true;
    }
  }
  return false;
}

/* 斜めの動き motion を、ctrl/run.c のターンの種類にする（斜め入り 45°・135°、斜め出 45°・135°、V90°） */
static run_fast_turn_t diagonal_turn_kind(motion_t motion)
{
  if (motion == MOTION_DIAG_IN45_LEFT || motion == MOTION_DIAG_IN45_RIGHT) {
    return RUN_FAST_TURN_IN45;
  }
  if (motion == MOTION_DIAG_IN135_LEFT || motion == MOTION_DIAG_IN135_RIGHT) {
    return RUN_FAST_TURN_IN135;
  }
  if (motion == MOTION_DIAG_OUT45_LEFT || motion == MOTION_DIAG_OUT45_RIGHT) {
    return RUN_FAST_TURN_OUT45;
  }
  if (motion == MOTION_DIAG_OUT135_LEFT || motion == MOTION_DIAG_OUT135_RIGHT) {
    return RUN_FAST_TURN_OUT135;
  }
  return RUN_FAST_TURN_V90;
}

/* 斜めの動き motion が、左に曲がるか */
static bool is_left_diagonal_turn(motion_t motion)
{
  if (motion == MOTION_DIAG_IN45_LEFT || motion == MOTION_DIAG_IN135_LEFT || motion == MOTION_DIAG_OUT45_LEFT
      || motion == MOTION_DIAG_OUT135_LEFT || motion == MOTION_DIAG_V90_LEFT) {
    return true;
  }
  return false;
}

/* 斜めの経路の i 番目の動きを走る（ctrl/run.c の run_fast_*。どの動きも、探索の速さで終わる）。
 * 縦横の直進を始める位置は、経路の始め・小回りのあとなら区画の境目の上（0mm）、斜め出のあとなら区画のまん中 */
static void run_known_path_step(int i)
{
  motion_t motion = s_known_path.steps[i].motion;
  float speed_mm_s = run_get_search_speed_mm_s();
  if (motion == MOTION_STRAIGHT) {
    float start_in_cell_mm = 0.0f;
    if (i > 0) {
      motion_t before = s_known_path.steps[i - 1].motion;
      if (before != MOTION_TURN_LEFT && before != MOTION_TURN_RIGHT) {
        start_in_cell_mm = HALF_CELL_MM;  /* 斜め出のあと */
      }
    }
    run_fast_straight(s_known_path.steps[i].half_count * HALF_CELL_MM, SUCTION_KNOWN_MAX_SPEED_MM_S, speed_mm_s,
                      SUCTION_KNOWN_ACCEL_MM_S2, start_in_cell_mm);
  } else if (motion == MOTION_DIAG_STRAIGHT) {
    run_fast_diag_straight(s_known_path.steps[i].half_count * DIAG_STEP_MM, KNOWN_DIAG_MAX_SPEED_MM_S, speed_mm_s,
                           KNOWN_DIAG_ACCEL_MM_S2);
  } else if (motion == MOTION_TURN_LEFT) {
    run_fast_turn_left();   /* 探索と同じ小回り */
  } else if (motion == MOTION_TURN_RIGHT) {
    run_fast_turn_right();
  } else {
    run_fast_turn(diagonal_turn_kind(motion), is_left_diagonal_turn(motion), speed_mm_s);  /* 組 1000 のターン */
  }
}

/* 既知区間を斜めで走る（No.12。先頭のコメントの「■ 既知区間を斜めで」）。走っているときに、今いる区画から先の、見たことのある
 * 区画の経路（maze/path.c の path_make_known_diagonal()）に斜めの動きがあれば、その経路をまとめて走る。
 * 走ったら true（位置を経路の終わりに進め、着いた区画の壁を記録してある）。走らなかったら false（このあと既知区間加速か、
 * ふつうの 1 区画ずつの動きにする）。歩数マップ（search_update_step_map()）は、呼ぶ前に作っておくこと */
static bool try_known_diagonal(void)
{
  if (!s_use_known_diagonal || s_stopped) {
    return false;
  }
  path_make_known_diagonal(s_x, s_y, s_dir, &s_known_path);
  if (!path_has_diagonal(&s_known_path) || path_has_uturn(&s_known_path)) {
    return false;  /* 斜めがない（既知区間加速で走る）か、U ターンがある（止まらないとできない） */
  }
  run_search_known_path_begin();
  for (int i = 0; i < s_known_path.count; i++) {
    run_known_path_step(i);
    if (failsafe_is_stopped()) {
      break;
    }
  }
  run_search_known_path_end();
  s_x = s_known_path.end_x;
  s_y = s_known_path.end_y;
  s_dir = s_known_path.end_dir;
  if (!failsafe_is_stopped()) {
    record_walls_here();
  }
  return true;
}

/* 見たことのある区画をまとめて走る（No.12 は斜め、No.7・12 はまっすぐの既知区間加速）。走ったら true */
static bool try_known_run(void)
{
  if (try_known_diagonal()) {
    return true;
  }
  return try_known_straight();
}

/* 横壁合わせ（先頭のコメント）：走っていて、今入った区画で横の壁が近すぎるなら、まん中で止まって、その壁で位置を直す。
 * 止まって直したら true（このあとは止まっている） */
static bool align_if_too_close(void)
{
  if (s_stopped) {
    return false;
  }
  bool left_too_close = wall_ctrl_is_left_too_close();
  bool right_too_close = false;
  if (!left_too_close) {
    right_too_close = wall_ctrl_is_right_too_close();
  }
  if (!left_too_close && !right_too_close) {
    return false;
  }
  stop_here();
  if (!failsafe_is_stopped()) {
    run_search_align_to_side(left_too_close);
  }
  return true;
}

/* ---------------------------------------------------------------------------
 * 区間ごとの走り方
 * ------------------------------------------------------------------------- */

/* 目的地（search_set_target() で決めた範囲）に着くまで、足立法で 1 区画ずつ走る。
 * 今いる区画の壁は、記録してあること。止まっているところから始めてもよい。
 * 着いたら EXPLORE_RESULT_DONE（その区画の入口の辺にいて、止まらない） */
static explore_result_t run_to_target(void)
{
  for (int move_count = 0; move_count < MOVE_LIMIT; move_count++) {
    if (search_is_in_target(s_x, s_y)) {
      return EXPLORE_RESULT_DONE;
    }

    if (align_if_too_close() && failsafe_is_stopped()) {
      return EXPLORE_RESULT_FAILSAFE;
    }
    search_update_step_map();
    if (try_known_run()) {
      if (failsafe_is_stopped()) {
        return EXPLORE_RESULT_FAILSAFE;
      }
      continue;  /* 見たことのある区画をまとめて走った。着いた区画で、また考える */
    }
    action_t action = search_decide_action(s_x, s_y, s_dir);
    if (action == ACTION_STOP) {
      stop_here();  /* どこへ進んでも目的地に着けない */
      return result_after_stop(EXPLORE_RESULT_NO_ROUTE);
    }

    move(action);
    if (failsafe_is_stopped()) {
      return EXPLORE_RESULT_FAILSAFE;
    }
    record_walls_here();
  }

  stop_here();  /* 動きすぎた */
  return result_after_stop(EXPLORE_RESULT_NO_ROUTE);
}

/* エセ全面探索（先頭のコメント）。ゴールの区画の入口の辺に着いたところから呼ぶ。
 * まだ入っていない区画を、行き止まり潰しで外しながら、いちばん近いものから順に見に行く。
 * 見に行く区画がなくなったか、残りの区画にたどり着けなくなったら、止まらずに EXPLORE_RESULT_DONE を返す
 * （このあと、そのままスタートへ戻る） */
static explore_result_t scan_all_cells(int goal_x, int goal_y, int goal_width, int goal_height)
{
  search_set_target_unvisited(goal_x, goal_y, goal_width, goal_height);
  for (int move_count = 0; move_count < MOVE_LIMIT; move_count++) {
    if (align_if_too_close() && failsafe_is_stopped()) {
      return EXPLORE_RESULT_FAILSAFE;
    }
    search_update_step_map();  /* 行き止まり潰しもここでする（maze/search.c） */
    if (search_get_target_cell_count() == 0) {
      return EXPLORE_RESULT_DONE;  /* 見に行く区画がなくなった */
    }
    action_t action = search_decide_action(s_x, s_y, s_dir);
    if (action == ACTION_STOP) {
      return EXPLORE_RESULT_DONE;  /* 残りの区画にたどり着けない（壁で閉じた区画など）。あきらめて帰る */
    }

    move(action);
    if (failsafe_is_stopped()) {
      return EXPLORE_RESULT_FAILSAFE;
    }
    record_walls_here();
  }
  return EXPLORE_RESULT_DONE;  /* 動きすぎた：エセ全面探索をやめて帰る */
}

/* ---- 未知壁探索で、最短走行の組ごとの経路を見る（KOGUMA_PORTING.md の B15。KOGUMA.Mk3.1 で足した） ----
 * KOGUMA.MkIIl と同じく、よく使う最短走行の値の組（組 1 = モード1 No.5、組 2 = No.8。config/run_params.h の
 * SHORTEST_PLAN_SET*_NUMBER）ごとに、その速さで走る時間がいちばん短い経路を求め、どの組の経路の壁も全部見る。
 * こうすると、どちらの組で最短走行をしても、本当の最短経路を走れる。
 *   - 計算するときは、まだ「見終わっていない」組を、組 1 から順に計算し、見に行く壁がある最初の組の経路を使う
 *   - その組の経路の壁を全部見終わったら、「見終わった」にして、次の組を計算し直す（止まって計算する）
 *   - ある組の経路の壁が全部見てあれば、そのあと壁を見ても、その組でそれより速い道は出てこない
 *     （見ていない壁は「ない」とみなして計算しているので、壁を見るほど、どの道も同じか遅くなる）。
 *     なので、見終わった組は計算し直さない */
#define PLAN_SET_COUNT 2  /* 組の数 */

static bool s_plan_set_done[PLAN_SET_COUNT];  /* 組ごとに、経路の壁を全部見終わったか */
static int s_plan_set = 0;                    /* 今の経路は、どの組のものか（0 = 組 1、1 = 組 2） */

/* 組 set（0 か 1）の、最短走行の No. */
static int plan_set_number(int set)
{
  if (set == 1) {
    return SHORTEST_PLAN_SET2_NUMBER;
  }
  return SHORTEST_PLAN_SET1_NUMBER;
}

/* 全部の組の経路の壁を、見終わったか */
static bool all_plan_sets_done(void)
{
  for (int set = 0; set < PLAN_SET_COUNT; set++) {
    if (!s_plan_set_done[set]) {
      return false;
    }
  }
  return true;
}

/* まだ見終わっていない組を、組 1 から順に計算し、見に行く壁がある最初の組の経路を覚える（search.c に覚えさせる）。
 * 見に行く壁がない組は「見終わった」にする。どの組も見終わったら、最後に計算した経路が残る
 * （このあとの search_check_unknown_walls() が UNKNOWN_WALL_NONE を返す） */
static void plan_unknown_walls(int goal_x, int goal_y, int goal_width, int goal_height)
{
  for (int set = 0; set < PLAN_SET_COUNT; set++) {
    if (s_plan_set_done[set]) {
      continue;
    }
    shortest_set_route_cost(plan_set_number(set));  /* その組の速さで走る時間で比べる */
    search_plan_unknown_walls(goal_x, goal_y, goal_width, goal_height);
    dijkstra_set_step_cost();                         /* ほかで使うときのために、元に戻す */
    s_plan_set = set;
    int cell_x;
    int cell_y;
    direction_t wall_dir;
    if (search_check_unknown_walls(&cell_x, &cell_y, &wall_dir) != UNKNOWN_WALL_NONE) {
      return;  /* 見に行く壁がある（か、道がない）：この組の経路を使う */
    }
    s_plan_set_done[set] = true;  /* この組の経路の壁は、もう全部見てある */
  }
}

/* 未知壁探索の経路を、止まって計算し直す（ダイクストラは重いので、止まっているときだけ計算する）。
 * ゴールへの道が見つからなければ、このあとの search_check_unknown_walls() が UNKNOWN_WALL_NO_PLAN を返す。
 * 止まる途中でぶつかった（フェイルセーフで止まった）ときは、計算せずに false を返す */
static bool stop_and_plan(int goal_x, int goal_y, int goal_width, int goal_height)
{
  stop_here();
  if (failsafe_is_stopped()) {
    return false;
  }
  plan_unknown_walls(goal_x, goal_y, goal_width, goal_height);
  return true;
}

/* 未知壁探索（流れは KOGUMA_PORTING.md の「ToraNeko の未知壁探索の流れ」）。ゴールの区画の入口の辺に着いたところから呼ぶ。
 * 最短経路の上の見ていない壁を、なくなるまで見に行く。なくなったら、止まらずに EXPLORE_RESULT_DONE を返す
 * （このあと、そのままスタートへ戻る）。
 * ゴールへの道が見つからないなど、おかしなことが起きたときも、未知壁探索をやめて EXPLORE_RESULT_DONE を返す
 * （止まったまま。このあとスタートへ戻る） */
static explore_result_t scan_unknown_walls(int goal_x, int goal_y, int goal_width, int goal_height)
{
  /* どの組も、まだ見終わっていない */
  for (int set = 0; set < PLAN_SET_COUNT; set++) {
    s_plan_set_done[set] = false;
  }

  /* ゴールの区画の真ん中で止まって、見に行く壁を計算する */
  if (!stop_and_plan(goal_x, goal_y, goal_width, goal_height)) {
    return EXPLORE_RESULT_FAILSAFE;
  }

  for (int move_count = 0; move_count < MOVE_LIMIT; move_count++) {
    int cell_x;
    int cell_y;
    direction_t wall_dir;
    unknown_wall_result_t result = search_check_unknown_walls(&cell_x, &cell_y, &wall_dir);

    if (result == UNKNOWN_WALL_NONE) {
      /* 今の組の経路の壁を、全部見終わった */
      s_plan_set_done[s_plan_set] = true;
      if (all_plan_sets_done()) {
        /* どの組の経路の壁も見終わった：止まらずにスタートへ戻る（最短経路は、最短走行のときに計算する） */
        return EXPLORE_RESULT_DONE;
      }
      /* まだ見終わっていない組がある：止まって、その組の経路を計算する */
      if (!stop_and_plan(goal_x, goal_y, goal_width, goal_height)) {
        return EXPLORE_RESULT_FAILSAFE;
      }
      continue;
    }
    if (result == UNKNOWN_WALL_NO_PLAN) {
      /* ゴールへの道が見つからなかった（壁の読み間違いなど）：未知壁探索をやめる */
      stop_here();
      return result_after_stop(EXPLORE_RESULT_DONE);
    }
    if (result == UNKNOWN_WALL_BLOCKED) {
      /* 経路の上に壁があった（経路がふさがれた）：止まって計算し直す */
      if (!stop_and_plan(goal_x, goal_y, goal_width, goal_height)) {
        return EXPLORE_RESULT_FAILSAFE;
      }
      continue;
    }

    /* UNKNOWN_WALL_FOUND：見ていない壁の手前の区画 (cell_x, cell_y) へ向かう。
     * その区画に入れば、センサでその壁を見られる */
    search_set_target(cell_x, cell_y, 1, 1);
    search_update_step_map();
    if (try_known_run()) {
      if (failsafe_is_stopped()) {
        return EXPLORE_RESULT_FAILSAFE;
      }
      continue;  /* 見たことのある区画をまとめて走った。着いた区画で、また見に行く壁を確かめる */
    }
    action_t action = search_decide_action(s_x, s_y, s_dir);
    if (action == ACTION_STOP) {
      /* その区画へ行けない（普通は起きない）。
       * 止まっているなら、計算し直したばかりでも行けないので、未知壁探索をやめる
       * （この関数で止まるのは、計算し直すときだけ）。走っているなら、止まって計算し直す */
      if (s_stopped) {
        return EXPLORE_RESULT_DONE;
      }
      if (!stop_and_plan(goal_x, goal_y, goal_width, goal_height)) {
        return EXPLORE_RESULT_FAILSAFE;
      }
      continue;
    }
    if (action == ACTION_UTURN && !s_stopped) {
      /* U ターンでは止まるので、止まったついでに計算し直す（行き先が変わるかもしれない） */
      if (!stop_and_plan(goal_x, goal_y, goal_width, goal_height)) {
        return EXPLORE_RESULT_FAILSAFE;
      }
      continue;
    }

    if (align_if_too_close() && failsafe_is_stopped()) {
      return EXPLORE_RESULT_FAILSAFE;
    }
    move(action);
    if (failsafe_is_stopped()) {
      return EXPLORE_RESULT_FAILSAFE;
    }
    record_walls_here();
  }

  stop_here();  /* 動きすぎた：未知壁探索をやめる */
  return result_after_stop(EXPLORE_RESULT_DONE);
}

/* 途中で終わったときの後始末をして、result をそのまま返す。吸引を止め、探索の値を吸引なしに戻す。
 * フェイルセーフで止まったときは、最後に壁を記録した 5 区画ぶんを戻してから、迷路を Flash に保存する
 * （ぶつかる直前に読み間違えたかもしれない壁を残さないため。モーターは止まっている） */
static explore_result_t finish_early(explore_result_t result)
{
  fan_stop();
  run_search_set_suction(false);
  if (result == EXPLORE_RESULT_FAILSAFE) {
    maze_map_undo_recent_cells();
    flash_save_maze();
  }
  return result;
}

/* 最後まで走ったときの後始末。今いる区画の真ん中で止まって、吸引を止め、迷路を Flash に保存する */
static explore_result_t finish_done(void)
{
  stop_here();
  if (failsafe_is_stopped()) {
    return finish_early(EXPLORE_RESULT_FAILSAFE);  /* 止まる途中でぶつかった */
  }
  fan_stop();
  run_search_set_suction(false);
  flash_save_maze();  /* 止まったあとはモーターの出力が 0 なので、CPU が止まっても動かない（run.h） */
  return EXPLORE_RESULT_DONE;
}

/* ---------------------------------------------------------------------------
 * 探索走行
 * ------------------------------------------------------------------------- */

/* 走り方 type から、既知区間加速（s_use_known）と既知区間を斜めで（s_use_known_diagonal）を決める */
static void set_known_options(explore_type_t type)
{
  s_use_known = false;
  s_use_known_diagonal = false;
  if (type == EXPLORE_UNKNOWN_WALLS_KNOWN) {
    s_use_known = true;
  }
  if (type == EXPLORE_UNKNOWN_WALLS_KNOWN_DIAG) {
    s_use_known = true;
    s_use_known_diagonal = true;
  }
}

/* 探索走行をする。機体をスタート区画に北向きに置いてから呼ぶ。走り終わるまで戻ってこない。
 * type で走り方を、suction で吸引するかを選ぶ（explore.h）。ゴールは、左下の区画 (goal_x, goal_y) と幅・高さ [区画] */
explore_result_t explore_run(explore_type_t type, bool suction, int goal_x, int goal_y, int goal_width,
                             int goal_height)
{
  explore_result_t result;

  /* 吸引探索なら、吸引を上げて少し待ち、探索の値を吸引探索の値にする（先頭のコメントの「■ 吸引」） */
  run_search_set_suction(suction);
  if (suction) {
    suction_ramp_up();
    ui_wait_ms(SUCTION_SETTLE_MS);
  }

  /* 既知区間加速は No.7・No.12、既知区間を斜めでは No.12 だけ。ほかは未知壁探索（No.6）と同じ */
  set_known_options(type);
  if (type == EXPLORE_UNKNOWN_WALLS_KNOWN || type == EXPLORE_UNKNOWN_WALLS_KNOWN_DIAG) {
    type = EXPLORE_UNKNOWN_WALLS;
  }

  /* この走行で記録した壁だけを、フェイルセーフのときに戻すようにする */
  maze_map_clear_recent_cells();

  /* 1. スタート区画から走り出し、北隣の区画 (0, 1) の入口の辺まで進む */
  run_search_start();
  s_x = 0;
  s_y = 1;
  s_dir = DIR_NORTH;
  s_stopped = false;
  if (failsafe_is_stopped()) {
    return finish_early(EXPLORE_RESULT_FAILSAFE);
  }
  record_walls_here();

  /* 2. 行き：ゴールまで */
  search_set_target(goal_x, goal_y, goal_width, goal_height);
  result = run_to_target();
  if (result != EXPLORE_RESULT_DONE) {
    return finish_early(result);
  }

  /* 3. 片道：ゴールの区画の真ん中で止まって終わり */
  if (type == EXPLORE_ONE_WAY) {
    return finish_done();
  }

  /* 4. 未知壁探索・エセ全面探索 */
  if (type == EXPLORE_UNKNOWN_WALLS) {
    result = scan_unknown_walls(goal_x, goal_y, goal_width, goal_height);
    if (result != EXPLORE_RESULT_DONE) {
      return finish_early(result);
    }
  }
  if (type == EXPLORE_ALL_CELLS) {
    result = scan_all_cells(goal_x, goal_y, goal_width, goal_height);
    if (result != EXPLORE_RESULT_DONE) {
      return finish_early(result);
    }
  }

  /* 5. 帰り：スタートまで戻って止まる */
  search_set_target(0, 0, 1, 1);
  result = run_to_target();
  if (result != EXPLORE_RESULT_DONE) {
    return finish_early(result);
  }
  return finish_done();
}

/* 区画 (x, y) のまん中で、向き dir を向いて止まっているところから、スタートへ帰る探索をする（KOGUMA.Mk3.1 で足した。
 * モード0 No.11 で、最短走行でゴールに着いたあと。KOGUMA.MkIIl の Maze_Return_From_Goal）。
 * type が未知壁探索（EXPLORE_UNKNOWN_WALLS・_KNOWN）なら、まず最短経路の上の見ていない壁を見に行ってから帰る。
 * suction で吸引するかを選ぶ。終わり方と結果は explore_run() と同じ（スタートの区画のまん中で止まり、迷路を保存する） */
explore_result_t explore_return_from(explore_type_t type, bool suction, int x, int y, direction_t dir, int goal_x,
                                     int goal_y, int goal_width, int goal_height)
{
  explore_result_t result;

  run_search_set_suction(suction);
  if (suction) {
    suction_ramp_up();
    ui_wait_ms(SUCTION_SETTLE_MS);
  }
  set_known_options(type);
  if (type == EXPLORE_UNKNOWN_WALLS_KNOWN || type == EXPLORE_UNKNOWN_WALLS_KNOWN_DIAG) {
    type = EXPLORE_UNKNOWN_WALLS;
  }
  maze_map_clear_recent_cells();  /* この走行で記録した壁だけを、フェイルセーフのときに戻すようにする */

  /* 今いる区画のまん中で、止まったまま制御を始める（このあとの動きは、まん中からの発進になる） */
  run_search_start_at_center();
  s_x = x;
  s_y = y;
  s_dir = dir;
  s_stopped = true;
  if (failsafe_is_stopped()) {
    return finish_early(EXPLORE_RESULT_FAILSAFE);
  }

  /* 未知壁探索：止まっているので、すぐに計算して見に行く */
  if (type == EXPLORE_UNKNOWN_WALLS) {
    result = scan_unknown_walls(goal_x, goal_y, goal_width, goal_height);
    if (result != EXPLORE_RESULT_DONE) {
      return finish_early(result);
    }
  }

  /* 帰り：スタートまで戻って止まる */
  search_set_target(0, 0, 1, 1);
  result = run_to_target();
  if (result != EXPLORE_RESULT_DONE) {
    return finish_early(result);
  }
  return finish_done();
}
