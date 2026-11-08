# hardware/

KOGUMA.Mk3 の回路・基板の設計データを置きます。**中身は作者に用意してもらいます**（ファームウェアで要るときに、何が要るかを頼みます）。

## 置き方（ToraNeko.Mk3 と同じ）

```
hardware/
├── kicad/
│   └── KOGUMA.Mk3/      # 回路図（.kicad_sch と PDF）・基板（.kicad_pcb）・BOM.csv・README.md
└── datasheets/          # 使っている部品のデータシート PDF
```

KiCad 以外のツールで描いた回路図なら、PDF だけでもかまいません（`kicad/` のかわりに `schematic/` などに置く）。

## ファームウェアで要るもの

| 要るもの | 何に使うか | いつ要るか |
|---|---|---|
| メイン基板の回路図（PDF でよい） | ピン割り当ての確定（[../software/main-controller/PIN_REQUIREMENTS.md](../software/main-controller/PIN_REQUIREMENTS.md)）。とくに PA10〜PA12 の用途、モータードライバの型番と IN の並び、壁センサの受光と ADC チャンネルの対応、スイッチ S2 のプルアップ／プルダウン | ステップ0（CubeMX のプロジェクトを作る前） |
| BOM（部品表） | モータードライバ・モーター・ファン・電池の型番。データシートの値で制御の係数を決める | ステップ5・7 |
| データシート（モータードライバ、モーター、ファン） | 電圧の上限、PWM の周波数の範囲 | ステップ5 |

## 運用メモ

- 回路を変えたら、それに対応するソフトウェアの変更と、なるべく同じコミット・PR にする
