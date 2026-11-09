/*
 * sensor_noise.h
 * 吸引のときに壁センサにのるノイズを調べる（モード5 No.8。中身は sys/sensor_noise.c）
 * KOGUMA.MkIIl の Core/Src/SensorNoise.c をもとに、KOGUMA.Mk3.1 で新しく作った（ToraNeko.Mk3 にはない）。
 */
#ifndef SYS_SENSOR_NOISE_H
#define SYS_SENSOR_NOISE_H

void sensor_noise_run(void);
void sensor_noise_print(void);

#endif /* SYS_SENSOR_NOISE_H */
