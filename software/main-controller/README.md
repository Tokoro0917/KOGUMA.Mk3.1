# main-controller

KOGUMA.Mk3 メイン基板（STM32F446RE）の、書き直したファームウェアです。

ToraNeko.Mk3 のファームウェア（`software/main-controller/TORANEKO.Mk3/App/`）を手本にし、KOGUMA.MkIIl のコード（[../reference/koguma-mkiil/](../reference/koguma-mkiil/)）から機体の値と機能の中身を持ってきます。
ToraNeko.Mk3 が KOGUMA のコードを作り直したときと同じように、機能ごとに 1 つずつ作り、実機で確かめたものだけを置きます。

- [PIN_REQUIREMENTS.md](PIN_REQUIREMENTS.md): ピン割り当て（KOGUMA.MkIIl の `.ioc` から読んだもの。回路図で確かめる）と、ToraNeko.Mk3 との違い
- [CUBEMX_SETUP.md](CUBEMX_SETUP.md): CubeMX（`.ioc`）の設定値と、プロジェクトをどう作ったか（ステップ0。CubeMX を使わず、KOGUMA.MkIIl と ToraNeko.Mk3 のプロジェクトを写して作った）
- [KOGUMA_PORTING.md](KOGUMA_PORTING.md): ToraNeko.Mk3 の `App/` のどれをそのまま持ってくるか、KOGUMA の機体に合わせて何を変えるか、KOGUMA にしかない機能をどうするか
- MODES.md（まだない）：機体の操作とモードの割り当て。ステップ9で、KOGUMA.MkIIl と ToraNeko.Mk3 の MODES.md を見ながら決める

## 書き直しの考え方

1. **作り（ファイルの分け方・層・書き方）は ToraNeko.Mk3 に合わせる。** ToraNeko.Mk3 の `App/` のファイルを持ってきて、KOGUMA の機体に合わせて直す。同じ名前の関数は同じ仕事をするようにしておくと、毎朝の確認で ToraNeko.Mk3 の変更をそのまま持ってきやすい
2. **機体の値（センサのしきい値・ターンの値・ゲイン・タイヤ径）は KOGUMA.MkIIl から持ってくる。** KOGUMA.MkIIl の実機のログ（`logs/`）で決めた値なので、単位と制御周期をそろえて書き直す
3. **ハーフサイズとクラシックの違いは定数にまとめる。** 迷路 16×16、1 区画 180mm、柱 12mm（`config/` と `maze/maze_map.h`）
4. 大会（2026-11-21〜22 の学生大会）は、今までどおり KOGUMA.MkIIl のファームウェアで走る。KOGUMA.Mk3.1 は、それと関係なく進める

## 決定事項

「案」は、作者にまだ確かめていないもの。決まったら「決定」にする。

**2026-11-08 に作者が決めたこと：ディレクトリの構成、ファイル名、制御方法、制御周期、フェイルセーフは ToraNeko.Mk3 に合わせる。**
KOGUMA.MkIIl と違うのは、機体（ピン・周辺機能）が違うところと、KOGUMA にしかない機能（[KOGUMA_PORTING.md](KOGUMA_PORTING.md) の B）だけにする。
何を ToraNeko に合わせ、KOGUMA の何をやめるかは [KOGUMA_PORTING.md](KOGUMA_PORTING.md) の E。

