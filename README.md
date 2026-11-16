# taro-debian

一太郎 for Linux (ジャストシステム、2005 年) を Debian 13 (trixie) amd64 で動かすためのスクリプトと補助プログラム。

- **非公式のもの。** 株式会社ジャストシステムとは関係がなく、同社のサポート対象でもない。
- **製品は含まない。** 正規に入手した一太郎 for Linux の CD-ROM が別に必要。
- 主な目的は、この製品を題材にして現行の Wine の動作を検証すること。製品同梱のランタイムで動かす経路も
  比較用に用意している。

English summary: [README.en.md](README.en.md)

## 概要

製品は「Windows 版一太郎 (TARO14) のバイナリ + ジャストシステム改造版 Wine 20031212 (`jwd`)」という構成。
動かし方は 2 通りあり、どちらも Debian 13.7 のユーザーランドで確認した。

| | A. Debian 13 の Wine 10 | B. 同梱ランタイム jwd |
|---|---|---|
| スクリプト | `install-wine.sh` (一般ユーザー) | `install.sh` (root) |
| 追加で入れるもの | `wine` `wine32:i386` | i386 ライブラリ、旧 `libstdc++5` / `libgif4`、このリポジトリの `jwdrun` / `jwdshim.so` |
| 入る場所 | `~/.wine-taro` | `/opt/jwd` |
| 複合文書 (.jtd) の読み書き | Wine の ole32 | ジャストシステム独自実装の ole32 (C++) |
| 製品向けの調整 (縦書き、ヘルプ、印刷、メニュー周りなど) | なし (現行 Wine の実装) | あり (ChangeLog.jwd の内容) |
| 実行時の特殊条件 | なし | NX 無効相当、実行可能スタック |

まず A を試し、表示や印刷で製品本来の挙動が必要になったら B、という順が無難。

jwd の変更が現行 Wine にどこまで入っているかは [docs/jwd-vs-current-wine.md](docs/jwd-vs-current-wine.md) にまとめた。

## 必要なもの

- 一太郎 for Linux の CD-ROM の内容をコピーしたディレクトリ。`bin/jwd/setup` と
  `bin/TARO14/MASTER/SETUP.EXE` がある階層を指す。以下では `~/ICHITARO_LINUX` とする。
  検証はディスク上にコピーした状態で行った。マウントした CD-ROM を直接指定する使い方は未確認。
