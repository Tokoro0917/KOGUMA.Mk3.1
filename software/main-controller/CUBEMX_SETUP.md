# CubeMX の設定（ステップ0）

KOGUMA.MkIIl の `.ioc`（STM32CubeIDE 用）をもとに、CMake で生成する `KOGUMA.Mk3.1` のプロジェクトを作る手順と、変える設定です。
**CubeMX は Claude の環境では動かせないので、この作業は作者にお願いします。** 生成して push してもらえれば、そのあとのビルド・`App/` の追加は Claude がやります。

設定の考え方は ToraNeko.Mk3 の `software/main-controller/CUBEMX_SETUP.md` と同じです。機体（ピン・クロック・周辺機能）は KOGUMA.MkIIl のまま変えず、ToraNeko.Mk3 で決めた作り（µs のタイマー、壁センサの連鎖、ログの DMA 送信）に要るものだけを足します。
ピンは [PIN_REQUIREMENTS.md](PIN_REQUIREMENTS.md)。

## 0. 用意するもの

- STM32CubeMX（6.x。ToraNeko.Mk3 で使っているもの）
- このリポジトリを手元に clone して、ブランチ `claude/clever-ride-k9xfb3` を checkout しておく（`software/reference/koguma-mkiil/KOGUMA.MkIIl.ioc` が入っている）

## 1. 元の .ioc を開いて、別の名前で保存する

1. エクスプローラーで `software/main-controller/` の中に、空のフォルダ `KOGUMA.Mk3.1` を作る
2. `software/reference/koguma-mkiil/KOGUMA.MkIIl.ioc` を、いま作った `software/main-controller/KOGUMA.Mk3.1/` にコピーし、名前を `KOGUMA.Mk3.1.ioc` に変える（`software/reference/` の中のものは編集しないので、必ずコピーしてから開く）
3. CubeMX で **File → Load Project** を押し、コピーした `KOGUMA.Mk3.1.ioc` を開く
4. 「Migrate」かどうか聞かれたら **Migrate** を押す（FW パッケージを新しいものにする。ToraNeko.Mk3 は STM32Cube FW_F4 V1.28.3）
5. **Project Manager** タブの **Project** で、Project Name が `KOGUMA.Mk3.1`、Project Location が `software/main-controller` になっていることを確かめる（違ったら直す）

## 2. Project Manager

| 場所 | 設定 | KOGUMA.MkIIl |
|---|---|---|
| Project → **Toolchain / IDE** | **CMake** | STM32CubeIDE |
| Project → Do not generate the main() | OFF | 同じ |
| Code Generator → STM32Cube MCU packages and embedded software packs | **Copy only the necessary library files**（`Drivers/` をプロジェクトの中に置き、コミットする） | 同じ |
| Code Generator → Generate peripheral initialization as a pair of '.c/.h' files per peripheral | ON | 同じ |
| Code Generator → Keep User Code when re-generating | ON | 同じ |
| Code Generator → Delete previously generated files when not re-generated | ON | 同じ |

## 3. Pinout & Configuration

### 3-1. GPIO のラベル（User Label）を ToraNeko.Mk3 の名前にそろえる

ピンは変えません。ラベルだけ、ToraNeko.Mk3 の `App/` で使っている名前に変えます（持ってきたコードをそのまま使えるように）。
ピンを右クリック → **Enter User Label** で書き換えます。

| ピン | 今のラベル | 新しいラベル | GPIO output level（System Core → GPIO） | 備考 |
|---|---|---|---|---|
| PA1 | `WallLED_12` | `WALL_LED_A` | Low | 壁センサの発光（1・2 の組） |
| PA0 | `WallLED_34` | `WALL_LED_B` | Low | 壁センサの発光（3・4 の組） |
| PA4 | `ICM_NSS` | `IMU_CS` | **High** | IMU の CS（Low で選ぶので、起動したときは High にしておく） |
| PC5 | `ENCO_L_NSS` | `ENC_L_CS` | **High** | 左エンコーダの CS |
| PA15 | `ENCO_R_NSS` | `ENC_R_CS` | **High** | 右エンコーダの CS |
| PB12〜PB15、PB4〜PB7 | `LED1`〜`LED8` | そのまま | Low | |
| PA3、PC15、PC14、PC13 | `LEDF1`〜`LEDF4` | そのまま | Low | 壁の表示の LED |

