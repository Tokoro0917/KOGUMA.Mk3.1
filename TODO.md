# やることリスト

2026-11-08 時点。上から順に進める。終わったことと決めたことは、いちばん下の「記録」。

KOGUMA.MkIIl の学生大会（2026-11-21〜22）までは、作者の実機の時間は KOGUMA.MkIIl が優先。KOGUMA.Mk3.1 は、実機が要らない作業（コードの移植、PC テスト）を先に進め、実機での確認は大会のあとにまとめてやってもよい。

## 作者にお願いすること

- [x] ~~CubeMX でプロジェクトを作る~~ → 2026-11-08 に Claude が KOGUMA.MkIIl と ToraNeko.Mk3 のプロジェクトを写して作った（作者の提案）
- [ ] （急がない）CubeMX で `KOGUMA.Mk3.1.ioc` を開いて、エラーなく開けるか・生成したコードが変わらないかを確かめる（手順は [software/main-controller/CUBEMX_SETUP.md](software/main-controller/CUBEMX_SETUP.md) の 1-2）
- [ ] **ステップ1〜7 を実機で確かめる**（書き込むと KOGUMA.MkIIl の大会用のファームウェアは消えるので、確かめたら書き戻す。手順は [software/main-controller/README.md](software/main-controller/README.md) の「ステップ1〜7の確かめ方」）
- [ ] **メイン基板の回路図**（PDF でよい）を `hardware/` に置く。とくに知りたいこと：PA10〜PA12 の用途、モータードライバの型番と TIM3 CH1〜4 のつながり、壁センサの受光と ADC チャンネル・発光の組の対応、電池の分圧の抵抗（[hardware/README.md](hardware/README.md)）
- [x] PC とのシリアルに何を使っているか → STLINK-V3 の仮想 COM ポート。921600bps にする（2026-11-08）
- [x] 決定事項の案と KOGUMA_PORTING.md の B の案を決める → 2026-11-08 のアンケートで決まった（下の「記録」）

## 移植（software/main-controller/README.md の「実装の進め方」）

- [x] ステップ0：CMake のプロジェクト（2026-11-08。KOGUMA.MkIIl と ToraNeko.Mk3 から作った。ビルドは警告なし。CI でビルドする）
- [ ] ステップ1：LED・シリアル出力（921600bps）・ブザー（2026-11-08 に作った。**作者に実機で確かめてもらう**：[software/main-controller/README.md](software/main-controller/README.md) の「ステップ1の確かめ方」）
- [ ] ステップ2：制御周期の割り込み（TIM11、2kHz。ToraNeko と同じ）と処理時間の計測（TIM5）（2026-11-08 に作った。実機はまだ）
- [ ] ステップ3：エンコーダ（SPI3）（2026-11-08 に作った。実機はまだ。磁石がどの軸に付いているか・ギア比は、機構で確かめる）
- [ ] ステップ4：IMU（SPI1）（2026-11-08 に作った。実機はまだ）
- [ ] ステップ5：モーター・吸引ファン（2026-11-08 に作った。実機はまだ。ファンの型番・定格を確かめる）
- [ ] ステップ6：壁センサ（2026-11-08 に作った。AD 値の読み取り、壁のあり・なし、壁までの距離。値は KOGUMA.MkIIl から。実機はまだ。ならさない値でしきい値を決め直す）
- [ ] ステップ7：速度・角度の制御と探索の動き（ctrl/run.c）を 2026-11-08 に作った（ToraNeko と同じ。係数は KOGUMA の値を換算、探索の値は KOGUMA の Move.c から。実機はまだ。確かめは software/main-controller/README.md の 10（直進）・11（スラローム））
- [x] ステップ8：迷路（16×16）と PC テスト（2026-11-08。全面探索・行き止まり潰し、既知区間の斜めも足した。探索モードは全部残すため）
- [ ] ステップ9：モードの土台（操作と合図・モード選択・ゴール・Flash の保存・ログ出力・調整の動き・センサの表示）と探索（モード0 No.1〜7）を 2026-11-08 に作った（MODES.md）。壁切れのテスト（モード5 No.4〜6）、エンコーダの歪み（モード6 No.6）、吸引のときのセンサのノイズ（モード5 No.8。B7）、ビューアとログの残し方も作った。ステップ9 で残っているのは実機の確かめだけ。モード0 No.8〜12 と B15（未知壁探索を最短走行の値の組ごとに）はステップ10
- [ ] ステップ10：最短走行（モード1 No.1〜9）・ターンの調整（モード3）・B15 を 2026-11-08 に作った（組 2000・組 2400 のターンは `test_turn --calc` で計算）。**このあと**：モード0 No.8〜12（探索のあとに続けて最短走行）。実機で足す前距離・後距離は、モード3 で合わせる

