/*
 * test_maze.c
 * 迷路（App/maze/）の PC 上でのテスト。実機がなくても動く
 *
 * ToraNeko.Mk3 の software/main-controller/test/test_maze.c（ecd8e10、2026-11-04）をもとにした。
 * KOGUMA.Mk3.1 で変えたところ：
 *   - 迷路を 16×16（クラシック）にした。テスト用の迷路は mazes/ の maze16_*.txt（2025 年の全日本のクラシックの迷路も入れた）
 *   - 探索走行（sys/explore.c）と最短走行（sys/shortest.c）を「仮の機体」で走らせるテスト（ToraNeko の 9・10）は、
 *     その 2 つのファイルを持ってくるとき（ステップ9・10）に足す
 *   - 速さの値は KOGUMA.MkIIl の値（下の TEST_* と RUN_SET_*）
 *   - KOGUMA.MkIIl にあって ToraNeko.Mk3 にない機能のテストを足した（12 全面探索と行き止まり潰し、13 既知区間の斜め）
 *
 * ビルドと実行（このフォルダで）:  make
 *
 * テストの内容（番号は表示の [ ] の番号。実行する順番は main() のとおり）:
 *   1. 壁の地図（maze_map）の基本動作。フェイルセーフで止まったときに、最後の 5 区画ぶんを戻せるか
 *   2. 歩数マップ（search）の基本動作
 *   3. 既知区間の経路（path_make_known）：小さな迷路で、正しい動きの並びになるか
 *   4. 探索のシミュレーション：テスト用の迷路（mazes フォルダの .txt）を、壁を知らない状態から
 *      足立法で スタート → ゴール → スタート と走り、ちゃんと帰ってこられるかを調べる。
 *      既知区間の経路を使って走っても、1 区画ずつ走ったときと同じところに着くかも調べる
 *   5. 最短経路（path_make_shortest）：何もない迷路と、探索したあとの迷路で、
 *      大回り・斜めを使った経路が壁を通らずにゴールに着くか
 *   6. 未知壁探索（KOGUMA のモード0 No.6・No.7 の考え方）：ゴールに着いたあと最短経路の上の見ていない壁を見に行く。
 *      見に行く壁がなくなった時点の最短経路が、全部の壁を知っているときと同じ速さになるか。
 *      ここでは search.c の関数を使って流れをまねる（既知区間の経路を使って走る）
 *   7. ランダムな迷路（たくさん）：全部の壁を知っているときの最短経路が、どれも壁を通らずにゴールに着くか
 *   8. ダイクストラの答えが本当の最短か：時間はかかるが確実な方法（総当たりに近い計算）で求めたコストと比べる。
 *      比べる計算は dijkstra.c の「動きの決まり」（どの向きに進めるか）をそのまま使うので、調べられるのは
 *      「探し方が最短か」だけ（動きの決まりが正しいかは 7 で調べる）
 *  11. ターンごとの時間のコスト（dijkstra_set_time_cost_diagonal。KOGUMA の #100）：
 *      見つけた道を動きにしたときの時間の見積もりが、報告したコストと同じか。ほかのコストで選んだ道より遅くないか。
 *      小さな迷路（6×6・7×6）で道を全部数え上げて、一番速い道と同じコストか。ダイクストラ 1 回の時間（PC）も表示する
 *  12. 全面探索と行き止まり潰し（KOGUMA のモード0 No.2・No.5）：袋小路が根元まで外れるか、スタートとゴールは外さないか。
 *      スタート → ゴール → 全面探索 → スタート と走り、そのあとの最短経路が、全部の壁を知っているときと同じ速さか。
 *      壁で閉じた区画があっても、ACTION_STOP で全面探索を終えてスタートへ帰れるか
 *  13. 既知区間の経路を斜めで走る（KOGUMA のモード0 No.12）：小さな通路で、正しい動きの並びになるか。
 *      斜めのまま終わるときは小回りに戻すか。迷路を探索して、斜めで走っても壁を通らず、1 区画ずつと同じ道を通るか
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "maze/dijkstra.h"
#include "maze/maze_map.h"
#include "maze/path.h"
#include "maze/search.h"
#include "config/run_params.h"
#include "config/turn_params.h"

/* テストで使う速さ。KOGUMA.MkIIl の値（ステップ9・10 で config/ に置いたら、そちらを使う）
 *   探索：スラローム探索の 500mm/s（main.c の SEARCH_SLALOM）
 *   小回りだけの最短：直進 2000mm/s・10000mm/s²（走行時間の表のテストで使う。どの No. とも同じではない）
 *   大回り・斜めの最短：よく使う最短走行の値の組（Core/Src/ShortParam.c の G_Short_Run_Set）
 *     組 1 = モード1 No.5（ターン 2000mm/s、直線 6000mm/s・20000mm/s²、斜め 15000mm/s²）
 *     組 2 = モード1 No.8（ターン 2400mm/s。ターンの形は今は組 1 と同じ。config/turn_params.h の先頭のコメント）
 *   KOGUMA の斜めの直線の最高速は縦横と同じなので、斜めの最高速も 6000mm/s にする */
#define TEST_SEARCH_SPEED_MM_S         500.0f
#define TEST_SMALL_TURN_MAX_SPEED_MM_S 2000.0f
#define TEST_SMALL_TURN_ACCEL_MM_S2    10000.0f
#define RUN_SET1_TURN_SPEED_MM_S 2000.0f
#define RUN_SET2_TURN_SPEED_MM_S 2400.0f
#define RUN_SET_MAX_SPEED_MM_S   6000.0f
#define RUN_SET_ACCEL_MM_S2      20000.0f
#define RUN_SET_DIAG_ACCEL_MM_S2 15000.0f

/* ダイクストラの中だけで使う関数（move_from など）を、8 のテストで使うため、dijkstra.c をファイルごと取り込む。
 * そのため Makefile では dijkstra.c を別にコンパイルしない */
#include "maze/dijkstra.c"

/* ---------------------------------------------------------------------------
 * テスト結果の数え方
 * ------------------------------------------------------------------------- */

static int s_fail_count = 0;

/* ok が false なら失敗として数え、message を表示する */
static void check(bool ok, const char *message)
{
  if (!ok) {
    s_fail_count = s_fail_count + 1;
    printf("  失敗: %s\n", message);
  }
}

/* ---------------------------------------------------------------------------
 * 本当の迷路（テストの答え）
 * mazes フォルダの .txt を読み込んで、どこに壁があるかを覚えておく
 * ------------------------------------------------------------------------- */

static bool s_true_wall[MAZE_SIZE][MAZE_SIZE][4];  /* [x][y][向き] 壁があれば true */
static int s_goal_x;       /* ゴールの左下の x */
static int s_goal_y;       /* ゴールの左下の y */
static int s_goal_width;   /* ゴールの幅 [区画] */
static int s_goal_height;  /* ゴールの高さ [区画] */

/* テキストの迷路ファイルを読み込む。読めたら true */
static bool load_maze_file(const char *path)
{
  /* ファイルの 1 行は 「o---」が MAZE_SIZE 個 + 「o」= 4 * MAZE_SIZE + 1 文字 */
  static char lines[2 * MAZE_SIZE + 1][4 * MAZE_SIZE + 8];

  FILE *file = fopen(path, "r");
  if (file == NULL) {
    printf("  ファイルが開けない: %s\n", path);
    return false;
  }
  for (int row = 0; row < 2 * MAZE_SIZE + 1; row++) {
    if (fgets(lines[row], sizeof(lines[row]), file) == NULL) {
      printf("  ファイルの行が足りない: %s\n", path);
      fclose(file);
      return false;
    }
  }
  fclose(file);

  int goal_min_x = MAZE_SIZE;
  int goal_min_y = MAZE_SIZE;
  int goal_max_x = -1;
  int goal_max_y = -1;

  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      /* ファイルの上の行ほど y が大きい。区画 (x, y) の中身がある行と列 */
      int cell_row = 2 * (MAZE_SIZE - 1 - y) + 1;
      int cell_col = 4 * x + 2;

      s_true_wall[x][y][DIR_NORTH] = (lines[cell_row - 1][cell_col] == '-');
      s_true_wall[x][y][DIR_SOUTH] = (lines[cell_row + 1][cell_col] == '-');
      s_true_wall[x][y][DIR_WEST] = (lines[cell_row][cell_col - 2] == '|');
      s_true_wall[x][y][DIR_EAST] = (lines[cell_row][cell_col + 2] == '|');

      if (lines[cell_row][cell_col] == 'G') {
        if (x < goal_min_x) {
          goal_min_x = x;
        }
        if (y < goal_min_y) {
          goal_min_y = y;
        }
        if (x > goal_max_x) {
          goal_max_x = x;
        }
        if (y > goal_max_y) {
          goal_max_y = y;
        }
      }
    }
  }

  if (goal_max_x < 0) {
    printf("  ゴール（G）が見つからない: %s\n", path);
    return false;
  }
  s_goal_x = goal_min_x;
  s_goal_y = goal_min_y;
  s_goal_width = goal_max_x - goal_min_x + 1;
  s_goal_height = goal_max_y - goal_min_y + 1;
  return true;
}

/* ---------------------------------------------------------------------------
 * 1. 壁の地図のテスト
 * ------------------------------------------------------------------------- */

static void test_maze_map(void)
{
  printf("[1] 壁の地図\n");
  maze_map_init();

  check(maze_map_get_wall(0, 0, DIR_EAST) == WALL_EXIST, "スタートの東は壁");
  check(maze_map_get_wall(0, 0, DIR_NORTH) == WALL_NONE, "スタートの北は開いている");
  check(maze_map_get_wall(0, 0, DIR_SOUTH) == WALL_EXIST, "スタートの南（外周）は壁");
  check(maze_map_get_wall(5, MAZE_SIZE - 1, DIR_NORTH) == WALL_EXIST, "北の外周は壁");
  check(maze_map_get_wall(5, 5, DIR_NORTH) == WALL_UNKNOWN, "中の壁は最初は「まだ見ていない」");
  check(maze_map_is_visited(0, 0), "スタートは見たことにする");
  check(!maze_map_is_visited(5, 5), "ほかの区画はまだ見ていない");

  /* 片側から書くと、隣の区画から見ても同じになる */
  maze_map_set_wall(5, 5, DIR_NORTH, WALL_EXIST);
  check(maze_map_get_wall(5, 6, DIR_SOUTH) == WALL_EXIST, "(5,5) の北 = (5,6) の南");
  maze_map_set_wall(5, 5, DIR_WEST, WALL_NONE);
  check(maze_map_get_wall(4, 5, DIR_EAST) == WALL_NONE, "(5,5) の西 = (4,5) の東");

  /* 外周の壁は消せない */
  maze_map_set_wall(0, 0, DIR_WEST, WALL_NONE);
  check(maze_map_get_wall(0, 0, DIR_WEST) == WALL_EXIST, "外周の壁は消えない");

  /* スタート区画の東の壁（規則で必ずある）も消せない */
  maze_map_set_wall(0, 0, DIR_EAST, WALL_NONE);
  check(maze_map_get_wall(0, 0, DIR_EAST) == WALL_EXIST, "スタートの東の壁は消えない");
  check(maze_map_get_wall(1, 0, DIR_WEST) == WALL_EXIST, "(1,0) から見たスタートの東の壁も消えない");

  /* センサで見た壁は足すだけで、1 回「見えなかった」だけでは消えない（KOGUMA と同じ） */
  maze_map_record_walls(3, 3, DIR_NORTH, true, false, false);
  check(maze_map_get_wall(3, 3, DIR_NORTH) == WALL_EXIST, "見えた前の壁は「あり」");
  check(maze_map_get_wall(3, 3, DIR_WEST) == WALL_NONE, "見えなかった左の壁は「なし」");
  maze_map_record_walls(3, 3, DIR_NORTH, false, true, false);
  check(maze_map_get_wall(3, 3, DIR_NORTH) == WALL_EXIST, "一度見えた壁は、あとで見えなくても消えない");
  check(maze_map_get_wall(3, 3, DIR_WEST) == WALL_EXIST, "「なし」だった壁も、あとで見えたら「あり」になる");

  /* 向きを変える */
  check(maze_turn_left(DIR_NORTH) == DIR_WEST, "北の左は西");
  check(maze_turn_right(DIR_WEST) == DIR_NORTH, "西の右は北");
  check(maze_turn_back(DIR_EAST) == DIR_WEST, "東の後ろは西");
}

/* 最後に壁を記録した 5 区画ぶんを戻す（フェイルセーフで止まったとき） */
static void test_undo_recent_cells(void)
{
  printf("[1] 壁の地図：最後の %d 区画ぶんを戻す\n", MAZE_UNDO_CELL_COUNT);

  /* 1. スタートから北へ 7 区画進んだとする。左（西）は外周、右（東）は壁なし、前も壁なし */
  maze_map_init();
  for (int y = 1; y <= 7; y++) {
    maze_map_record_walls(0, y, DIR_NORTH, false, true, false);
  }
  maze_map_undo_recent_cells();
  /* 古い 2 区画 (0,1)・(0,2) は残り、新しい 5 区画 (0,3)〜(0,7) は記録する前に戻る */
  check(maze_map_is_visited(0, 1) && maze_map_is_visited(0, 2), "古い 2 区画は入ったことのあるまま");
  check(maze_map_get_wall(0, 2, DIR_EAST) == WALL_NONE, "古い区画の壁は残る");
  check(maze_map_get_wall(0, 2, DIR_NORTH) == WALL_NONE, "古い区画 (0,2) で見た前の壁（= (0,3) の南）は残る");
  check(!maze_map_is_visited(0, 3) && !maze_map_is_visited(0, 7), "新しい 5 区画は入ったことのない区画に戻る");
  check(maze_map_get_wall(0, 3, DIR_EAST) == WALL_UNKNOWN, "新しい区画の壁は「まだ見ていない」に戻る");
  check(maze_map_get_wall(1, 5, DIR_WEST) == WALL_UNKNOWN, "隣の区画から見た同じ壁も戻る");
  check(maze_map_get_wall(0, 7, DIR_NORTH) == WALL_UNKNOWN, "いちばん新しい区画の前の壁も戻る");
  check(maze_map_get_wall(0, 5, DIR_WEST) == WALL_EXIST, "外周の壁は「あり」のまま");

  /* 2. 続けてもう一度戻しても、何も変わらない（覚えている区画はもうない） */
  maze_map_undo_recent_cells();
  check(maze_map_is_visited(0, 2), "2 回目は何も戻さない");

  /* 3. 同じ区画を 2 回記録した（2 回目で前の壁が見えた）ときは、1 回目の前の状態まで戻る */
  maze_map_init();
  maze_map_record_walls(5, 5, DIR_NORTH, false, false, false);
  maze_map_record_walls(5, 5, DIR_NORTH, true, false, false);
  check(maze_map_get_wall(5, 5, DIR_NORTH) == WALL_EXIST, "2 回目の記録で前の壁が「あり」");
  maze_map_undo_recent_cells();
  check(maze_map_get_wall(5, 5, DIR_NORTH) == WALL_UNKNOWN, "2 回とも戻り、「まだ見ていない」になる");
  check(maze_map_get_wall(5, 6, DIR_SOUTH) == WALL_UNKNOWN, "(5,6) から見た同じ壁も戻る");
  check(!maze_map_is_visited(5, 5), "入ったことのない区画に戻る");

  /* 4. 探索を始めるときに忘れた区画は、戻さない */
  maze_map_init();
  maze_map_record_walls(0, 1, DIR_NORTH, false, true, true);
  maze_map_clear_recent_cells();
  maze_map_record_walls(0, 2, DIR_NORTH, true, true, false);
  maze_map_undo_recent_cells();
  check(maze_map_get_wall(0, 1, DIR_EAST) == WALL_EXIST, "忘れる前に記録した区画は戻さない");
  check(maze_map_is_visited(0, 1), "忘れる前に記録した区画は入ったことのあるまま");
  check(maze_map_get_wall(0, 2, DIR_NORTH) == WALL_UNKNOWN, "忘れたあとに記録した区画は戻す");
}