### 3-2. スイッチ S2（PB3）を足す

KOGUMA.MkIIl では CubeMX で設定せず、`UI.c` の `Switch_Init()` で入力にしていました。CubeMX で設定しておきます。

1. PB3 を左クリック → **GPIO_Input**
2. 右クリック → Enter User Label → `SW_USER`
3. System Core → GPIO → PB3：GPIO Pull-up/Pull-down は **No pull-up and no pull-down**（基板の 10k（R31）で 3.3V に引き上げてあり、押すと 0 になる。KOGUMA.MkIIl の `UI.c` のコメントから）

### 3-3. SYS

| 項目 | 設定 | 備考 |
|---|---|---|
| Debug | **Serial Wire** | PB3 をスイッチに使うので、JTAG（PB3 = JTDO）にしない。今の設定のままならそれでよい |
| Timebase Source | SysTick | 同じ |

### 3-4. タイマー

| 用途 | タイマー | 変更 | 設定 | 周波数 |
|---|---|---|---|---|
| モーター PWM | TIM3 CH1〜CH4 | なし | PSC 9-1、ARR 200-1 | 50kHz |
| 吸引ファン PWM | TIM2 CH1 | なし | PSC 18-1、ARR 100-1 | 50kHz |
| ブザー | TIM4 CH4 | なし | PSC 90-1、ARR 100-1 | 10kHz |
| 制御周期 | TIM6 | **NVIC を有効にする**（下の 3-8） | PSC 90-1、ARR 1000-1 | 1kHz |
| µs を数える | **TIM5（新しく足す）** | Clock Source = **Internal Clock** | PSC **90-1**、ARR **4294967295**（0xFFFFFFFF） | 1MHz で数え続ける。割り込みは使わない |
| 壁センサの待ち時間 | **TIM8（新しく足す）** | Clock Source = **Internal Clock**、**One Pulse Mode** に ✓ | PSC **180-1**、ARR **35-1**、Trigger Event Selection（TRGO）= **Update Event** | 1 回 35µs。割り込みは使わない |

- TIM5・TIM6 は APB1（タイマー 90MHz）、TIM8 は APB2（タイマー 180MHz）なので、PSC が違う
- TIM8 は、LED を点けたのと同時に動かし、35µs たったら ADC を起動するためのもの（ToraNeko.Mk3 の TIM3 と同じ役目）。TIM6・TIM7 は ADC を起動できないので TIM8 にした
- TIM8 はピンを使わない（チャンネルは全部 Disable のまま）

### 3-5. ADC1

ピンとチャンネル（IN9〜IN13）、変換の順番（Rank 1〜5）は変えません。起動のしかたを、ToraNeko.Mk3 と同じ「タイマーで起動して 1 回ずつ」にします。

| 項目 | 設定 | KOGUMA.MkIIl |
|---|---|---|
| Clock Prescaler | **PCLK2 divided by 4**（22.5MHz。上限 36MHz） | PCLK2 / 8 |
| Continuous Conversion Mode | **Disabled** | Enabled |
| DMA Continuous Requests | **Disabled** | Enabled |
| External Trigger Conversion Source | **Timer 8 Trigger Out event** | Software |
| External Trigger Conversion Edge | **Trigger detection on the rising edge** | - |
| Sampling Time（Rank 1〜5 すべて） | **15 Cycles** | 3 Cycles |
| DMA（DMA Settings タブ） | ADC1 → DMA2 Stream0、Peripheral To Memory、Half Word、Mode **Normal** | Circular |

- 5ch の変換時間は (15 + 12) × 5 ÷ 22.5MHz ≒ 6µs
- ToraNeko.Mk3 と同じく、壁センサの値は「組A を点ける → 35µs 待つ → 測る → 全部消して待つ → 組B を点ける → …」と、DMA の完了割り込みで次に進める（ステップ6で作る）

