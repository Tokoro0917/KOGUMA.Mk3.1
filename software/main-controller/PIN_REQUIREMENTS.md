# ピン割り当て

KOGUMA.Mk3 メイン基板（STM32F446RE、LQFP64）のピン割り当てです。
**今の表は、KOGUMA.MkIIl の `KOGUMA.MkIIl.ioc` とコードから読んだもので、回路図ではまだ確かめていません。** 回路図を受け取ったら「回路図」の列を埋め、違うところは直します。

新しいラベルは、ToraNeko.Mk3 の `App/` と同じ名前にそろえたもの（[CUBEMX_SETUP.md](CUBEMX_SETUP.md) の 3-1）。

## 表

| ピン | 機能 | 新しいラベル | KOGUMA.MkIIl のラベル | 使い方 | 回路図 |
|---|---|---|---|---|---|
| PH0 / PH1 | RCC_OSC_IN / OUT | - | - | HSE 10MHz | 未確認 |
| PA13 / PA14 | SWDIO / SWCLK | - | - | 書き込み・デバッグ | 未確認 |
| PA2 | USART2_TX（Single Wire） | - | - | PC へのログ出力（115200bps） | 未確認 |
| PA5 / PA6 / PA7 | SPI1 SCK / MISO / MOSI | - | - | IMU（LSM6DSR） | 未確認 |
| PA4 | GPIO 出力 | `IMU_CS` | `ICM_NSS` | IMU の CS | 未確認 |
| PC10 / PC11 / PB0 | SPI3 SCK / MISO / MOSI | - | - | エンコーダ（AS5047P ×2） | 未確認 |
| PC5 | GPIO 出力 | `ENC_L_CS` | `ENCO_L_NSS` | 左エンコーダの CS | 未確認 |
| PA15 | GPIO 出力 | `ENC_R_CS` | `ENCO_R_NSS` | 右エンコーダの CS | 未確認 |
| PC6 / PC7 / PC8 / PC9 | TIM3 CH1〜CH4（PWM） | - | - | モーター。`motor.c` では CH1・CH2 が左、CH3・CH4 が右と思われる | 未確認（ドライバの型番と IN の並び） |
| PB8 | TIM2 CH1（PWM） | - | - | 吸引ファン | 未確認 |
| PB9 | TIM4 CH4（PWM） | - | - | ブザー | 未確認 |
| PB1 | ADC1_IN9 | - | - | 電池の電圧（Rank 1、バッファ `[0]`） | 未確認（分圧の抵抗。KOGUMA は 47k / 10k として計算） |
| PC0 | ADC1_IN10 | - | - | 壁センサの受光（Rank 2、`[1]`） | 未確認 |
| PC1 | ADC1_IN11 | - | - | 壁センサの受光（Rank 3、`[2]`） | 未確認 |
| PC2 | ADC1_IN12 | - | - | 壁センサの受光（Rank 4、`[3]`） | 未確認 |
| PC3 | ADC1_IN13 | - | - | 壁センサの受光（Rank 5、`[4]`） | 未確認 |
| PA1 | GPIO 出力 | `WALL_LED_A` | `WallLED_12` | 壁センサの発光（1・2 の組） | 未確認 |
| PA0 | GPIO 出力 | `WALL_LED_B` | `WallLED_34` | 壁センサの発光（3・4 の組） | 未確認 |
| PB12 / PB13 / PB14 / PB15 | GPIO 出力 | `LED1`〜`LED4` | 同じ | UI の LED | 未確認 |
| PB4 / PB5 / PB6 / PB7 | GPIO 出力 | `LED5`〜`LED8` | 同じ | UI の LED | 未確認 |
| PA3 / PC15 / PC14 / PC13 | GPIO 出力 | `LEDF1`〜`LEDF4` | 同じ | 壁の表示の LED（`Wallsensor.c`） | 未確認 |
| PB3 | GPIO 入力 | `SW_USER` | （CubeMX では未設定。コードでは `SW`） | スイッチ S2。外の 10k（R31）で 3.3V に引き上げ、押すと 0 | 未確認 |
| PA10 / PA11 / PA12 | GPIO 出力 | - | - | **用途がわからない**（ラベルなし、コードからも見つからない） | **要確認** |

## 壁センサの番号（KOGUMA.MkIIl の `PL_sensor.c` から）

`g_sensor_on[0]` = バッファ `[4]`（PC3）、`[1]` = `[3]`（PC2）、`[2]` = `[2]`（PC1）、`[3]` = `[1]`（PC0）。
どれが左前・左・右・右前かと、発光の組（A・B）との組み合わせは、ステップ6で KOGUMA.MkIIl の `Wallsensor.c` と回路図から確かめる。

## ToraNeko.Mk3 との違い（コードを持ってくるときに直すところ）

| 項目 | KOGUMA.Mk3 | ToraNeko.Mk3 |
|---|---|---|
| マイコン | STM32F446RE（180MHz） | STM32F411CEU6（100MHz） |
| タイマーのクロック | APB1 90MHz、APB2 180MHz | 全部 100MHz |
| ログの UART | USART2（PA2）、DMA1 Stream6 | USART1（PA9）、DMA2 Stream7 |
| IMU の SPI | SPI1（PA5〜7） | SPI1（PB3〜5） |
| エンコーダの SPI | SPI3 | SPI2 |
| モーターの PWM | TIM3 CH1〜4（50kHz、ARR 200） | TIM4 CH1〜4（100kHz、ARR 100） |
| 吸引ファン | TIM2 CH1 | TIM2 CH2 |
| 制御周期 | TIM11（2kHz。ToraNeko と同じ。KOGUMA.MkIIl は TIM6・1kHz） | TIM11（2kHz） |
| 壁センサの待ち時間 | TIM8 | TIM3 |
| ADC のチャンネル | IN9〜13 | IN2〜6 |
| LED | 8 個＋壁の表示 4 個 | 7 個 |
| ブザー | あり（TIM4 CH4） | なし |
| スイッチ | S2（PB3、押すと 0） | SW1（PA12、押すと 1） |
