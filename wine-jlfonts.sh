#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
#
# Wine プレフィックスに一太郎 for Linux 付属の JL フォントを入れ、ＭＳ ゴシック /
# ＭＳ 明朝 / MS UI Gothic などの要求を JL フォントに振り向ける。
# install-wine.sh から呼ばれる。既存のプレフィックスに後から適用してもよい。
#
#   ./wine-jlfonts.sh /path/to/ICHITARO_LINUX
#   WINEPREFIX=$HOME/.wine-taro64 ./wine-jlfonts.sh /path/to/ICHITARO_LINUX
#
# Wine の日本語ロケールでは、メニューやダイアログの文字は MS UI Gothic で描かれる。
# その名前のフォントも、代わりになる日本語フォントもシステムに無いと、メニューが
# 「□□□□(F)」になる。同梱ランタイム jwd は user.conf の [substitute.fonts] で
# 同じ振り向けをしているので、それに合わせる。
#
set -e

SRC="${1:?usage: $0 <ICHITARO_LINUX ディレクトリ>}"
SRC="$(cd "$SRC" && pwd)"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-taro}"
export WINEDEBUG="${WINEDEBUG:--all}"
WINE="${WINE:-wine}"
FONTS="$SRC/bin/jwd/ENV/C/WINDOWS/Fonts"

die() { echo "ERROR: $*" >&2; exit 1; }

case "${LC_ALL:-${LC_CTYPE:-$LANG}}" in ja_JP.*) ;; *) export LC_ALL=ja_JP.UTF-8 ;; esac
[ -d "$WINEPREFIX/drive_c/windows/Fonts" ] || die "$WINEPREFIX は Wine プレフィックスではありません"
[ -f "$FONTS/JLGOTHIC.TTC" ] || die "$FONTS に JL フォントがありません"

install -m 644 "$FONTS"/JL* "$WINEPREFIX/drive_c/windows/Fonts/"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
# regedit 用。UTF-16LE (BOM 付き、CRLF) にして渡す
{
    printf '\xff\xfe'
    sed 's/$/\r/' <<'REG' | iconv -f UTF-8 -t UTF-16LE
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\Software\Wine\Fonts\Replacements]
"MS UI Gothic"="JLPゴシック"
"MS Gothic"="JLゴシック"
"ＭＳ ゴシック"="JLゴシック"
"MS PGothic"="JLPゴシック"
"ＭＳ Ｐゴシック"="JLPゴシック"
"MS Mincho"="JL明朝"
"ＭＳ 明朝"="JL明朝"
"MS PMincho"="JLP明朝"
"ＭＳ Ｐ明朝"="JLP明朝"
REG
} > "$TMP/jlfonts.reg"
"$WINE" regedit /S "$TMP/jlfonts.reg"
"$WINE"server -w 2>/dev/null || wineserver -w
echo "JL フォントを $WINEPREFIX に設定しました"
