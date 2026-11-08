/*
 * app.c
 * App 全体の入口。main.c から app_init() と app_loop() が呼ばれる
 *
 * ToraNeko.Mk3 の App/app.c と同じ役目のファイル。
 * 今はステップ0（プロジェクトを作って、ビルドが通るところまで）なので、まだ何もしない。
 * ステップ1 で LED とシリアル出力、ステップ2 で制御周期の割り込み、… と、ToraNeko.Mk3 と同じ順に足していく
 * （software/main-controller/README.md の「実装の進め方」）。
 */
#include "app.h"

/* 起動時に 1 回だけ呼ばれる（周辺機能の初期化 MX_*_Init が終わったあと） */
void app_init(void)
{
}

/* main() の while(1) から繰り返し呼ばれる */
void app_loop(void)
{
}