| 項目 | 決定 | 理由・補足 |
|---|---|---|
| ディレクトリの構成 | **決定：ToraNeko.Mk3 と同じ**（下の「ファイル構成」） | 2026-11-08 に決定。リポジトリの一番上（`software/`・`hardware/`・`mechanical/`・`docs/`）、`software/main-controller/` の中（資料・`tools/`・`logs/`・`test/`・CubeMX のプロジェクト）、`App/` の中の層（`config/`・`drv/`・`ctrl/`・`maze/`・`sys/`）まで同じにする |
| ファイル名 | **決定：ToraNeko.Mk3 と同じ名前**（下の「ファイル構成」「命名規則」） | 2026-11-08 に決定。同じ仕事のファイルは同じ名前にする（`Move.c` → `ctrl/run.c` など。下の「KOGUMA.MkIIl のファイルとの対応」）。資料の名前も同じにした（`PORTING.md` → `KOGUMA_PORTING.md`）。ToraNeko にないものだけ新しい名前を付ける（`drv/buzzer.c` など）。毎朝の確認で、ToraNeko の変更を同じ名前のファイルに持ってこられる |
| 制御方法 | **決定：ToraNeko.Mk3 と同じ**（`ctrl/odometry.c`・`trajectory.c`・`control.c`・`wall_ctrl.c`・`side_edge.c`・`run.c`） | 2026-11-08 に決定。中身は [KOGUMA_PORTING.md](KOGUMA_PORTING.md) の E2。係数の値だけは KOGUMA の機体に合わせて決め直す（ステップ7） |
| 制御周期 | **決定：2kHz**（`config/robot_params.h` の `CTRL_FREQ_HZ`。ToraNeko.Mk3 と同じ） | 2026-11-08 に決定（前の案は 1kHz）。係数は周期から換算する作り（ToraNeko と同じ）なので、KOGUMA の 1kHz の値も読み替えられる。F446 は 180MHz で、ToraNeko の F411（100MHz）より速く、迷路も 16×16 で小さいので、処理時間は ToraNeko より余裕がある見込み（ステップ2で測る） |
| 制御周期のタイマー | **決定：TIM11**（ToraNeko.Mk3 と同じ。PSC 180-1、ARR 500-1） | 2026-11-08 に決定。F446 でも TIM11 は空いていて、割り込みの名前（`TIM1_TRG_COM_TIM11_IRQn`）も F411 と同じなので、`sys/scheduler.c` をそのまま持ってこられる。KOGUMA の TIM6 は使わない |
| フェイルセーフ | **決定：ToraNeko.Mk3 と同じ**（見つけ方は `ctrl/control.c`、止まっているかは `sys/failsafe.c`、迷路を戻すのは `sys/explore.c`） | 2026-11-08 に決定。中身は [KOGUMA_PORTING.md](KOGUMA_PORTING.md) の E3。しきい値の値だけ、クラシックの速さに合わせて決め直す（ステップ7） |
| 開発環境 | **決定：STM32CubeMX（コード生成、Toolchain = CMake）＋ VS Code（STM32Cube for Visual Studio Code 拡張）**（2026-11-08 のアンケートで決定） | ToraNeko.Mk3 と同じ。コマンドラインだけでビルドできるので、CI や Claude の環境でもファームのビルドを確かめられる。KOGUMA.MkIIl の CubeIDE はやめる |
| プロジェクト名 | 案：`KOGUMA.Mk3.1` | ToraNeko の `TORANEKO.Mk3` と同じく、機体の名前。成果物は `build/Debug/KOGUMA.Mk3.1.elf` |
| ビルド | `cmake --preset Debug && cmake --build --preset Debug`（`software/main-controller/KOGUMA.Mk3.1/` で） | CubeMX が生成する `CMakePresets.json` をそのまま使う |
| ライブラリ | STM32 HAL | ToraNeko.Mk3 と同じ |
| クロック | HSE 10MHz → SYSCLK 180MHz（KOGUMA.MkIIl と同じ） | APB1 45MHz（タイマー 90MHz）、APB2 90MHz（タイマー 180MHz） |
| 書き込み・デバッグ | SWD（PA13・PA14） | KOGUMA.MkIIl と同じ。書き込み器は作者に確認 |
| PC との通信・printf | USART2 の TX だけ（PA2）、DMA で送る。**921600bps**（2026-11-08 のアンケートで決定。STLINK-V3 の仮想 COM ポートを使っているので、ToraNeko と同じ速さにする） | KOGUMA.MkIIl と同じピン。送り方は ToraNeko.Mk3 の `drv/uart.c`（DMA・リングバッファ）。APB1 45MHz で 921600bps は約 0.35% のずれで、問題ない |
| 処理時間計測・µs 待ち | TIM5（32bit）を 1MHz でフリーラン（**新しく足す**） | ToraNeko.Mk3 と同じ。KOGUMA.MkIIl にはない |
| ms 単位の待ち | SysTick（`HAL_GetTick()`） | ToraNeko.Mk3 と同じ。KOGUMA の「共通カウンタを 0 に戻す `wait_ms`」はやめる |
| 壁センサの点灯・測定の順番 | タイマーで待ち時間を作ってから ADC を起動し、DMA 完了割り込みで次の LED に進める連鎖（ToraNeko.Mk3 の方式）。待ち時間のタイマーは TIM8 | ToraNeko.Mk3 は TIM3 を使うが、KOGUMA の TIM3 はモーターの PWM（ピンが決まっているので変えられない）。ADC を起動できるタイマーのうち空いているのは TIM8 だけ（TIM6・TIM7 は ADC を起動できない） |
| 割り込み優先度 | 壁センサの連鎖（ADC の DMA）> 制御周期（TIM11）> ログ送信（USART の DMA） | ToraNeko.Mk3 と同じ |
| ブザー | **決定：持ち込む**（`drv/buzzer.c`。TIM4 CH4。2026-11-08 のアンケートで決定：合図は LED とブザーの両方） | ToraNeko.Mk3 にはない。KOGUMA は合図をブザーで出しているので残す |
| 迷路 | 16×16、ゴールの大きさは 2×2。**位置は機体で入れて Flash に保存する**（ToraNeko と同じ。2026-11-08 のアンケートで決定） | 大会ごとにゴールの位置が違うときや、ベンチテストでゴールを動かすときも、コードを書き換えなくてよい。初めの値は中央（7, 7） |
| 迷路と設定の保存 | Flash のセクタ6・7（ToraNeko.Mk3 と同じ。プログラムは前の 256KB に収める）。迷路・ゴールの位置・会場ごとの感度合わせの値を保存する（2026-11-08 のアンケートで決定） | F446RE と F411CE は Flash のセクタの分け方が同じ。KOGUMA.MkIIl は保存していなかった |
| 操作とモード | **ToraNeko の形と、ToraNeko のモードの並び**（2026-11-08 のアンケートで決定）。タイヤで選ぶ → スイッチ S2 で決定 → 前センサに手をかざして離すとスタート。モード0 探索（中の No. は KOGUMA の No.1〜12）、1 最短走行、2 探索のターン調整、3 最短走行のターン調整、4 実験用、5 センサ、6 基本調整、7 出力と設定 | 詳しくはステップ9で MODES.md を作るときに、ToraNeko の MODES.md をもとに決める |

## ファイル構成（予定）

ToraNeko.Mk3 と同じ形・同じファイル名にする（2026-11-08 に決定）。印のないファイルは ToraNeko.Mk3 の同じ名前のファイルを持ってきて KOGUMA に合わせて直すもの、「（新）」は ToraNeko にないので KOGUMA.Mk3.1 で新しく作るもの、「（作った）」はもうあるもの。