## 参照元の変更から

毎朝、KOGUMA.MkIIl と ToraNeko.Mk3 の変更を見て、ここに足す（手順は [docs/reference_sync.md](docs/reference_sync.md)）。
書き方：`- [ ] 中身（どちらのリポジトリ・コミット）→ 効くところ（KOGUMA.Mk3.1 のファイル・ステップ）`。「取り込む」「様子を見る」を分けて書く。

### 取り込む

- [ ] 2026-11-08 の基準より前の変更は、移植のときに最新のコードを持ってくるので、ここには書かない（[docs/reference_sync.md](docs/reference_sync.md) の 2026-11-08 の記録に、最初から入れるものを並べた）

### 様子を見る

- [ ] KOGUMA.MkIIl の学生大会に向けた変更（電池を替えたあとのターンの合わせ直し、直線の加速の限界の測定、2400 を使うか）。結果が出たら、ステップ7・10 の値に使う（KOGUMA.MkIIl `TODO.md`）

## 記録

### 2026-11-08（ステップ10 の最短走行）

- config/turn_params.h に組 2400（KOGUMA.MkIIl の G_Turn2400）を足し、test_turn --calc で前距離・後距離を計算した。斜め入り 135° は KOGUMA の角加速度では後距離が − になるので、斜め出 135° と同じ角加速度にした
- ctrl/run.c でターンの組を選べるようにした（run_fast_set_turn_set()）
- sys/shortest.c（モード1 No.1〜9）：No.4〜9 は KOGUMA.MkIIl のモード1 No.4〜9 と同じ値、No.1 は小回りだけ、No.2・3 は直進の最高速 ＝ ターンの速さ（KOGUMA のモード3 No.1・2）。KOGUMA の直進は減速が加速の 2 倍だったが、ToraNeko の台形は同じなので、実機で加速度を見直す
- sys/turn_test.c（モード3 No.1〜7 組 2000、No.9〜15 組 2400）。斜めの直進のテストはモード4 No.2・3 に移した
- 未知壁探索で、最短走行の組 1（No.5）・組 2（No.8）ごとの経路の壁を全部見るようにした（B15）
- PC テスト：test_maze に [10]（仮の機体で No.1〜9 を走らせる）を足し、[9] の未知壁探索の確かめを組ごとにした。test_turn に組 2400、test_mode に最短走行・ターンの調整のテストを足した

### 2026-11-08（ステップ9 のモードの土台）

