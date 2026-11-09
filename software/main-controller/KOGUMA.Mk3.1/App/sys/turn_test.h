/*
 * turn_test.h
 * 最短走行のターンを 1 つずつ走らせて調整する（モード3 No.1〜7・No.9〜15）、斜めの直進のテスト（モード4 No.2・3）
 * ToraNeko.Mk3 の App/sys/turn_test.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で、斜めの直進のテストを
 * turn_test_run_diag() に分けた（モード4 に移したため）。
 */
#ifndef SYS_TURN_TEST_H
#define SYS_TURN_TEST_H

#include <stdbool.h>

void turn_test_run(int number);
void turn_test_run_diag(bool use_correction);
void turn_test_run_1000(int number);

#endif /* SYS_TURN_TEST_H */
