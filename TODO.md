# やることリスト

2026-11-08 時点。上から順に進める。終わったことと決めたことは、いちばん下の「記録」。

KOGUMA.MkIIl の学生大会（2026-11-21〜22）までは、作者の実機の時間は KOGUMA.MkIIl が優先。KOGUMA.Mk3.1 は、実機が要らない作業（コードの移植、PC テスト）を先に進め、実機での確認は大会のあとにまとめてやってもよい。

## 作者にお願いすること

- [ ] **CubeMX で `KOGUMA.Mk3.1` のプロジェクトを作って push する**（ステップ0。手順は [software/main-controller/CUBEMX_SETUP.md](software/main-controller/CUBEMX_SETUP.md)。フォルダ `software/main-controller/KOGUMA.Mk3.1/` はもうあり、中に `App/` が入っている）
- [ ] **メイン基板の回路図**（PDF でよい）を `hardware/` に置く。とくに知りたいこと：PA10〜PA12 の用途、モータードライバの型番と TIM3 CH1〜4 のつながり、壁センサの受光と ADC チャンネル・発光の組の対応、電池の分圧の抵抗（[hardware/README.md](hardware/README.md)）
- [ ] PC とのシリアルに何を使っているか（STLINK-V3 の仮想 COM ポートか、USB シリアル変換か）。速さを 921600bps に上げられるか決めるため
- [ ] [software/main-controller/README.md](software/main-controller/README.md) の「決定事項（案）」と、[software/main-controller/PORTING.md](software/main-controller/PORTING.md) の B の「案」を見て、決める（探索モードは 2026-11-08 に「全部残す」と決まった。残りはとくに B5 感度合わせを Flash に保存、B11 モードの選び方）

## 移植（software/main-controller/README.md の「実装の進め方」）

- [ ] ステップ0：CubeMX のプロジェクト（作者）→ `App/app.c`、`CMakeLists.txt`、CI（Claude）
- [ ] ステップ1：LED・シリアル出力・ブザー
- [ ] ステップ2：制御周期の割り込み（TIM6、1kHz）と処理時間の計測（TIM5）
- [ ] ステップ3：エンコーダ（SPI3）
- [ ] ステップ4：IMU（SPI1）
- [ ] ステップ5：モーター・吸引ファン
- [ ] ステップ6：壁センサ（TIM8 → ADC → DMA の連鎖）
- [ ] ステップ7：速度・角度の制御、探索の動き
- [x] ステップ8：迷路（16×16）と PC テスト（2026-11-08。全面探索・行き止まり潰し、既知区間の斜めも足した。探索モードは全部残すため）
- [ ] ステップ9：探索走行・モード（MODES.md を作る）・ログ・フェイルセーフ・保存・ビューア。探索は KOGUMA のモード0 No.1〜12 を全部作る（[software/main-controller/PORTING.md](software/main-controller/PORTING.md) の「モード0（探索）の 12 種類」）。未知壁探索は最短走行の値の組ごとに回す（B15）
- [ ] ステップ10：最短走行（大回り・斜め）とターンの値。`config/turn_params.h` の仮の値（KOGUMA の 2000 の値）を `test_turn --calc` で計算し直し、2400 の値も入れる

## 参照元の変更から

毎朝、KOGUMA.MkIIl と ToraNeko.Mk3 の変更を見て、ここに足す（手順は [docs/reference_sync.md](docs/reference_sync.md)）。
書き方：`- [ ] 中身（どちらのリポジトリ・コミット）→ 効くところ（KOGUMA.Mk3.1 のファイル・ステップ）`。「取り込む」「様子を見る」を分けて書く。

### 取り込む

- [ ] 2026-11-08 の基準より前の変更は、移植のときに最新のコードを持ってくるので、ここには書かない（[docs/reference_sync.md](docs/reference_sync.md) の 2026-11-08 の記録に、最初から入れるものを並べた）

### 様子を見る

- [ ] KOGUMA.MkIIl の学生大会に向けた変更（電池を替えたあとのターンの合わせ直し、直線の加速の限界の測定、2400 を使うか）。結果が出たら、ステップ7・10 の値に使う（KOGUMA.MkIIl `TODO.md`）

## 記録

### 2026-11-08（ステップ8）

- 作者の決定：**KOGUMA.MkIIl の今の探索モード（モード0 No.1〜12）は全部残す**
- ステップ8（迷路）を作った。ToraNeko.Mk3 の `App/maze/` を 16×16 にし、KOGUMA にしかない全面探索・行き止まり潰しと、既知区間の斜めの経路を足した。PC テスト（`test/test_maze.c`）は全部通る。CI で実行する

### 2026-11-08

- リポジトリの土台を作った：ToraNeko.Mk3 と同じディレクトリ構成（`software/`・`hardware/`・`mechanical/`・`docs/`）、作業ルール（`CLAUDE.md`）、ファームウェアの決定事項の案・ファイル構成・実装ステップ（`software/main-controller/README.md`）、ピン表（`PIN_REQUIREMENTS.md`）、CubeMX の手順（`CUBEMX_SETUP.md`）、持ってくるものの判断（`PORTING.md`）
- KOGUMA.MkIIl `fa64b58` を `software/reference/koguma-mkiil/` に写した
- 参照元の監視を始めた（`docs/reference_sync.md`）。毎朝の確認は、このセッションの予定のタスクで行う