- ToraNeko の sys/ui.c・mode.c・goal.c・flash.c（drv/flash_memory.c）・output.c・adjust.c・sensor_view.c・sensor_record.c・suction.c を持ってきた。LED は KOGUMA.MkIIl と同じ並び（モード LED1〜4、No. LED5〜8）、合図に KOGUMA.MkIIl と同じ高さの音を足した（決定・スタート・できなかった）。タイヤ 1 段は KOGUMA と同じ 300 カウント
- app.c の確かめの動きをモードに移した：部品の確かめ（モード5 No.7）、モーターの向き（モード6 No.2）、直進（モード6 No.1）、スラローム（モード2 No.1）。確かめ方を software/main-controller/README.md に書き直した
- KOGUMA にあって ToraNeko にないものを足した：吸引探索のスラローム（モード2 No.3）、直線の加速の限界テスト（モード4 No.12。B16）
- MODES.md を作り、README.md に「モード一覧」を足した
- 探索（sys/explore.c）を ToraNeko から持ってきて、吸引探索とエセ全面探索を足した。モード0 No.1〜7（KOGUMA.MkIIl と同じ番号）。PC テスト test_maze の [9]（仮の機体で、5 つの走り方・吸引・横壁合わせ・フェイルセーフ）を持ってきた
- PC テストを 3 つ足した（test_mode・test_flash・test_output。ToraNeko から）
- 壁切れ・柱の見え方テスト（sys/edge_test.c。速さは KOGUMA.MkIIl のモード3 No.6〜8 の 500・1000・2000mm/s）、エンコーダの歪み（sys/encoder_cal.c。回す向きを KOGUMA に合わせた）、吸引のときのセンサのノイズ（sys/sensor_noise.c。KOGUMA.MkIIl の SensorNoise.c を、今の壁センサの読み方で作り直した。B7）を足した
- ビューア（tools/viewer.html）、logs/README.md、tools/push_logs.sh、tools/wall_table.py を ToraNeko から持ってきた

### 2026-11-08（ステップ7 の探索の動き）

- ToraNeko の ctrl/run.c を持ってきた。探索を 2 とおり（吸引なし 500mm/s・吸引あり 1000mm/s）にして、`run_search_set_suction()` で選ぶ。値は KOGUMA.MkIIl の Move.c（スラロームの最高角速度 700・1640deg/s、その場旋回 600deg/s・20000deg/s²、既知区間加速 4000mm/s など）
- KOGUMA の角速度は cos の形、ToraNeko は台形なので、台形の角加速度を KOGUMA の w_ac × 2/π にした（上げきる時間が同じ）
- test/test_turn.c を持ってきて、180mm の区画で前距離・後距離を計算した。探索のスラロームは前 19.3・後 19.5mm（KOGUMA の実機の値 20・25mm に近い）。吸引探索は 32.3・32.5mm（KOGUMA は 15・50mm で違う。滑りのせいかもしれないので、実機で見る）。最短走行の 7 つのターンも計算し直した
- 前壁補正・壁切れは測るだけにした（KOGUMA の前センサは区画の入口では値が小さいのと、壁切れの値が仮のため）
- app.c に、スラロームのテストを足した（前センサに 2 秒以上手をかざして離す）

### 2026-11-08（ステップ6・7）

- ステップ6：壁のあり・なし・壁までの距離（KOGUMA.MkIIl の Wallsensor.c・WallDistance.c の値）。前のコミットで左右の受光を逆にしていたのを直した（KOGUMA の並びは ToraNeko と同じだった）
- ステップ7：速さと距離・台形の目標・速度と角度の制御・フェイルセーフの見つけ方・走行ログを ToraNeko から持ってきた。係数は KOGUMA.MkIIl の値を換算した（`config/ctrl_gains.h` の先頭の表）。PC テストを 3 つ足した（test_wall_dist・test_trajectory・test_control）

### 2026-11-08（ステップ1〜6 のドライバ）

- 作者の指示で、実機で確かめる前に続きを進めた。ステップ1（LED・シリアル・ブザー）、2（制御周期）、3（エンコーダ）、4（IMU）、5（モーター・ファン）、6 の AD 値の読み取り（壁センサ・電池）を作った。どれも ToraNeko.Mk3 のファイルを持ってきて、KOGUMA のピンと KOGUMA.MkIIl の値（向き、ジャイロの倍率 1.01、電池の分圧、ファンの上限）に合わせた。確かめの動きは app.c（1 秒ごとにシリアルに出す、S2 を押すとモーターが回る）

