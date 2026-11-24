# ピン割り当て

KOGUMA.Mk3 メイン基板（STM32F446RE、LQFP64）のピン割り当てです。
表は、KOGUMA.MkIIl の `KOGUMA.MkIIl.ioc` とコードから読んだものです。2026-11-25 に、KOGUMA Mk3 の基板データ（KOGUMA.MkIIl `90f1b71` の `hardware/ecad/KOGUMA-CHAN.MkII_v2.kicad_pcb`。作者が「Mk3 の基板」と確認したもの）で、マイコン（IC1）の 64 ピンのつながり（ネット名）を読み、「回路図」の列に書きました。**全部、表のとおりでした。**

基板データの読み方：`.kicad_pcb` の IC1 の各パッドに、つながっているネットの名前が書いてある（回路図と同じ名前）。ほかの部品も、同じネットの名前でたどった。

新しいラベルは、ToraNeko.Mk3 の `App/` と同じ名前にそろえたもの（[CUBEMX_SETUP.md](CUBEMX_SETUP.md) の 3-1）。

## 表

| ピン | 機能 | 新しいラベル | KOGUMA.MkIIl のラベル | 使い方 | 回路図 |
|---|---|---|---|---|---|
| PH0 / PH1 | RCC_OSC_IN / OUT | - | - | HSE 10MHz | OK（Y1 CSTNE10M0G52Z000R0、10MHz） |
| PA13 / PA14 | SWDIO / SWCLK | - | - | 書き込み・デバッグ | OK（J8 の 6 ピン） |
| PA2 | USART2_TX（Single Wire） | - | - | PC へのログ出力（115200bps） | OK（ネット TX。J8 の 2 番ピン〈書き込みのコネクタと同じ〉に出ている） |
| PA5 / PA6 / PA7 | SPI1 SCK / MISO / MOSI | - | - | IMU（LSM6DSR） | OK（IMU-SCK・IMU-MISO・IMU-MOSI。LSM6DSRXTR） |
| PA4 | GPIO 出力 | `IMU_CS` | `ICM_NSS` | IMU の CS | OK（IMU-NSS） |
| PC10 / PC11 / PB0 | SPI3 SCK / MISO / MOSI | - | - | エンコーダ（AS5047P ×2） | OK（ENCO-SCK・ENCO-MISO・ENCO-MOSI） |
| PC5 | GPIO 出力 | `ENC_L_CS` | `ENCO_L_NSS` | 左エンコーダの CS | OK（ENCO-L-NSS） |
| PA15 | GPIO 出力 | `ENC_R_CS` | `ENCO_R_NSS` | 右エンコーダの CS | OK（ENCO-R-NSS） |
| PC6 / PC7 / PC8 / PC9 | TIM3 CH1〜CH4（PWM） | - | - | モーター。`motor.c` では CH1・CH2 が左、CH3・CH4 が右と思われる | OK。PC6・PC7 = PWML-1・PWML-2（左のドライバ IC3 の IN1・IN2）、PC8・PC9 = PWMR-1・PWMR-2（右の IC4）。ドライバは **MP6550GG-P**（電源は VBAT）。電流を測る出力（ISEN、ネット MotorSen-L・R）はコンデンサだけで、マイコンにはつながっていない |
| PB8 | TIM2 CH1（PWM） | - | - | 吸引ファン | OK（FunPWM）。N チャネル FET（Q2 IRLML6344）でファンの − 側を GND につなぐ形。ファンの + 側は VBAT（J7）。ゲートは 10kΩ で GND に引き下げ |
| PB9 | TIM4 CH4（PWM） | - | - | ブザー | OK（Buzzer）。FET（Q3 2N7002）で、+5V につないだブザー（LS1 SMT-0540-S-R）を鳴らす |
| PB1 | ADC1_IN9 | - | - | 電池の電圧（Rank 1、バッファ `[0]`） | OK（BATT）。分圧は R3 47kΩ（VBAT 側）・R4 10kΩ（GND 側）で、`drv/wall.c` の 57 / 10 と同じ |
| PC0 | ADC1_IN10 | - | - | 壁センサの受光（Rank 2、`[1]`） | OK（Wallsen_FR = 右前） |
| PC1 | ADC1_IN11 | - | - | 壁センサの受光（Rank 3、`[2]`） | OK（Wallsen_L = 左） |
| PC2 | ADC1_IN12 | - | - | 壁センサの受光（Rank 4、`[3]`） | OK（Wallsen_R = 右） |
| PC3 | ADC1_IN13 | - | - | 壁センサの受光（Rank 5、`[4]`） | OK（Wallsen_FL = 左前） |
| PA1 | GPIO 出力 | `WALL_LED_A` | `WallLED_12` | 壁センサの発光（1・2 の組） | OK（WallLED_L）。定電流の IC（U6 NJU6080）で、左前の受光のとなりの発光（D3）と、右の受光のとなりの発光（D4）を点ける。`drv/wall.c` の組A（受光 1 左前・2 右）と同じ |
| PA0 | GPIO 出力 | `WALL_LED_B` | `WallLED_34` | 壁センサの発光（3・4 の組） | OK（WallLED_R）。U7 NJU6080 で、左の受光のとなり（D6）と右前の受光のとなり（D5）を点ける。組B（受光 3 左・4 右前）と同じ |
| PB12 / PB13 / PB14 / PB15 | GPIO 出力 | `LED1`〜`LED4` | 同じ | UI の LED | OK（LED1〜LED4。ピンから LED のアノードにつながる〈High で点く〉） |
| PB4 / PB5 / PB6 / PB7 | GPIO 出力 | `LED5`〜`LED8` | 同じ | UI の LED | OK（LED5〜LED8。High で点く） |
| PA3 / PC15 / PC14 / PC13 | GPIO 出力 | `LEDF1`〜`LEDF4` | 同じ | 壁の表示の LED（`Wallsensor.c`） | OK（LEDF1〜LEDF4。High で点く） |
| PB3 | GPIO 入力 | `SW_USER` | （CubeMX では未設定。コードでは `SW`） | スイッチ S2。外の 10k（R31）で 3.3V に引き上げ、押すと 0 | OK（SW）。R31 10kΩ で 3.3V に引き上げ、S2 を押すと GND |
| PA10 / PA11 / PA12 | GPIO 出力 | - | - | **用途がわからない**（ラベルなし、コードからも見つからない） | **どこにもつながっていない**（ほかに PA8・PA9・PB2・PB10・PC4・PC12・PD2 も）。.ioc の GPIO 出力の設定は、なくてもよい（作者に聞いてから消す） |

## 壁センサの番号（KOGUMA.MkIIl の `PL_sensor.c` から）

（2026-11-25 に基板データで確かめた：受光は SFH 229 FA ×4〈フォトダイオード〉で、AD8606 の電流→電圧の変換〈820kΩ・2.2pF〉を通って ADC に入る。発光は OPV332 ×4。どの受光がどの ADC のピンかは、上の表のとおり）

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
