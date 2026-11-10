# 参照元の監視

KOGUMA.Mk3.1 は、2 つのリポジトリを見ながら作ります。どちらも毎日のように変わるので、**1 日の始めに変更を見て、関係するものを [../TODO.md](../TODO.md) の「参照元の変更から」に足し、作者に提案します。**

## 最後に見たコミット

毎朝の確認が終わったら、ここを書き換える（次の日は、ここからの変更を見る）。

| リポジトリ | ブランチ | 最後に見たコミット | その日付 |
|---|---|---|---|
| [Tokoro0917/KOGUMA.MkIIl](https://github.com/Tokoro0917/KOGUMA.MkIIl) | `master` | `58a6329` | 2026-11-11 |
| [Tokoro0917/ToraNeko.Mk3](https://github.com/Tokoro0917/ToraNeko.Mk3) | `main` | `4981fb9` | 2026-11-11 |

## 確かめ方（毎朝の手順）

1. 2 つのリポジトリの最新を取ってくる（`git fetch origin master` / `git fetch origin main`）
2. 上の表のコミットからの変更を見る：`git log --oneline <最後に見たコミット>..origin/<ブランチ>` と `git diff --stat`。中身は、コミットごとに `git show` で読む
3. 変更を次の 3 つに分ける
   - **取り込む**：KOGUMA.Mk3.1 にも入れたほうがよいもの（不具合の修正、両方の機体に効く仕組み、KOGUMA の機体の値の変更）
   - **様子を見る**：まだ実機で確かめていないもの、ハーフサイズだけの話、大会の前の一時的な変更
   - **関係ない**：ログの追加、ハーフサイズの機体だけの値、回路・機構（ToraNeko の）
4. 「取り込む」「様子を見る」を [../TODO.md](../TODO.md) の「参照元の変更から」に足す。どちらのリポジトリのどのコミットかと、KOGUMA.Mk3.1 のどのファイルに効くかを書く
5. 下の「記録」にその日の行を足し、上の表のコミットを書き換えて、push する
6. 作者に提案する：何を取り込むとよいか、なぜか、どの順か

### どこを見るか

| リポジトリ | よく見るところ | 気にすること |
|---|---|---|
| KOGUMA.MkIIl | `Core/Src`、`Core/Inc`、`docs/changelog.md`、`TODO.md` | 機体の値（`Define.h`、センサのしきい値、ターンの値）。新しい機能。大会での結果 |
| ToraNeko.Mk3 | `software/main-controller/TORANEKO.Mk3/App/`、`software/main-controller/README.md`、`KOGUMA_PORTING.md`、`MODES.md`、`test/`、`tools/` | `App/` の作りの変更（KOGUMA.Mk3.1 でも同じ形にする）。不具合の修正。PC テスト・ビューアの変更 |

## 記録

新しい日を上に足す。

### 2026-11-11

- KOGUMA.MkIIl `fa64b58..58a6329`（3 コミットとマージ）：STM32CubeIDE をやめて、VS Code + CMake でビルドするようにした（ToraNeko.Mk3 と同じ形。`docs/vscode_setup.md`）。`main.c` の `Motor.h` → `motor.h`、CI に ARM のビルド。機体の値・機能の変更はない → **関係ない**（KOGUMA.Mk3.1 は最初から同じ形。新しい PC に入れた VS Code・arm-none-eabi-gcc〈11 以降〉・CMake・Ninja で、KOGUMA.Mk3.1 もそのままビルドできる）
- ToraNeko.Mk3 `ecd8e10..4981fb9`（21 コミットとマージ）
  - **取り込む**（TODO.md の「参照元の変更から」）
    - `16e912f`・`66381ae`：探索でスタートに戻ったら、前壁合わせをしつつ北を向き、お尻を後ろの壁に当ててから迷路を保存する
    - `47d1187`：最短走行は、ゴールに入ってからもう 1 区画進んで、ゴールの 2 つ目の区画のまん中で止まる
    - `ec4055e`・`2ce9248`・`93921c9`・`b794c55`：会場ごとの感度合わせ（倍率を全部のセンサの値にかける、区画の境目で測る、壁制御の目標を区画の中心で別に測る、Flash に保存、モード7 No.3〜5）。KOGUMA.Mk3.1 の B5 にそのまま使う
  - **様子を見る**
    - `0b6e308`・`b58673b`・`94096da`：吸引の力のテスト（モード4 No.3）と、ファンの電圧の上限 5.0 → 7.0V（ToraNeko のファン）
    - `cb4d39e`・`c5c389b`：壁のしきい値の変更（ToraNeko の機体の値）。行き止まりで前の壁を見落とした話は、KOGUMA でも実機で見る
  - **関係ない**
    - `80855d0`：7 つとも 1000mm/s のターンの組（組 2）と最短 No.6。KOGUMA.Mk3.1 には、もう組 1000 と `run_fast_set_turn_set()` がある（KOGUMA.MkIIl の G_Turn1000）
    - `ecd1e4f`・`f0140c5`・`3810e27`・`079fc39`：スカート枠・センタリング板（ToraNeko の機構）
    - `bfe414e`・`f167e8e`・`78b0b73`：ログ。`08e09f6`：test_wall_ctrl の実行ファイルを外した（KOGUMA.Mk3.1 には test_wall_ctrl がまだない）
### 2026-11-10

- 変更なし（KOGUMA.MkIIl `master` は `fa64b58`、ToraNeko.Mk3 `main` は `ecd8e10` のまま）。TODO.md に足すものはない

### 2026-11-09

- 変更なし（KOGUMA.MkIIl `master` は `fa64b58`、ToraNeko.Mk3 `main` は `ecd8e10` のまま）。TODO.md に足すものはない

### 2026-11-08（はじめ）

- 監視を始めた。この日の最新を基準にした（KOGUMA.MkIIl `fa64b58`（2026-10-27）、ToraNeko.Mk3 `ecd8e10`（2026-11-04））
- KOGUMA.MkIIl は 2026-10-27 のフェイルセーフの戻り方の変更（ToraNeko.Mk3 と同じにした）が最後。`software/reference/koguma-mkiil/` に `fa64b58` を写した
- ToraNeko.Mk3 の 2026-11-01〜11-04 の主な変更（書き直しで最初から入れるもの）：
  - 壁切れを柱も使えるように作り直した（横のセンサの山の半分で端を見つける。`ctrl/side_edge.c`）。探索・最短走行の両方で位置を直す
  - 壁のない区間で向きを迷路の向きに戻す、両側の壁が続いたら基準の向きをリセットする
  - 斜めの直進の補正（前センサのしきい値と `DIAG_KP`）
  - 速さを 2ms 前のカウントとの差で求める（エンコーダのぶれでゆれないように。`ctrl/odometry.c`）
  - ダイクストラのコストを「直線 ＋ ターン 1 つ」の走る時間にした（KOGUMA の #100 と同じ）
  - フェイルセーフの異常の見つけ方（位置・角度のずれ、ぶつかった、壁を押している。10ms 続いたら止める）
