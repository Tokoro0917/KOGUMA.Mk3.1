# KOGUMA.Mk3.1 — Claude 向けの作業ルール

マイクロマウス（クラシックサイズ）KOGUMA.Mk3 のファームウェアを、ToraNeko.Mk3 を手本に書き直すリポジトリ。概要は [README.md](README.md)、ファームウェアの決定事項・ファイル構成・実装ステップは [software/main-controller/README.md](software/main-controller/README.md) を参照。

## 参照元（毎日見る）

| リポジトリ | ブランチ | このリポジトリでの使い方 |
|---|---|---|
| [Tokoro0917/KOGUMA.MkIIl](https://github.com/Tokoro0917/KOGUMA.MkIIl) | `master` | 書き直す元のコード。機体の値（センサのしきい値、ターンの値、ゲイン）と、機能の中身はここから持ってくる |
| [Tokoro0917/ToraNeko.Mk3](https://github.com/Tokoro0917/ToraNeko.Mk3) | `main` | 書き直しの手本。ファイルの分け方・書き方・作り（`App/` の層）はここに合わせる |

- **1 日の始めに必ず**、2 つのリポジトリの、[docs/reference_sync.md](docs/reference_sync.md) に書いた「最後に見たコミット」からの変更を読む。KOGUMA.Mk3.1 に関係する変更は [TODO.md](TODO.md) の「参照元の変更から」に足し、作者に提案する（何を、なぜ、どの順で）。見たコミットと記録は `docs/reference_sync.md` に書き、同じコミットで push する
- 参照元のリポジトリは**読むだけ**。このセッションから書き換えたり push したりしない
- `software/reference/` のコード（KOGUMA.MkIIl の写し）は参照用。編集しない。KOGUMA.MkIIl の最新は、リポジトリそのもの（`master`）を見る

## コードの書き方（最優先のルール）

コードを読むのは作者本人で、プログラミングは得意ではない。**作者が自分で読んで理解し、直せること**を何より優先する。短さ・賢さ・汎用性よりも、素直で読みやすい書き方を選ぶ。ToraNeko.Mk3 の `App/` の書き方が基準。

- **素直に書く**。同じ処理が数行並ぶくらいなら、ループや表にまとめるより並べて書いたほうがよい
- **避ける書き方**。使わずに書けるなら使わない。どうしても必要なら、何をしているかをコメントで説明する
  - ビット演算（`<<`、`>>`、`&`、`|` によるフラグ操作）
  - 三項演算子（`a ? b : c`）→ `if` で書く
  - `%` による巻き戻し（リングバッファの添字など）→ `if (i >= N) { i = 0; }` で書く
  - ポインタ演算、関数ポインタ、関数のようなマクロ、共用体
  - 1行に複数の処理を詰め込む書き方
  - `printf` のための型キャスト（`(unsigned long)x` など）。arm-none-eabi-gcc では `uint32_t` はそのまま `%lu` で出せる
- **名前で意味がわかるようにする**。物理量には単位を付ける（`speed_mm_s`、`wait_ms`、`angle_deg`）。1文字の変数は、ループの `i` と、区画の座標の `x`・`y` だけ
- **関数は短く、1つの仕事だけ**にする。関数の前に「何をする関数か」を1〜2行で書く
- **コメントは日本語**で、「何をしているか」と「なぜそうするか」を書く。ファイルの先頭には、そのファイルの役割を書く
- **難しい仕組み**（割り込み、DMA、`volatile`、割り込み禁止など）を使う箇所は、処理の流れをコメントで順に説明する
- 新しい書き方や HAL の機能を初めて使ったときは、返答の中でもその説明を添える
- ToraNeko.Mk3 のファイルを持ってきたときは、ファイルの先頭に「ToraNeko.Mk3 の `App/○○.c`（コミット）をもとにした」と、KOGUMA に合わせて変えたところを書く

## 作業の進め方

- 返答・ドキュメント・コメントは日本語
- 作者に作業をお願いするときは、上から順にやればよい手順（どの画面で何を押すか、何が表示されれば成功か）を示す
- 回路図・CAD データは、要るときに作者に頼む（作者が用意してくれる）。推測で決めたピンや値には「回路図で確認」と書いておく
- ファームウェアのビルドは `software/main-controller/KOGUMA.Mk3.1/` で `cmake --preset Debug && cmake --build --preset Debug`。push 前に必ずビルドを通す（`App/` は `-Wall -Wextra` で警告なし）
- PC のテストは `software/main-controller/test/` で `make`（`App/maze/` などを変えたら、push 前に必ず通す。CI でも実行する）
- 実機の出力は `software/main-controller/logs/` に残す。値を決めるときは、まずここと KOGUMA.MkIIl の `logs/` のデータを見る
- モードを足したり、中身・番号を変えたりしたら、`software/main-controller/README.md` の「モード一覧」と `MODES.md` を同じコミットで直す
- CubeMX の生成コード（`Core/`、`Drivers/`、`cmake/stm32cubemx/`）は `USER CODE` 欄以外を編集しない
