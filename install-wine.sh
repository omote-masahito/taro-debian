#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
#
# 一太郎 for Linux (2005) を Debian 13 (trixie) 標準の Wine で動かす
# (同梱ランタイム jwd を使わず、Windows 用バイナリだけを使う経路)
#
#   sudo dpkg --add-architecture i386 && sudo apt update
#   sudo apt install wine wine32:i386
#   ./install-wine.sh /path/to/ICHITARO_LINUX        # 一般ユーザーで実行
#
# 別の Wine (WineHQ の winehq-devel など) で試すときは、プレフィックスとランチャ名を
# 変えて並べて入れられる。
#   WINE=/opt/wine-devel/bin/wine WINEPREFIX=$HOME/.wine-taro-devel \
#       TARO_LAUNCHER=taro-wine-devel ./install-wine.sh /path/to/ICHITARO_LINUX
#
# 要点。
#   - Windows のバージョンを XP として報告させる。既定 (Windows 10) ではセット
#     アップが拒否され、98/Me では空き容量チェックでゼロ除算になる。
#   - JL 明朝 / JL ゴシック / JLRoman / JLSwiss は Windows 用セットアップではなく
#     jwd 側 (bin/jwd/ENV) に入っているので、Fonts にコピーし、ＭＳ ゴシック等の
#     要求も JL フォントに振り向ける (wine-jlfonts.sh)。
#
set -e

SRC="${1:?usage: $0 <ICHITARO_LINUX ディレクトリ> [--yes]}"
YES="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$SRC" && pwd)"
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-taro}"
WINE="${WINE:-wine}"
export WINEDEBUG="${WINEDEBUG:--all}"
BIN="${TARO_BIN:-$HOME/.local/bin}"
LAUNCHER="${TARO_LAUNCHER:-taro-wine}"

die() { echo "ERROR: $*" >&2; exit 1; }

# Wine は Unix 側のロケールから ANSI コードページを決める。日本語ロケールでないと
# CP932 にならず、セットアップの文字列が "????" になり、日本語名のファイルを作れずに
# エラーダイアログで止まる。ja_JP.* でなければ ja_JP.UTF-8 にして実行する。
case "${LC_ALL:-${LC_CTYPE:-$LANG}}" in
    ja_JP.*) ;;
    *) export LC_ALL=ja_JP.UTF-8 ;;
esac
locale -a 2>/dev/null | grep -qi '^ja_JP\.utf-\?8$' \
    || die "ja_JP.UTF-8 ロケールがありません (sudo dpkg-reconfigure locales)"

[ "$(id -u)" != 0 ] || die "一般ユーザーで実行してください"
command -v "$WINE" >/dev/null || die "$WINE がありません (apt install wine wine32:i386)"

# プレフィックスの種類。Wine 10 までは 32bit 専用 (win32) にする。Wine 11 以降は
# win32 プレフィックスが非推奨なので既定 (64bit プレフィックス + WoW64) に任せる。
# 明示したいときは WINEARCH=win32 / win64 / wow64 を付けて実行する。
if [ -z "${WINEARCH+x}" ]; then
    major="$("$WINE" --version | sed -n 's/^wine-\([0-9]*\).*/\1/p')"
    if [ "${major:-0}" -le 10 ]; then export WINEARCH=win32; fi
fi
[ -f "$SRC/bin/TARO14/MASTER/SETUP.EXE" ] || die "$SRC に一太郎 for Linux の製品ファイルがありません (bin/jwd/setup, bin/TARO14/MASTER/SETUP.EXE)"
[ -n "$DISPLAY" ] || die "DISPLAY がありません"
# 既に一太郎が入っているプレフィックスなら、セットアップは飛ばしてフォントと
# ランチャだけ作り直す (スクリプトを更新した後の再設定用)。
RECONF=0
if [ -e "$WINEPREFIX" ]; then
    ls "$WINEPREFIX"/drive_c/Program\ Files*/Justsystem/TARO14/TARO14.EXE >/dev/null 2>&1 \
        || die "$WINEPREFIX が既にありますが、一太郎は入っていません。削除してからやり直してください"
    RECONF=1
    # プレフィックスの種類は作成時のものに従う
    if grep -q '^#arch=win32' "$WINEPREFIX/system.reg" 2>/dev/null; then export WINEARCH=win32; else unset WINEARCH; fi
    echo "== $WINEPREFIX は導入済みです。フォントとランチャだけ再設定します"
fi

if [ "$RECONF" = 0 ] && [ "$YES" != "--yes" ]; then
    echo "本製品の使用には使用許諾契約書 ($SRC/doc/license.html) への同意が必要です。"
    read -r -p "同意する(y)/同意しない(n) " ans
    [ "$ans" = y ] || [ "$ans" = Y ] || die "インストールを中止します"
fi

if [ "$RECONF" = 0 ]; then
echo "== 1. Wine プレフィックス ($WINEPREFIX)"
"$WINE" wineboot -i
"$WINE" winecfg /v winxp
"$WINE"server -w 2>/dev/null || wineserver -w

echo "== 2. 一太郎のセットアップ (数分かかります)"
# SETUP.EXE は第 2 段階 (JSMSETUP.EXE) を起動して非 0 で戻ることがあるので、
# 終了コードではなく全プロセスの終了と TARO14.EXE の有無で判定する。
"$WINE" "$SRC/bin/TARO14/MASTER/SETUP.EXE" /V || true
"$WINE"server -w 2>/dev/null || wineserver -w
fi

# 64bit プレフィックスでは Program Files (x86) に入る
PF=""
for d in "Program Files" "Program Files (x86)"; do
    [ -f "$WINEPREFIX/drive_c/$d/Justsystem/TARO14/TARO14.EXE" ] && PF="$d"
done
[ -n "$PF" ] || die "セットアップに失敗しました ($WINEPREFIX/drive_c/windows/JSINS.LOG)"

echo "== 3. JL フォント"
WINE="$WINE" "$HERE/wine-jlfonts.sh" "$SRC"

echo "== 4. ランチャ ($BIN/$LAUNCHER)"
mkdir -p "$BIN"
cat > "$BIN/$LAUNCHER" <<LAUNCHER
#!/bin/bash
# WINEPREFIX などはこのスクリプトの中だけで設定する。シェルで export すると
# 同梱ランタイム版 (taro) も同じ変数を読んでしまう。
export ${WINEARCH:+WINEARCH=$WINEARCH }WINEPREFIX="$WINEPREFIX" WINEDEBUG="\${WINEDEBUG:--all}"
# 日本語ロケールでないと CP932 にならない
case "\${LC_ALL:-\${LC_CTYPE:-\$LANG}}" in ja_JP.*) ;; *) export LC_ALL=ja_JP.UTF-8 ;; esac
locale -a 2>/dev/null | grep -qi '^ja_JP\.utf-\?8\$' \\
    || echo "\$0: ja_JP.UTF-8 ロケールがありません。文字化けします (sudo dpkg-reconfigure locales)" >&2
args=()
for a in "\$@"; do
    [ -e "\$a" ] && a="\$("$WINE" winepath -w "\$a")"
    args+=("\$a")
done
exec "$WINE" 'C:\\$PF\\Justsystem\\TARO14\\TARO14.EXE' "\${args[@]}"
LAUNCHER
chmod +x "$BIN/$LAUNCHER"

echo "== 完了: $BIN/$LAUNCHER [file.jtd]"