```
software/main-controller/
├── README.md                     決定事項・ファイル構成・実装ステップ（このファイル）
├── PIN_REQUIREMENTS.md           ピン割り当て
├── CUBEMX_SETUP.md               CubeMX の手順と .ioc の設定値
├── KOGUMA_PORTING.md             KOGUMA にしかない機能をどうするか、ToraNeko に合わせて KOGUMA の何をやめるか
├── MODES.md                      機体の操作とモード（作った。ステップ9）
├── tools/                        viewer.html（ビューア）、wall_table.py（壁までの距離の表）、push_logs.sh（ログを送る）、vscode/（書き込みのタスク）（作った）
├── logs/                         実機の出力を残すフォルダ
├── test/                         PC 上で動かすテスト（CI で実行）。ホストの gcc でビルドするので、ARM 向けの CMake プロジェクトとは分ける
│   ├── test_maze.c               迷路と探索走行のテスト（ToraNeko の test_maze.c を 16×16 にして、全面探索・既知区間の斜め・吸引探索のテストを足した）
│   ├── test_mode.c・test_flash.c・test_output.c  モード選択と操作、Flash への保存、ログ出力のテスト（ToraNeko から）
│   ├── test_trajectory.c・test_control.c・test_turn.c・test_wall_dist.c  制御とターンと壁センサのテスト。`make` で全部実行
│   ├── Makefile
│   └── mazes/                    テスト用の 16×16 の迷路と、作る道具（gen_maze.py、KOGUMA の迷路の出力を直す from_koguma_dump.py）
│
└── KOGUMA.Mk3.1/                 CubeMX（CMake）プロジェクト（作った。KOGUMA.MkIIl と ToraNeko.Mk3 のプロジェクトを写して直した。CUBEMX_SETUP.md の 0）
    ├── KOGUMA.Mk3.1.ioc          CubeMX 設定（KOGUMA.MkIIl.ioc をもとに直した）
    ├── CMakeLists.txt            ビルド設定（初回だけ生成。App/ の追加はここに書く）
    ├── CMakePresets.json         Debug / Release のプリセット（生成）
    ├── cmake/                    ツールチェーン設定（生成）
    ├── startup_stm32f446xx.s     スタートアップ（生成）
    ├── STM32F446xx_FLASH.ld      リンカスクリプト（ToraNeko の F411 用を写した。メモリの大きさは同じ）
    ├── Core/                     生成コード（CubeMX で生成し直してもよい）。USER CODE 欄以外は編集しない
    ├── Drivers/                  HAL / CMSIS（生成。コミットする）
    │
    └── App/                      自作コードはすべてここ
        ├── app.c / app.h         app_init()、app_loop()。全体の入口
        │
        ├── config/               機体の定数（値は KOGUMA.MkIIl から）
        │   ├── robot_params.h    制御周期（2kHz）、電池の下限、タイヤ径、トレッド幅、ギア比
        │   ├── encoder_correction.h  エンコーダの角度の歪みを直す表
        │   ├── ctrl_gains.h      速度・角度の制御の係数、フェイルセーフのしきい値
        │   ├── run_params.h      区画の大きさ（180mm）、スタートの位置、探索の動き（吸引なし 500mm/s・吸引あり 1000mm/s）の値（作った）
        │   └── turn_params.h     最短走行のターンの値（作った。KOGUMA の 2000 の速さ・角速度で、前距離・後距離を test_turn --calc で計算した。実機はステップ10）
        │
        ├── drv/                  ハードに直接触れる層（HAL を呼ぶのは、ここと sys/scheduler.c だけ）
        │   ├── encoder.c/.h      AS5047P ×2（SPI3）
        │   ├── imu.c/.h          LSM6DSR（SPI1）
        │   ├── motor.c/.h        モーターの PWM（TIM3 CH1〜4。電圧を指定）
        │   ├── fan.c/.h          吸引ファン（TIM2 CH1）
        │   ├── wall.c/.h         壁センサ（ADC1、LED 2 組。TIM8 → ADC → DMA の連鎖）と電池の電圧
        │   ├── flash_memory.c/.h 内蔵 Flash の保存場所（セクタ6・7）を消す・書く・読む
        │   ├── led.c/.h          LED1〜8、壁の表示の LED 4 個
        │   ├── switch.c/.h       スイッチ S2（PB3）
        │   ├── buzzer.c/.h       ブザー（TIM4 CH4）（新）
        │   ├── timebase.c/.h     経過時間（ms は SysTick、µs は TIM5）
        │   └── uart.c/.h         ログ送信（USART2、DMA）。printf の出力先
        │
        ├── ctrl/                 制御（ToraNeko と同じ制御方法。KOGUMA_PORTING.md の E2）
        │   ├── odometry.c/.h     エンコーダからタイヤの速さと走った距離を求める
        │   ├── trajectory.c/.h   台形加減速の目標の速さ・角速度を作る（直進・その場旋回・スラローム）
        │   ├── control.c/.h      速度と角度の制御（フィードフォワード ＋ P・I）、フェイルセーフの異常の見つけ方
        │   ├── wall_ctrl.c/.h    壁のあり・なし、壁に沿った姿勢の補正、前壁合わせ、壁までの距離 [mm]
        │   ├── wall_dist.c/.h    壁センサの値 → 壁までの距離 [mm]（表）
        │   ├── side_edge.c/.h    壁切れ（壁の終わり・柱の端）
        │   └── run.c/.h          探索・最短走行の動き
        │
        ├── maze/                 迷路（HAL に依存しない。PC でテストできる）（作った。ステップ8）
        │   ├── maze_map.c/.h     壁の情報の保持・更新（16×16）、最後に壁を記録した 5 区画ぶんを戻す
        │   ├── search.c/.h       足立法・歩数マップ・未知壁探索で見に行く壁。全面探索と行き止まり潰し（KOGUMA から足した）
        │   ├── dijkstra.c/.h     ダイクストラ：スタートからゴールまでの速く走れる道
        │   └── path.c/.h         経路：既知区間（小回り。斜めにする形も KOGUMA から足した）、最短経路の大回り・斜めへの変換
        │
        └── sys/                  全体の管理
            ├── scheduler.c/.h    制御周期（2kHz、TIM11）の割り込みで制御の処理を呼び、処理時間と間隔を記録する
            ├── mode.c/.h         走行モードの選択と実行（モード表）（作った）
            ├── ui.c/.h           モード選択の操作、LED とブザーでの表示と合図（作った）
            ├── goal.c/.h         ゴールの位置（作った）
            ├── explore.c/.h      探索走行（maze と ctrl をつなぐ唯一の場所）。KOGUMA のモード0 No.1〜7（作った）
            ├── shortest.c/.h     最短走行
            ├── adjust.c/.h       動きを 1 つずつ走らせて調整する（作った。吸引探索のスラローム・直線の加速の限界テスト・モーターの向きを足した）
            ├── turn_test.c/.h    最短走行のターンを 1 つずつ走らせる
            ├── edge_test.c/.h    壁切れ・柱の見え方テスト（作った）
            ├── encoder_cal.c/.h  エンコーダの歪みを測る（作った）
            ├── sensor_view.c/.h  センサの値の表示、部品の確かめ（作った）
            ├── sensor_record.c/.h  センサの値の記録（壁までの距離の表を作る）（作った）
            ├── sensor_noise.c/.h  吸引のときのセンサのノイズ（新）（作った。KOGUMA.MkIIl の SensorNoise.c）
            ├── suction.c/.h      吸引ファンを少しずつ上げて回し始める（作った）
            ├── log.c/.h          走行ログの記録と吸い出し
            ├── output.c/.h       出力（迷路と走行ログを、ビューアで読める形で PC へ出す）（作った）
            ├── failsafe.c/.h     フェイルセーフで止まっているかを覚える、止める
            └── flash.c/.h        迷路とゴールの位置の Flash への保存と読み込み（作った）
```

