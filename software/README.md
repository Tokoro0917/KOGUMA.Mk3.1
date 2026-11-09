# software/

KOGUMA.Mk3.1 のソフトウェア一式を置きます。

```
software/
├── main-controller/   # メイン基板（STM32F446RE）のファームウェア（書き直したもの）
└── reference/         # 参照用に写した、KOGUMA.MkIIl のファームウェア（編集しない）
```

## 参照元について

- 書き直す元の KOGUMA.MkIIl のファームウェアを `reference/koguma-mkiil/` に写しました（ToraNeko.Mk3 と同じ置き方）。最新は KOGUMA.MkIIl のリポジトリそのものを見ます
- 手本の ToraNeko.Mk3 のファームウェアは写していません。ToraNeko.Mk3 のリポジトリの `software/main-controller/` を見ます
- どちらも毎朝変更を見ます（[../docs/reference_sync.md](../docs/reference_sync.md)）
