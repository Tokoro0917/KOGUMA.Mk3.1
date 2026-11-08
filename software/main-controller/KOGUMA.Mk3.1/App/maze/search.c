/*
 * search.c
 * 探索（足立法）：歩数マップを作って、次にどちらへ進むかを決める
 *
 * ■ もとにしたもの
 * ToraNeko.Mk3 の software/main-controller/TORANEKO.Mk3/App/maze/search.c（ecd8e10、2026-11-04）。
 * KOGUMA.Mk3.1 で変えたところ：座標の幅のコメントを 16×16 に直した。
 * 全面探索（目的地を「まだ見ていない区画すべて」にする）と、行き止まり潰しを足した（下の「全面探索」。KOGUMA.MkIIl の
 * Maze_Unkown_ALL_ModeSet・Maze_Gool_Setting・Maze_DeadEnd_Fill。ToraNeko.Mk3 には持ち込まれていない）。
 *
 * ■ 歩数マップ
 * 各区画に「目的地まであと何区画か」を書いた地図。目的地の区画が 0 で、
 * 1 区画離れるごとに 1 ずつ増える。まだ見ていない壁は「ない」ものとして数える
 * （足立法。見ていない場所は通れると考えて、近そうな道から試す）。
 *
 * ■ 次の動きの決め方
 * 前・左・右・後ろの区画のうち、壁がなくて歩数がいちばん小さい区画へ進む。
 * 同じ歩数なら 前 → 左 → 右 → 後ろ の順に選ぶ（KOGUMA と同じ。直進を優先すると速い）。
 */
#include "maze/search.h"

#include "maze/dijkstra.h"

/* 目的地（長方形の範囲）。(左下の x, 左下の y) と 幅・高さ [区画] */
static int s_target_x = 0;
static int s_target_y = 0;
static int s_target_width = 1;
static int s_target_height = 1;

/* 全面探索のとき true（下の「全面探索」）。そのときは上の長方形は使わない */
static bool s_target_is_unvisited = false;

/* 全面探索で、行き止まり潰しで守る大会のゴールの範囲（search_set_target_unvisited() で受け取る） */
static int s_goal_x = 0;
static int s_goal_y = 0;
static int s_goal_width = 1;
static int s_goal_height = 1;

/* 行き止まり潰しで「最短経路に乗らない」とわかった区画（全面探索の目的地にしない）。
 * 走るときに通れるかどうかには使わない（壁の地図は書き換えない） */
static bool s_dead_end[MAZE_SIZE][MAZE_SIZE];

/* 直前の search_update_step_map() で、目的地になった区画の数 */
static int s_target_cell_count = 0;

static uint16_t s_step_map[MAZE_SIZE][MAZE_SIZE];

/* 歩数マップを作るときの「次に調べる区画」の列（待ち行列）。
 * 1 つの区画は 1 回しか入らないので、大きさは区画の数あれば足りる。
 * 座標は 0〜15 なので、メモリを節約するため 1 バイトの整数にする */
static uint8_t s_queue_x[MAZE_SIZE * MAZE_SIZE];
static uint8_t s_queue_y[MAZE_SIZE * MAZE_SIZE];

/* 目的地を決める。ゴールなら大会のゴールの範囲、帰りならスタート (0, 0, 1, 1) を渡す。
 * 全面探索（search_set_target_unvisited()）をしていたら、それも終わりにする */
void search_set_target(int x, int y, int width, int height)
{
  s_target_x = x;
  s_target_y = y;
  s_target_width = width;
  s_target_height = height;
  s_target_is_unvisited = false;
}

/* (x, y) が目的地か。
 * ふつうは、決めた長方形の範囲の中か。全面探索のときは、まだ入っていない区画で、行き止まり潰しで外していないか
 * （スタートの区画は、いつも入ったことになっているので目的地にならない） */
bool search_is_in_target(int x, int y)
{
  if (s_target_is_unvisited) {
    if (!maze_is_inside(x, y)) {
      return false;
    }
    if (maze_map_is_visited(x, y)) {
      return false;
    }
    if (s_dead_end[x][y]) {
      return false;
    }
    return true;
  }
  return maze_is_in_area(x, y, s_target_x, s_target_y, s_target_width, s_target_height);
}