/* ---------------------------------------------------------------------------
 * 2. 歩数マップのテスト
 * ------------------------------------------------------------------------- */

static void test_step_map(void)
{
  printf("[2] 歩数マップ\n");
  maze_map_init();

  /* 壁を何も見ていない迷路で、目的地 (3, 2) までの歩数は、縦横の距離の合計になる。
   * ただしスタートの東は壁なので、(0,0) からは北に回る必要がある（それでも距離は同じ） */
  search_set_target(3, 2, 1, 1);
  search_update_step_map();
  check(search_get_step(3, 2) == 0, "目的地は 0");
  check(search_get_step(0, 0) == 5, "(0,0) から (3,2) は 5 区画");
  check(search_get_step(3, 5) == 3, "(3,5) から (3,2) は 3 区画");

  /* 目的地の真下の道に壁を置くと、回り道の分だけ増える */
  maze_map_set_wall(3, 2, DIR_SOUTH, WALL_EXIST);
  search_update_step_map();
  check(search_get_step(3, 1) == 3, "(3,1) は真上に壁があるので、横から回って 3 区画");

  /* 歩数が小さい方へ進む。同じなら 前 → 左 → 右 の順 */
  check(search_decide_action(3, 0, DIR_NORTH) == ACTION_FORWARD, "前・左・右の歩数が同じなら、直進を選ぶ（前 → 左 → 右 → 後ろの順）");
  check(search_decide_action(0, 2, DIR_NORTH) == ACTION_TURN_RIGHT, "右が近ければ右");

  /* 未知壁探索の経路を計算していないときは「全部見た」ではなく「経路がない」 */
  int cell_x;
  int cell_y;
  direction_t wall_dir;
  maze_map_init();
  maze_map_set_wall(0, 0, DIR_NORTH, WALL_EXIST);
  check(!search_plan_unknown_walls(10, 10, 1, 1), "閉じ込められていたら未知壁探索の経路は計算できない");
  check(search_check_unknown_walls(&cell_x, &cell_y, &wall_dir) == UNKNOWN_WALL_NO_PLAN,
        "経路が計算できなかったら UNKNOWN_WALL_NO_PLAN（「全部見た」と区別する）");

  /* 囲まれていてたどり着けないときは止まる */
  maze_map_init();
  maze_map_set_wall(0, 0, DIR_NORTH, WALL_EXIST);
  search_set_target(10, 10, 1, 1);
  search_update_step_map();
  check(search_decide_action(0, 0, DIR_NORTH) == ACTION_STOP, "閉じ込められたら ACTION_STOP");
}

/* ---------------------------------------------------------------------------
 * 経路を読みやすく表示する（テストの結果を人が見るため）
 * ------------------------------------------------------------------------- */

static void print_path(const path_t *path)
{
  printf("  経路:");
  for (int i = 0; i < path->count; i++) {
    switch (path->steps[i].motion) {
    case MOTION_STRAIGHT:
      printf(" 直進%d", path->steps[i].half_count);
      break;
    case MOTION_TURN_LEFT:
      printf(" 左");
      break;
    case MOTION_TURN_RIGHT:
      printf(" 右");
      break;
    case MOTION_UTURN:
      printf(" Uターン");
      break;
    case MOTION_BIG_TURN_LEFT_90:
      printf(" 大回り左90");
      break;
    case MOTION_BIG_TURN_RIGHT_90:
      printf(" 大回り右90");
      break;
    case MOTION_BIG_TURN_LEFT_180:
      printf(" 大回り左180");
      break;
    case MOTION_BIG_TURN_RIGHT_180:
      printf(" 大回り右180");
      break;
    case MOTION_DIAG_IN45_LEFT:
      printf(" 斜め入り左45");
      break;
    case MOTION_DIAG_IN45_RIGHT:
      printf(" 斜め入り右45");
      break;
    case MOTION_DIAG_IN135_LEFT:
      printf(" 斜め入り左135");
      break;
    case MOTION_DIAG_IN135_RIGHT:
      printf(" 斜め入り右135");
      break;
    case MOTION_DIAG_STRAIGHT:
      printf(" 斜め直進%d", path->steps[i].half_count);
      break;
    case MOTION_DIAG_V90_LEFT:
      printf(" V90左");
      break;
    case MOTION_DIAG_V90_RIGHT:
      printf(" V90右");
      break;
    case MOTION_DIAG_OUT45_LEFT:
      printf(" 斜め出左45");
      break;
    case MOTION_DIAG_OUT45_RIGHT:
      printf(" 斜め出右45");
      break;
    case MOTION_DIAG_OUT135_LEFT:
      printf(" 斜め出左135");
      break;
    case MOTION_DIAG_OUT135_RIGHT:
      printf(" 斜め出右135");
      break;
    }
  }
  printf("\n");
}

/* 経路の i 番目が、motion で長さ half_count の動きか */
static bool step_is(const path_t *path, int i, motion_t motion, int half_count)
{
  if (i >= path->count) {
    return false;
  }
  if (path->steps[i].motion != motion) {
    return false;
  }
  if (path->steps[i].half_count != half_count) {
    return false;
  }
  return true;
}

/* ---------------------------------------------------------------------------
 * 3. 既知区間の経路のテスト
 * ------------------------------------------------------------------------- */

static void test_known_path(void)
{
  printf("[3] 既知区間の経路\n");
  path_t path;

  /* 例 1：北に 2 区画進んで右に曲がり、東に 2 区画で目的地
   *   (0,2)→(1,2)→(2,2)→(3,2)=目的地
   *     ↑
   *   (0,1)
   *     ↑
   *   (0,0)=スタート
   * 既知区間では大回りにしないので「直進4 → 右 → 直進4」 */
  maze_map_init();
  maze_map_record_walls(0, 0, DIR_NORTH, false, true, true);
  maze_map_record_walls(0, 1, DIR_NORTH, false, true, true);
  maze_map_record_walls(0, 2, DIR_NORTH, true, true, false);
  maze_map_record_walls(1, 2, DIR_EAST, false, true, true);
  maze_map_record_walls(2, 2, DIR_EAST, false, true, true);
  maze_map_record_walls(3, 2, DIR_EAST, true, true, true);
  search_set_target(3, 2, 1, 1);
  search_update_step_map();
  path_make_known(0, 0, DIR_NORTH, &path);
  print_path(&path);
  check(path.count == 3, "例1：動きは 3 つ");
  check(step_is(&path, 0, MOTION_STRAIGHT, 4), "例1：最初は直進 4 半区画");
  check(step_is(&path, 1, MOTION_TURN_RIGHT, 0), "例1：次は右（小回り）");
  check(step_is(&path, 2, MOTION_STRAIGHT, 4), "例1：最後は直進 4 半区画");
  check(path.end_x == 3 && path.end_y == 2 && path.end_dir == DIR_EAST, "例1：(3,2) で東向きに終わる");

  /* 例 2：北に 2 区画、右、右で折り返して南に 1 区画で目的地
   *   (0,2)→(1,2)
   *     ↑     ↓
   *   (0,1) (1,1)
   *     ↑     ↓
   *   (0,0) (1,0)=目的地
   * 「直進4 → 右 → 右 → 直進2」 */
  maze_map_init();
  maze_map_record_walls(0, 0, DIR_NORTH, false, true, true);
  maze_map_record_walls(0, 1, DIR_NORTH, false, true, true);
  maze_map_record_walls(0, 2, DIR_NORTH, true, true, false);
  maze_map_record_walls(1, 2, DIR_EAST, true, true, false);
  maze_map_record_walls(1, 1, DIR_SOUTH, false, true, true);
  maze_map_record_walls(1, 0, DIR_SOUTH, true, true, true);
  search_set_target(1, 0, 1, 1);
  search_update_step_map();
  path_make_known(0, 0, DIR_NORTH, &path);
  print_path(&path);
  check(path.count == 4, "例2：動きは 4 つ");
  check(step_is(&path, 0, MOTION_STRAIGHT, 4), "例2：最初は直進 4 半区画");
  check(step_is(&path, 1, MOTION_TURN_RIGHT, 0), "例2：次は右");
  check(step_is(&path, 2, MOTION_TURN_RIGHT, 0), "例2：もう一度右");
  check(step_is(&path, 3, MOTION_STRAIGHT, 2), "例2：最後は直進 2 半区画");
  check(path.end_x == 1 && path.end_y == 0 && path.end_dir == DIR_SOUTH, "例2：(1,0) で南向きに終わる");

  /* 例 3：目的地が今いる区画 (0,0) なら、動かないので経路は空 */
  maze_map_init();
  maze_map_record_walls(0, 0, DIR_NORTH, false, true, true);
  search_set_target(0, 0, 1, 1);  /* 目的地が今いる区画なら、経路は空 */
  search_update_step_map();
  path_make_known(0, 0, DIR_NORTH, &path);
  check(path.count == 0, "例3：目的地にいるなら経路は空");

  /* 例 4：まだ見ていない区画に入るところで止まる */
  maze_map_init();
  maze_map_record_walls(0, 0, DIR_NORTH, false, true, true);
  search_set_target(0, 5, 1, 1);
  search_update_step_map();
  path_make_known(0, 0, DIR_NORTH, &path);
  check(path.count == 1 && step_is(&path, 0, MOTION_STRAIGHT, 2), "例4：見た区画 (0,0) だけ直進する");
  check(path.end_x == 0 && path.end_y == 1, "例4：まだ見ていない (0,1) の入口で止まる");
}

/* ---------------------------------------------------------------------------
 * 4. 探索のシミュレーション
 * ------------------------------------------------------------------------- */

#define MOVE_LIMIT 5000  /* これだけ動いても着かなければ失敗とする */

/* 経路を、1 区画ずつの動き（直進・左・右・Uターン）に戻しながら本当の迷路の上で走らせ、
 * 壁を通り抜けずに経路の終わりの位置まで行けるか調べる。行ければ true。
 *
 * 大回りは「半区画の直進 + ターン + 半区画の直進」に戻せる。直進の半区画を貯めておき、
 * ターンの手前で 2 半区画ずつ 1 区画の直進にする（半区画が余ったら経路がおかしい） */
static int s_pending_half = 0;     /* まだ走らせていない直進 [半区画] */
static int s_replayed_cells = 0;  /* 経路を走らせて進んだ区画数（シミュレーションの集計用） */

/* 1 区画ぶん走らせる。壁にぶつかったら false */
static bool replay_one_cell(int *x, int *y, direction_t *dir, action_t action)
{
  if (action == ACTION_TURN_LEFT) {
    *dir = maze_turn_left(*dir);
  } else if (action == ACTION_TURN_RIGHT) {
    *dir = maze_turn_right(*dir);
  } else if (action == ACTION_UTURN) {
    *dir = maze_turn_back(*dir);
  }
  if (s_true_wall[*x][*y][*dir]) {
    return false;
  }
  *x = maze_next_x(*x, *dir);
  *y = maze_next_y(*y, *dir);
  s_replayed_cells = s_replayed_cells + 1;
  return true;
}

/* 貯めていた直進を走らせる。半区画が余ったり壁にぶつかったら false */
static bool replay_pending_straight(int *x, int *y, direction_t *dir)
{
  if (s_pending_half % 2 != 0) {
    return false;  /* 奇数（2 で割り切れない）なら、半区画が余っている */
  }
  while (s_pending_half > 0) {
    if (!replay_one_cell(x, y, dir, ACTION_FORWARD)) {
      return false;
    }
    s_pending_half = s_pending_half - 2;
  }
  return true;
}

/* ターンを 1 つ走らせる（その前に貯めていた直進を走らせる） */
static bool replay_turn(int *x, int *y, direction_t *dir, action_t action)
{
  if (!replay_pending_straight(x, y, dir)) {
    return false;
  }
  return replay_one_cell(x, y, dir, action);
}

/* 動きが左向きのものか */
static bool is_left_motion(motion_t motion)
{
  switch (motion) {
  case MOTION_TURN_LEFT:
  case MOTION_BIG_TURN_LEFT_90:
  case MOTION_BIG_TURN_LEFT_180:
  case MOTION_DIAG_IN45_LEFT:
  case MOTION_DIAG_IN135_LEFT:
  case MOTION_DIAG_V90_LEFT:
  case MOTION_DIAG_OUT45_LEFT:
  case MOTION_DIAG_OUT135_LEFT:
    return true;
  default:
    return false;
  }
}

/* 経路を本当の迷路の上で走らせる。壁を通らずに経路の終わりの位置まで行ければ true。
 * ends_at_cell_center：最短経路のように、最後に区画の真ん中まで半区画進んで終わる経路か
 *
 * 各動きを、1 区画ずつの動き（直進・左・右・Uターン）に戻して走らせる。
 *   大回り 90°      = 半区画 + ターン + 半区画
 *   大回り 180°     = 半区画 + ターン + 同じターン + 半区画
 *   斜め入り 45°    = 半区画 + ターン（このあと斜めの中は、左右が交互のターンが続く）
 *   斜め入り 135°   = 半区画 + ターン + 同じターン
 *   斜めの直進 n    = 左右交互のターン n 個（前のターンの反対から始まる）
 *   V90             = ターン + 同じターン
 *   斜め出 45°      = ターン + 半区画
 *   斜め出 135°     = ターン + 同じターン + 半区画 */
