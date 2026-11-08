/*
 * shortest.h
 * 最短走行（モード1。中身は shortest.c）
 * ToraNeko.Mk3 の App/sys/shortest.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で、shortest_set_route_cost() を足した（B15）。
 */
#ifndef SYS_SHORTEST_H
#define SYS_SHORTEST_H

#include <stdbool.h>

/* 最短走行の結果 */
typedef enum {
  SHORTEST_RESULT_DONE,      /* ゴールの区画のまん中で止まった */
  SHORTEST_RESULT_FAILSAFE,  /* フェイルセーフで止まった */
  SHORTEST_RESULT_NO_ROUTE,  /* スタートからゴールへの道がない（走らない） */
} shortest_result_t;

bool shortest_has_route(int number, int goal_x, int goal_y);
shortest_result_t shortest_run(int number, int goal_x, int goal_y);
void shortest_set_route_cost(int number);

#endif /* SYS_SHORTEST_H */