### 2026-11-08（ステップ0）

- CubeMX を使わずに、CMake のプロジェクトを作った（作者の提案）。`.ioc`・`Drivers/`・起動ファイルは KOGUMA.MkIIl、CMake の一式と `Core/` のひな形は ToraNeko.Mk3 から。ピンと周辺機能は、決めた設定（制御周期 2kHz・TIM11、µs の TIM5、壁センサの TIM8、ADC をタイマーで起動、SPI を 10MHz 以下に、USART2 を 921600bps・DMA 送信）にした。ビルドは警告なし（Flash 約 40KB、RAM 約 3KB）

### 2026-11-08（アンケートで決めたこと）

| 聞いたこと | 決まったこと |
|---|---|
| 制御の係数・フェイルセーフのしきい値の初めの値 | **KOGUMA の値を ToraNeko の式に換算**して 1 組にする（KOGUMA_PORTING.md の E2） |
| フェイルセーフから戻る先 | **同じモード・No.**（ToraNeko と同じ） |
| モードの選び方・スタートのしかた | **ToraNeko の形**（タイヤで選ぶ → S2 で決定 → 前センサに手をかざして離す） |
| 合図 | **LED とブザーの両方** |
| 会場ごとの感度合わせ | **Flash に保存**（ビルドし直さない） |
| ゴールの位置 | **機体で入れて Flash に保存**（大きさは 2×2） |
| モードの番号 | **ToraNeko の並び**（探索の中の No. は KOGUMA の No.1〜12） |
| PC とのシリアル | STLINK-V3 の仮想 COM ポート → **921600bps** |
| 開発環境 | **CubeMX ＋ VS Code**（ToraNeko と同じ） |
| 電池が少ないとき | **外側の 4 個（LED1・2・7・8）を点滅させ、ブザーも鳴らす** |
| 最初から入れる KOGUMA の調整機能 | **吸引のときのセンサのノイズ**（B7）、**直線の加速の限界テスト**（B16）。limits.py 向けのログ（B13）と、速さで加速を変える（B9）は、要るときに足す |

### 2026-11-08（ToraNeko に合わせるもの）

- 作者の決定：**ディレクトリの構成、ファイル名、制御方法、制御周期、フェイルセーフは ToraNeko.Mk3 に合わせる**（[software/main-controller/KOGUMA_PORTING.md](software/main-controller/KOGUMA_PORTING.md) の E）
- 制御周期を 1kHz（TIM6）の案から 2kHz（TIM11）に変え、CubeMX の手順を直した。`PORTING.md` を ToraNeko と同じ名前の `KOGUMA_PORTING.md` にした

### 2026-11-08（ステップ8）

- 作者の決定：**KOGUMA.MkIIl の今の探索モード（モード0 No.1〜12）は全部残す**
- ステップ8（迷路）を作った。ToraNeko.Mk3 の `App/maze/` を 16×16 にし、KOGUMA にしかない全面探索・行き止まり潰しと、既知区間の斜めの経路を足した。PC テスト（`test/test_maze.c`）は全部通る。CI で実行する

### 2026-11-08

- リポジトリの土台を作った：ToraNeko.Mk3 と同じディレクトリ構成（`software/`・`hardware/`・`mechanical/`・`docs/`）、作業ルール（`CLAUDE.md`）、ファームウェアの決定事項の案・ファイル構成・実装ステップ（`software/main-controller/README.md`）、ピン表（`PIN_REQUIREMENTS.md`）、CubeMX の手順（`CUBEMX_SETUP.md`）、持ってくるものの判断（`KOGUMA_PORTING.md`）
- KOGUMA.MkIIl `fa64b58` を `software/reference/koguma-mkiil/` に写した
- 参照元の監視を始めた（`docs/reference_sync.md`）。毎朝の確認は、このセッションの予定のタスクで行う