static bool replay_path(const path_t *path, int x, int y, direction_t dir, bool ends_at_cell_center)
{
  bool ok = true;
  bool last_turn_left = false;  /* 最後のターンが左だったか（斜めの直進で交互にするため） */
  s_pending_half = 0;

  for (int i = 0; i < path->count && ok; i++) {
    motion_t motion = path->steps[i].motion;
    action_t turn = ACTION_TURN_RIGHT;
    if (is_left_motion(motion)) {
      turn = ACTION_TURN_LEFT;
    }

    switch (motion) {
    case MOTION_STRAIGHT:
      s_pending_half = s_pending_half + path->steps[i].half_count;
      break;
    case MOTION_TURN_LEFT:
    case MOTION_TURN_RIGHT:
      ok = replay_turn(&x, &y, &dir, turn);
      break;
    case MOTION_UTURN:
      ok = replay_turn(&x, &y, &dir, ACTION_UTURN);
      break;
    case MOTION_BIG_TURN_LEFT_90:
    case MOTION_BIG_TURN_RIGHT_90:
      s_pending_half = s_pending_half + 1;
      ok = replay_turn(&x, &y, &dir, turn);
      s_pending_half = 1;
      break;
    case MOTION_BIG_TURN_LEFT_180:
    case MOTION_BIG_TURN_RIGHT_180:
      s_pending_half = s_pending_half + 1;
      ok = replay_turn(&x, &y, &dir, turn);
      if (ok) {
        ok = replay_turn(&x, &y, &dir, turn);
      }
      s_pending_half = 1;
      break;
    case MOTION_DIAG_IN45_LEFT:
    case MOTION_DIAG_IN45_RIGHT:
      s_pending_half = s_pending_half + 1;
      ok = replay_turn(&x, &y, &dir, turn);
      break;
    case MOTION_DIAG_IN135_LEFT:
    case MOTION_DIAG_IN135_RIGHT:
    case MOTION_DIAG_V90_LEFT:
    case MOTION_DIAG_V90_RIGHT:
      if (motion == MOTION_DIAG_IN135_LEFT || motion == MOTION_DIAG_IN135_RIGHT) {
        s_pending_half = s_pending_half + 1;
      }
      ok = replay_turn(&x, &y, &dir, turn);
      if (ok) {
        ok = replay_turn(&x, &y, &dir, turn);
      }
      break;
    case MOTION_DIAG_STRAIGHT:
      for (int turn_count = 0; turn_count < path->steps[i].half_count && ok; turn_count++) {
        if (last_turn_left) {
          ok = replay_turn(&x, &y, &dir, ACTION_TURN_RIGHT);
          last_turn_left = false;
        } else {
          ok = replay_turn(&x, &y, &dir, ACTION_TURN_LEFT);
          last_turn_left = true;
        }
      }
      break;
    case MOTION_DIAG_OUT45_LEFT:
    case MOTION_DIAG_OUT45_RIGHT:
      ok = replay_turn(&x, &y, &dir, turn);
      s_pending_half = 1;
      break;
    case MOTION_DIAG_OUT135_LEFT:
    case MOTION_DIAG_OUT135_RIGHT:
      ok = replay_turn(&x, &y, &dir, turn);
      if (ok) {
        ok = replay_turn(&x, &y, &dir, turn);
      }
      s_pending_half = 1;
      break;
    }

    if (motion != MOTION_STRAIGHT && motion != MOTION_DIAG_STRAIGHT && motion != MOTION_UTURN) {
      last_turn_left = is_left_motion(motion);
    }
  }

  /* 最後に区画の真ん中まで進む経路なら、半区画が 1 つ余るのが正しい */
  if (ok && ends_at_cell_center) {
    if (s_pending_half % 2 == 1) {  /* 2 で割った余りが 1 = 奇数なら、半区画が 1 つ余っている */
      s_pending_half = s_pending_half - 1;
    } else {
      ok = false;
    }
  }
  if (ok) {
    ok = replay_pending_straight(&x, &y, &dir);
  }
  if (!ok) {
    return false;
  }
  if (x != path->end_x || y != path->end_y || dir != path->end_dir) {
    return false;
  }
  return true;
}

/* シミュレーションで数えるもの */
static int s_known_path_count = 0;       /* 既知区間の経路でまとめて走った回数 */
static int s_big_turn_count = 0;         /* そのうち大回りの数 */
static int s_known_path_error_count = 0; /* 経路を走らせたら壁にぶつかった、または位置が合わなかった回数 */

/* 経路の中の大回りを数える */
static int count_big_turns(const path_t *path)
{
  int count = 0;
  for (int i = 0; i < path->count; i++) {
    motion_t motion = path->steps[i].motion;
    if (motion == MOTION_BIG_TURN_LEFT_90 || motion == MOTION_BIG_TURN_RIGHT_90
        || motion == MOTION_BIG_TURN_LEFT_180 || motion == MOTION_BIG_TURN_RIGHT_180) {
      count = count + 1;
    }
  }
  return count;
}

/* 未知壁探索で見に行く壁。s_watch_enabled が true なら、run_to_target() はこの壁を見た時点でも止まる */
static bool s_watch_enabled = false;
static int s_watch_x = 0;
static int s_watch_y = 0;
static direction_t s_watch_dir = DIR_NORTH;

/* s_stop_on_uturn が true なら、run_to_target() は U ターンする手前で止まり、s_stopped_for_uturn を true にする
 * （未知壁探索では、U ターンで止まるついでにダイクストラを計算し直すため）。
 * 走り出してすぐの U ターン（止まって計算したあとの最初の動き）では止まらない */
static bool s_stop_on_uturn = false;
static bool s_stopped_for_uturn = false;

/* 今いる (x, y, dir) から、目的地に着くまで足立法で走る。
 * use_known_path が true なら、見たことのある区画が続くところは既知区間の経路でまとめて走る。
 * 進んだ区画数を返す（既知区間の経路でまとめて走った分は数えない）。着けなかったら -1。
 * 走り終わった位置と向きは x, y, dir に入る */
/* true なら、run_to_target() の既知区間の経路を、斜めで走る形（path_make_known_diagonal）にする（[13] のテスト） */
static bool s_known_path_diagonal = false;
static int s_known_diagonal_count = 0;  /* 斜めの動きが入った既知区間の経路の数 */

/* 経路に斜めの動きがあるか（[13] のテストで使う。path_has_diagonal() は後ろで作るので、ここに別に書く） */
static bool known_path_has_diagonal(const path_t *path)
{
  for (int i = 0; i < path->count; i++) {
    motion_t motion = path->steps[i].motion;
    if (motion != MOTION_STRAIGHT && motion != MOTION_TURN_LEFT && motion != MOTION_TURN_RIGHT && motion != MOTION_UTURN) {
      return true;
    }
  }
  return false;
}

static int run_to_target(int *x, int *y, direction_t *dir, bool use_known_path)
{
  static path_t s_path;
  int move_count = 0;

  while (move_count < MOVE_LIMIT) {
    /* センサで前・左・右の壁を見る（本当の迷路から読む） */
    bool wall_front = s_true_wall[*x][*y][*dir];
    bool wall_left = s_true_wall[*x][*y][maze_turn_left(*dir)];
    bool wall_right = s_true_wall[*x][*y][maze_turn_right(*dir)];
    maze_map_record_walls(*x, *y, *dir, wall_front, wall_left, wall_right);

    if (search_is_in_target(*x, *y)) {
      return move_count;
    }
    if (s_watch_enabled && maze_map_get_wall(s_watch_x, s_watch_y, s_watch_dir) != WALL_UNKNOWN) {
      return move_count;  /* 見に行く壁を見た */
    }

    search_update_step_map();

    if (use_known_path) {
      if (s_known_path_diagonal) {
        path_make_known_diagonal(*x, *y, *dir, &s_path);
        if (known_path_has_diagonal(&s_path)) {
          s_known_diagonal_count = s_known_diagonal_count + 1;
        }
      } else {
        path_make_known(*x, *y, *dir, &s_path);
      }
      /* 2 区画以上まとめて走れるときだけ使う（1 区画なら普通の探索と同じ） */
      bool is_long = true;
      if (s_path.count == 0) {
        is_long = false;
      }
      if (s_path.count == 1 && s_path.steps[0].motion == MOTION_STRAIGHT && s_path.steps[0].half_count <= 2) {
        is_long = false;
      }
      if (s_path.count == 1 && s_path.steps[0].motion != MOTION_STRAIGHT) {
        is_long = false;
      }
      if (is_long) {
        if (!replay_path(&s_path, *x, *y, *dir, false)) {
          s_known_path_error_count = s_known_path_error_count + 1;
        }
        s_known_path_count = s_known_path_count + 1;
        s_big_turn_count = s_big_turn_count + count_big_turns(&s_path);
        *x = s_path.end_x;
        *y = s_path.end_y;
        *dir = s_path.end_dir;
        continue;
      }
    }

    action_t action = search_decide_action(*x, *y, *dir);

    if (s_stop_on_uturn && action == ACTION_UTURN && move_count > 0) {
      s_stopped_for_uturn = true;
      return move_count;  /* U ターンの前に止まる */
    }

    if (action == ACTION_STOP) {
      printf("  (%d,%d) で進めなくなった\n", *x, *y);
      return -1;
    } else if (action == ACTION_TURN_LEFT) {
      *dir = maze_turn_left(*dir);
    } else if (action == ACTION_TURN_RIGHT) {
      *dir = maze_turn_right(*dir);
    } else if (action == ACTION_UTURN) {
      *dir = maze_turn_back(*dir);
    }

    /* 進む先に本当に壁があったら、探索が壁を無視したことになる */
    if (s_true_wall[*x][*y][*dir]) {
      printf("  (%d,%d) で壁に突っ込んだ\n", *x, *y);
      return -1;
    }
    *x = maze_next_x(*x, *dir);
    *y = maze_next_y(*y, *dir);
    move_count = move_count + 1;
  }

  printf("  %d 区画動いても着かなかった\n", MOVE_LIMIT);
  return -1;
}

/* 記録した壁が、本当の迷路と食い違っていないか調べる。食い違いの数を返す */
static int count_wrong_walls(void)
{
  int wrong_count = 0;
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      for (int i = 0; i < 4; i++) {
        direction_t dir = (direction_t)i;
        wall_state_t recorded = maze_map_get_wall(x, y, dir);
        if (recorded == WALL_EXIST && !s_true_wall[x][y][dir]) {
          wrong_count = wrong_count + 1;
        }
        if (recorded == WALL_NONE && s_true_wall[x][y][dir]) {
          wrong_count = wrong_count + 1;
        }
      }
    }
  }
  return wrong_count;
}

/* スタート → ゴール → スタート を走る。進んだ区画数を moves_to_goal, moves_to_start に入れる */
static void explore_round_trip(bool use_known_path, int *moves_to_goal, int *moves_to_start)
{
  maze_map_init();
  int x = 0;
  int y = 0;
  direction_t dir = DIR_NORTH;

  /* 行き：スタート → ゴール */
  search_set_target(s_goal_x, s_goal_y, s_goal_width, s_goal_height);
  *moves_to_goal = run_to_target(&x, &y, &dir, use_known_path);
  check(*moves_to_goal >= 0, "ゴールに着く");

  /* 帰り：ゴール → スタート */
  search_set_target(0, 0, 1, 1);
  *moves_to_start = run_to_target(&x, &y, &dir, use_known_path);
  check(*moves_to_start >= 0, "スタートに戻る");
  check(x == 0 && y == 0, "最後はスタートにいる");

  check(count_wrong_walls() == 0, "記録した壁が本当の迷路と同じ");
}

/* 迷路ファイルの迷路を、1 区画ずつ探索した場合と、既知区間をまとめて走った場合で比べる */
static void test_explore(const char *file_path)
{
  printf("[4] 探索のシミュレーション: %s\n", file_path);
  if (!load_maze_file(file_path)) {
    check(false, "迷路ファイルが読めない");
    return;
  }
  printf("  ゴール: (%d,%d) から %dx%d 区画\n", s_goal_x, s_goal_y, s_goal_width, s_goal_height);

  /* 1 区画ずつ探索する */
  int goal_moves = 0;
  int start_moves = 0;
  explore_round_trip(false, &goal_moves, &start_moves);
  printf("  1 区画ずつ:       行き %d 区画、帰り %d 区画\n", goal_moves, start_moves);

  /* 既知区間の経路を使って探索する */
  s_known_path_count = 0;
  s_big_turn_count = 0;
  s_known_path_error_count = 0;
  s_replayed_cells = 0;
  int goal_moves_known = 0;
  int start_moves_known = 0;
  explore_round_trip(true, &goal_moves_known, &start_moves_known);
  printf("  既知区間を使う:   行き %d 区画、帰り %d 区画（このほかに既知区間を %d 回まとめて走った）\n",
         goal_moves_known, start_moves_known, s_known_path_count);
  check(s_big_turn_count == 0, "既知区間の経路には大回りが入らない");

  check(s_known_path_error_count == 0, "既知区間の経路が壁を通らず、経路の終わりの位置に着く");
  check(s_known_path_count > 0, "既知区間の経路が使われている");

  /* 見たことのある区画では新しい情報が増えないので、まとめて走っても 1 区画ずつ走っても
   * 通る道は同じはず。進んだ区画数の合計が同じかで確かめる */
  int total_one_by_one = goal_moves + start_moves;
  int total_known = goal_moves_known + start_moves_known + s_replayed_cells;
  printf("  進んだ区画数の合計: 1 区画ずつ %d、既知区間を使う %d\n", total_one_by_one, total_known);
  check(total_one_by_one == total_known, "既知区間を使っても、1 区画ずつと同じ道を通る");
}

/* ---------------------------------------------------------------------------
 * 5. 最短経路のテスト
 * ------------------------------------------------------------------------- */

/* 経路の中に motion があるか */
static bool path_has(const path_t *path, motion_t motion)
{
  for (int i = 0; i < path->count; i++) {
    if (path->steps[i].motion == motion) {
      return true;
    }
  }
  return false;
}

/* 経路の中に斜めの動きがあるか */
static bool path_has_diagonal(const path_t *path)
{
  if (path_has(path, MOTION_DIAG_IN45_LEFT) || path_has(path, MOTION_DIAG_IN45_RIGHT)) {
    return true;
  }
  if (path_has(path, MOTION_DIAG_IN135_LEFT) || path_has(path, MOTION_DIAG_IN135_RIGHT)) {
    return true;
  }
  return false;
}

/* 区画 (x, y) がゴールの中か */
static bool is_in_goal(int x, int y)
{
  if (x < s_goal_x || x >= s_goal_x + s_goal_width) {
    return false;
  }
  if (y < s_goal_y || y >= s_goal_y + s_goal_height) {
    return false;
  }
  return true;
}

/* 本当の迷路の壁を、全部「見た」ことにして地図に写す */
static void copy_true_maze_to_map(void)
{
  maze_map_init();
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      for (int i = 0; i < 4; i++) {
        direction_t dir = (direction_t)i;
        if (s_true_wall[x][y][dir]) {
          maze_map_set_wall(x, y, dir, WALL_EXIST);
        } else {
          maze_map_set_wall(x, y, dir, WALL_NONE);
        }
      }
    }
  }
}