/* ===========================================================================
 * 全面探索（KOGUMA.MkIIl のモード0 No.2・No.5 の「エセ全面探索」）
 *
 * 目的地を「まだ入っていない区画すべて」にして、歩数マップで一番近いものから見に行く。
 * 区画に入ると壁を記録して「入った」になるので、目的地は 1 つずつ減っていく。
 *
 * ■ 行き止まり潰し（KOGUMA.MkIIl の Maze_DeadEnd_Fill）
 * 「見た壁だけで、出入口が 1 つ以下」の区画は、入ってきた辺からしか出られないので、
 * スタートでもゴールでもなければ、最短経路の上には絶対に乗らない。見に行っても得をしないので、目的地から外す。
 * 外した区画を通れないものとして数えると、その隣も行き止まりになることがあるので、続けて外していく
 * （袋小路の通路が、根元までまとめて外れる）。
 *   - 見ていない壁は「ない」（通れる）として数える。外しすぎる向きには間違えない
 *   - 外した区画は、目的地にしないだけ。歩数マップで通れるかどうかには使わないので、間違えて外しても、
 *     その区画を見に行かないだけで、道がふさがることはない
 *   - 歩数マップを作るたびに、最初から作り直す（間違いが残り続けない）
 *   - スタートの区画と、大会のゴールの範囲は外さない
 *
 * ■ 使い方（sys/explore.c から）
 *   1. search_set_target_unvisited() で全面探索にする
 *   2. いつもの探索と同じく、壁を記録して search_update_step_map()、search_decide_action() で進む
 *   3. search_get_target_cell_count() が 0 になったら（見に行く区画がなくなったら）、search_set_target() で
 *      スタートを目的地にして帰る。search_decide_action() が ACTION_STOP を返したとき
 *      （残っている区画にたどり着けない。壁で閉じた区画があるとき）も、同じくスタートへ帰る
 * ======================================================================== */

/* 全面探索にする。(goal_x, goal_y, goal_width, goal_height) は大会のゴールの範囲（行き止まり潰しで外さないため） */
void search_set_target_unvisited(int goal_x, int goal_y, int goal_width, int goal_height)
{
  s_target_is_unvisited = true;
  s_goal_x = goal_x;
  s_goal_y = goal_y;
  s_goal_width = goal_width;
  s_goal_height = goal_height;
}

/* 全面探索をしているか */
bool search_is_target_unvisited(void)
{
  return s_target_is_unvisited;
}

/* 直前の search_update_step_map() で、目的地になった区画の数（全面探索で、見に行く区画が残っているか） */
int search_get_target_cell_count(void)
{
  return s_target_cell_count;
}

/* 行き止まり潰しで外した区画か（PC のテストと、出力で見るため） */
bool search_is_dead_end(int x, int y)
{
  if (!maze_is_inside(x, y)) {
    return false;
  }
  return s_dead_end[x][y];
}

/* 行き止まり潰しで外してはいけない区画か（スタートと、大会のゴールの範囲） */
static bool is_protected_cell(int x, int y)
{
  if (x == 0 && y == 0) {
    return true;
  }
  return maze_is_in_area(x, y, s_goal_x, s_goal_y, s_goal_width, s_goal_height);
}

/* 区画 (x, y) から出られる辺の数。「壁がある」辺と、「その先を外した」辺は数えない（見ていない壁は通れるとみなす） */
static int count_open_sides(int x, int y)
{
  int count = 0;
  for (int i = 0; i < 4; i++) {
    direction_t dir = (direction_t)i;
    if (maze_map_get_wall(x, y, dir) == WALL_EXIST) {
      continue;
    }
    int next_x = maze_next_x(x, dir);
    int next_y = maze_next_y(y, dir);
    if (!maze_is_inside(next_x, next_y)) {
      continue;
    }
    if (s_dead_end[next_x][next_y]) {
      continue;
    }
    count = count + 1;
  }
  return count;
}

/* 区画 (x, y) が、外してよい行き止まりか */
static bool is_dead_end_cell(int x, int y)
{
  if (s_dead_end[x][y]) {
    return false;  /* もう外してある */
  }
  if (is_protected_cell(x, y)) {
    return false;
  }
  if (count_open_sides(x, y) <= 1) {
    return true;
  }
  return false;
}

/* 行き止まり潰しを、最初から作り直す。
 * 外した区画は、隣を調べ直すために待ち行列（s_queue_x / s_queue_y。歩数マップと同じものを使い回す）に入れる。
 * 1 つの区画は 1 回しか外さないので、待ち行列は区画の数あれば足りる */
