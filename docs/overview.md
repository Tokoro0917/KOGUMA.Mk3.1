# KOGUMA.Mk3 概要（KOGUMA.Mk3.1 で動かす機体）

## これは何か

マイクロマウス（クラシックサイズ、16×16 区画・1 区画 180mm）の機体です。
機体は KOGUMA.Mk3（ファームウェアのリポジトリ名は KOGUMA.MkIIl）と同じで、KOGUMA.Mk3.1 ではファームウェアだけを書き直します。

## 関連する機体・リポジトリ

| 名前 | 中身 | KOGUMA.Mk3.1 との関係 |
|---|---|---|
| KOGUMA.MkIIl | KOGUMA.Mk3 の今のファームウェア（STM32CubeIDE、`Core/Src` に全部） | 書き直す元。機体の値と機能の中身はここから |
| ToraNeko.Mk3 | ハーフサイズ機のファームウェア・回路・機構（CubeMX + CMake、`App/` を層に分けた） | 書き直しの手本。KOGUMA のコードを層ごとに作り直したもの |

## ハードウェア仕様（KOGUMA.MkIIl のコードと `.ioc` から読んだもの）

回路図を見て確かめたものではありません。回路図・CAD データを受け取ったら、ここと [../software/main-controller/PIN_REQUIREMENTS.md](../software/main-controller/PIN_REQUIREMENTS.md) を直します。

### マイコン

- STM32F446RET6（Cortex-M4、Flash 512KB、RAM 128KB）
- クロック：HSE 10MHz → SYSCLK 180MHz（PLL M=5、N=180、P=2）。APB1 45MHz（タイマー 90MHz）、APB2 90MHz（タイマー 180MHz）

### 駆動系

- モーター 2 個。ドライバは TIM3 の CH1〜CH4（PC6〜PC9）で動かす（左右で 2ch ずつ。50kHz）。モータードライバの型番は回路図で確認
- ギア比 1.7073、タイヤの直径 23.42mm（2026-10-21 に実測）、トレッド 55mm、重さ 約 78g（`Define.h`）
- エンコーダ：AS5047P × 2（SPI3。CS は左 PC5・右 PA15）。KOGUMA はギア比を掛けて距離にしているので、磁石はモーターの軸にあると思われる（回路図・機構で確認）
- 吸引ファン：TIM2 CH1（PB8、50kHz）の PWM

### センサ

- 壁センサ 4 組（前の左右・横の左右）。発光は 2 組ずつ（`WallLED_12` = PA1、`WallLED_34` = PA0）、受光は ADC1 の IN10〜IN13（PC0〜PC3）
- 電池の電圧：ADC1 の IN9（PB1）
- IMU：LSM6DSR（SPI1。CS は PA4。ラベルは `ICM_NSS`）

### 操作・表示

- LED 8 個（LED1〜8：PB12〜PB15、PB4〜PB7）と、壁の表示の LED 4 個（LEDF1〜4：PA3、PC15、PC14、PC13）
- ブザー：TIM4 CH4（PB9）
- スイッチ S2（PB3）。CubeMX では設定しておらず、KOGUMA は `UI.c` で入力にしている
- モードの選び方（KOGUMA.MkIIl）：タイヤを回してモードとナンバー（どちらも 0〜15）を選び、左前センサに手をかざして決定、左右の横センサに手をかざしてスタート
- PC への出力：USART2 の TX（PA2、115200bps、Single Wire）
- 書き込み：SWD（PA13、PA14）

### 用途がわからないもの（回路図で確認）

- PA10、PA11、PA12：GPIO 出力に設定してあるが、ラベルがなく、コードからも使い方が見つからなかった

## 前の機体のよくない点

KOGUMA.MkIIl の [docs/next_robot.md](../software/reference/koguma-mkiil/docs/next_robot.md) にまとめてある（電源の内部抵抗、モーターの電圧、RAM など）。
ソフトで直せるもの（左右の車輪の速度をログに出す、設定を Flash に保存する、など）は、書き直しの最初から入れる。