/* 中に壁が 1 枚もない迷路（スタートの東の壁だけある）を、本当の迷路にする */
static void make_open_true_maze(void)
{
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      s_true_wall[x][y][DIR_NORTH] = (y == MAZE_SIZE - 1);
      s_true_wall[x][y][DIR_SOUTH] = (y == 0);
      s_true_wall[x][y][DIR_EAST] = (x == MAZE_SIZE - 1);
      s_true_wall[x][y][DIR_WEST] = (x == 0);
    }
  }
  s_true_wall[0][0][DIR_EAST] = true;
  s_true_wall[1][0][DIR_WEST] = true;
}

/* 最短経路を作って、本当の迷路の上で走らせて確かめる */
static void check_shortest(const char *name)
{
  static path_t s_path;
  bool found = path_make_shortest(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path);
  check(found, "最短経路が見つかる");
  if (!found) {
    return;
  }
  printf("  %s: 区画 %d 個を通る、動き %d 個\n", name, dijkstra_get_route_length(), s_path.count);
  print_path(&s_path);
  check(replay_path(&s_path, 0, 0, DIR_NORTH, true), "最短経路が壁を通らず、経路の終わりの位置に着く");
  check(is_in_goal(s_path.end_x, s_path.end_y), "最短経路の終わりはゴールの中");

  /* 小回りだけの最短経路（モード1 で今使うもの）も、同じ区画を通ってゴールに着く */
  static path_t s_small;
  int end_x = s_path.end_x;
  int end_y = s_path.end_y;
  check(path_make_shortest_small_turn(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_small),
        "小回りだけの最短経路も見つかる");
  check(replay_path(&s_small, 0, 0, DIR_NORTH, true), "小回りだけの最短経路が壁を通らず、経路の終わりの位置に着く");
  check(s_small.end_x == end_x && s_small.end_y == end_y, "小回りだけの最短経路の終わりも同じ区画");
  check(!path_has_diagonal(&s_small) && !path_has(&s_small, MOTION_BIG_TURN_LEFT_90)
        && !path_has(&s_small, MOTION_BIG_TURN_RIGHT_90) && !path_has(&s_small, MOTION_BIG_TURN_LEFT_180)
        && !path_has(&s_small, MOTION_BIG_TURN_RIGHT_180) && !path_has(&s_small, MOTION_UTURN),
        "小回りだけの最短経路には、大回り・斜め・U ターンがない");
}

/* 中に壁がない迷路で、最短経路が斜めを使うかを調べる */
static void test_shortest_open(void)
{
  printf("[5] 最短経路：中に壁がない迷路で、(0,0) → (6,6)\n");
  make_open_true_maze();
  copy_true_maze_to_map();
  s_goal_x = 6;
  s_goal_y = 6;
  s_goal_width = 1;
  s_goal_height = 1;
  check_shortest("壁なし");

  static path_t s_path;
  path_make_shortest(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path);
  check(path_has_diagonal(&s_path), "壁がなければ斜めに走る");
  check(path_has(&s_path, MOTION_DIAG_STRAIGHT), "斜めの直進がある");

  /* まだ見ていない壁は「ある」とみなすので、何も見ていなければ道はない */
  maze_map_init();
  check(!path_make_shortest(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path),
        "何も見ていない迷路では最短経路は見つからない");
}

/* 迷路ファイルの迷路で、探索のあとと、全部の壁を知っているときの最短経路を調べる */
static void test_shortest_maze(const char *file_path)
{
  printf("[5] 最短経路: %s\n", file_path);
  if (!load_maze_file(file_path)) {
    check(false, "迷路ファイルが読めない");
    return;
  }

  /* 探索（行き・帰り）で見た壁だけを使う */
  int goal_moves = 0;
  int start_moves = 0;
  explore_round_trip(true, &goal_moves, &start_moves);
  check_shortest("探索のあと");

  /* 迷路の壁を全部知っているとき */
  copy_true_maze_to_map();
  check_shortest("全部の壁を知っているとき");
}

/* ---------------------------------------------------------------------------
 * 6. 未知壁探索のテスト
 * ------------------------------------------------------------------------- */

/* 今の本当の迷路で、未知壁探索の流れ（行き → 見ていない壁を見に行く → 帰り）を走らせる。
 * 最後の最短経路が、全部の壁を知っているときと同じ速さなら true。verbose が true なら途中経過を表示する */
static bool run_unknown_wall_flow(bool verbose)
{
  /* 全部の壁を知っているときの最短経路のコスト（これが目標） */
  copy_true_maze_to_map();
  bool found_best = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
  check(found_best, "全部の壁を知っているとき、ゴールへの道がある");
  int best_cost = dijkstra_get_route_cost();

  /* 1. 行き：スタート → ゴール */
  maze_map_init();
  int x = 0;
  int y = 0;
  direction_t dir = DIR_NORTH;
  search_set_target(s_goal_x, s_goal_y, s_goal_width, s_goal_height);
  int moves_to_goal = run_to_target(&x, &y, &dir, true);
  check(moves_to_goal >= 0, "ゴールに着く");

  bool found_after_goal = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
  check(found_after_goal, "ゴールに着いた時点で、見た壁だけでゴールへの道がある");
  int cost_after_goal = dijkstra_get_route_cost();

  /* 2. ゴールで止まって、未知壁探索の経路を計算する（ダイクストラは止まっているときだけ） */
  int plan_count = 1;     /* 止まってダイクストラを計算した回数 */
  int blocked_count = 0;  /* そのうち、経路がふさがれたため */
  int uturn_count = 0;    /* そのうち、U ターンのついで */
  check(search_plan_unknown_walls(s_goal_x, s_goal_y, s_goal_width, s_goal_height), "ゴールで未知壁探索の経路を計算できる");

  /* 3. 経路の上の見ていない壁を、なくなるまで見に行く */
  int wall_count = 0;
  int moves_for_walls = 0;
  while (true) {
    if (wall_count >= 300) {
      check(false, "見に行く壁が 300 枚を超えた（終わらない）");
      break;
    }

    int cell_x;
    int cell_y;
    direction_t wall_dir;
    unknown_wall_result_t result = search_check_unknown_walls(&cell_x, &cell_y, &wall_dir);
    if (result == UNKNOWN_WALL_NONE) {
      break;  /* 経路の壁を全部見た */
    }
    if (result == UNKNOWN_WALL_NO_PLAN) {
      check(false, "未知壁探索の経路がない");
      break;
    }
    if (result == UNKNOWN_WALL_BLOCKED) {
      /* 経路がふさがれた：止まって計算し直す */
      plan_count = plan_count + 1;
      blocked_count = blocked_count + 1;
      search_plan_unknown_walls(s_goal_x, s_goal_y, s_goal_width, s_goal_height);
      continue;
    }

    /* 見ていない壁の手前の区画へ、止まらずに走る */
    wall_count = wall_count + 1;
    s_watch_enabled = true;
    s_watch_x = cell_x;
    s_watch_y = cell_y;
    s_watch_dir = wall_dir;
    s_stop_on_uturn = true;
    s_stopped_for_uturn = false;
    search_set_target(cell_x, cell_y, 1, 1);
    int moves = run_to_target(&x, &y, &dir, true);
    s_watch_enabled = false;
    s_stop_on_uturn = false;

    check(moves >= 0, "見に行く壁の手前の区画に着く");
    if (moves < 0) {
      break;
    }
    moves_for_walls = moves_for_walls + moves;

    if (s_stopped_for_uturn) {
      /* U ターンで止まったついでに計算し直す */
      plan_count = plan_count + 1;
      uturn_count = uturn_count + 1;
      search_plan_unknown_walls(s_goal_x, s_goal_y, s_goal_width, s_goal_height);
    }
  }

  /* 4. 見に行く壁がなくなった。実機は止まらずに帰りに入る。
   *    テストでは、この時点の最短経路を計算して、もう全部の壁を知っているときと同じになっているかを確かめる */
  if (verbose) {
    printf("  見に行く壁がなくなった：(%d,%d)\n", x, y);
  }
  static path_t s_shortest;
  bool found = path_make_shortest(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_shortest);
  check(found, "見に行く壁がなくなった時点で、最短経路を計算できる");
  int cost_final = dijkstra_get_route_cost();

  /* 5. 帰り：スタートへ（最短走行はスタートから始まるので） */
  search_set_target(0, 0, 1, 1);
  int moves_to_start = run_to_target(&x, &y, &dir, true);
  check(moves_to_start >= 0, "スタートに戻る");
  check(count_wrong_walls() == 0, "記録した壁が本当の迷路と同じ");

  /* 結果：見に行く壁がなくなった時点の最短経路が、全部の壁を知っているときと同じ速さか。
   * 帰りに新しい壁を見ても、最短経路は変わらない（もう一度計算して確かめる） */
  bool found_after_return = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
  check(found_after_return, "帰ったあとも、ゴールへの道がある");
  check(dijkstra_get_route_cost() == cost_final, "帰りに壁を見ても、見に行く壁がなくなった時点の最短経路は変わらない");
  if (verbose) {
    printf("  ゴールに着いた時点の最短経路のコスト %d → 見に行く壁がなくなった時点 %d（全部の壁を知っているとき %d）\n",
           cost_after_goal, cost_final, best_cost);
  }
  if (verbose) {
    printf("  見に行った壁 %d 枚、そのために進んだ区画 %d（既知区間でまとめて走った分を除く）\n",
           wall_count, moves_for_walls);
  }
  if (verbose) {
    printf("  止まってダイクストラを計算した回数 %d（ゴール 1、経路がふさがれた %d、U ターンのついで %d）\n",
           plan_count, blocked_count, uturn_count);
  }
  return cost_final == best_cost;
}

/* 迷路ファイルの迷路で、未知壁探索の流れを調べる */
static void test_unknown_wall_explore(const char *file_path)
{
  printf("[6] 未知壁探索: %s\n", file_path);
  if (!load_maze_file(file_path)) {
    check(false, "迷路ファイルが読めない");
    return;
  }
  check(run_unknown_wall_flow(true), "未知壁探索のあとの最短経路は、全部の壁を知っているときと同じ速さ");
}

/* ---------------------------------------------------------------------------
 * 7. ランダムな迷路のテスト
 * ------------------------------------------------------------------------- */

/* 決まった順に乱数を出す（毎回同じ迷路になるように、自分で作る） */
static unsigned int s_random_seed = 1;

/* 0〜(limit - 1) のでたらめな数（いつも同じ並びになるように、自分で計算する）。
 * limit で割った余りは 0〜(limit - 1) になる（% は一周させるためではなく、範囲を決めるために使う） */
static int random_below(int limit)
{
  s_random_seed = s_random_seed * 1103515245u + 12345u;
  return (int)((s_random_seed / 65536u) % (unsigned int)limit);
}

/* 本当の迷路の、(x, y) の dir 側の壁を取る（隣の区画の側も） */
static void open_true_wall(int x, int y, direction_t dir)
{
  s_true_wall[x][y][dir] = false;
  s_true_wall[maze_next_x(x, dir)][maze_next_y(y, dir)][maze_turn_back(dir)] = false;
}

/* ランダムな迷路を作って、本当の迷路にする（gen_maze.py と同じ作り方）。
 * ゴールは 2x2〜3x3 で、場所もランダム */
static void make_random_true_maze(void)
{
  static bool s_visited_cell[MAZE_SIZE][MAZE_SIZE];
  static int s_stack_x[MAZE_SIZE * MAZE_SIZE];
  static int s_stack_y[MAZE_SIZE * MAZE_SIZE];

  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      for (int i = 0; i < 4; i++) {
        s_true_wall[x][y][i] = true;
      }
      s_visited_cell[x][y] = false;
    }
  }

  /* 1. 穴掘り法で、全部の区画をつなぐ */
  int stack_count = 1;
  s_stack_x[0] = 0;
  s_stack_y[0] = 0;
  s_visited_cell[0][0] = true;
  while (stack_count > 0) {
    int x = s_stack_x[stack_count - 1];
    int y = s_stack_y[stack_count - 1];
    direction_t candidates[4];
    int candidate_count = 0;
    for (int i = 0; i < 4; i++) {
      direction_t dir = (direction_t)i;
      int next_x = maze_next_x(x, dir);
      int next_y = maze_next_y(y, dir);
      if (maze_is_inside(next_x, next_y) && !s_visited_cell[next_x][next_y]) {
        candidates[candidate_count] = dir;
        candidate_count = candidate_count + 1;
      }
    }
    if (candidate_count == 0) {
      stack_count = stack_count - 1;
      continue;
    }
    direction_t dir = candidates[random_below(candidate_count)];
    open_true_wall(x, y, dir);
    int next_x = maze_next_x(x, dir);
    int next_y = maze_next_y(y, dir);
    s_visited_cell[next_x][next_y] = true;
    s_stack_x[stack_count] = next_x;
    s_stack_y[stack_count] = next_y;
    stack_count = stack_count + 1;
  }

  /* 2. 壁を 15% くらい壊して、道が何通りもある迷路にする */
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      if (y + 1 < MAZE_SIZE && random_below(100) < 15) {
        open_true_wall(x, y, DIR_NORTH);
      }
      if (x + 1 < MAZE_SIZE && random_below(100) < 15) {
        open_true_wall(x, y, DIR_EAST);
      }
    }
  }

  /* 3. ゴール（中の壁はなし） */
  s_goal_width = 2 + random_below(2);
  s_goal_height = s_goal_width;
  s_goal_x = 4 + random_below(MAZE_SIZE - 8);
  s_goal_y = 4 + random_below(MAZE_SIZE - 8);
  for (int x = s_goal_x; x < s_goal_x + s_goal_width; x++) {
    for (int y = s_goal_y; y < s_goal_y + s_goal_height; y++) {
      if (y + 1 < s_goal_y + s_goal_height) {
        open_true_wall(x, y, DIR_NORTH);
      }
      if (x + 1 < s_goal_x + s_goal_width) {
        open_true_wall(x, y, DIR_EAST);
      }
    }
  }

  /* 4. スタートの東は必ず壁、北は必ず開いている（規則。東を閉じても閉じ込められないように北を開ける） */
  s_true_wall[0][0][DIR_EAST] = true;
  s_true_wall[1][0][DIR_WEST] = true;
  open_true_wall(0, 0, DIR_NORTH);
}

