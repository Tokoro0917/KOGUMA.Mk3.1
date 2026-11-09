/*
 * sensor_record.h
 * センサ値の記録（モード5 No.2。区画のまん中で壁センサを 1 秒測り、平均をモード7 No.1 で出す）
 * ToraNeko.Mk3 の App/sys/sensor_record.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で変えたところはない。
 */
#ifndef SYS_SENSOR_RECORD_H
#define SYS_SENSOR_RECORD_H

void sensor_record_run(void);
void sensor_record_print(void);

#endif /* SYS_SENSOR_RECORD_H */
