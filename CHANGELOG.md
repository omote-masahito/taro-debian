# 変更履歴

## 0.1.0 (2026-11-13)

最初の公開版。

- A: Debian 13 の Wine 10 で動かす `install-wine.sh` と `wine-jlfonts.sh`
  (Windows XP として報告、日本語ロケールの強制、JL フォントの導入と ＭＳ 系フォントからの振り向け)
- B: 製品同梱のランタイム jwd を現行のカーネル / glibc で動かす `install.sh`、`jwdrun`、`jwdshim.so`
- A と B をまとめて入れる `install-both.sh`。導入済みの環境で再実行するとフォントとランチャだけ作り直す
- `install.sh` が取得する Debian パッケージ 2 つを HTTPS で取得し、SHA256 を照合する
- 製品なしで `jwdrun` / `jwdshim.so` を確かめる `make check`
- jwd の変更が現行 Wine にどこまで入っているかの調査結果 (`docs/jwd-vs-current-wine.md`) と
  検査プログラム (`tools/jwd-probe.c`)