### 層の依存関係（ToraNeko.Mk3 と同じ）

```
sys（explore / mode / ...）
 ├→ ctrl  →  drv  →  HAL（Core/, Drivers/）
 └→ maze
```

- 呼び出しは上の層から下の層への一方向のみ
- `ctrl` と `maze` はお互いを知らない。「迷路で次の行き先を決める → その動作を走らせる → 壁を見て迷路を更新する」の繰り返しは `sys/explore.c` だけが書く
- HAL を直接呼ぶのは `drv/` と `sys/scheduler.c` だけ
- `maze/` は `drv` も HAL も使わない。PC 上でそのままテストできる
- `Core/` 側の生成コードには、USER CODE 欄に `App/` の関数呼び出しを 1 行書くだけにする

## 命名規則（ToraNeko.Mk3 と同じ）

| 対象 | 規則 | 例 |
|---|---|---|
| ファイル名 | 小文字＋アンダースコア。層はフォルダで表し、ファイル名には付けない | `drv/encoder.c` |
| 関数名 | `モジュール名_動詞_目的語`（小文字＋アンダースコア） | `encoder_read_angle()`, `motor_set_voltage()` |
| 型名 | 小文字＋アンダースコア＋`_t` | `odometry_state_t` |
| ファイルをまたぐ変数 | `g_` を付ける（なるべく使わず、関数経由で受け渡す） | `g_maze_map` |
| ファイル内だけの変数・関数 | `static` を付ける | `static float s_prev_error;` |
| 定数・マクロ | 大文字＋アンダースコア | `MAZE_SIZE`, `CTRL_PERIOD_S` |
| インクルードガード | `ファイルパスを大文字に_H` | `DRV_ENCODER_H` |

## 実装の進め方

ToraNeko.Mk3 のステップ（0〜9）と同じ順に進める。各ステップで「ToraNeko.Mk3 の該当ファイルを持ってくる → KOGUMA の機体に合わせて直す（ピン・周辺機能・機体の値）→ ビルド → 作者が実機で確かめる」を行う。
ToraNeko.Mk3 では作り方を考えながら進めたが、KOGUMA.Mk3.1 では手本があるので、1 ステップは「持ってくる・直す・確かめる」だけになる。

