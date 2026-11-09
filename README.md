# KOGUMA.Mk3.1

マイクロマウス（クラシックサイズ）KOGUMA の 3 代目機体（KOGUMA.Mk3）の、**ソフトウェアを書き直した版**です。
機体（回路・機構）は KOGUMA.Mk3 と同じで、ファームウェアだけを作り直します。

- 今までの KOGUMA.Mk3 のファームウェア（人の手で書いてきたもの）は [Tokoro0917/KOGUMA.MkIIl](https://github.com/Tokoro0917/KOGUMA.MkIIl)。大会ではこちらを使い続ける
- 書き直しの手本は、ハーフサイズ機 [Tokoro0917/ToraNeko.Mk3](https://github.com/Tokoro0917/ToraNeko.Mk3) のファームウェア。KOGUMA のコードを、層ごとに分けて 1 つずつ確かめながら作り直したもの
- ディレクトリの構成・書き方・進め方も ToraNeko.Mk3 に合わせる

機体の概要は [docs/overview.md](./docs/overview.md)、ファームウェアの決定事項・ファイル構成・進め方は [software/main-controller/README.md](software/main-controller/README.md)、やることは [TODO.md](TODO.md)。

## ディレクトリ構成

```
.
├── TODO.md      # やることリスト（参照元の 2 つのリポジトリの変更から足したものも、ここに書く）
├── software/    # マイコン・PC 側のソフトウェア一式
├── hardware/    # 回路図・基板データ・BOM（作者に用意してもらう）
├── mechanical/  # 機構部品・3D モデル・図面（作者に用意してもらう）
└── docs/        # 機体の概要、参照元の監視の記録など、全体にかかわる資料
```

各ディレクトリの詳細は、それぞれの README.md を参照してください。

## 参照元の監視

KOGUMA.MkIIl と ToraNeko.Mk3 は、このリポジトリを作っているあいだも変わり続けます。
**1 日の始めに、2 つのリポジトリの変更を見て、KOGUMA.Mk3.1 に関係するものを [TODO.md](TODO.md) に足し、作者に提案します。**
どこまで見たか（コミット）と、その日の記録は [docs/reference_sync.md](docs/reference_sync.md)。
