#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
#
# 一太郎 for Linux (2005) を A (Debian の Wine) と B (同梱ランタイム jwd) の両方で
# 一度に入れる。一般ユーザーで実行する。root が要る部分だけ途中で sudo を使う。
#
#   ./install-both.sh /path/to/ICHITARO_LINUX
#
# 既に入っている側は入れ直さないので、片方だけ入れた後に実行してもよい
# (A が導入済みならフォントとランチャの再設定だけ行う)。A の入れ先などは install-wine.sh と同じ環境変数
# (WINEPREFIX, WINEARCH, WINE, TARO_LAUNCHER) で変えられる。
#
set -e

SRC="${1:?usage: $0 <ICHITARO_LINUX ディレクトリ> [--yes]}"
YES="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$SRC" && pwd)"
PREFIX="${WINEPREFIX:-$HOME/.wine-taro}"
LAUNCHER="${TARO_BIN:-$HOME/.local/bin}/${TARO_LAUNCHER:-taro-wine}"

die() { echo "ERROR: $*" >&2; exit 1; }

[ "$(id -u)" != 0 ] || die "一般ユーザーで実行してください (途中で sudo を使います)"
command -v sudo >/dev/null || die "sudo がありません"
[ -f "$SRC/bin/TARO14/MASTER/SETUP.EXE" ] || die "$SRC に一太郎 for Linux の製品ファイルがありません (bin/jwd/setup, bin/TARO14/MASTER/SETUP.EXE)"
[ -n "$DISPLAY" ] || die "DISPLAY がありません。どちらのセットアップも X の画面を必要とします"

DO_B=1; [ -e /opt/jwd ] && DO_B=0
NEW_A=1; [ -e "$PREFIX" ] && NEW_A=0

# 使用許諾の確認はここで 1 回だけ行い、以降のスクリプトには同意済みとして渡す
if [ "$NEW_A$DO_B" != 00 ] && [ "$YES" != "--yes" ]; then
    echo "本製品の使用には使用許諾契約書 ($SRC/doc/license.html) への同意が必要です。"
    read -r -p "同意する(y)/同意しない(n) " ans
    [ "$ans" = y ] || [ "$ans" = Y ] || die "インストールを中止します"
fi

if [ "$NEW_A" = 1 ] && [ -z "$WINE" ]; then
    echo "#### Wine のパッケージ (sudo)"
    sudo dpkg --add-architecture i386
    sudo apt-get update -q
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q wine wine32:i386 wine64
fi

# アプリの外側 (ウィンドウマネージャが描くタイトルバーなど) は Wine のフォント設定とは
# 無関係に、システムの日本語フォントで描かれる。スケーラブルな日本語フォントが 1 つも
# 無ければ IPA ゴシック (main) を入れる。
FONT_ADDED=0
if ! command -v fc-list >/dev/null || [ -z "$(fc-list ':lang=ja:scalable=true' family 2>/dev/null | head -1)" ]; then
    echo "#### 日本語フォント (sudo)"
    sudo apt-get update -q
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -q fonts-ipafont-gothic
    FONT_ADDED=1
fi

if [ "$DO_B" = 1 ]; then
    echo "#### B. 同梱ランタイム版 (sudo)"
    # セットアップは root で動き、X の画面を使う
    granted=0
    if command -v xhost >/dev/null && xhost +si:localuser:root >/dev/null 2>&1; then granted=1; fi
    rc=0
    sudo -H DISPLAY="$DISPLAY" ${XAUTHORITY:+XAUTHORITY="$XAUTHORITY"} \
        "$HERE/install.sh" "$SRC" --yes || rc=$?
    [ "$granted" = 0 ] || xhost -si:localuser:root >/dev/null 2>&1 || true
    [ "$rc" = 0 ] || die "B のインストールに失敗しました"
else
    echo "#### B. /opt/jwd が既にあるので飛ばします"
fi

# A は導入済みでも呼ぶ。その場合 install-wine.sh はフォントとランチャだけ再設定する
echo "#### A. Wine 版"
"$HERE/install-wine.sh" "$SRC" --yes || die "A のインストールに失敗しました"

echo
echo "#### 完了"
echo "  B (同梱ランタイム): taro"
echo "  A (Wine)          : $LAUNCHER"
echo "  日本語入力は XIM 経由。uim なら uim-xim を起動し XMODIFIERS=@im=uim を付けて起動する。"
if [ "$FONT_ADDED" = 1 ] && { [ -n "$WSL_DISTRO_NAME" ] || grep -qi microsoft /proc/version 2>/dev/null; }; then
    echo "  WSL: 日本語フォントを追加しました。タイトルバーに反映するには PowerShell で"
    echo "       wsl --shutdown を実行してから起動し直してください。"
fi