| # | 内容 | 持ってくる ToraNeko.Mk3 のファイル | KOGUMA で変えるところ | KOGUMA.MkIIl の対応ファイル | 状態 |
|---|---|---|---|---|---|
| 0 | F446 用の CMake プロジェクト作成 | `CMakeLists.txt`、`cmake/`、`Core/`（ひな形）、`App/app.c` | `.ioc` と `Core/` を、KOGUMA.MkIIl の設定と決めた設定（2kHz・TIM11 など）に合わせる | `KOGUMA.MkIIl.ioc`、`Core/`、`Drivers/` | **完了**（2026-11-08。CubeMX を使わずに作った。[CUBEMX_SETUP.md](CUBEMX_SETUP.md) の 0。ビルドは警告なし、CI でも確かめる。実機と、CubeMX で開けるかはまだ） |
| 1 | LED・シリアル出力（printf）・ブザー | `drv/led.c`, `drv/uart.c`, `drv/timebase.c` | LED 8 個＋壁の表示 4 個、USART2（921600bps）、ブザー（新、`drv/buzzer.c`） | `UI.c`, `syscalls.c` | **作った**（2026-11-08。ビルドは警告なし。実機はまだ。下の「ステップ1の確かめ方」） |
| 2 | 制御周期の割り込み | `sys/scheduler.c` | TIM11、2kHz（ToraNeko と同じ。タイマーのクロックが 180MHz なので PSC だけ違う） | `PL_timer.c` | **作った**（2026-11-08。ビルドは警告なし。実機はまだ。下の「確かめ方」） |
| 3 | エンコーダ | `drv/encoder.c` | SPI3、CS 2 本。磁石がモーターの軸ならギア比を掛ける。角度の歪みの補正（ToraNeko のモード6 No.6）を持ち込むか | `PL_encoder.c` | **作った**（2026-11-08。ビルドは警告なし。実機はまだ。下の「確かめ方」）。向き（`ENCODER_*_SIGN`）は KOGUMA.MkIIl に合わせた。角度の歪みの補正の表は全部 0（測るのは `sys/encoder_cal.c` を持ってきてから） |
| 4 | IMU | `drv/imu.c` | CS 名 `ICM_NSS` → `IMU_CS`。軸の向きと符号を確かめる | `lsm6dsr.c` | **作った**（2026-11-08。ビルドは警告なし。実機はまだ。下の「確かめ方」）。KOGUMA.MkIIl の角速度の倍率 1.01 も入れた |
| 5 | モーター・吸引ファン | `drv/motor.c`, `drv/fan.c` | TIM3 の 4ch（ドライバの型番で IN の使い方が変わる。回路図で確認）、TIM2 CH1。電圧の上限 | `motor.c`（出力の部分） | **作った**（2026-11-08。ビルドは警告なし。実機はまだ。下の「確かめ方」）。電圧の上限は KOGUMA.MkIIl に合わせた（モーターは電池の電圧まで、ファンは 13.4V） |
| 6 | 壁センサ | `drv/wall.c`, `ctrl/wall_ctrl.c`, `ctrl/wall_dist.c` | ADC1 の 5ch（電池＋4）、LED 2 組。しきい値と距離の表は KOGUMA.MkIIl の値（`WallDistance.c`）から | `PL_sensor.c`, `Wallsensor.c`, `WallDistance.c` | **作った**（2026-11-08。実機はまだ）。AD 値の読み取り（`drv/wall.c`）、壁のあり・なしと前壁合わせ・横の壁のずれ（`ctrl/wall_ctrl.c`。しきい値は KOGUMA.MkIIl の `Wallsensor.c` の値）、壁までの距離（`ctrl/wall_dist.c`。KOGUMA.MkIIl の `WallDistance.c` の表と会場ごとの倍率。区画のまん中で 84mm。PC テスト `test_wall_dist`）。受光の並びと発光の組は ToraNeko と同じ。KOGUMA.MkIIl は値をならして（10 回の平均など）使っていたが、ToraNeko と同じく、ならさずに使う。実機の値を見て、しきい値とならすかを決め直す |
| 7 | 速度・角度の制御、探索の動き、フェイルセーフの見つけ方 | `ctrl/odometry.c`, `ctrl/trajectory.c`, `ctrl/control.c`, `ctrl/run.c`, `ctrl/side_edge.c`, `config/ctrl_gains.h` | 制御方法は ToraNeko のまま（[KOGUMA_PORTING.md](KOGUMA_PORTING.md) の E2・E3）。タイヤ径・トレッド・ギア比・区画 180mm。係数とフェイルセーフのしきい値は KOGUMA の機体で決め直す（フィードフォワードの初めの値は KOGUMA のログのモデルから） | `motor.c`, `Move.c`, `SpeedPlan.c`, `PillarEdge.c`, `Failsafe.c` | **速度・角度の制御を作った**（2026-11-08。実機はまだ）：速さと距離（`ctrl/odometry.c`。1 カウントの距離は KOGUMA.MkIIl の計算と同じ）、台形の目標（`ctrl/trajectory.c`）、制御とフェイルセーフの見つけ方（`ctrl/control.c`）、壁切れの端（`ctrl/side_edge.c`。値は仮）、フェイルセーフ（`sys/failsafe.c`）、走行ログ（`sys/log.c`。1600 行）。どれも ToraNeko のまま。係数は `config/ctrl_gains.h` に KOGUMA.MkIIl の値から換算して入れた（換算の表はそのファイルの先頭）。PC テスト `test_trajectory`・`test_control`。確かめは下の「確かめ方」の 11（直進）。**探索の動きも作った**（2026-11-08。`ctrl/run.c`。ToraNeko のまま、探索を吸引なし 500mm/s・吸引あり 1000mm/s の 2 とおりにした。値は KOGUMA.MkIIl の `Move.c` から `config/run_params.h` に入れ、スラロームの前距離・後距離は PC で計算した（`test/test_turn.c --calc`）。前壁補正・壁切れは測るだけ）。確かめは 12（スラローム） |
| 8 | 迷路探索と最短経路 | `maze/*`, `test/` | `MAZE_SIZE` 16。テストの迷路を 16×16 に（KOGUMA.MkIIl の `mazes/` を使う） | `Maze.c` | **完了**（2026-11-08、PC テスト済み）。下の「ステップ8でやったこと」 |
| 9 | 探索走行・モード・ログ・フェイルセーフ・保存 | `sys/*`, `drv/flash_memory.c`, `drv/switch.c`, `tools/viewer.html` | モードの割り当てを決める（MODES.md）。KOGUMA にしかない機能（全面探索、感度合わせなど）をどうするか（[KOGUMA_PORTING.md](KOGUMA_PORTING.md)） | `main.c`, `UI.c`, `LOG.c`, `Failsafe.c` | **モードの土台を作った**（2026-11-08。実機はまだ）：操作と合図（`sys/ui.c`。LED 8 個とブザー）、モード選択（`sys/mode.c`）、ゴール（`sys/goal.c`）、Flash への保存（`sys/flash.c`・`drv/flash_memory.c`）、ログ出力（`sys/output.c`）、調整の動き（`sys/adjust.c`）、センサの表示と記録（`sys/sensor_view.c`・`sensor_record.c`）、吸引（`sys/suction.c`）。どれも ToraNeko のまま、LED の並びと音、区画の大きさを KOGUMA に合わせた。モードは下の「モード一覧」と [MODES.md](MODES.md)。確かめ方は下の「確かめ方」。**探索も作った**（`sys/explore.c`。モード0 No.1〜7。ToraNeko の探索に、吸引探索とエセ全面探索を足した。PC テスト `test_maze` の [9] で、仮の機体で走らせて確かめた）。**壁切れのテスト（`sys/edge_test.c`）、エンコーダの歪み（`sys/encoder_cal.c`）、吸引のときのセンサのノイズ（`sys/sensor_noise.c`。B7）、ビューア（`tools/viewer.html`）・ログの残し方（`logs/README.md`・`tools/push_logs.sh`）・壁までの距離の表を作る道具（`tools/wall_table.py`）も作った**（2026-11-08）。モード0 No.8〜12 はステップ10 |
| 10 | 最短走行（大回り・斜め）とターンの値 | `sys/shortest.c`, `sys/turn_test.c`, `config/turn_params.h`, `test/test_turn.c` | ターンの値は KOGUMA.MkIIl の 2000・2400 の値から。180mm の区画で `test_turn --calc` をやり直す | `Move.c`, `ShortParam.c` | **作った**（2026-11-08。実機はまだ）：`test/test_turn.c` で、組 2000・組 2400 のターンの前距離・後距離を計算した（角加速度は KOGUMA の w_ac × 2/π、大回り 180° の角速度は幅が 180mm になる値）。最短走行（`sys/shortest.c`。モード1 No.1〜9。No.4〜9 は KOGUMA.MkIIl と同じ値）、ターンの調整（`sys/turn_test.c`。モード3）、未知壁探索で最短走行の組ごとの経路を見る（B15。`sys/explore.c`）。PC テスト `test_maze` の [10] で、仮の機体で No.1〜9 を走らせた。**モード0 No.8〜12（探索のあとに続けて最短走行）はこのあと**。実機で足す前距離・後距離はまだ |