- Debian 13 (trixie) amd64。WSL2 + WSLg でもよい ([WSL で使う場合](#wsl-wslg-で使う場合))。
- X の画面 (`DISPLAY`)。セットアップは GUI で動く。
- `sudo` と、パッケージを取得できるネットワーク接続。
- `ja_JP.UTF-8` ロケール (`sudo dpkg-reconfigure locales` で生成)。

## 両方を一度に入れる

```sh
git clone https://github.com/omote-masahito/taro-debian.git taro-debian
cd taro-debian
./install-both.sh ~/ICHITARO_LINUX      # 一般ユーザーで実行。途中で sudo を使う
```

Wine のパッケージ導入、B (`install.sh`)、A (`install-wine.sh`) を順に実行する。使用許諾の確認は最初に
1 回だけ。既に入っている側は入れ直さないので、片方だけ入れた環境で実行してもよい。終わると `taro` (B) と
`~/.local/bin/taro-wine` (A) が使える。A の入れ先は下記の環境変数で変えられる。

A が導入済みのプレフィックスに対して `install-wine.sh` (または `install-both.sh`) をもう一度実行すると、
セットアップは飛ばして JL フォントの設定とランチャだけ作り直す。このリポジトリを更新したときはこれで反映する。

## A. Debian 13 の Wine で動かす

```sh
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install wine wine32:i386
./install-wine.sh ~/ICHITARO_LINUX
XMODIFIERS=@im=uim ~/.local/bin/taro-wine [file.jtd]
```

要点は 3 つ。

- **Windows のバージョンを XP (または 2000) にする。** 既定の Windows 10 ではセットアップが
  「このバージョンの Windows にセットアップすることはできません」で止まる。98 / Me にすると
  「空き容量チェック」でゼロ除算になる。後者は ChangeLog.jwd の 2004-02-04
  (`dlls/winedos/int21.c`) にある件で、Win9x 系ではセットアップがセクタ 0 を読んでクラスタサイズを
  求めようとし、Wine が未実装のまま正常復帰するため。jwd はここでキャリーを立てて回避している。
  NT 系として報告させればこの経路を通らない。
- **日本語ロケールで動かす。** Wine は Unix 側のロケールから ANSI コードページを決める。`C.UTF-8` などの
  ままだと CP932 にならず、セットアップが日本語名のファイルを作れずにエラーダイアログで止まる。
  `install-wine.sh` とランチャは、ロケールが `ja_JP.*` でなければ `LC_ALL=ja_JP.UTF-8` にして実行する。
  導入後でも、日本語以外のロケールで `wine` を直接起動すると CP1252 になり、タイトルが `??? - [??1]`、
  メニューが `ƒtƒ@ƒCƒ‹(F)` のように化ける。これはフォントでは直らない。ランチャ経由で起動すること。
- **JL フォントを入れ、ＭＳ 系フォントの要求を JL フォントに振り向ける。** JL明朝 / JLゴシック /
  JLRoman / JLSwiss は Windows 用セットアップではなく `bin/jwd/ENV/C/WINDOWS/Fonts` にある。無いと文書を
  開くたびにフォント置換の確認が出る。また、メニューやダイアログは MS UI Gothic で描かれるので、
  システムに日本語フォントが 1 つも無いとメニューが「□□□□(F)」になる。jwd の `[substitute.fonts]` と
  同じく、MS UI Gothic / ＭＳ ゴシック / ＭＳ 明朝 などを JL フォントに置き換える設定を入れる
  (`HKCU\Software\Wine\Fonts\Replacements`)。`install-wine.sh` が `wine-jlfonts.sh` を呼んで行う。
  既存のプレフィックスには `./wine-jlfonts.sh ~/ICHITARO_LINUX` で後から適用できる。

確認済み: セットアップ完走 (JSINS.LOG に NG なし)、起動、uim-xim + anthy での日本語入力、`.jtd` 保存、
B で保存した `.jtd` の読み込み。未確認: 印刷、ヘルプ、XWayland 上での表示、長時間の使用。

### 別の Wine / 別のプレフィックスで試す

`install-wine.sh` は環境変数で入れ先を変えられるので、Wine のバージョンやプレフィックスの種類ごとに
並べて入れられる。

```sh
# 64bit プレフィックス (Debian の Wine 10 では wine64 パッケージも必要)
WINEARCH=win64 WINEPREFIX=$HOME/.wine-taro64 TARO_LAUNCHER=taro-wine64 \
    ./install-wine.sh ~/ICHITARO_LINUX

# 別の場所に入れた Wine (例: WineHQ の wine-devel)
WINE=/opt/wine-devel/bin/wine WINEPREFIX=$HOME/.wine-taro-devel TARO_LAUNCHER=taro-wine-devel \
    ./install-wine.sh ~/ICHITARO_LINUX
```

| 環境変数 | 既定 | 意味 |
|---|---|---|
| `WINE` | `wine` | 使う Wine のコマンド |
| `WINEPREFIX` | `~/.wine-taro` | 入れ先のプレフィックス |
| `WINEARCH` | Wine 10 以下は `win32`、11 以降は指定なし | プレフィックスの種類 (`win32` / `win64` / `wow64`) |
| `TARO_LAUNCHER` | `taro-wine` | ランチャの名前 |
| `TARO_BIN` | `~/.local/bin` | ランチャの置き場所 |

- `WINEARCH` を指定しないとき、Wine 10 以下では `win32`、Wine 11 以降では指定なし
  (64bit プレフィックス + WoW64) にする。Wine 11 で 32bit 専用プレフィックスが非推奨になったため。
- 64bit プレフィックスでは `C:\Program Files (x86)\Justsystem` に入る。ランチャは自動でそちらを指す。
- 確認済み: Debian の Wine 10.0 で win32 / win64 の両プレフィックス。
  未確認: Wine 11 以降 (新 WoW64 モードでセットアップの 16bit 部分が通るかどうかを含む)。

## WSL (WSLg) で使う場合

A は 2 台の WSL2 + WSLg 環境で起動と表示を確認した。B は WSL 上でインストール完走まで確認、起動は未確認。

- **日本語フォントは WSL のディストリビューション (Debian) 側に入れる。** タイトルバーは Wine ではなく
  WSLg の System ディストリビューションが描くが、フォントは Debian 側の `/usr/share/fonts` を
  `/mnt/wslg/distro/usr/share/fonts` として参照している。System ディストリビューション側に入れる必要は
  ない (入れても `wsl --shutdown` で消える)。必要なのはスケーラブルな日本語フォントが 1 つあることで、
  main の `fonts-ipafont-gothic` で足りるはず (System 側の `fc-match "sans:lang=ja"` が IPAGothic を
  返すことからの判断。このパッケージだけを入れた WSL 環境では未確認)。non-free の
  `fonts-ipafont-nonfree-uigothic` を前提にする必要はない。`install-both.sh` は日本語フォントが 1 つも無ければ
  `fonts-ipafont-gothic` を入れる。手で入れるなら `sudo apt install fonts-ipafont`。
- **フォントを入れたら `wsl --shutdown`。** 入れた時点で既に動いていた WSLg のコンポジタは新しいフォントを
  拾わないので、タイトルバーが 16 進コード入りの箱 (例: `4E00 592A 90CE` =「一太郎」) のままになる。
  PowerShell で `wsl --shutdown` して起動し直すと直る。確認には `wsl --system fc-list :lang=ja`。
- アプリの中 (メニュー、ダイアログ、本文) はこのフォントに依存しない。A は付属の JL フォントへの振り向け、
  B は jwd の設定で描かれる。
- ロケールの既定が `C.UTF-8` / `en_US.UTF-8` のことが多い。`ja_JP.UTF-8` を生成しておくこと
  (`sudo dpkg-reconfigure locales`)。ランチャと各スクリプトは必要なら自分で `ja_JP.UTF-8` に切り替える。

## A と B の共存

共存できる。同じユーザーで B と A (win32 プレフィックス)、A (win64 プレフィックス) の 3 つを同時に起動して確認した。

| | A | B |
|---|---|---|
| プログラム本体 | `~/.wine-taro*/drive_c` | `/opt/jwd/C` |
| ユーザー設定 | 同じプレフィックス内 | `~/.jwd`、`~/justsystem` |
| wineserver | Wine のもの (プレフィックスごと) | `/opt/jwd/bin/wineserver` (`/tmp/.jwd-<user>`) |
| 起動コマンド | `~/.local/bin/taro-wine*` | `/usr/local/bin/taro` |

注意点は 1 つ。**`WINEPREFIX` や `WINEDLLOVERRIDES` をシェルで export しない。** jwd の Wine も
`WINEPREFIX` を読むので (`libs/wine/config.c`)、export してあると B が A のプレフィックスを自分の設定
ディレクトリとして使おうとする。`taro-wine*` はスクリプトの中だけで設定している。

## B. 同梱ランタイム (jwd) で動かす

付属のセットアップはそのまま使い、同梱ランタイムが現行のカーネル / glibc で動くように手当てする。

### 手順

```sh
xhost +si:localuser:root                 # インストーラが X の画面を使う (要 x11-xserver-utils)
sudo -H ./install.sh ~/ICHITARO_LINUX
xhost -si:localuser:root
taro                                     # 一般ユーザーで起動
taro /home/you/doc.jtd                   # ファイル指定は絶対パスで
```

日本語入力は XIM 経由。uim なら `uim-xim` を起動し、`XMODIFIERS=@im=uim` を付けて `taro` を起動する。
Wayland セッションでは XWayland 上の X11 アプリとして動く。

### install.sh が行うこと

| 問題 | 症状 | 対処 |
|---|---|---|
| 32bit ELF のみ | `required file not found` | i386 マルチアーキテクチャと X11 等の 32bit ライブラリ |
| `libstdc++.so.5` が trixie に無い | `ole32.dll` がロードできずセットアップが止まる | bookworm の `libstdc++5` を導入 |
| `libungif.so.4` が無い | `oleaut32.dll` がロードできない | jessie の `libgif4` を `/opt/jwd/lib` に私的に配置 |
| glibc 2.41 が実行可能スタック要求の `dlopen` を拒否 | `cannot enable executable stack` | `GLIBC_TUNABLES=glibc.rtld.execstack=2` (jwdrun) |
| mmap が 2GB より上を返す | `failed to create the process heap` | `ADDR_COMPAT_LAYOUT` (jwdrun) |
| NX。Wine のサンク類がデータ領域にある | 起動直後に SIGSEGV | `READ_IMPLIES_EXEC` (jwdrun) |
| 物理メモリ 2GB 超で 32bit 演算が溢れる | 「メモリが足りません」 | 偽の `/proc/meminfo` (jwdshim) |
| `mmap_min_addr` 未満のヒントが無視される | セットアップ第 2 段階が winedos で落ちる | 低位 1MB の確保を肩代わり (jwdshim) |
| シグナルフレームが 4096 バイトのシグナルスタックに収まらない (AVX-512 で約 3.7KB) | TEB 末尾が壊れ、保存ダイアログ等で落ちる | シグナルスタックを上方向に延長 (jwdshim) |
| `/bin/sh` が dash | `test: unexpected operator` | 同梱スクリプトのシェバンを bash に変更 |
| メッセージ変換の `e2j` がロケール `ja_JP.UTF-8` ちょうどのときしか変換しない | セットアップのメッセージが文字化け (`LANGUAGE=ja:en` などでは `e2j` が異常終了) | 付属スクリプトを `LC_ALL=ja_JP.UTF-8` 固定で呼ぶ |

`vm.mmap_min_addr` などシステム全体の設定は変更しない。

`libstdc++5` と `libgif4` は Debian のミラーから HTTPS で取得し、SHA256 を Debian の Packages ファイルに
記載の値と照合する。一致しなければ中止する。

### 確認できたこと / できていないこと

Debian 13.7 のユーザーランド (glibc 2.41) を chroot で用意し、AVX-512 搭載機・カーネル 6.18 上で確認した。
Debian 13 標準カーネル 6.12 は設定 (`IA32_EMULATION`, `MODIFY_LDT_SYSCALL`, `X86_16BIT`,
`mmap_min_addr=65536`) が同条件であることを確認したのみで、実機では未実行。

- 確認済み: インストール完走、一般ユーザーでの起動、英数入力、uim-xim + anthy での日本語入力、
  `.jtd` 保存、終了、再起動して読み込み
- 未確認: 印刷 (`lpr` へパイプする設定)、ヘルプ、同梱 ATOK X / IIIMF、XWayland 上での表示、長時間の使用
- 既知: インストールログに `JSOLESV.DLL=Exec Function NG` が 1 件残る (MAPI に依存する 1997 年の OLE サーバー)

### 安全上の注意

- 2003 年の Wine を NX 無効相当 (`READ_IMPLIES_EXEC`) で動かすことになる。`libgif4` も Debian 8 のもので、
  更新は終わっている。出所の分からないファイルは開かないほうがよい。
- セットアップは root で実行され、`/opt/jwd`、`/usr/local/bin/taro` などに書き込む。

### 補足

- 64bit の子プロセス (lpr など) が `LD_PRELOAD` の 32bit ライブラリを無視する旨の警告を出すことがあるが無害。
- ヘルプや HTML を開くブラウザは `/opt/jwd/etc/user.conf` の `[JWDHelpers]` で決まり、既定は
  `mozilla,konqueror`。Debian 13 では書き換えが必要なはず (未確認)。
- 環境変数 `NOXIM` をセットすると XIM を無効にできる (ChangeLog.jwd 2004-09-14、デバッグ用)。
- Docker の既定の seccomp プロファイルは `personality()` に渡せる値を限定しているので、コンテナの中では
  `jwdrun` が必要な設定をできないはず (未確認)。
- 削除: `sudo -H /opt/jwd/bin/jwd.uninstall`、`sudo apt remove libstdc++5:i386`

## リポジトリの構成

| パス | 内容 |
|---|---|
| `install-both.sh` | A と B をまとめて入れる |
| `install-wine.sh`、`wine-jlfonts.sh` | A: Wine プレフィックスの作成、セットアップ、JL フォント、ランチャ |
| `install.sh` | B: 同梱ランタイムのセットアップと現行環境向けの手当て |
| `src/jwdrun.c` | B: personality を設定して jwd の Wine を起動する 32bit ランチャ |
| `src/jwdshim.c` | B: `LD_PRELOAD` で読み込む互換シム (低位メモリ、`/proc/meminfo`、シグナルスタック) |
| `tests/shimtest.c` | `jwdrun` と `jwdshim.so` のテスト。製品は要らない |
| `tools/jwd-probe.c` | jwd が手を入れた挙動を、手元の Wine で確かめる Windows 用の検査プログラム |
| `docs/jwd-vs-current-wine.md` | jwd の変更と現行 Wine の対応状況 |

製品由来のファイル (バイナリ、フォント、ドキュメント、画面写真、jwd のソース) は置いていない。
プルリクエストにも含めないこと。

## テスト

```sh
sudo apt install gcc-multilib shellcheck gcc-mingw-w64-i686
make lint      # スクリプトの構文チェックと shellcheck
make check     # jwdrun + jwdshim.so を製品なしで確かめる
make probe     # build/jwd-probe.exe をビルド。wine build/jwd-probe.exe で実行
```

`make check` は、jwd の Wine が行う低位メモリの確保、`/proc/meminfo` の読み取り、TEB 直上のシグナルスタックでの
シグナル処理を小さな 32bit プログラムで再現し、シム経由で期待どおりになることを確かめる。同じ内容を
GitHub Actions でも実行するように設定してある (`.github/workflows/ci.yml`)。一太郎本体を使う確認は自動化していない。

## 不具合の報告

次の情報があると切り分けやすい。

- A か B か。A なら `wine --version` とプレフィックスの種類 (win32 / win64)
- `uname -r`、`cat /etc/debian_version`、WSL なら `wsl --version`
- `locale` の出力
- A: `WINEDEBUG=err+all ~/.local/bin/taro-wine` の端末出力 (ランチャは既定で Wine のメッセージを止めている)、
  セットアップの問題なら `~/.wine-taro/drive_c/windows/JSINS.LOG`
- B: `JWDSHIM_DEBUG=1 taro` の端末出力、セットアップの問題なら `/root/SETUP_TARO14.msg`

Wine 本体の不具合と分かったものは [WineHQ の Bugzilla](https://bugs.winehq.org/) に報告するのがよい。

## 作成について

このリポジトリのスクリプト、プログラム、文書は、Anthropic の AI「Claude」によるコード作成支援と検証を受けて
作成した。chroot 環境でのインストールと動作の確認、jwd と現行 Wine のソースの比較は Claude が行い、
WSL 環境での確認は作者が行った。

## ライセンス

このリポジトリのスクリプト、プログラム、文書は [BSD 3-Clause License](LICENSE) で配布する。
一太郎 for Linux と、それに含まれる jwd (LGPL の Wine を基にしたもの) は、それぞれの許諾条件に従う。

## 商標

「一太郎」「ATOK」は株式会社ジャストシステムの登録商標。その他の会社名、製品名は各社の商標または登録商標。
