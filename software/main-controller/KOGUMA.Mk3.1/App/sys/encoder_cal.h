/*
 * encoder_cal.h
 * エンコーダの歪みを測る（モード6 No.6。MODES.md）
 * ToraNeko.Mk3 の App/sys/encoder_cal.h（ecd8e10）をもとにした。KOGUMA.Mk3.1 で変えたところはない。
 */
#ifndef SYS_ENCODER_CAL_H
#define SYS_ENCODER_CAL_H

void encoder_cal_run(void);
void encoder_cal_record(void);

#endif /* SYS_ENCODER_CAL_H */
