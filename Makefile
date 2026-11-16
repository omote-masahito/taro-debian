# SPDX-License-Identifier: BSD-3-Clause
#
# install.sh は自分で jwdrun / jwdshim.so をビルドするので、インストールに make は要らない。
# この Makefile は開発用 (ビルドの確認、テスト、検査プログラム)。
#
#   make          build/bin/jwdrun と build/lib/jwdshim.so
#   make check    製品なしで jwdrun + jwdshim.so の動作を確かめる (tests/shimtest.c)
#   make lint     スクリプトの構文チェックと shellcheck
#   make probe    Wine 用の検査プログラム build/jwd-probe.exe (要 gcc-mingw-w64-i686)

CC     ?= gcc
CFLAGS ?= -O2 -Wall
MINGW  ?= i686-w64-mingw32-gcc
B      := build
SCRIPTS := install.sh install-wine.sh install-both.sh wine-jlfonts.sh

all: $(B)/bin/jwdrun $(B)/lib/jwdshim.so

$(B)/bin/jwdrun: src/jwdrun.c
	@mkdir -p $(@D)
	$(CC) -m32 $(CFLAGS) -o $@ $<

$(B)/lib/jwdshim.so: src/jwdshim.c
	@mkdir -p $(@D)
	$(CC) -m32 $(CFLAGS) -shared -fPIC -o $@ $< -ldl

$(B)/shimtest: tests/shimtest.c
	@mkdir -p $(@D)
	$(CC) -m32 $(CFLAGS) -o $@ $<

check: all $(B)/shimtest
	JWDDIR=$(CURDIR)/$(B) $(B)/bin/jwdrun $(B)/shimtest

lint:
	@for f in $(SCRIPTS); do bash -n $$f || exit 1; done
	LC_ALL=C.UTF-8 shellcheck -x $(SCRIPTS)

probe: $(B)/jwd-probe.exe

$(B)/jwd-probe.exe: tools/jwd-probe.c
	@mkdir -p $(@D)
	$(MINGW) -O1 -finput-charset=UTF-8 -o $@ $< -lgdi32 -luser32

clean:
	rm -rf $(B)

.PHONY: all check lint probe clean
