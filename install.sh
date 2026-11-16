#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
#
# 一太郎 for Linux (2005) を Debian 13 (trixie) amd64 に入れるための補助スクリプト
#
#   sudo -H ./install.sh /path/to/ICHITARO_LINUX      # 製品 CD-ROM の内容をコピーしたディレクトリ
#
# 製品付属のセットアップ (bin/jwd/setup, bin/TARO14/setup) はそのまま使い、
# 現行カーネル/glibc で同梱ランタイム (Wine 20031212 ベースの "jwd") が動くよう
# 前後に次の手当てを入れる。
#   - i386 マルチアーキテクチャと必要な 32bit ライブラリ
#   - trixie に無い libstdc++.so.5 / libungif.so.4
#   - jwdrun (32bit ランチャ) と jwdshim.so (LD_PRELOAD) のビルドと組み込み
#   - /bin/sh が dash でも動くようシェバンを bash に変更
#
set -e

SRC="${1:?usage: sudo -H $0 <ICHITARO_LINUX ディレクトリ> [--yes]}"
YES="$2"
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$SRC" && pwd)"
JWD=/opt/jwd

# trixie に無い 2 つのライブラリ。SHA256 は Debian の Packages ファイル
# (bookworm main / jessie main の binary-i386) に記載の値。
LIBSTDCPP5_URL=https://deb.debian.org/debian/pool/main/g/gcc-3.3/libstdc++5_3.3.6-34_i386.deb
LIBSTDCPP5_SHA256=55d14a8e77551f500b61893a006d960872a35a29229c782acfa7f8cd48d21063
LIBGIF4_URL=https://archive.debian.org/debian/pool/main/g/giflib/libgif4_4.1.6-11+deb8u1_i386.deb
LIBGIF4_SHA256=eef17ff120f6306089249327583b5bd2b520fbe80d8bddfa8c989b3cc5068e25

die() { echo "ERROR: $*" >&2; exit 1; }

# fetch <URL> <SHA256> <出力先>: ダウンロードしてチェックサムを確かめる
fetch() {
    curl -fsSL -o "$3" "$1" || die "ダウンロードに失敗しました: $1"
    echo "$2  $3" | sha256sum -c --quiet - || die "チェックサムが一致しません: $1"
}

# 製品付属のスクリプトは EUC-JP のメッセージを同梱の e2j に通して表示する。
# e2j は LC_ALL, LC_CTYPE, LANGUAGE, LC_MESSAGES, LANG の順で最初に見つかった値が
# ちょうど ja_JP.UTF-8 のときだけ UTF-8 に変換し、それ以外 (C.UTF-8, en_US.UTF-8,
# ja_JP.utf8 など) では EUC-JP のまま出力するので文字化けする。LANGUAGE=ja:en の
# ように国や文字コードを欠く値では異常終了して何も出力せず、同じ e2j を通している
# アンインストール用のファイル一覧も欠ける。ロケールを固定して呼び出す。
jsrun() { env -u LC_CTYPE -u LANGUAGE -u LC_MESSAGES LC_ALL=ja_JP.UTF-8 LANG=ja_JP.UTF-8 "$@"; }

[ "$(id -u)" = 0 ] || die "root で実行してください (sudo -H)"
[ "$HOME" = "$(getent passwd root | cut -d: -f6)" ] || die "HOME が root のものではありません。sudo -H を使ってください"
{ [ -x "$SRC/bin/jwd/setup" ] && [ -f "$SRC/bin/TARO14/MASTER/SETUP.EXE" ]; } || die "$SRC に一太郎 for Linux の製品ファイルがありません (bin/jwd/setup, bin/TARO14/MASTER/SETUP.EXE)"
[ -n "$DISPLAY" ] || die "DISPLAY がありません。インストーラは X の画面を必要とします (xhost +si:localuser:root)"
{ [ -f "$HERE/src/jwdrun.c" ] && [ -f "$HERE/src/jwdshim.c" ]; } || die "$HERE/src に jwdrun.c / jwdshim.c がありません"
[ ! -e "$JWD" ] || die "$JWD が既にあります。先に削除してください"

