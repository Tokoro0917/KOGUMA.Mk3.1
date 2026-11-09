# reference/

参照用に取り込んだ、今までの KOGUMA.Mk3 のファームウェアです。**ここのコードは編集しません。**
KOGUMA.Mk3.1 の実装は `../main-controller/` で行います。

| ディレクトリ | 元リポジトリ | コミット | 備考 |
|---|---|---|---|
| `koguma-mkiil/` | [Tokoro0917/KOGUMA.MkIIl](https://github.com/Tokoro0917/KOGUMA.MkIIl) | `fa64b58`（2026-10-27） | STM32F446RE 向け（STM32CubeIDE）。旧版 `KOGUMA.MkII/`、`.github/`、CubeIDE のプロジェクトファイル（`.cproject` など）、`logs/` の実機ログ（`logs/README.md` 以外）は除外（ToraNeko.Mk3 の写しと同じ） |

KOGUMA.MkIIl が変わっても、この写しはすぐには更新しません。毎朝の確認（[../../docs/reference_sync.md](../../docs/reference_sync.md)）で変更を見て、移植する機能を作るときに、元のリポジトリの最新を読みます。写しを更新したら、上の表のコミットを書き換えます。