/* ランダムな迷路 maze_count 個で、全部の壁を知っているときの最短経路が走れるかを調べる */
static void test_random_mazes(int maze_count)
{
  printf("[7] ランダムな迷路 %d 個で、全部の壁を知っているときの最短経路\n", maze_count);
  static path_t s_path;
  int fail_count = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 1);
    make_random_true_maze();
    copy_true_maze_to_map();
    bool found = path_make_shortest(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path);
    bool ok = found;
    if (ok) {
      ok = replay_path(&s_path, 0, 0, DIR_NORTH, true);
    }
    if (ok) {
      ok = is_in_goal(s_path.end_x, s_path.end_y);
    }
    if (!ok) {
      fail_count = fail_count + 1;
      if (fail_count <= 3) {
        printf("  迷路 %d 番（乱数の種 %d）で失敗\n", maze_index, maze_index + 1);
      }
    }
  }
  printf("  失敗した迷路: %d 個\n", fail_count);
  check(fail_count == 0, "ランダムな迷路の最短経路が、どれも壁を通らずにゴールに着く");
}

/* ランダムな迷路 maze_count 個で、未知壁探索のあとの最短経路が、全部の壁を知っているときと同じ速さかを調べる */
static void test_unknown_wall_random(int maze_count)
{
  printf("[6] 未知壁探索：ランダムな迷路 %d 個\n", maze_count);
  int fail_count = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 10001);
    make_random_true_maze();
    if (!run_unknown_wall_flow(false)) {
      fail_count = fail_count + 1;
    }
  }
  printf("  全部の壁を知っているときより遅い最短経路になった迷路: %d 個\n", fail_count);
  check(fail_count == 0, "ランダムな迷路でも、未知壁探索のあとの最短経路は全部の壁を知っているときと同じ速さ");
}

/* ---------------------------------------------------------------------------
 * 8. ダイクストラの答えが本当の最短かのテスト
 * ------------------------------------------------------------------------- */

#define REF_COST_MAX 1000000  /* まだたどり着いていない */

/* 確認用の計算で使う、状態「点 + その点に来た向き（8 方向）」ごとのコスト [横の壁か][x][y][向き] */
static int s_ref_cost[2][MAZE_SIZE + 1][MAZE_SIZE + 1][8];

/* 確認用：ダイクストラとは別のやり方で、本当の最短のコストを求める。
 * 状態に「来た向き」まで含め、コストが変わらなくなるまで全部の状態を何度も見直す（遅いが確実）。
 * まだ見ていない壁は「ある」とみなす。道がなければ REF_COST_MAX */
static int calc_reference_cost(int goal_x, int goal_y, int goal_width, int goal_height)
{
  for (int row = 0; row < 2; row++) {
    for (int x = 0; x <= MAZE_SIZE; x++) {
      for (int y = 0; y <= MAZE_SIZE; y++) {
        for (int i = 0; i < 8; i++) {
          s_ref_cost[row][x][y][i] = REF_COST_MAX;
        }
      }
    }
  }

  /* ゴールの入口を、ゴールから外へ向かう向きでコスト 0 にする（dijkstra.c の add_goal_entrance と同じ） */
  for (int x = goal_x; x < goal_x + goal_width; x++) {
    for (int y = goal_y; y < goal_y + goal_height; y++) {
      for (int i = 0; i < 4; i++) {
        direction_t dir = (direction_t)i;
        if (maze_is_in_area(maze_next_x(x, dir), maze_next_y(y, dir), goal_x, goal_y, goal_width, goal_height)) {
          continue;
        }
        edge_t edge;
        dir8_t outward;
        if (dir == DIR_NORTH) {
          edge = make_edge(true, x, y + 1);
          outward = DIR8_NORTH;
        } else if (dir == DIR_SOUTH) {
          edge = make_edge(true, x, y);
          outward = DIR8_SOUTH;
        } else if (dir == DIR_EAST) {
          edge = make_edge(false, x + 1, y);
          outward = DIR8_EAST;
        } else {
          edge = make_edge(false, x, y);
          outward = DIR8_WEST;
        }
        if (is_open(edge, true)) {
          s_ref_cost[edge.is_row][edge.x][edge.y][outward] = 0;
        }
      }
    }
  }

  /* コストが変わらなくなるまで見直す */
  bool changed = true;
  while (changed) {
    changed = false;
    for (int row = 0; row < 2; row++) {
      for (int x = 0; x <= MAZE_SIZE; x++) {
        for (int y = 0; y <= MAZE_SIZE; y++) {
          for (int i = 0; i < 8; i++) {
            int cost = s_ref_cost[row][x][y][i];
            if (cost == REF_COST_MAX) {
              continue;
            }
            dir8_t came_dir = (dir8_t)i;
            edge_t from = make_edge(row == 1, x, y);
            direction_t crossing = crossing_after(from, came_dir);
            for (int dir_index = 0; dir_index < 8; dir_index++) {
              dir8_t dir = (dir8_t)dir_index;
              if (!is_forward_move(crossing, dir)) {
                continue;
              }
              edge_t to;
              int cell_x;
              int cell_y;
              if (!move_from(from, dir, &to, &cell_x, &cell_y)) {
                continue;
              }
              if (!maze_is_inside(cell_x, cell_y) || !is_open(to, true)) {
                continue;
              }
              int step_cost;
              if (dir == came_dir) {
                step_cost = COST_CONTINUE;
              } else if (is_orthogonal(dir)) {
                step_cost = COST_STRAIGHT;
              } else {
                step_cost = COST_DIAGONAL;
              }
              if (cost + step_cost < s_ref_cost[to.is_row][to.x][to.y][dir]) {
                s_ref_cost[to.is_row][to.x][to.y][dir] = cost + step_cost;
                changed = true;
              }
            }
          }
        }
      }
    }
  }

  /* スタートの区画の北の壁を、ゴール側から見て南向き（南・南東・南西）に通る状態のうち、いちばん安いもの */
  int best = s_ref_cost[1][0][1][DIR8_SOUTH];
  if (s_ref_cost[1][0][1][DIR8_SOUTH_EAST] < best) {
    best = s_ref_cost[1][0][1][DIR8_SOUTH_EAST];
  }
  if (s_ref_cost[1][0][1][DIR8_SOUTH_WEST] < best) {
    best = s_ref_cost[1][0][1][DIR8_SOUTH_WEST];
  }
  return best;
}

/* 区画に向き heading_in で入り、向き heading_out で出るときの、点から点への向き（8 方向）。
 * 同じなら縦横の向き、違えば 2 つを合わせた斜めの向き（例：北から入って東へ出るなら北東） */
static dir8_t hop_direction(direction_t heading_in, direction_t heading_out)
{
  bool north = (heading_in == DIR_NORTH || heading_out == DIR_NORTH);
  bool south = (heading_in == DIR_SOUTH || heading_out == DIR_SOUTH);
  bool east = (heading_in == DIR_EAST || heading_out == DIR_EAST);
  bool west = (heading_in == DIR_WEST || heading_out == DIR_WEST);
  if (north && east) {
    return DIR8_NORTH_EAST;
  }
  if (north && west) {
    return DIR8_NORTH_WEST;
  }
  if (south && east) {
    return DIR8_SOUTH_EAST;
  }
  if (south && west) {
    return DIR8_SOUTH_WEST;
  }
  if (north) {
    return DIR8_NORTH;
  }
  if (south) {
    return DIR8_SOUTH;
  }
  if (east) {
    return DIR8_EAST;
  }
  return DIR8_WEST;
}

/* dijkstra_find_route() が返した区画の並びを実際にたどったときのコストを、区画の並びから数え直す。
 * コストはゴール側から数える（ゴールに入る向きにそのまま進むところは COST_CONTINUE） */
static int count_route_cost(void)
{
  int length = dijkstra_get_route_length();
  int last = length - 1;  /* ゴールの区画の番号 */
  direction_t into_goal = maze_direction_between(dijkstra_get_route_x(last - 1), dijkstra_get_route_y(last - 1),
                                                 dijkstra_get_route_x(last), dijkstra_get_route_y(last));
  dir8_t previous = hop_direction(into_goal, into_goal);
  int cost = 0;
  /* スタートの区画（0 番）とゴールの区画（last 番）の間の区画を、ゴール側から順に */
  for (int i = last - 1; i >= 1; i--) {
    direction_t heading_in = maze_direction_between(dijkstra_get_route_x(i - 1), dijkstra_get_route_y(i - 1),
                                                    dijkstra_get_route_x(i), dijkstra_get_route_y(i));
    direction_t heading_out = maze_direction_between(dijkstra_get_route_x(i), dijkstra_get_route_y(i),
                                                     dijkstra_get_route_x(i + 1), dijkstra_get_route_y(i + 1));
    dir8_t hop = reverse_dir8(hop_direction(heading_in, heading_out));  /* ゴール側から見た向き */
    if (hop == reverse_dir8(previous)) {
      cost = cost + COST_CONTINUE;
    } else if (is_orthogonal(hop)) {
      cost = cost + COST_STRAIGHT;
    } else {
      cost = cost + COST_DIAGONAL;
    }
    previous = reverse_dir8(hop);
  }
  return cost;
}

/* ランダムな迷路 maze_count 個で、ダイクストラの答えのコストが本当の最短と同じか、
 * 返した区画の並びのコストが報告したコストと同じかを調べる */
static void test_dijkstra_exact(int maze_count)
{
  printf("[8] ダイクストラの答えが本当の最短か：ランダムな迷路 %d 個\n", maze_count);
  int not_shortest_count = 0;
  int wrong_cost_count = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 20001);
    make_random_true_maze();
    copy_true_maze_to_map();
    if (!dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true)) {
      not_shortest_count = not_shortest_count + 1;
      continue;
    }
    if (dijkstra_get_route_cost() != calc_reference_cost(s_goal_x, s_goal_y, s_goal_width, s_goal_height)) {
      not_shortest_count = not_shortest_count + 1;
    }
    if (dijkstra_get_route_cost() != count_route_cost()) {
      wrong_cost_count = wrong_cost_count + 1;
    }
  }
  printf("  本当の最短でなかった迷路: %d 個、返した道のコストと報告したコストが違った迷路: %d 個\n",
         not_shortest_count, wrong_cost_count);
  check(not_shortest_count == 0, "ダイクストラの答えは本当の最短");
  check(wrong_cost_count == 0, "返した道のコストと、報告したコストが同じ");
}

/* ---------------------------------------------------------------------------
 * 8b. 走行時間のコスト（dijkstra_set_time_cost_small_turn）のテスト
 * ------------------------------------------------------------------------- */

/* 確認用の計算で使う、状態「点 + 来た向き + その向きに続けて来た歩数」ごとのコスト */
static int s_ref_time_cost[2][MAZE_SIZE + 1][MAZE_SIZE + 1][8][DIJKSTRA_TABLE_LENGTH];

/* 走行時間のコストで、向き dir の区間の step 歩目のコスト（表より長い区間は表の最後の値） */
static int time_step_cost(dir8_t dir, int step)
{
  if (is_orthogonal(dir)) {
    return dijkstra_get_orth_step_cost(step);
  }
  return dijkstra_get_diag_step_cost(step);
}

/* 確認用：走行時間のコストで、本当の最短のコストを求める（calc_reference_cost の走行時間版）。
 * 状態に「来た向き」と「その向きに続けて来た歩数」まで含め、コストが変わらなくなるまで何度も見直す。
 * 歩数は表の長さ − 1 で止める（それより先は 1 歩のコストが同じなので、止めても答えは変わらない） */
static int calc_reference_time_cost(int goal_x, int goal_y, int goal_width, int goal_height)
{
  int last_step = DIJKSTRA_TABLE_LENGTH - 1;
  for (int row = 0; row < 2; row++) {
    for (int x = 0; x <= MAZE_SIZE; x++) {
      for (int y = 0; y <= MAZE_SIZE; y++) {
        for (int i = 0; i < 8; i++) {
          for (int k = 0; k <= last_step; k++) {
            s_ref_time_cost[row][x][y][i][k] = REF_COST_MAX;
          }
        }
      }
    }
  }

  /* ゴールの入口は、ゴールから外へ向かう向きで、歩数 0・コスト 0 */
  for (int x = goal_x; x < goal_x + goal_width; x++) {
    for (int y = goal_y; y < goal_y + goal_height; y++) {
      for (int i = 0; i < 4; i++) {
        direction_t dir = (direction_t)i;
        if (maze_is_in_area(maze_next_x(x, dir), maze_next_y(y, dir), goal_x, goal_y, goal_width, goal_height)) {
          continue;
        }
        edge_t edge;
        dir8_t outward;
        if (dir == DIR_NORTH) {
          edge = make_edge(true, x, y + 1);
          outward = DIR8_NORTH;
        } else if (dir == DIR_SOUTH) {
          edge = make_edge(true, x, y);
          outward = DIR8_SOUTH;
        } else if (dir == DIR_EAST) {
          edge = make_edge(false, x + 1, y);
          outward = DIR8_EAST;
        } else {
          edge = make_edge(false, x, y);
          outward = DIR8_WEST;
        }
        if (is_open(edge, true)) {
          s_ref_time_cost[edge.is_row][edge.x][edge.y][outward][0] = 0;
        }
      }
    }
  }

  bool changed = true;
  while (changed) {
    changed = false;
    for (int row = 0; row < 2; row++) {
      for (int x = 0; x <= MAZE_SIZE; x++) {
        for (int y = 0; y <= MAZE_SIZE; y++) {
          for (int i = 0; i < 8; i++) {
            for (int k = 0; k <= last_step; k++) {
              int cost = s_ref_time_cost[row][x][y][i][k];
              if (cost == REF_COST_MAX) {
                continue;
              }
              dir8_t came_dir = (dir8_t)i;
              edge_t from = make_edge(row == 1, x, y);
              direction_t crossing = crossing_after(from, came_dir);
              for (int dir_index = 0; dir_index < 8; dir_index++) {
                dir8_t dir = (dir8_t)dir_index;
                if (!is_forward_move(crossing, dir)) {
                  continue;
                }
                edge_t to;
                int cell_x;
                int cell_y;
                if (!move_from(from, dir, &to, &cell_x, &cell_y)) {
                  continue;
                }
                if (!maze_is_inside(cell_x, cell_y) || !is_open(to, true)) {
                  continue;
                }
                int next_k = 1;
                if (dir == came_dir) {
                  next_k = k + 1;  /* 同じ向きに続ける（ゴールの入口の歩数 0 からなら 1 歩目） */
                }
                int next_cost = cost + time_step_cost(dir, next_k);
                if (next_k > last_step) {
                  next_k = last_step;
                }
                if (next_cost < s_ref_time_cost[to.is_row][to.x][to.y][dir][next_k]) {
                  s_ref_time_cost[to.is_row][to.x][to.y][dir][next_k] = next_cost;
                  changed = true;
                }
              }
            }
          }
        }
      }
    }
  }

  /* スタートの区画の北の壁を、ゴール側から見て南向き（南・南東・南西）に通る状態のうち、いちばん安いもの */
  int best = REF_COST_MAX;
  for (int k = 0; k <= last_step; k++) {
    if (s_ref_time_cost[1][0][1][DIR8_SOUTH][k] < best) {
      best = s_ref_time_cost[1][0][1][DIR8_SOUTH][k];
    }
    if (s_ref_time_cost[1][0][1][DIR8_SOUTH_EAST][k] < best) {
      best = s_ref_time_cost[1][0][1][DIR8_SOUTH_EAST][k];
    }
    if (s_ref_time_cost[1][0][1][DIR8_SOUTH_WEST][k] < best) {
      best = s_ref_time_cost[1][0][1][DIR8_SOUTH_WEST][k];
    }
  }
  return best;
}