static void update_dead_ends(void)
{
  int queue_read = 0;
  int queue_write = 0;

  /* 1. 全部外していない状態にしてから、見た壁だけで行き止まりになっている区画を外す */
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      s_dead_end[x][y] = false;
    }
  }
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      if (is_dead_end_cell(x, y)) {
        s_dead_end[x][y] = true;
        s_queue_x[queue_write] = (uint8_t)x;
        s_queue_y[queue_write] = (uint8_t)y;
        queue_write = queue_write + 1;
      }
    }
  }

  /* 2. 外した区画の隣を調べ直す（外した区画は通れないものとして数えるので、隣も行き止まりになることがある） */
  while (queue_read < queue_write) {
    int x = s_queue_x[queue_read];
    int y = s_queue_y[queue_read];
    queue_read = queue_read + 1;

    for (int i = 0; i < 4; i++) {
      int next_x = maze_next_x(x, (direction_t)i);
      int next_y = maze_next_y(y, (direction_t)i);
      if (!maze_is_inside(next_x, next_y)) {
        continue;
      }
      if (is_dead_end_cell(next_x, next_y)) {
        s_dead_end[next_x][next_y] = true;
        s_queue_x[queue_write] = (uint8_t)next_x;
        s_queue_y[queue_write] = (uint8_t)next_y;
        queue_write = queue_write + 1;
      }
    }
  }
}

/* (x, y) から向き dir へ進めるか（足立法なので、まだ見ていない壁は通れるとみなす） */
static bool can_pass(int x, int y, direction_t dir)
{
  if (maze_map_get_wall(x, y, dir) == WALL_EXIST) {
    return false;
  }
  return true;
}

/* 歩数マップを作り直す。
 * 目的地の区画を 0 にして、そこから壁のないところへ 1 区画ずつ広げていく（幅優先探索） */
void search_update_step_map(void)
{
  int queue_read = 0;   /* 次に取り出す位置 */
  int queue_write = 0;  /* 次に入れる位置 */

  /* 0. 全面探索なら、先に行き止まり潰しを作り直す（目的地から外す区画が決まる） */
  if (s_target_is_unvisited) {
    update_dead_ends();
  }

  /* 1. 全部「たどり着けない」にしてから、目的地の区画を 0 にして待ち行列に入れる */
  for (int x = 0; x < MAZE_SIZE; x++) {
    for (int y = 0; y < MAZE_SIZE; y++) {
      if (search_is_in_target(x, y)) {
        s_step_map[x][y] = 0;
        s_queue_x[queue_write] = (uint8_t)x;
        s_queue_y[queue_write] = (uint8_t)y;
        queue_write = queue_write + 1;
      } else {
        s_step_map[x][y] = STEP_MAX;
      }
    }
  }
  s_target_cell_count = queue_write;

  /* 2. 待ち行列から 1 つ取り出し、壁のない隣の区画で、まだ歩数が決まっていないところに
   *    「自分の歩数 + 1」を書いて待ち行列に入れる。待ち行列が空になるまで繰り返す */
  while (queue_read < queue_write) {
    int x = s_queue_x[queue_read];
    int y = s_queue_y[queue_read];
    queue_read = queue_read + 1;

    for (int i = 0; i < 4; i++) {
      direction_t dir = (direction_t)i;
      if (!can_pass(x, y, dir)) {
        continue;
      }
      int next_x = maze_next_x(x, dir);
      int next_y = maze_next_y(y, dir);
      if (!maze_is_inside(next_x, next_y)) {
        continue;
      }
      if (s_step_map[next_x][next_y] != STEP_MAX) {
        continue;  /* もう歩数が決まっている（もっと近い道がある） */
      }
      s_step_map[next_x][next_y] = s_step_map[x][y] + 1;
      s_queue_x[queue_write] = (uint8_t)next_x;
      s_queue_y[queue_write] = (uint8_t)next_y;
      queue_write = queue_write + 1;
    }
  }
}

/* 歩数マップの値を読む。迷路の外なら STEP_MAX */
uint16_t search_get_step(int x, int y)
{
  if (!maze_is_inside(x, y)) {
    return STEP_MAX;
  }
  return s_step_map[x][y];
}

/* (x, y) から向き dir に進んだ先の区画の歩数。壁があって進めなければ STEP_MAX */
static uint16_t get_step_toward(int x, int y, direction_t dir)
{
  if (!can_pass(x, y, dir)) {
    return STEP_MAX;
  }
  return search_get_step(maze_next_x(x, dir), maze_next_y(y, dir));
}

/* 区画 (x, y) で向き dir を向いているときの、次の動きを決める。
 * 先に search_update_step_map() で歩数マップを作っておくこと */
