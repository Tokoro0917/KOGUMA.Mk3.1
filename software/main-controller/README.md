# main-controller

KOGUMA.Mk3 メイン基板（STM32F446RE）の、書き直したファームウェアです。

ToraNeko.Mk3 のファームウェア（`software/main-controller/TORANEKO.Mk3/App/`）を手本にし、KOGUMA.MkIIl のコード（[../reference/koguma-mkiil/](../reference/koguma-mkiil/)）から機体の値と機能の中身を持ってきます。
ToraNeko.Mk3 が KOGUMA のコードを作り直したときと同じように、機能ごとに 1 つずつ作り、実機で確かめたものだけを置きます。

- [PIN_REQUIREMENTS.md](PIN_REQUIREMENTS.md): ピン割り当て（KOGUMA.MkIIl の `.ioc` から読んだもの。回路図で確かめる）と、ToraNeko.Mk3 との違い
- [CUBEMX_SETUP.md](CUBEMX_SETUP.md): CubeMX（`.ioc`）で CMake のプロジェクトを作る手順と設定値（ステップ0。作者にやってもらう）
- [PORTING.md](PORTING.md): ToraNeko.Mk3 の `App/` のどれをそのまま持ってくるか、KOGUMA の機体に合わせて何を変えるか、KOGUMA にしかない機能をどうするか
- MODES.md（まだない）：機体の操作とモードの割り当て。ステップ9で、KOGUMA.MkIIl と ToraNeko.Mk3 の MODES.md を見ながら決める

## 書き直しの考え方

1. **作り（ファイルの分け方・層・書き方）は ToraNeko.Mk3 に合わせる。** ToraNeko.Mk3 の `App/` のファイルを持ってきて、KOGUMA の機体に合わせて直す。同じ名前の関数は同じ仕事をするようにしておくと、毎朝の確認で ToraNeko.Mk3 の変更をそのまま持ってきやすい
2. **機体の値（センサのしきい値・ターンの値・ゲイン・タイヤ径）は KOGUMA.MkIIl から持ってくる。** KOGUMA.MkIIl の実機のログ（`logs/`）で決めた値なので、単位と制御周期をそろえて書き直す
3. **ハーフサイズとクラシックの違いは定数にまとめる。** 迷路 16×16、1 区画 180mm、柱 12mm（`config/` と `maze/maze_map.h`）
4. 大会（2026-11-21〜22 の学生大会）は、今までどおり KOGUMA.MkIIl のファームウェアで走る。KOGUMA.Mk3.1 は、それと関係なく進める

## 決定事項（案）

「案」は、作者にまだ確かめていないもの。決まったら「決定」にする。