/* dijkstra_find_route() が返した区画の並びのコストを、走行時間のコストで数え直す（ゴール側から） */
static int count_route_time_cost(void)
{
  int length = dijkstra_get_route_length();
  int last = length - 1;
  dir8_t previous = DIR8_NONE;  /* ゴールの入口からは、どの向きでも 1 歩目から数える */
  int run_length = 0;
  int cost = 0;
  for (int i = last - 1; i >= 1; i--) {
    direction_t heading_in = maze_direction_between(dijkstra_get_route_x(i - 1), dijkstra_get_route_y(i - 1),
                                                    dijkstra_get_route_x(i), dijkstra_get_route_y(i));
    direction_t heading_out = maze_direction_between(dijkstra_get_route_x(i), dijkstra_get_route_y(i),
                                                     dijkstra_get_route_x(i + 1), dijkstra_get_route_y(i + 1));
    dir8_t hop = reverse_dir8(hop_direction(heading_in, heading_out));  /* ゴール側から見た向き */
    if (hop == previous) {
      run_length = run_length + 1;
    } else {
      run_length = 1;
    }
    cost = cost + time_step_cost(hop, run_length);
    previous = hop;
  }
  return cost;
}

/* 表の形：1 歩目からだんだん安くなり（増えない）、どれも 1 以上。速さを上げないときは 1 歩のコストが同じ */
static void test_time_cost_table(void)
{
  printf("[8b] 走行時間のコストの表\n");
  dijkstra_set_time_cost_small_turn(300.0f, 300.0f, 1000.0f);
  check(dijkstra_get_orth_step_cost(1) == 120, "300mm/s のまま 180mm は 0.6 秒 = コスト 120（5ms 単位）");
  check(dijkstra_get_orth_step_cost(10) == 120, "最高速が 300 なら、何歩目でも 120");
  check(dijkstra_get_diag_step_cost(1) == 120, "斜め（小回りのジグザグ）の 1 歩も 120");

  dijkstra_set_time_cost_small_turn(300.0f, 1000.0f, 3000.0f);
  bool ok = true;
  for (int k = 1; k < DIJKSTRA_TABLE_LENGTH; k++) {
    if (dijkstra_get_orth_step_cost(k) < 1 || dijkstra_get_diag_step_cost(k) < 1) {
      ok = false;
    }
    if (k > 1 && dijkstra_get_orth_step_cost(k) > dijkstra_get_orth_step_cost(k - 1)) {
      ok = false;
    }
  }
  check(ok, "表はどれも 1 以上で、k 歩目は k−1 歩目より大きくない");
  check(dijkstra_get_orth_step_cost(30) < dijkstra_get_orth_step_cost(1), "長い直線ほど 1 歩が安い（速度が乗る）");
  check(dijkstra_get_orth_step_cost(30) == 36, "1000mm/s に乗ったら 1 歩 180mm は 0.18 秒 = コスト 36");
  dijkstra_set_step_cost();
  check(!dijkstra_is_time_cost(), "dijkstra_set_step_cost() で 3 つの数のコストに戻る");
}

/* ランダムな迷路で、走行時間のコストでも、ダイクストラの答えが本当の最短か */
static void test_dijkstra_exact_time(int maze_count, float max_speed_mm_s, float accel_mm_s2)
{
  printf("[8b] 走行時間のコスト（最高速 %.0fmm/s）で本当の最短か：ランダムな迷路 %d 個\n", max_speed_mm_s, maze_count);
  int not_shortest_count = 0;
  int wrong_cost_count = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 30001);
    make_random_true_maze();
    copy_true_maze_to_map();
    dijkstra_set_time_cost_small_turn(300.0f, max_speed_mm_s, accel_mm_s2);
    bool found = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
    if (!found) {
      not_shortest_count = not_shortest_count + 1;
    } else {
      if (dijkstra_get_route_cost() != calc_reference_time_cost(s_goal_x, s_goal_y, s_goal_width, s_goal_height)) {
        not_shortest_count = not_shortest_count + 1;
      }
      if (dijkstra_get_route_cost() != count_route_time_cost()) {
        wrong_cost_count = wrong_cost_count + 1;
      }
    }
    dijkstra_set_step_cost();
  }
  printf("  本当の最短でなかった迷路: %d 個、返した道のコストと報告したコストが違った迷路: %d 個\n",
         not_shortest_count, wrong_cost_count);
  check(not_shortest_count == 0, "走行時間のコストでも、ダイクストラの答えは本当の最短");
  check(wrong_cost_count == 0, "走行時間のコストでも、返した道のコストと報告したコストが同じ");
}

/* 経路の中の、探索用の左右のターンの数 */
static int count_small_turns(const path_t *path)
{
  int turns = 0;
  for (int i = 0; i < path->count; i++) {
    if (path->steps[i].motion == MOTION_TURN_LEFT || path->steps[i].motion == MOTION_TURN_RIGHT) {
      turns = turns + 1;
    }
  }
  return turns;
}

/* 中に壁がない迷路で (7,7) へ：3 つの数のコストではジグザグ（斜め）になり、
 * 走行時間のコスト（小回り、直進 1000mm/s）では曲がる回数が少なくなる */
static void test_time_cost_no_zigzag(void)
{
  printf("[8b] 走行時間のコストでは、小回りのジグザグを選ばない\n");
  make_open_true_maze();
  copy_true_maze_to_map();
  s_goal_x = 7;
  s_goal_y = 7;
  s_goal_width = 2;
  s_goal_height = 2;
  static path_t s_path;

  dijkstra_set_step_cost();
  path_make_shortest_small_turn(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path);
  int turns_step_cost = count_small_turns(&s_path);

  dijkstra_set_time_cost_small_turn(300.0f, 1000.0f, 3000.0f);
  bool found = path_make_shortest_small_turn(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path);
  int turns_time_cost = count_small_turns(&s_path);
  dijkstra_set_step_cost();

  printf("  ターンの数：3 つの数のコスト %d 回、走行時間のコスト %d 回\n", turns_step_cost, turns_time_cost);
  check(found, "道が見つかる");
  check(turns_time_cost <= 2, "走行時間のコストでは、曲がるのは 2 回まで（直進 → 曲がる → 直進）");
  check(replay_path(&s_path, 0, 0, DIR_NORTH, true), "その経路は壁を通らず、ゴールに着く");
}

/* ---------------------------------------------------------------------------
 * 11. ターンごとの時間のコスト（dijkstra_set_time_cost_diagonal。KOGUMA の #100）のテスト
 * ------------------------------------------------------------------------- */

/* KOGUMA の最短走行の値の組 number（1 か 2。ファイルの先頭の RUN_SET_*）で、ターンごとの時間のコストにする */
static void set_turn_cost(int number)
{
  float turn_speed_mm_s = RUN_SET1_TURN_SPEED_MM_S;
  if (number == 2) {
    turn_speed_mm_s = RUN_SET2_TURN_SPEED_MM_S;
  }
  dijkstra_set_time_cost_diagonal(turn_speed_mm_s, RUN_SET_MAX_SPEED_MM_S, RUN_SET_ACCEL_MM_S2,
                                  RUN_SET_MAX_SPEED_MM_S, RUN_SET_DIAG_ACCEL_MM_S2);
}

/* 今の道（dijkstra.c の s_route_x/y）を大回り・斜めの動きにして、ターンごとの時間のコストで数える。
 * 大回り・斜めで走れない道（小回りが残る）なら -1。数えるときだけ、ターンごとの時間のコストにする */
static int count_route_turn_cost(int number)
{
  static path_t s_count_path;
  cost_model_t saved_model = s_cost_model;
  set_turn_cost(number);
  int cost = -1;
  if (path_make_shortest_from_route(&s_count_path)) {
    cost = dijkstra_count_path_cost(&s_count_path);
  }
  s_cost_model = saved_model;
  return cost;
}

/* ランダムな迷路で、ターンごとの時間のコストの道を調べる（値の組 number）。
 *   - 見つけた道を動きにすると、小回りが残らず、壁を通らずにゴールに着く
 *   - その動きの時間の見積もり（dijkstra_count_path_cost）が、ダイクストラが報告したコストと同じ
 *   - 3 つの数のコスト・小回りの走行時間の表で選んだ道より遅くない（同じ見積もりで比べる） */
static void test_turn_cost_random(int maze_count, int number)
{
  printf("[11] ターンごとの時間のコスト（組 %d の値）：ランダムな迷路 %d 個\n", number, maze_count);
  static path_t s_path;
  int fail_count = 0;
  int wrong_cost_count = 0;
  int slower_count = 0;
  int faster_than_step_count = 0;
  long gain_step_total = 0;
  long gain_small_total = 0;
  int compared_count = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 40001);
    make_random_true_maze();
    copy_true_maze_to_map();

    /* 比べるための道：3 つの数のコスト、小回りの走行時間の表 */
    dijkstra_set_step_cost();
    bool found_step = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
    int step_route_cost = count_route_turn_cost(number);
    dijkstra_set_time_cost_small_turn(TEST_SEARCH_SPEED_MM_S, TEST_SMALL_TURN_MAX_SPEED_MM_S, TEST_SMALL_TURN_ACCEL_MM_S2);
    bool found_small = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
    int small_route_cost = count_route_turn_cost(number);

    set_turn_cost(number);
    bool found = path_make_shortest(s_goal_x, s_goal_y, s_goal_width, s_goal_height, &s_path);
    bool ok = found && found_step && found_small;
    if (ok) {
      ok = (count_small_turns(&s_path) == 0) && replay_path(&s_path, 0, 0, DIR_NORTH, true)
           && is_in_goal(s_path.end_x, s_path.end_y);
    }
    if (!ok) {
      fail_count = fail_count + 1;
      dijkstra_set_step_cost();
      continue;
    }
    int best = dijkstra_get_route_cost();
    if (dijkstra_count_path_cost(&s_path) != best) {
      wrong_cost_count = wrong_cost_count + 1;
      if (wrong_cost_count <= 3) {
        printf("  迷路 %d 番：報告したコスト %d、動きの見積もり %d\n", maze_index, best, dijkstra_count_path_cost(&s_path));
      }
    }
    if ((step_route_cost >= 0 && step_route_cost < best) || (small_route_cost >= 0 && small_route_cost < best)) {
      slower_count = slower_count + 1;
    }
    if (step_route_cost >= 0 && small_route_cost >= 0) {
      compared_count = compared_count + 1;
      gain_step_total = gain_step_total + (step_route_cost - best);
      gain_small_total = gain_small_total + (small_route_cost - best);
      if (step_route_cost > best) {
        faster_than_step_count = faster_than_step_count + 1;
      }
    }
    dijkstra_set_step_cost();
  }
  printf("  うまくいかなかった迷路 %d 個、コストが見積もりと違った迷路 %d 個、ほかのコストの道より遅かった迷路 %d 個\n",
         fail_count, wrong_cost_count, slower_count);
  if (compared_count > 0) {
    printf("  見積もりで比べた %d 個の平均：3 つの数のコストの道より %.1fms、小回りの表の道より %.1fms 速い"
           "（3 つの数のコストの道より速くなった迷路 %d 個）\n",
           compared_count, 2.0 * gain_step_total / compared_count, 2.0 * gain_small_total / compared_count,
           faster_than_step_count);
  }
  check(fail_count == 0, "ターンごとの時間のコストの道は、大回り・斜めで走れて、壁を通らずにゴールに着く");
  check(wrong_cost_count == 0, "報告したコストが、動きにしたときの走る時間の見積もりと同じ");
  check(slower_count == 0, "ほかのコストで選んだ道より遅くない");
}

/* ターンごとの時間のコストの値を、手で計算した値と比べる（ターン 2000mm/s、直線 2500mm/s・25000mm/s²、斜め 2000mm/s・25000mm/s²。
 * 手で計算しやすいように、最短走行の値の組ではなく、ここで決めた値を使う） */
static void test_turn_cost_table(void)
{
  printf("[11] ターンごとの時間のコストの値\n");
  dijkstra_set_time_cost_diagonal(2000.0f, 2500.0f, 25000.0f, 2000.0f, 25000.0f);
  /* 止まる：2000mm/s から 30000mm/s²（SHORTEST_STOP_ACCEL_MM_S2）で 0.0667 秒 = 33.3（2ms 単位） */
  check(s_turn_stop_cost == 33, "ターンのあと止まる時間は 33（0.067 秒）");
  /* 180mm：2000 → 2500 に上げて戻すのに 45mm ずつ・0.02 秒ずつ、残り 90mm を 2500mm/s で 0.036 秒。合計 0.076 秒 = 38.0 */
  check(orth_straight_cost(2) == 38, "縦横の直線 180mm は 38（0.076 秒）");
  check(orth_straight_cost(0) == 0, "長さ 0 の直線は 0");
  /* 大回り 90°：前後 39.6 + 40.2 = 79.8mm（config/turn_params.h）を 2000mm/s で 0.0399 秒。
   * スラロームは最高 2000deg/s・25465deg/s² で、90° では最高まで上がりきらない（上げて戻すと 157°）ので
   * 2 × √(90 ÷ 25465) = 0.1189 秒。合計 0.1588 秒 = 79.4（2ms 単位） */
  check(s_turn_cost[TURN_BIG90] == 79, "大回り 90° は 79（0.159 秒）");
  dijkstra_set_step_cost();
}

/* 総当たり（下の test_turn_cost_brute_force）で使う */
static bool s_brute_used[MAZE_SIZE][MAZE_SIZE];  /* 今の道で通った区画 */
static int s_brute_best = -1;                    /* 見つけた一番速い道のコスト（なければ -1） */
static long s_brute_route_count = 0;             /* 調べた道の数 */
static int s_brute_number = 1;                   /* 最短走行の値の組 */

/* 今の道（dijkstra.c の s_route_x/y、length 区画）を 1 区画ずつ延ばして、ゴールまでの道を全部調べる
 * （同じ区画は 2 回通らない。1 歩目は北）。ゴールに入ったら、その道の時間の見積もりを数える */
