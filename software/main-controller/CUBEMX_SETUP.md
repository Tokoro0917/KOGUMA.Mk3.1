# CubeMX の設定（ステップ0）

`KOGUMA.Mk3.1` の CubeMX（CMake）プロジェクトの設定値です。

**2026-11-08 に、CubeMX を使わずに Claude が作った**（作者の提案「以前のプロジェクトをコピーして、変更箇所を変更すればいい」）。作り方は下の「0. どう作ったか」。
このファイルの 2〜3 の表が、`KOGUMA.Mk3.1.ioc` と `Core/` に入っている設定。あとで CubeMX で開いて確かめるとき（下の 1）や、設定を変えるときに見る。

設定の考え方は ToraNeko.Mk3 の `software/main-controller/CUBEMX_SETUP.md` と同じです。機体（ピン・クロック）は KOGUMA.MkIIl のまま変えず、ToraNeko.Mk3 で決めた作り（制御周期 2kHz を TIM11 で、µs のタイマー、壁センサの連鎖、ログの DMA 送信）に要るものを足しました（制御周期を ToraNeko に合わせるのは 2026-11-08 に決定）。
ピンは [PIN_REQUIREMENTS.md](PIN_REQUIREMENTS.md)。

## 0. どう作ったか

| ファイル | どこから持ってきたか | 直したところ |
|---|---|---|
| `KOGUMA.Mk3.1.ioc` | KOGUMA.MkIIl の `KOGUMA.MkIIl.ioc` | 下の 2・3 の表のとおり（Toolchain を CMake、ラベル、PB3、TIM5・TIM8・TIM11 を足し TIM6 を外す、ADC・SPI・USART2・DMA・NVIC）。書き方は ToraNeko.Mk3 の `.ioc` の同じ項目にそろえた |
| `Drivers/` | KOGUMA.MkIIl の `Drivers/`（STM32Cube FW_F4 V1.28.2。F446 のヘッダ `stm32f446xx.h` が入っている） | なし |
| `startup_stm32f446xx.s` | KOGUMA.MkIIl の `Core/Startup/startup_stm32f446retx.s`（中身は ST の `startup_stm32f446xx.s` と同じ） | 名前だけ |
| `STM32F446xx_FLASH.ld` | ToraNeko.Mk3 の `STM32F411xx_FLASH.ld`（F411 と F446RE は Flash 512KB・RAM 128KB で同じ） | 先頭のコメント |
| `CMakeLists.txt`、`CMakePresets.json`、`cmake/` | ToraNeko.Mk3 | プロジェクト名、`STM32F446xx`、起動ファイルとリンカスクリプトの名前、`App/` のファイル |
| `Core/` | ToraNeko.Mk3 の `Core/`（CubeMX が CMake 用に生成したもの）をひな形にした | ピンと周辺機能を KOGUMA.MkIIl の生成コードに合わせ、下の 2・3 の表の設定にした（`main.h`、`gpio.c`、`tim.c`、`adc.c`、`dma.c`、`spi.c`、`usart.c`、`stm32f4xx_it.c`、`main.c` のクロックの設定） |

`cmake --preset Debug && cmake --build --preset Debug` が警告なしで通ることを確かめた（2026-11-08。Flash 約 40KB、RAM 約 3KB）。CI でも同じビルドをする。

## 1. 作者にお願いすること（実機で確かめる）

### 1-1. 書き込んで、動くか見る（ステップ1 ができてから）

今の `App/app.c` は何もしないので、書き込んでも何も起きない。ステップ1（LED とシリアル出力）を入れたら、ToraNeko.Mk3 と同じ手順で書き込んで、LED が順に点くこと、シリアルに文字が出ること（921600bps）を見てもらう。

### 1-2. CubeMX で開いて確かめる（急がない。CubeMX を使える日に）

`.ioc` は手で書き換えたので、CubeMX で開けるかと、生成したコードが今の `Core/` と同じになるかを、一度確かめておきたい。