action_t search_decide_action(int x, int y, direction_t dir)
{
  uint16_t step_front = get_step_toward(x, y, dir);
  uint16_t step_left = get_step_toward(x, y, maze_turn_left(dir));
  uint16_t step_right = get_step_toward(x, y, maze_turn_right(dir));
  uint16_t step_back = get_step_toward(x, y, maze_turn_back(dir));

  /* いちばん小さい歩数を探す */
  uint16_t step_min = step_front;
  if (step_left < step_min) {
    step_min = step_left;
  }
  if (step_right < step_min) {
    step_min = step_right;
  }
  if (step_back < step_min) {
    step_min = step_back;
  }

  if (step_min == STEP_MAX) {
    return ACTION_STOP;  /* どの方向からも目的地にたどり着けない */
  }

  /* 同じ歩数なら 前 → 左 → 右 → 後ろ の順に選ぶ */
  if (step_front == step_min) {
    return ACTION_FORWARD;
  }
  if (step_left == step_min) {
    return ACTION_TURN_LEFT;
  }
  if (step_right == step_min) {
    return ACTION_TURN_RIGHT;
  }
  return ACTION_UTURN;
}

/* 未知壁探索で使う経路（ダイクストラで計算した、スタートからゴールまでの区画の並び）。
 * 座標は 0〜15 なので、メモリを節約するため 1 バイトの整数にする */
static uint8_t s_plan_x[MAZE_SIZE * MAZE_SIZE];
static uint8_t s_plan_y[MAZE_SIZE * MAZE_SIZE];
static int s_plan_length = 0;

/* 未知壁探索の経路を計算して覚える（KOGUMA の Maze_Unknown_Walls_Plan）。重いので、止まっているときに呼ぶ。
 *
 * まだ見ていない壁を「ない」とみなしてダイクストラで最短経路を探す。
 * 見ていない壁は通れるとみなす、いちばん都合のいい考え方なので、この経路の壁を全部見て
 * 全部なかったら、本当の迷路でもこれより速い道はない。
 * ゴールへの道がなければ false（普通は起きない）。 */
bool search_plan_unknown_walls(int goal_x, int goal_y, int goal_width, int goal_height)
{
  s_plan_length = 0;
  if (!dijkstra_find_route(goal_x, goal_y, goal_width, goal_height, false)) {
    return false;
  }
  int length = dijkstra_get_route_length();
  for (int i = 0; i < length; i++) {
    s_plan_x[i] = (uint8_t)dijkstra_get_route_x(i);
    s_plan_y[i] = (uint8_t)dijkstra_get_route_y(i);
  }
  s_plan_length = length;
  return true;
}

/* 覚えた経路の i 番目の区画から i+1 番目の区画へ向かう向き */
static direction_t plan_direction(int i)
{
  return maze_direction_between(s_plan_x[i], s_plan_y[i], s_plan_x[i + 1], s_plan_y[i + 1]);
}

/* 覚えた経路の壁を見直す（KOGUMA の Maze_Unknown_Wall_Scan と Maze_Unknown_Wall_Is_Wall）。軽いので、走りながら呼んでよい。
 *
 *   1. 経路の上の壁に「ある」ものが 1 枚でもあれば UNKNOWN_WALL_BLOCKED（止まって計算し直す）。
 *      見に行った壁だけでなく、途中で見えた経路の上の壁も調べる
 *   2. そうでなく、「まだ見ていない」壁があれば UNKNOWN_WALL_FOUND。スタート側から見て最初の 1 枚について、
 *      手前の区画を (cell_x, cell_y) に、その区画から見た壁の向きを wall_dir に入れる。
 *      その区画まで行けば、センサでその壁を見られる（まだ入ったことのない区画なので、入ると前・左・右の壁を見る）
 *   3. どちらでもなければ UNKNOWN_WALL_NONE（経路の壁は全部見てあり、全部なかった）。
 * 経路を覚えていなければ UNKNOWN_WALL_NO_PLAN（「全部見た」と区別するため）。 */
unknown_wall_result_t search_check_unknown_walls(int *cell_x, int *cell_y, direction_t *wall_dir)
{
  /* 0. 経路を覚えていない（search_plan_unknown_walls をまだ呼んでいないか、ゴールへの道がなかった） */
  if (s_plan_length == 0) {
    return UNKNOWN_WALL_NO_PLAN;
  }

  /* 1. ふさがれていないか */
  for (int i = 0; i + 1 < s_plan_length; i++) {
    if (maze_map_get_wall(s_plan_x[i], s_plan_y[i], plan_direction(i)) == WALL_EXIST) {
      return UNKNOWN_WALL_BLOCKED;
    }
  }

  /* 2. まだ見ていない壁 */
  for (int i = 0; i + 1 < s_plan_length; i++) {
    direction_t dir = plan_direction(i);
    if (maze_map_get_wall(s_plan_x[i], s_plan_y[i], dir) == WALL_UNKNOWN) {
      *cell_x = s_plan_x[i];
      *cell_y = s_plan_y[i];
      *wall_dir = dir;
      return UNKNOWN_WALL_FOUND;
    }
  }

  return UNKNOWN_WALL_NONE;
}