### ステップ1〜7・9の確かめ方（作者にお願いすること）

2026-11-08 から、確かめはモードを選んで行う（[MODES.md](MODES.md)。ステップ9 で ToraNeko と同じ操作にしたため）。

**注意：書き込むと、機体に入っている KOGUMA.MkIIl のファームウェア（大会用）は消える。** 学生大会（11-21〜22）の前なら、確かめたあとに CubeIDE で KOGUMA.MkIIl を書き戻すこと。

1. GitHub Desktop で、ブランチ `claude/clever-ride-k9xfb3` に切り替えて、**Fetch origin → Pull** を押す（`software/main-controller/KOGUMA.Mk3.1/` に `CMakeLists.txt` などが入っていれば成功）
2. VS Code で **File → Open Folder** を押し、`software/main-controller/KOGUMA.Mk3.1` フォルダを開く
3. 書き込みのタスクを入れる（1 回だけ）：[tools/vscode/README.md](tools/vscode/README.md) の「入れ方」のとおり、`tools/vscode/tasks.json` を `KOGUMA.Mk3.1/.vscode/tasks.json` にコピーする
4. 機体に STLINK-V3 をつなぎ、電源を入れる
5. **Ctrl+Shift+B** を押す。下の「ターミナル」に、ビルドのあと `File download complete` と `MCU Reset` が出たら書き込みは成功
6. 機体を見る。うまくいっていれば、電源を入れた（リセットした）ときに、表示 LED が LED1 → LED8 の順に 1 個ずつ点いて消える（「ロード中」、1 秒）。そのあと全部消えて、モードを選ぶ状態になる
   - 外側の 4 個（LED1・2・7・8）が点滅し続けて、低い音が鳴り続けたら、電池が少ない（15.6V より低い）。電池を替える
7. シリアルを見る：VS Code の Serial Monitor（ToraNeko.Mk3 と同じ拡張）で、STLINK の仮想 COM ポートを選び、**Baud rate を 921600** にして **Start Monitoring** を押す。機体のリセットボタンを押すと、次の行が出る
   - `KOGUMA.Mk3.1 boot (SYSCLK 180000000 Hz)`（180MHz で動いている）
   - `Encoder L: AGC=... MAGL=0 MAGH=0 ...`（左右）、`IMU OK (LSM6DSR)`、`Battery 16.xx V`。`MAGL=1`（磁石が弱い・遠い）や `IMU NG` が出たら、その行を Claude に貼る
8. モードの選び方（[MODES.md](MODES.md) の 2・3）：**左タイヤを手で前に回すと、モードの LED（LED1〜4）が 1 つずつ増える**。**右タイヤを前に回すと、No. の LED（LED5〜8）が増える**。どちらも後ろに回すと減る
   - LED1〜8 が基板のどこにあるか（左から順に並んでいるか）を見て、Claude に伝える（並びが違えば `sys/ui.c` を直す）
   - タイヤ 1 段の回す量（KOGUMA.MkIIl と同じ）が回しやすいかも伝える
9. 部品の確かめ（モード5 No.7）：機体を**動かさずに**置いて、モード5（LED2・LED4）・No.7（LED6・LED7・LED8）を選び、S2 を押す。すぐに始まり、1 秒ごとに 5 行が出る（S2 を押すと終わる）
   - `tick=2000 period=500-500 us busy=...`：制御周期の割り込みが 1 秒に 2000 回、0.5ms ごとに来ている（ステップ2）。`busy` は 1 回の処理の時間で、500 より十分小さい
   - `enc total L=... R=...`：**タイヤを手で前に転がすと、L も R も増える**（後ろに転がすと減る）。どちらかが減ったら、その向きを Claude に伝える。`err` は 0 のまま（ステップ3）
   - `imu ready=1 gyro=0.0 dps angle=...`：置いたままなら `gyro` は 0 の近く。**機体を上から見て左回り（反時計回り）に回すと、`angle` が増える**。ちょうど 1 回転（360°）回して戻したときの `angle` を Claude に伝える（ステップ4。KOGUMA.MkIIl の倍率 1.01 が合っているか）
   - `batt=16.xx V`：テスターで測った電池の電圧と近いか（ステップ6。分圧の抵抗が合っているか）
   - `wall FL=... L=... R=... FR=...`：**左前・左・右・右前のセンサの前に、1 つずつ手をかざすと、その値だけが大きくなる**。違うセンサの値が大きくなったら、どれをかざしたときにどれが大きくなったかを Claude に伝える。`overrun` は 0 のまま（ステップ6）
   - `mm FL=... wall front=... left=... right=...`：機体を区画のまん中に置き、前・左・右に壁があるとき、距離がどれも 84 の近くになり、`front=1 left=1 right=1` になる。壁を 1 枚ずつ外すと、その壁の数字が 0 になる。表示 LED（左の壁 LED1・2、前の壁 LED3〜6、右の壁 LED7・8）と、壁の表示の LED（LEDF1 左前・LEDF2 右・LEDF3 左・LEDF4 右前）も同じように点く。**区画のまん中に置いて、3 枚とも壁あり、前の壁なし、左の壁なし、右の壁なし のときの `wall FL=... L=... R=... FR=...` の値**を Claude に伝える（しきい値を、ならさない値で決め直すため）