| 項目 | 決定 | 理由・補足 |
|---|---|---|
| 開発環境 | 案：**STM32CubeMX（コード生成、Toolchain = CMake）＋ VS Code（STM32Cube for Visual Studio Code 拡張）** | ToraNeko.Mk3 と同じ。コマンドラインだけでビルドできるので、CI や Claude の環境でもファームのビルドを確かめられる。KOGUMA.MkIIl の CubeIDE はやめる |
| プロジェクト名 | 案：`KOGUMA.Mk3.1` | リポジトリ名と揃える。成果物は `build/Debug/KOGUMA.Mk3.1.elf` |
| ビルド | `cmake --preset Debug && cmake --build --preset Debug`（`software/main-controller/KOGUMA.Mk3.1/` で） | CubeMX が生成する `CMakePresets.json` をそのまま使う |
| ライブラリ | STM32 HAL | ToraNeko.Mk3 と同じ |
| クロック | HSE 10MHz → SYSCLK 180MHz（KOGUMA.MkIIl と同じ） | APB1 45MHz（タイマー 90MHz）、APB2 90MHz（タイマー 180MHz） |
| 書き込み・デバッグ | SWD（PA13・PA14） | KOGUMA.MkIIl と同じ。書き込み器は作者に確認 |
| PC との通信・printf | USART2 の TX だけ（PA2）、DMA で送る | KOGUMA.MkIIl と同じピン。送り方は ToraNeko.Mk3 の `drv/uart.c`（DMA・リングバッファ）。速さ（今は 115200bps）を上げるかは作者に確認 |
| 制御周期 | 案：**1kHz から始める**（`config/robot_params.h` の `CTRL_FREQ_HZ` で変えられるようにする） | KOGUMA.MkIIl のゲインとログが 1kHz なので、まず同じ周期で比べる。ToraNeko.Mk3 は 2kHz。F446 は 180MHz で余裕があるので、処理時間を測ってから 2kHz にするか決める |
| 制御周期のタイマー | 案：TIM6（KOGUMA.MkIIl と同じ） | ピンを使わない内部タイマー |
| 処理時間計測・µs 待ち | 案：TIM5（32bit）を 1MHz でフリーラン（**新しく足す**） | ToraNeko.Mk3 と同じ。KOGUMA.MkIIl にはない |
| ms 単位の待ち | SysTick（`HAL_GetTick()`） | ToraNeko.Mk3 と同じ。KOGUMA の「共通カウンタを 0 に戻す `wait_ms`」はやめる |
| 壁センサの点灯・測定の順番 | 案：タイマーで待ち時間を作ってから ADC を起動し、DMA 完了割り込みで次の LED に進める連鎖（ToraNeko.Mk3 の方式）。待ち時間のタイマーは TIM8 | ToraNeko.Mk3 は TIM3 を使うが、KOGUMA の TIM3 はモーターの PWM。ADC を起動できるタイマーのうち空いているのは TIM8 だけ（TIM6・TIM7 は ADC を起動できない） |
| ブザー | 案：持ち込む（`drv/buzzer.c`。TIM4 CH4） | ToraNeko.Mk3 にはない。KOGUMA は合図をブザーで出しているので残す |
| 迷路 | 16×16、ゴールは中央 2×2 | ゴールの位置を機体で入れて Flash に保存するか（ToraNeko.Mk3 の方式）は、ステップ9で決める |
| 迷路と設定の保存 | 案：Flash のセクタ6・7（ToraNeko.Mk3 と同じ。プログラムは前の 256KB に収める） | F446RE と F411CE は Flash のセクタの分け方が同じ。KOGUMA.MkIIl は保存していなかった |
| ファイル構成・命名規則 | ToraNeko.Mk3 と同じ（下の「ファイル構成」「命名規則」） | |

## ファイル構成（予定）

ToraNeko.Mk3 と同じ形にする。「（ToraNeko）」は ToraNeko.Mk3 の同じ名前のファイルを持ってきて直すもの、「（新）」は KOGUMA.Mk3.1 で新しく作るもの。