echo "== 1. パッケージ"
export DEBIAN_FRONTEND=noninteractive
dpkg --add-architecture i386
apt-get update -q
apt-get install -y -q --no-install-recommends \
    libc6:i386 zlib1g:i386 libjpeg62-turbo:i386 \
    libx11-6:i386 libxext6:i386 libxrender1:i386 libxrandr2:i386 libxv1:i386 \
    libsm6:i386 libice6:i386 libgl1:i386 libglu1-mesa:i386 \
    gcc gcc-multilib libc6-dev-i386 bzip2 curl ca-certificates locales
locale -a | grep -qi '^ja_JP\.utf-\?8$' || die "ja_JP.UTF-8 ロケールがありません (dpkg-reconfigure locales)"

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
if ! ldconfig -p | grep -q 'libstdc++\.so\.5 '; then
    fetch "$LIBSTDCPP5_URL" "$LIBSTDCPP5_SHA256" "$TMP/libstdc++5.deb"
    apt-get install -y -q "$TMP/libstdc++5.deb"
fi
fetch "$LIBGIF4_URL" "$LIBGIF4_SHA256" "$TMP/libgif4.deb"
dpkg-deb -x "$TMP/libgif4.deb" "$TMP/gif"

echo "== 2. ランチャと shim のビルド"
gcc -m32 -O2 -Wall -o "$TMP/jwdrun" "$HERE/src/jwdrun.c"
gcc -m32 -O2 -Wall -shared -fPIC -o "$TMP/jwdshim.so" "$HERE/src/jwdshim.c" -ldl

echo "== 3. 動作環境 (jwd)"
# 末尾のレジストリ登録だけは素のランチャで走るため失敗する。4. の後でやり直す。
jsrun bash "$SRC/bin/jwd/setup"
[ -x "$JWD/bin/wine" ] || die "動作環境の展開に失敗しました ($HOME/SETUP_TARO14.msg)"

echo "== 4. 現行環境向けの組み込み"
chmod u+w "$JWD/bin" "$JWD/bin/jwd" "$JWD/bin/jwd.config" "$JWD/bin/jwd.uninstall"
install -m 755 "$TMP/jwdrun" "$JWD/bin/jwdrun"
install -m 644 "$TMP/jwdshim.so" "$JWD/lib/jwdshim.so"
install -m 644 "$TMP"/gif/usr/lib/i386-linux-gnu/libgif.so.4.1.6 "$JWD/lib/"
ln -sf libgif.so.4.1.6 "$JWD/lib/libungif.so.4"
sed -i '1s|^#!/bin/sh|#!/bin/bash|' "$JWD/bin/jwd" "$JWD/bin/jwd.config" "$JWD/bin/jwd.uninstall"
# shellcheck disable=SC2016  # $JWDDIR は書き換え先のスクリプトの中で展開させる
sed -i 's|^exec "\$JWDDIR/bin/wine" |exec "$JWDDIR/bin/jwdrun" "$JWDDIR/bin/wine" |' "$JWD/bin/jwd"
grep -q 'jwdrun' "$JWD/bin/jwd" || die "$JWD/bin/jwd の書き換えに失敗しました"
"$JWD/bin/jwd" --version || die "同梱ランタイムが起動しません"
"$JWD/bin/jwd" regedit "$JWD/C/WINDOWS/JWDINIT.REG"

echo "== 5. 一太郎本体"
if [ "$YES" = "--yes" ]; then
    echo y | jsrun bash "$SRC/bin/TARO14/setup"
else
    jsrun bash "$SRC/bin/TARO14/setup"
fi
chmod u+w "$JWD/bin/taro" "$JWD/bin/jsiinita" 2>/dev/null || true
sed -i '1s|^#!/bin/sh|#!/bin/bash|' "$JWD/bin/taro" "$JWD/bin/jsiinita"
# アンインストーラ自身のメッセージは EUC-JP の直書きなので UTF-8 に直しておく
if iconv -f EUC-JP -t UTF-8 "$JWD/bin/jwd.uninstall" > "$TMP/jwd.uninstall" 2>/dev/null; then
    cat "$TMP/jwd.uninstall" > "$JWD/bin/jwd.uninstall"
fi
[ -f "$JWD/C/Program Files/Justsystem/TARO14/TARO14.EXE" ] || die "一太郎のインストールに失敗しました ($HOME/SETUP_TARO14.msg)"

echo "== 完了: 一般ユーザーで taro を実行してください"