10. モーターの向き（モード6 No.2。ステップ5）：**機体をメンテナンス台などに載せて、タイヤを浮かせる**。モード6（LED2・LED3）・No.2（LED7）を選んで S2 を押し、前センサに手をかざして離す。音が鳴って 1 秒後に、左右のタイヤが 1.0V で 2 秒ゆっくり回って止まる。**左右とも、機体が前に進む向きに回るか**を見る。シリアルの `MOTOR enc L=... R=...` は、前に回っていれば両方とも増える。逆に回ったほうを Claude に伝える
11. 直進（モード6 No.1。ステップ7）：10 で左右とも前に回ることを確かめてから、機体を**スタート区画に、お尻を後ろの壁に付けて置き**、前に 4 区画あけておく。モード6・No.1（LED8）を選んで S2 を押し、前センサに手をかざして離すと、1 秒後（ジャイロのゼロ点を 0.5 秒測ってから）、564mm（3 区画と少し。4 区画目のまん中まで）を 500mm/s で走って止まる
    - まっすぐ走って止まるか、壁に当たらないか、止まる位置（4 区画目のまん中のはず）を見る
    - **ものさしで、走った距離**（後ろの壁からお尻まで）**を測る**
    - `failsafe` で止まったとき（8 個全部が点滅して低い音）は、S2 で戻る。モーターかエンコーダの向きが逆のことが多い
12. スラローム（モード2 No.1。ステップ7 の探索の動き）：11 の直進がまっすぐ走れてから。機体を**区画のまん中に**置き、前の区画と、その左の区画をあけておく。モード2（LED3）・No.1（LED8）を選んで S2 を押し、前センサに手をかざして離す
    - 次の区画の入口まで速くしながら進み、500mm/s のまま左にスラロームで曲がって、左の区画の**まん中**で止まる
    - 止まった位置が、区画のまん中から前後・左右に何 mm ずれたか、向きが何度ずれたかを見る（外にふくらんだか、内側に入ったか）
13. 結果を出す（モード7 No.1）：11・12 のあと、ケーブルを挿して、モード7（LED2・LED3・LED4）・No.1（LED8）を選んで S2 を押し、前センサに手をかざして離す。`#BEGIN RESULT`（`Straight: target 564.0 mm, encoder ... mm, angle ... deg` か `Slalom: target 90.0 deg, gyro ... deg, encoder ... mm`）、迷路、走行ログ（`#BEGIN LOG` から `#END LOG` まで）、`#DONE` が出る。
    - ビューア（`tools/viewer.html`）で保存する：[logs/README.md](logs/README.md) の「残し方」のとおり、Chrome か Edge で `tools/viewer.html` を開き、「保存先フォルダ」で `software/main-controller/logs/` を選んでから「接続」し、モード7 No.1 を実行する。届いたデータが 1 つずつファイルになるので、`git logs`（同じ README の 6）で push する。Claude はそのファイルを読む
    - ビューアを使わないときは、Serial Monitor に出た文字を全部そのまま Claude に貼る
    - 結果は、最後に走らせた 1 つだけ覚えている。11 と 12 は、それぞれ走らせたあとに 13 をする
14. Claude に伝えること
   - 6〜13 のとおりになったか（ならなかったら、どこが違ったか。シリアルに出た文字はそのまま貼る）
   - 11 でものさしで測った距離
   - LED1〜8・LEDF1〜4 の**場所**（8 で見たもの）
   - ブザーの音の大きさ・高さ（KOGUMA.MkIIl と同じくらいか）

### モード一覧（2026-11-08。くわしくは [MODES.md](MODES.md)）

操作は ToraNeko.Mk3 と同じ（左タイヤでモード、右タイヤで No.、S2 で決定、前センサに手をかざして離すとスタート）。モードは LED1〜4、No. は LED5〜8 に 2 進数で出る。