### 3-6. SPI

KOGUMA.MkIIl は、どちらもデータシートの上限（10MHz）を超える速さで動かしていました。ToraNeko.Mk3 と同じく上限の中に収めます。

| 項目 | SPI1（IMU：LSM6DSR） | SPI3（エンコーダ：AS5047P ×2） |
|---|---|---|
| Prescaler | **16**（APB2 90MHz ÷ 16 = 5.625MHz） | **8**（APB1 45MHz ÷ 8 = 5.625MHz） |
| CPOL / CPHA | High / 2 Edge（変えない） | **Low** / 2 Edge（AS5047P のデータシートの指定。ToraNeko.Mk3 で、High のままだとエラーの印が立った） |
| KOGUMA.MkIIl | Prescaler 8（11.25MHz） | Prescaler 2（22.5MHz）、CPOL High |

### 3-7. USART2（ログ出力）

| 項目 | 設定 | KOGUMA.MkIIl |
|---|---|---|
| Mode | Single Wire (Half-Duplex)（変えない） | 同じ |
| Baud Rate | 115200 bps（まずは変えない。PC とつなぐのに何を使っているかを確かめてから上げる。ToraNeko.Mk3 は 921600） | 115200 |
| Data Direction | **Transmit Only** | Receive and Transmit |
| DMA（DMA Settings タブ → Add） | **USART2_TX** → DMA1 Stream6、Memory To Peripheral、Byte、Normal | なし |

- 生成したあと、`Core/Src/usart.c` の USER CODE 欄で PA2 をプッシュプルにし直す（ToraNeko.Mk3 の CUBEMX_SETUP.md の 7 と同じ。Claude がやる）

### 3-8. NVIC（割り込みの優先度）

System Core → NVIC で、次のように設定します（数字が小さいほうが先）。

| 割り込み | Enabled | Preemption Priority | 用途 |
|---|---|---|---|
| DMA2 stream0 global interrupt（ADC1） | ✓ | **1** | 壁センサの連鎖（次の LED へ進める） |
| TIM6 global interrupt, DAC1 and DAC2 underrun error interrupts | ✓ | **2** | 制御周期 |
| DMA1 stream6 global interrupt（USART2_TX） | ✓ | **5** | ログ送信が終わった |
| USART2 global interrupt | ✓ | **5** | HAL の DMA 送信の終わりを知らせる |

### 3-9. 変えないもの

- クロック（HSE 10MHz → 180MHz）、PA10〜PA12（GPIO 出力、用途は回路図で確認）、SWD（PA13・PA14）

## 4. 生成して push する

1. 右上の **GENERATE CODE** を押す
2. `software/main-controller/KOGUMA.Mk3.1/` に `CMakeLists.txt`、`CMakePresets.json`、`Core/`、`Drivers/`、`cmake/`、`startup_stm32f446xx.s`、リンカスクリプト（`STM32F446xx_FLASH.ld` のような名前）ができていれば成功
3. （できれば）VS Code で `KOGUMA.Mk3.1` フォルダを開き、STM32Cube 拡張でビルドが通るか見る。通らなくても、そのまま push してよい（Claude が直す）
4. `software/main-controller/KOGUMA.Mk3.1/` を、`Drivers/` も含めて全部コミットし、ブランチ `claude/clever-ride-k9xfb3` に push する（`build/` は `.gitignore` で除外される）
5. Claude に「CubeMX のプロジェクトを push した」と伝える

## 5. 生成したあと Claude がやること

1. `Core/Src/main.c` の USER CODE 欄に `app_init();` と `app_loop();` の呼び出しを書く
2. `CMakeLists.txt` に `App/` のソースとインクルードパス、`-Wall -Wextra` を足す（書き方は ToraNeko.Mk3 の CUBEMX_SETUP.md の 10 と同じ）
3. `cmake --preset Debug && cmake --build --preset Debug` が警告なしで通ることを確かめる
4. CI（`.github/workflows/firmware-build.yml`）を ToraNeko.Mk3 から持ってきて、KOGUMA.Mk3.1 の名前に直す
5. ステップ1（LED・シリアル出力）に進む