static void brute_force_extend(int length)
{
  int x = s_route_x[length - 1];
  int y = s_route_y[length - 1];
  if (is_in_goal(x, y)) {
    s_route_length = length;
    s_brute_route_count = s_brute_route_count + 1;
    int cost = count_route_turn_cost(s_brute_number);
    if (cost >= 0 && (s_brute_best < 0 || cost < s_brute_best)) {
      s_brute_best = cost;
    }
    return;
  }
  if (length >= ROUTE_CELL_MAX) {
    return;
  }
  for (int i = 0; i < 4; i++) {
    direction_t dir = (direction_t)i;
    if (length == 1 && dir != DIR_NORTH) {
      continue;  /* スタートの区画からは北へ出る */
    }
    if (s_true_wall[x][y][dir]) {
      continue;
    }
    int next_x = maze_next_x(x, dir);
    int next_y = maze_next_y(y, dir);
    if (!maze_is_inside(next_x, next_y) || s_brute_used[next_x][next_y]) {
      continue;
    }
    s_brute_used[next_x][next_y] = true;
    s_route_x[length] = (uint8_t)next_x;
    s_route_y[length] = (uint8_t)next_y;
    brute_force_extend(length + 1);
    s_brute_used[next_x][next_y] = false;
  }
}

/* 左下の width × height 区画だけが開いた小さな迷路（中の壁はでたらめ。ループあり）を作る。ゴールも中にでたらめに置く */
static void make_small_true_maze(int width, int height)
{
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      for (int i = 0; i < 4; i++) {
        s_true_wall[x][y][i] = true;
      }
    }
  }
  for (int x = 0; x < width; x++) {
    for (int y = 0; y < height; y++) {
      if (x + 1 < width && random_below(100) < 80) {
        open_true_wall(x, y, DIR_EAST);
      }
      if (y + 1 < height && random_below(100) < 80) {
        open_true_wall(x, y, DIR_NORTH);
      }
    }
  }
  s_true_wall[0][0][DIR_EAST] = true;
  s_true_wall[1][0][DIR_WEST] = true;
  open_true_wall(0, 0, DIR_NORTH);

  /* ゴール：1×1 か 2×2。スタートの近く（スタートの区画と、そのすぐ北）にはしない */
  s_goal_width = 1 + random_below(2);
  s_goal_height = s_goal_width;
  s_goal_x = random_below(width - s_goal_width + 1);
  s_goal_y = 2 + random_below(height - s_goal_height - 1);
  for (int x = s_goal_x; x + 1 < s_goal_x + s_goal_width; x++) {
    for (int y = s_goal_y; y < s_goal_y + s_goal_height; y++) {
      open_true_wall(x, y, DIR_EAST);
    }
  }
  for (int x = s_goal_x; x < s_goal_x + s_goal_width; x++) {
    for (int y = s_goal_y; y + 1 < s_goal_y + s_goal_height; y++) {
      open_true_wall(x, y, DIR_NORTH);
    }
  }
}

/* 小さな迷路で、スタートからゴールまでの道を全部数え上げて、ターンごとの時間のコストのダイクストラが、
 * 本当に一番速い道（大回り・斜めで走れる道のうち、見積もりが一番小さいもの）を選ぶかを調べる */
static void test_turn_cost_brute_force(int maze_count, int width, int height, int number)
{
  printf("[11] ターンごとの時間のコスト（組 %d の値）で本当の最短か：%d×%d の迷路 %d 個で道を総当たり\n", number, width,
         height, maze_count);
  int not_shortest_count = 0;
  long route_total = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 50001);
    make_small_true_maze(width, height);
    copy_true_maze_to_map();

    set_turn_cost(number);
    bool found = dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
    int dijkstra_cost = -1;
    if (found) {
      dijkstra_cost = dijkstra_get_route_cost();
    }

    /* 総当たり */
    for (int x = 0; x < MAZE_SIZE; x++) {
      for (int y = 0; y < MAZE_SIZE; y++) {
        s_brute_used[x][y] = false;
      }
    }
    s_brute_used[0][0] = true;
    s_route_x[0] = 0;
    s_route_y[0] = 0;
    s_brute_best = -1;
    s_brute_route_count = 0;
    s_brute_number = number;
    brute_force_extend(1);
    route_total = route_total + s_brute_route_count;

    if (dijkstra_cost != s_brute_best) {
      not_shortest_count = not_shortest_count + 1;
      if (not_shortest_count <= 3) {
        printf("  迷路 %d 番：ダイクストラ %d、総当たり %d（道 %ld 本）\n", maze_index, dijkstra_cost, s_brute_best,
               s_brute_route_count);
      }
    }
    dijkstra_set_step_cost();
  }
  printf("  調べた道 %ld 本、一番速い道と違った迷路 %d 個\n", route_total, not_shortest_count);
  check(not_shortest_count == 0, "ターンごとの時間のコストのダイクストラは、総当たりの一番速い道と同じコスト");
}

/* ダイクストラ 1 回にかかる時間を、PC で測って表示する（確かめるのではなく、目安。実機は PC の数十倍遅い） */
static void print_dijkstra_time(int maze_count)
{
  printf("[11] ダイクストラ 1 回にかかる時間（PC、ランダムな迷路 %d 個の平均）\n", maze_count);
  double step_total_s = 0.0;
  double turn_total_s = 0.0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 60001);
    make_random_true_maze();
    copy_true_maze_to_map();
    dijkstra_set_step_cost();
    clock_t begin = clock();
    dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, false);
    step_total_s = step_total_s + (double)(clock() - begin) / CLOCKS_PER_SEC;
    set_turn_cost(1);
    begin = clock();
    dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true);
    turn_total_s = turn_total_s + (double)(clock() - begin) / CLOCKS_PER_SEC;
    dijkstra_set_step_cost();
  }
  printf("  3 つの数のコスト（未知壁探索）%.2fms、ターンごとの時間のコスト（最短走行の組 1）%.2fms\n",
         1000.0 * step_total_s / maze_count, 1000.0 * turn_total_s / maze_count);
}

/* ---------------------------------------------------------------------------
 * 12. 全面探索と行き止まり潰し（KOGUMA.Mk3.1 で足した。search.c の「全面探索」）
 * ------------------------------------------------------------------------- */

/* 行き止まり潰し：見た壁で袋小路になっている通路が、根元までまとめて外れるか。スタートとゴールは外さないか */
static void test_dead_end_fill(void)
{
  printf("[12] 行き止まり潰し\n");
  /* 東西の通路 (3,0)〜(5,0) の東の端 (5,0) が行き止まり。(3,0)・(4,0)・(5,0) の北と南は壁、(5,0) の東も壁。
   * (2,0) の北は見ていない（通れるとみなす）
   *   (2,0) - (3,0) - (4,0) - (5,0)|
   * (5,0) の出入口は西だけ → 外す。すると (4,0) は東が外した区画なので、出入口は西だけ → 外す。(3,0) も同じ。
   * (2,0) は北が通れるので外さない */
  maze_map_init();
  for (int x = 3; x <= 5; x++) {
    maze_map_set_wall(x, 0, DIR_NORTH, WALL_EXIST);
  }
  maze_map_set_wall(5, 0, DIR_EAST, WALL_EXIST);
  /* ゴールの区画 (8,8) も、3 方向を壁にしておく（ゴールの範囲は外さない） */
  maze_map_set_wall(8, 8, DIR_NORTH, WALL_EXIST);
  maze_map_set_wall(8, 8, DIR_EAST, WALL_EXIST);
  maze_map_set_wall(8, 8, DIR_SOUTH, WALL_EXIST);

  search_set_target_unvisited(7, 7, 2, 2);
  search_update_step_map();
  check(search_is_dead_end(5, 0), "通路の端 (5,0) を外す");
  check(search_is_dead_end(4, 0) && search_is_dead_end(3, 0), "通路の根元まで (4,0)・(3,0) も外す");
  check(!search_is_dead_end(2, 0), "北が見ていない（通れる）(2,0) は外さない");
  check(!search_is_dead_end(0, 0), "スタートの区画は外さない（東は壁で、出入口は北だけでも）");
  check(!search_is_dead_end(8, 8), "ゴールの範囲の区画は外さない");
  check(!search_is_in_target(5, 0), "外した区画は目的地にしない");
  check(search_is_in_target(2, 0), "まだ入っていない区画は目的地");
  check(!search_is_in_target(0, 0), "スタートの区画（入ったことになっている）は目的地にしない");
  check(search_get_target_cell_count() == MAZE_SIZE * MAZE_SIZE - 1 - 3, "目的地は、全部の区画から スタート と 外した 3 区画 を引いた数");

  /* 普通の目的地に戻すと、全面探索は終わる */
  search_set_target(7, 7, 2, 2);
  check(!search_is_target_unvisited(), "search_set_target() で全面探索は終わる");
  check(search_is_in_target(7, 7) && !search_is_in_target(2, 0), "目的地はゴールの範囲に戻る");
}

/* 本当の迷路で、(area_x, area_y) から幅 width・高さ height の場所のまわりを壁で囲む（中の壁はそのまま） */
static void close_true_area(int area_x, int area_y, int width, int height)
{
  for (int x = area_x; x < area_x + width; x++) {
    for (int y = area_y; y < area_y + height; y++) {
      for (int i = 0; i < 4; i++) {
        direction_t dir = (direction_t)i;
        int next_x = maze_next_x(x, dir);
        int next_y = maze_next_y(y, dir);
        if (maze_is_in_area(next_x, next_y, area_x, area_y, width, height)) {
          continue;  /* 中の壁 */
        }
        s_true_wall[x][y][dir] = true;
        if (maze_is_inside(next_x, next_y)) {
          s_true_wall[next_x][next_y][maze_turn_back(dir)] = true;
        }
      }
    }
  }
}

/* 全面探索を、今いる区画 (*x, *y)、向き *dir から、見に行く区画がなくなるまで走らせる（1 区画ずつ）。
 * 進んだ区画の数を返す。壁に突っ込んだら -1。*stopped には、残っている区画にたどり着けなくて止めたら true が入る */
static int run_unvisited_search(int *x, int *y, direction_t *dir, bool *stopped)
{
  int move_count = 0;
  *stopped = false;
  search_set_target_unvisited(s_goal_x, s_goal_y, s_goal_width, s_goal_height);

  while (move_count < MOVE_LIMIT) {
    bool wall_front = s_true_wall[*x][*y][*dir];
    bool wall_left = s_true_wall[*x][*y][maze_turn_left(*dir)];
    bool wall_right = s_true_wall[*x][*y][maze_turn_right(*dir)];
    maze_map_record_walls(*x, *y, *dir, wall_front, wall_left, wall_right);

    search_update_step_map();
    if (search_get_target_cell_count() == 0) {
      return move_count;  /* 見に行く区画がなくなった */
    }
    action_t action = search_decide_action(*x, *y, *dir);
    if (action == ACTION_STOP) {
      *stopped = true;  /* 残っている区画にたどり着けない（壁で閉じた区画がある） */
      return move_count;
    } else if (action == ACTION_TURN_LEFT) {
      *dir = maze_turn_left(*dir);
    } else if (action == ACTION_TURN_RIGHT) {
      *dir = maze_turn_right(*dir);
    } else if (action == ACTION_UTURN) {
      *dir = maze_turn_back(*dir);
    }
    if (s_true_wall[*x][*y][*dir]) {
      printf("  (%d,%d) で壁に突っ込んだ\n", *x, *y);
      return -1;
    }
    *x = maze_next_x(*x, *dir);
    *y = maze_next_y(*y, *dir);
    move_count = move_count + 1;
  }
  printf("  %d 区画動いても終わらなかった\n", MOVE_LIMIT);
  return -1;
}

/* 全部の壁を知っているときの最短経路のコスト（ターンごとの時間、組 1）。今の地図を本当の迷路で上書きするので、先に保存しておくこと */
static int true_shortest_cost(void)
{
  copy_true_maze_to_map();
  set_turn_cost(1);
  int cost = -1;
  if (dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true)) {
    cost = dijkstra_get_route_cost();
  }
  dijkstra_set_step_cost();
  return cost;
}

/* 今の地図（見た壁だけ。見ていない壁は「ある」）での最短経路のコスト（ターンごとの時間、組 1） */
static int recorded_shortest_cost(void)
{
  set_turn_cost(1);
  int cost = -1;
  if (dijkstra_find_route(s_goal_x, s_goal_y, s_goal_width, s_goal_height, true)) {
    cost = dijkstra_get_route_cost();
  }
  dijkstra_set_step_cost();
  return cost;
}

/* KOGUMA の「往復、復路で全面探索」（モード0 No.2）の流れ：スタート → ゴール → 全面探索 → スタート。
 * 走り終わったときの最短経路が、全部の壁を知っているときと同じ速さかを調べる。
 * 結果：0 なら成功、1 なら走れなかった、2 なら最短経路が違った。*cells には進んだ区画の数の合計、*skipped には外した区画の数 */
static int run_all_search_flow(int *cells, int *skipped, bool *stopped)
{
  maze_map_init();
  int x = 0;
  int y = 0;
  direction_t dir = DIR_NORTH;
  *cells = 0;
  *skipped = 0;
  *stopped = false;

  search_set_target(s_goal_x, s_goal_y, s_goal_width, s_goal_height);
  int moves = run_to_target(&x, &y, &dir, false);
  if (moves < 0) {
    return 1;
  }
  *cells = *cells + moves;

  moves = run_unvisited_search(&x, &y, &dir, stopped);
  if (moves < 0) {
    return 1;
  }
  *cells = *cells + moves;
  for (int cell_x = 0; cell_x < MAZE_SIZE; cell_x++) {
    for (int cell_y = 0; cell_y < MAZE_SIZE; cell_y++) {
      if (search_is_dead_end(cell_x, cell_y)) {
        *skipped = *skipped + 1;
      }
    }
  }

  search_set_target(0, 0, 1, 1);
  moves = run_to_target(&x, &y, &dir, false);
  if (moves < 0 || x != 0 || y != 0) {
    return 1;
  }
  *cells = *cells + moves;
  if (count_wrong_walls() != 0) {
    return 1;
  }

  int recorded_cost = recorded_shortest_cost();
  int true_cost = true_shortest_cost();
  if (recorded_cost != true_cost) {
    return 2;
  }
  return 0;
}

/* 迷路ファイルの迷路で、全面探索の流れを調べる */
static void test_all_search(const char *file_path)
{
  printf("[12] 全面探索（往復、復路で全面探索）: %s\n", file_path);
  if (!load_maze_file(file_path)) {
    check(false, "迷路ファイルが読めない");
    return;
  }
  int cells = 0;
  int skipped = 0;
  bool stopped = false;
  int result = run_all_search_flow(&cells, &skipped, &stopped);
  printf("  進んだ区画 %d、行き止まり潰しで外した区画 %d\n", cells, skipped);
  check(result != 1, "スタート → ゴール → 全面探索 → スタート と、壁に当たらずに走れて、記録した壁が本当の迷路と同じ");
  check(result != 2, "走り終わったときの最短経路が、全部の壁を知っているときと同じ速さ");
  check(!stopped, "この迷路では、見に行く区画がなくなって終わる（たどり着けない区画はない）");
}