| モード | No. | 中身 | 状態 |
|---|---|---|---|
| 0 | 1〜7 | 探索（KOGUMA.MkIIl のモード0 と同じ番号。No.1〜3 は吸引なし、No.4〜7 は吸引あり） | 作った |
| 0 | 8〜12 | 探索のあとに続けて最短走行、既知区間を斜めで | ステップ10 |
| 1 | 1 | 最短走行（小回りだけ、吸引なし。道を確かめる） | 作った |
| 1 | 2・3 | 最短走行（大回り・斜め、直進の最高速 ＝ ターンの速さ。組 2000・2400。KOGUMA.MkIIl のモード3 No.1・2） | 作った |
| 1 | 4〜9 | 最短走行（KOGUMA.MkIIl のモード1 No.4〜9 と同じ値） | 作った |
| 2 | 1 | スラローム 90°（吸引なし 500mm/s） | 作った |
| 2 | 2 | U ターン | 作った |
| 2 | 3 | 吸引探索のスラローム 90°（吸引 5.5V、1000mm/s） | 作った（KOGUMA.Mk3.1 で足した） |
| 2 | 4 | その場 180° 旋回 | 作った |
| 2 | 5 | 壁合わせだけ | 作った |
| 3 | 1〜7 | 最短走行のターン調整（組 2000） | 作った |
| 3 | 9〜15 | 最短走行のターン調整（組 2400） | 作った |
| 4 | 1 | 吸引なしの加速度テスト | 作った |
| 4 | 2・3 | 斜めの直進のテスト（補正なし・あり。ToraNeko のモード3 No.8・9） | 作った |
| 4 | 4〜11 | 吸引ありの加速度テスト（8000〜40000mm/s²） | 作った |
| 4 | 12 | 直線の加速の限界テスト（KOGUMA.MkIIl のモード6 No.0） | 作った（B16） |
| 5 | 1 | センサ値の表示 | 作った |
| 5 | 2 | センサ値の記録 | 作った |
| 5 | 3 | 壁までの距離の表示 | 作った |
| 5 | 4〜6 | 壁切れ・柱の見え方テスト（500・1000・2000mm/s） | 作った |
| 5 | 7 | 部品の確かめ | 作った（KOGUMA.Mk3.1 で足した） |
| 5 | 8 | 吸引のときのセンサのノイズ（KOGUMA.MkIIl のモード3 No.10） | 作った（B7） |
| 6 | 1 | 直進 | 作った |
| 6 | 2 | モーターの向きの確かめ | 作った（KOGUMA.Mk3.1 で足した） |
| 6 | 3 | 吸引 ON | 作った |
| 6 | 6 | エンコーダの歪みを測る | 作った |
| 6 | 7 | 壁に沿って直進 | 作った |
| 7 | 1 | ログ出力 | 作った |
| 7 | 2 | ゴールの位置を入れる | 作った |
| 7 | 15 | 迷路呼び出し | 作った |

### ステップ8でやったこと（2026-11-08）

- ToraNeko.Mk3 の `App/maze/`（`ecd8e10`）を持ってきて、`MAZE_SIZE` を 16 にした。迷路の処理は大きさを定数で持っているので、ほかはそのまま動いた
- 走る時間の見積もり（ダイクストラのコスト）に使う値は、KOGUMA の値にした：区画 180mm、スタートから境目まで 114mm、ターンは KOGUMA の 2000 の値（`config/run_params.h`・`config/turn_params.h`。迷路が使う値だけ。ターンの値は仮で、ステップ10で計算し直す）
- 作者の決定（2026-11-08）「今の探索モードは全部残す」に合わせて、KOGUMA にあって ToraNeko にない迷路の機能を足した（[KOGUMA_PORTING.md](KOGUMA_PORTING.md) の B）
  - **全面探索と行き止まり潰し**（モード0 No.2・5）：`search_set_target_unvisited()`。目的地を「まだ入っていない区画すべて」にし、見た壁で袋小路とわかった区画は外す。見に行く区画がなくなるか、たどり着けなくなったらスタートへ帰る
  - **既知区間を斜めで走る経路**（モード0 No.12）：`path_make_known_diagonal()`。斜めのまま終わるときは小回りに戻す
- PC テスト（`test/test_maze.c`）：ToraNeko の迷路のテスト（1〜8、11）を 16×16 にし、全面探索（12）と既知区間の斜め（13）のテストを足した。2025 年の全日本のクラシックの迷路（KOGUMA.MkIIl の `mazes/`）でも走らせる。全部通る（PC で約 5 秒）。CI（`.github/workflows/firmware-build.yml`）でも実行する
- 迷路の処理の RAM は約 15KB（ARM 向けにコンパイルして確かめた。ダイクストラ 10.7KB、壁の地図 1.4KB、経路 1.1KB、探索 1.8KB）
- 探索走行（`sys/explore.c`）と最短走行（`sys/shortest.c`）は、ステップ9・10 で持ってくる。そのときに、仮の機体で走らせるテスト（ToraNeko の test_maze の 9・10）も足す

### KOGUMA.MkIIl のファイルとの対応

ToraNeko.Mk3 の README の表と同じ。

| KOGUMA.MkIIl | KOGUMA.Mk3.1 |
|---|---|
| `PL_encoder.c` | `drv/encoder.c`、`sys/ui.c`（タイヤを回してモードを選ぶ部分） |
| `lsm6dsr.c` | `drv/imu.c` |
| `PL_sensor.c` | `drv/wall.c`（LED 点灯・AD 読み取り・電池の電圧） |
| `Wallsensor.c`、`WallDistance.c` | `ctrl/wall_ctrl.c`、`ctrl/wall_dist.c` |
| `motor.c` | `drv/motor.c`、`drv/fan.c`、`ctrl/control.c`、`ctrl/odometry.c` |
| `Move.c`、`SpeedPlan.c` | `ctrl/trajectory.c`、`ctrl/run.c`、`sys/explore.c`（迷路判断の部分）、`sys/shortest.c` |
| `PillarEdge.c` | `ctrl/side_edge.c` |
| `Maze.c` | `maze/maze_map.c`、`maze/search.c`、`maze/path.c`、`maze/dijkstra.c` |
| `PL_timer.c` | `sys/scheduler.c` |
| `main.c`（モード選択部分） | `sys/mode.c`、`sys/explore.c`、`sys/adjust.c` など |
| `UI.c` | `sys/ui.c`、`drv/led.c`、`drv/buzzer.c`、`drv/switch.c` |
| `LOG.c`、`DistHist.c`、`SensorNoise.c` | `sys/log.c`、`sys/output.c`（`DistHist`・`SensorNoise` を持ち込むかは [KOGUMA_PORTING.md](KOGUMA_PORTING.md)） |
| `Failsafe.c` | `sys/failsafe.c`、`ctrl/control.c`（異常の見つけ方） |
| `ShortParam.c` | `config/turn_params.h`、`sys/shortest.c` |
| `Define.h` | `config/robot_params.h`、`config/ctrl_gains.h`、`config/run_params.h` |