```
software/main-controller/
├── README.md                     決定事項・ファイル構成・実装ステップ（このファイル）
├── PIN_REQUIREMENTS.md           ピン割り当て
├── CUBEMX_SETUP.md               CubeMX の手順と .ioc の設定値
├── PORTING.md                    ToraNeko.Mk3 の App/ を持ってくるときの判断
├── MODES.md                      機体の操作とモード（ステップ9で作る）
├── tools/                        ビューア（viewer.html）など（ToraNeko）
├── logs/                         実機の出力を残すフォルダ
├── test/                         PC 上で動かすテスト（CI で実行）。ホストの gcc でビルドするので、ARM 向けの CMake プロジェクトとは分ける
│   ├── test_maze.c               迷路のテスト（ToraNeko の test_maze.c を 16×16 にして、全面探索・既知区間の斜めのテストを足した）。`make` で実行
│   ├── Makefile
│   └── mazes/                    テスト用の 16×16 の迷路と、作る道具（gen_maze.py、KOGUMA の迷路の出力を直す from_koguma_dump.py）
│
└── KOGUMA.Mk3.1/                 CubeMX（CMake）プロジェクト（作者に CubeMX で作ってもらう）
    ├── KOGUMA.Mk3.1.ioc          CubeMX 設定（KOGUMA.MkIIl.ioc をもとにする）
    ├── CMakeLists.txt            ビルド設定（初回だけ生成。App/ の追加はここに書く）
    ├── CMakePresets.json         Debug / Release のプリセット（生成）
    ├── cmake/                    ツールチェーン設定（生成）
    ├── startup_stm32f446xx.s     スタートアップ（生成）
    ├── STM32F446xx_FLASH.ld      リンカスクリプト（生成。名前は CubeMX が決める）
    ├── Core/                     CubeMX が生成。USER CODE 欄以外は編集しない
    ├── Drivers/                  HAL / CMSIS（生成。コミットする）
    │
    └── App/                      自作コードはすべてここ
        ├── app.c / app.h         app_init()、app_loop()。全体の入口（ToraNeko）
        ├── config/               機体の定数（値は KOGUMA.MkIIl から）
        │   ├── robot_params.h    制御周期、電池の下限、タイヤ径、トレッド幅、ギア比
        │   ├── ctrl_gains.h      速度・角度の制御の係数
        │   ├── run_params.h      区画の大きさ（180mm）とスタートの位置。探索の動きの値はステップ7で足す
        │   └── turn_params.h     最短走行のターンの値（今は KOGUMA の 2000 の値を仮に入れた。ステップ10で計算し直す）
        ├── drv/                  ハードに直接触れる層（ToraNeko。ピンと周辺機能を KOGUMA に合わせる）
        │   ├── encoder.c/.h      AS5047P ×2（SPI3）
        │   ├── imu.c/.h          LSM6DSR（SPI1）
        │   ├── motor.c/.h        モーターの PWM（TIM3 CH1〜4）
        │   ├── fan.c/.h          吸引ファン（TIM2 CH1）
        │   ├── wall.c/.h         壁センサ（ADC1、LED 2 組）と電池の電圧
        │   ├── buzzer.c/.h       ブザー（TIM4 CH4）（新）
        │   ├── led.c/.h          LED1〜8、壁の表示の LED 4 個
        │   ├── switch.c/.h       スイッチ S2（PB3）
        │   ├── flash_memory.c/.h 内蔵 Flash の保存場所を消す・書く・読む
        │   ├── timebase.c/.h     経過時間（ms は SysTick、µs は TIM5）
        │   └── uart.c/.h         ログ送信（USART2、DMA）
        ├── ctrl/                 制御（ToraNeko）：odometry、trajectory、control、wall_ctrl、wall_dist、side_edge、run
        ├── maze/                 迷路（ToraNeko。HAL に依存しない。PC でテストできる）。**ステップ8で作った**
        │   ├── maze_map.c/.h     壁の情報の保持・更新（16×16）、最後に壁を記録した 5 区画ぶんを戻す
        │   ├── search.c/.h       足立法・歩数マップ・未知壁探索で見に行く壁。全面探索と行き止まり潰し（KOGUMA から足した）
        │   ├── dijkstra.c/.h     ダイクストラ：スタートからゴールまでの速く走れる道
        │   └── path.c/.h         経路：既知区間（小回り。斜めにする形も KOGUMA から足した）、最短経路の大回り・斜めへの変換
        └── sys/                  全体の管理（ToraNeko）：scheduler、mode、ui、explore、shortest、log、output、failsafe、flash、goal、adjust など
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
| 0 | F446 用の CMake プロジェクト作成 | `CMakeLists.txt` の App/ の足し方、`App/app.c` | `.ioc` は KOGUMA.MkIIl.ioc から作る（[CUBEMX_SETUP.md](CUBEMX_SETUP.md)） | `KOGUMA.MkIIl.ioc` | **作者の作業待ち**（CubeMX で生成） |
| 1 | LED・シリアル出力（printf）・ブザー | `drv/led.c`, `drv/uart.c`, `drv/timebase.c` | LED 8 個＋壁の表示 4 個、USART2、ブザー（新） | `UI.c`, `syscalls.c` | |
| 2 | 制御周期の割り込み | `sys/scheduler.c` | TIM6、1kHz から | `PL_timer.c` | |
| 3 | エンコーダ | `drv/encoder.c` | SPI3、CS 2 本。磁石がモーターの軸ならギア比を掛ける。角度の歪みの補正（ToraNeko のモード6 No.6）を持ち込むか | `PL_encoder.c` | |
| 4 | IMU | `drv/imu.c` | CS 名 `ICM_NSS` → `IMU_CS`。軸の向きと符号を確かめる | `lsm6dsr.c` | |
| 5 | モーター・吸引ファン | `drv/motor.c`, `drv/fan.c` | TIM3 の 4ch（ドライバの型番で IN の使い方が変わる。回路図で確認）、TIM2 CH1。電圧の上限 | `motor.c`（出力の部分） | |
| 6 | 壁センサ | `drv/wall.c`, `ctrl/wall_ctrl.c`, `ctrl/wall_dist.c` | ADC1 の 5ch（電池＋4）、LED 2 組。しきい値と距離の表は KOGUMA.MkIIl の値（`WallDistance.c`）から | `PL_sensor.c`, `Wallsensor.c`, `WallDistance.c` | |
| 7 | 速度・角度の制御、探索の動き | `ctrl/odometry.c`, `ctrl/trajectory.c`, `ctrl/control.c`, `ctrl/run.c`, `ctrl/side_edge.c` | タイヤ径・トレッド・ギア比・区画 180mm。ゲインとスラロームの値は KOGUMA.MkIIl から（周期をそろえて） | `motor.c`, `Move.c`, `SpeedPlan.c`, `PillarEdge.c` | |
| 8 | 迷路探索と最短経路 | `maze/*`, `test/` | `MAZE_SIZE` 16。テストの迷路を 16×16 に（KOGUMA.MkIIl の `mazes/` を使う） | `Maze.c` | **完了**（2026-11-08、PC テスト済み）。下の「ステップ8でやったこと」 |
| 9 | 探索走行・モード・ログ・フェイルセーフ・保存 | `sys/*`, `drv/flash_memory.c`, `drv/switch.c`, `tools/viewer.html` | モードの割り当てを決める（MODES.md）。KOGUMA にしかない機能（全面探索、感度合わせなど）をどうするか（[PORTING.md](PORTING.md)） | `main.c`, `UI.c`, `LOG.c`, `Failsafe.c` | |
| 10 | 最短走行（大回り・斜め）とターンの値 | `sys/shortest.c`, `sys/turn_test.c`, `config/turn_params.h`, `test/test_turn.c` | ターンの値は KOGUMA.MkIIl の 2000・2400 の値から。180mm の区画で `test_turn --calc` をやり直す | `Move.c`, `ShortParam.c` | |

### ステップ8でやったこと（2026-11-08）

- ToraNeko.Mk3 の `App/maze/`（`ecd8e10`）を持ってきて、`MAZE_SIZE` を 16 にした。迷路の処理は大きさを定数で持っているので、ほかはそのまま動いた
- 走る時間の見積もり（ダイクストラのコスト）に使う値は、KOGUMA の値にした：区画 180mm、スタートから境目まで 114mm、ターンは KOGUMA の 2000 の値（`config/run_params.h`・`config/turn_params.h`。迷路が使う値だけ。ターンの値は仮で、ステップ10で計算し直す）
- 作者の決定（2026-11-08）「今の探索モードは全部残す」に合わせて、KOGUMA にあって ToraNeko にない迷路の機能を足した（[PORTING.md](PORTING.md) の B）
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
| `LOG.c`、`DistHist.c`、`SensorNoise.c` | `sys/log.c`、`sys/output.c`（`DistHist`・`SensorNoise` を持ち込むかは [PORTING.md](PORTING.md)） |
| `Failsafe.c` | `sys/failsafe.c`、`ctrl/control.c`（異常の見つけ方） |
| `ShortParam.c` | `config/turn_params.h`、`sys/shortest.c` |
| `Define.h` | `config/robot_params.h`、`config/ctrl_gains.h`、`config/run_params.h` |