1. CubeMX で **File → Load Project** を押し、`software/main-controller/KOGUMA.Mk3.1/KOGUMA.Mk3.1.ioc` を開く
2. エラーが出ずに開ければ成功。「Migrate」かどうか聞かれたら **Migrate** を押してよい
3. **Pinout & Configuration** で、下の 3 の表と同じになっているかを見る（とくにタイマー TIM5・TIM8・TIM11、ADC の Trigger、USART2 の DMA）
4. 右上の **GENERATE CODE** を押す
5. GitHub Desktop などで、変わったファイルを見る。`Core/` のコメントや並び順が少し変わるのは問題ない。設定の値が変わっていたら、そのまま push して Claude に伝える（`App/` は CubeMX が触らないので、消えない）

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
| 制御周期 | **TIM11（新しく足す）** | **Activated** に ✓（内部クロック、ピンは使わない）。NVIC を有効にする（下の 3-8） | PSC **180-1**、ARR **500-1** | **2kHz**（ToraNeko.Mk3 と同じ） |
| （使わない） | **TIM6** | **Disable にする**（KOGUMA.MkIIl の制御周期。KOGUMA.Mk3.1 では TIM11 に変えた） | - | - |
| µs を数える | **TIM5（新しく足す）** | Clock Source = **Internal Clock** | PSC **90-1**、ARR **4294967295**（0xFFFFFFFF） | 1MHz で数え続ける。割り込みは使わない |
| 壁センサの待ち時間 | **TIM8（新しく足す）** | Clock Source = **Internal Clock**、**One Pulse Mode** に ✓ | PSC **180-1**、ARR **35-1**、Trigger Event Selection（TRGO）= **Update Event** | 1 回 35µs。割り込みは使わない |

- TIM5 は APB1（タイマー 90MHz）、TIM8・TIM11 は APB2（タイマー 180MHz）なので、PSC が違う（ToraNeko の F411 は全部 100MHz）
- TIM11 は、ToraNeko.Mk3 と同じタイマー。割り込みの名前（`TIM1_TRG_COM_TIM11_IRQn`）も F411 と同じなので、`sys/scheduler.c` をそのまま持ってこられる
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
| Baud Rate | **921600 bps**（STLINK-V3 の仮想 COM ポートなので、ToraNeko と同じにする。2026-11-08 のアンケートで決定） | 115200 |
| Data Direction | **Transmit Only** | Receive and Transmit |
| DMA（DMA Settings タブ → Add） | **USART2_TX** → DMA1 Stream6、Memory To Peripheral、Byte、Normal | なし |

- 生成したあと、`Core/Src/usart.c` の USER CODE 欄で PA2 をプッシュプルにし直す（ToraNeko.Mk3 の CUBEMX_SETUP.md の 7 と同じ。Claude がやる）

### 3-8. NVIC（割り込みの優先度）

System Core → NVIC で、次のように設定します（数字が小さいほうが先）。

| 割り込み | Enabled | Preemption Priority | 用途 |
|---|---|---|---|
| DMA2 stream0 global interrupt（ADC1） | ✓ | **1** | 壁センサの連鎖（次の LED へ進める） |
| TIM1 trigger and commutation interrupts and TIM11 global interrupt | ✓ | **2** | 制御周期（2kHz） |
| DMA1 stream6 global interrupt（USART2_TX） | ✓ | **5** | ログ送信が終わった |
| USART2 global interrupt | ✓ | **5** | HAL の DMA 送信の終わりを知らせる |

### 3-9. 変えないもの

- クロック（HSE 10MHz → 180MHz）、PA10〜PA12（GPIO 出力、用途は回路図で確認）、SWD（PA13・PA14）
- TIM6 の割り込み（KOGUMA.MkIIl では有効）は、TIM6 を Disable にすると一緒になくなる。NVIC の一覧に残っていたら ✓ を外す

## 4. 生成したあとに直したところ（USER CODE 欄）

- `Core/Src/main.c`：USER CODE 欄で `app_init()` と `app_loop()` を呼ぶ（ToraNeko.Mk3 と同じ）
- `Core/Src/usart.c`：USER CODE 欄で PA2 をプッシュプルにし直す（Single Wire だとオープンドレインで生成されるため。ToraNeko.Mk3 の 7 と同じ）