/* 壁で閉じた場所があるとき：見に行けない区画が残るので、ACTION_STOP になってスタートへ帰る（KOGUMA と同じ逃げ道）。
 * 閉じた区画が 1 つだけなら、まわりから 4 方向の壁が見えて行き止まり潰しで外れるので、目的地は残らない。
 * 2×2 の場所を閉じると、中の壁は見えない（通れるとみなす）ので、外れずに目的地として残る */
static void test_all_search_closed_cell(void)
{
  printf("[12] 全面探索：壁で閉じた場所があるとき\n");

  /* 1 区画だけ閉じる：(12,3) */
  make_open_true_maze();
  s_goal_x = 7;
  s_goal_y = 7;
  s_goal_width = 2;
  s_goal_height = 2;
  close_true_area(12, 3, 1, 1);
  int cells = 0;
  int skipped = 0;
  bool stopped = false;
  int result = run_all_search_flow(&cells, &skipped, &stopped);
  printf("  1 区画を閉じたとき：進んだ区画 %d、外した区画 %d\n", cells, skipped);
  check(!stopped && search_is_dead_end(12, 3), "閉じた 1 区画は、まわりの壁が見えたら外れるので、見に行く区画がなくなって終わる");
  check(result == 0, "スタートへ帰れて、最短経路も全部の壁を知っているときと同じ");

  /* 2×2 を閉じる：(12,3)〜(13,4) */
  make_open_true_maze();
  close_true_area(12, 3, 2, 2);
  result = run_all_search_flow(&cells, &skipped, &stopped);
  printf("  2×2 を閉じたとき：進んだ区画 %d、外した区画 %d\n", cells, skipped);
  check(stopped, "閉じた 2×2 にはたどり着けないので、ACTION_STOP で全面探索を終える");
  check(!maze_map_is_visited(12, 3) && !maze_map_is_visited(13, 4), "閉じた場所には入っていない");
  check(result == 0, "そのあとスタートへ帰れて、最短経路も全部の壁を知っているときと同じ");
}

/* ランダムな迷路で、全面探索の流れを調べる */
static void test_all_search_random(int maze_count)
{
  printf("[12] 全面探索：ランダムな迷路 %d 個\n", maze_count);
  int fail_count = 0;
  int wrong_cost_count = 0;
  long cell_total = 0;
  long skipped_total = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 70001);
    make_random_true_maze();
    int cells = 0;
    int skipped = 0;
    bool stopped = false;
    int result = run_all_search_flow(&cells, &skipped, &stopped);
    if (result == 1) {
      fail_count = fail_count + 1;
    }
    if (result == 2) {
      wrong_cost_count = wrong_cost_count + 1;
    }
    cell_total = cell_total + cells;
    skipped_total = skipped_total + skipped;
  }
  printf("  走れなかった迷路 %d 個、最短経路が違った迷路 %d 個。平均：進んだ区画 %.1f、外した区画 %.1f\n", fail_count,
         wrong_cost_count, (double)cell_total / maze_count, (double)skipped_total / maze_count);
  check(fail_count == 0, "どの迷路でも、壁に当たらずに全面探索してスタートへ帰れる");
  check(wrong_cost_count == 0, "どの迷路でも、走り終わったときの最短経路が、全部の壁を知っているときと同じ速さ");
}

/* ---------------------------------------------------------------------------
 * 13. 既知区間の経路を斜めで走る（path_make_known_diagonal。KOGUMA.Mk3.1 で足した）
 * ------------------------------------------------------------------------- */

/* 本当の迷路を「全部壁」にしてから、区画の並び cells_x/y（count 個）を通路として開け、全部見た地図にする。
 * 最後の区画だけは「まだ入っていない」にする（既知区間の経路はその手前で終わる） */
static void make_known_corridor(const int cells_x[], const int cells_y[], int count)
{
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      for (int i = 0; i < 4; i++) {
        s_true_wall[x][y][i] = true;
      }
    }
  }
  for (int i = 0; i + 1 < count; i++) {
    direction_t dir = maze_direction_between(cells_x[i], cells_y[i], cells_x[i + 1], cells_y[i + 1]);
    open_true_wall(cells_x[i], cells_y[i], dir);
  }
  copy_true_maze_to_map();
  for (int i = 0; i + 1 < count; i++) {
    maze_map_set_visited(cells_x[i], cells_y[i], true);
  }
  maze_map_set_visited(cells_x[count - 1], cells_y[count - 1], false);
  search_set_target(cells_x[count - 1], cells_y[count - 1], 1, 1);
  search_update_step_map();
}

static void test_known_diagonal(void)
{
  printf("[13] 既知区間の経路を斜めで走る\n");
  static path_t s_path;

  /* 例 1：北に 2 区画、右・左・右・左・右のジグザグ、東に 2 区画
   *   既知区間：直進4 → 右 → 左 → 右 → 左 → 右 → 直進4
   *   斜めにすると：直進3 → 斜め入り 45° 右 → 斜めの直進 3 → 斜め出 45° 右 → 直進3 */
  {
    const int xs[] = { 0, 0, 0, 1, 1, 2, 2, 3, 4, 5 };
    const int ys[] = { 0, 1, 2, 2, 3, 3, 4, 4, 4, 4 };
    make_known_corridor(xs, ys, 10);
    path_make_known_diagonal(0, 0, DIR_NORTH, &s_path);
    print_path(&s_path);
    check(s_path.count == 5, "例1：動きは 5 つ");
    check(step_is(&s_path, 0, MOTION_STRAIGHT, 3), "例1：直進 3 半区画");
    check(step_is(&s_path, 1, MOTION_DIAG_IN45_RIGHT, 0), "例1：斜め入り 45° 右");
    check(step_is(&s_path, 2, MOTION_DIAG_STRAIGHT, 3), "例1：斜めの直進 3");
    check(step_is(&s_path, 3, MOTION_DIAG_OUT45_RIGHT, 0), "例1：斜め出 45° 右");
    check(step_is(&s_path, 4, MOTION_STRAIGHT, 3), "例1：直進 3 半区画");
    check(s_path.end_x == 5 && s_path.end_y == 4 && s_path.end_dir == DIR_EAST, "例1：(5,4) の入口で東向きに終わる");
    check(replay_path(&s_path, 0, 0, DIR_NORTH, false), "例1：壁を通らず、経路の終わりの位置に着く");
  }

  /* 例 2：ジグザグのまま既知区間が終わる（直進4 → 右 → 左 → 右 で、まだ入っていない区画の入口）。
   * 斜めから出られないので、小回りのまま（path_make_known と同じ） */
  {
    const int xs[] = { 0, 0, 0, 1, 1, 2 };
    const int ys[] = { 0, 1, 2, 2, 3, 3 };
    make_known_corridor(xs, ys, 6);
    static path_t s_known;
    path_make_known(0, 0, DIR_NORTH, &s_known);
    path_make_known_diagonal(0, 0, DIR_NORTH, &s_path);
    print_path(&s_path);
    bool same = (s_path.count == s_known.count);
    for (int i = 0; same && i < s_path.count; i++) {
      if (s_path.steps[i].motion != s_known.steps[i].motion || s_path.steps[i].half_count != s_known.steps[i].half_count) {
        same = false;
      }
    }
    check(same, "例2：斜めのまま終わるので、小回りに戻す（path_make_known と同じ経路）");
    check(!known_path_has_diagonal(&s_path), "例2：斜めの動きがない");
  }

  /* 例 3：同じ向きに 2 回曲がって直進（直進4 → 右 → 右 → 直進2）。大回り 180° にはしないので、
   * 135° に入るには、そのあともターンが続かないといけない。ここでは小回りのまま */
  {
    const int xs[] = { 0, 0, 0, 1, 1, 1 };
    const int ys[] = { 0, 1, 2, 2, 1, 0 };
    make_known_corridor(xs, ys, 6);
    path_make_known_diagonal(0, 0, DIR_NORTH, &s_path);
    print_path(&s_path);
    check(!known_path_has_diagonal(&s_path), "例3：直進 → 右 → 右 → 直進 は小回りのまま");
  }

  /* 例 4：直進4 → 右 → 右 → 左 → 直進4。斜め入り 135° 右 → 斜め出 45° 左 になる */
  {
    const int xs[] = { 0, 0, 0, 1, 1, 2, 3, 4 };
    const int ys[] = { 0, 1, 2, 2, 1, 1, 1, 1 };
    make_known_corridor(xs, ys, 8);
    path_make_known_diagonal(0, 0, DIR_NORTH, &s_path);
    print_path(&s_path);
    check(step_is(&s_path, 0, MOTION_STRAIGHT, 3), "例4：直進 3 半区画");
    check(step_is(&s_path, 1, MOTION_DIAG_IN135_RIGHT, 0), "例4：斜め入り 135° 右");
    check(step_is(&s_path, 2, MOTION_DIAG_OUT45_LEFT, 0), "例4：斜め出 45° 左");
    check(step_is(&s_path, 3, MOTION_STRAIGHT, 3), "例4：直進 3 半区画");
    check(replay_path(&s_path, 0, 0, DIR_NORTH, false), "例4：壁を通らず、経路の終わりの位置に着く");
  }
}

/* 迷路ファイルの迷路で、既知区間の経路を斜めで走っても、1 区画ずつと同じ道を通るか */
static void test_explore_known_diagonal(const char *file_path)
{
  printf("[13] 既知区間を斜めで走る探索のシミュレーション: %s\n", file_path);
  if (!load_maze_file(file_path)) {
    check(false, "迷路ファイルが読めない");
    return;
  }
  int goal_moves = 0;
  int start_moves = 0;
  explore_round_trip(false, &goal_moves, &start_moves);

  s_known_path_diagonal = true;
  s_known_diagonal_count = 0;
  s_known_path_count = 0;
  s_big_turn_count = 0;
  s_known_path_error_count = 0;
  s_replayed_cells = 0;
  int goal_moves_known = 0;
  int start_moves_known = 0;
  explore_round_trip(true, &goal_moves_known, &start_moves_known);
  s_known_path_diagonal = false;

  int total_one_by_one = goal_moves + start_moves;
  int total_known = goal_moves_known + start_moves_known + s_replayed_cells;
  printf("  既知区間を %d 回まとめて走り、そのうち斜めが入ったのは %d 回。進んだ区画数の合計: 1 区画ずつ %d、斜め %d\n",
         s_known_path_count, s_known_diagonal_count, total_one_by_one, total_known);
  check(s_big_turn_count == 0, "既知区間の経路には大回りが入らない");
  check(s_known_path_error_count == 0, "斜めの既知区間の経路が壁を通らず、経路の終わりの位置に着く");
  check(total_one_by_one == total_known, "斜めで走っても、1 区画ずつと同じ道を通る");
}

/* ランダムな迷路で、斜めの既知区間の経路が壁を通らず、1 区画ずつと同じ道を通るか */
static void test_explore_known_diagonal_random(int maze_count)
{
  printf("[13] 既知区間を斜めで走る探索：ランダムな迷路 %d 個\n", maze_count);
  int error_count = 0;
  int different_count = 0;
  int diagonal_total = 0;
  for (int maze_index = 0; maze_index < maze_count; maze_index++) {
    s_random_seed = (unsigned int)(maze_index + 80001);
    make_random_true_maze();
    int goal_moves = 0;
    int start_moves = 0;
    explore_round_trip(false, &goal_moves, &start_moves);

    s_known_path_diagonal = true;
    s_known_diagonal_count = 0;
    s_known_path_error_count = 0;
    s_replayed_cells = 0;
    int goal_moves_known = 0;
    int start_moves_known = 0;
    explore_round_trip(true, &goal_moves_known, &start_moves_known);
    s_known_path_diagonal = false;

    if (s_known_path_error_count != 0) {
      error_count = error_count + 1;
    }
    if (goal_moves + start_moves != goal_moves_known + start_moves_known + s_replayed_cells) {
      different_count = different_count + 1;
    }
    diagonal_total = diagonal_total + s_known_diagonal_count;
  }
  printf("  壁を通った・位置が合わなかった迷路 %d 個、道が違った迷路 %d 個、斜めが入った既知区間の経路 %d 本\n",
         error_count, different_count, diagonal_total);
  check(error_count == 0, "どの迷路でも、斜めの既知区間の経路が壁を通らず、経路の終わりの位置に着く");
  check(different_count == 0, "どの迷路でも、斜めで走っても 1 区画ずつと同じ道を通る");
  check(diagonal_total > 0, "斜めの既知区間の経路が使われている");
}

/* ---------------------------------------------------------------------------
 * main
 * ------------------------------------------------------------------------- */

int main(void)
{
  test_maze_map();
  test_undo_recent_cells();
  test_step_map();
  test_known_path();
  test_explore("mazes/maze16_center2x2.txt");
  test_explore("mazes/maze16_offset3x3.txt");
  test_explore("mazes/maze16_alljapan2025.txt");
  test_shortest_open();
  test_shortest_maze("mazes/maze16_center2x2.txt");
  test_shortest_maze("mazes/maze16_offset3x3.txt");
  test_shortest_maze("mazes/maze16_alljapan2025.txt");
  test_unknown_wall_explore("mazes/maze16_center2x2.txt");
  test_unknown_wall_explore("mazes/maze16_offset3x3.txt");
  test_unknown_wall_explore("mazes/maze16_alljapan2025.txt");
  test_random_mazes(2000);
  test_unknown_wall_random(300);
  test_dijkstra_exact(500);
  test_time_cost_table();
  test_dijkstra_exact_time(100, 1000.0f, 3000.0f);
  test_dijkstra_exact_time(100, 2000.0f, 10000.0f);
  test_time_cost_no_zigzag();
  test_turn_cost_table();
  test_turn_cost_random(300, 1);
  test_turn_cost_random(300, 2);
  test_turn_cost_brute_force(300, 6, 6, 1);
  test_turn_cost_brute_force(100, 7, 6, 2);
  print_dijkstra_time(100);
  test_dead_end_fill();
  test_all_search("mazes/maze16_center2x2.txt");
  test_all_search("mazes/maze16_offset3x3.txt");
  test_all_search("mazes/maze16_alljapan2025.txt");
  test_all_search_closed_cell();
  test_all_search_random(300);
  test_known_diagonal();
  test_explore_known_diagonal("mazes/maze16_center2x2.txt");
  test_explore_known_diagonal("mazes/maze16_offset3x3.txt");
  test_explore_known_diagonal("mazes/maze16_alljapan2025.txt");
  test_explore_known_diagonal_random(300);

  if (s_fail_count == 0) {
    printf("すべて成功\n");
    return 0;
  }
  printf("失敗: %d 件\n", s_fail_count);
  return 1;
}
