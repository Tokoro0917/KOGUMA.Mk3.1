/*
 * sensor_view.h
 * センサ値の表示（モード5 No.1）と、壁までの距離の表示（モード5 No.3）。MODES.md
 * ToraNeko.Mk3 の App/sys/sensor_view.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で、部品の確かめ（モード5 No.7）を足した。
 */
#ifndef SYS_SENSOR_VIEW_H
#define SYS_SENSOR_VIEW_H

void sensor_view_run(void);
void sensor_view_distance_run(void);
void sensor_view_parts_run(void);

#endif /* SYS_SENSOR_VIEW_H */
