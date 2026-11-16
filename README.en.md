# taro-debian

Scripts and helpers for running **Ichitaro for Linux** (JustSystems, 2005) on Debian 13 (trixie) amd64.
The full documentation is in Japanese: [README.md](README.md).

- Unofficial. Not affiliated with or supported by JustSystems Corporation.
- The product is **not** included. You need your own copy of the Ichitaro for Linux CD-ROM.
- The main purpose is to test current Wine with this product. Running it on the runtime that shipped
  with the product is provided for comparison.

## What the product is

Ichitaro for Linux is the Windows build of Ichitaro (TARO14) bundled with "jwd", a JustSystems fork of
Wine 20031212. Everything is 32-bit x86. This repository gives two ways to run it.

| | A. Wine 10 from Debian 13 | B. Bundled runtime (jwd) |
|---|---|---|
| Script | `install-wine.sh` (as a normal user) | `install.sh` (as root) |
| Installs into | `~/.wine-taro` | `/opt/jwd` |
| Uses | Windows binaries only | The product's own setup and its 2003 Wine |
| Special runtime conditions | none | `READ_IMPLIES_EXEC`, executable stack |

## Quick start

```sh
git clone https://github.com/omote-masahito/taro-debian.git taro-debian
cd taro-debian
./install-both.sh ~/ICHITARO_LINUX     # directory holding a copy of the CD-ROM (bin/, doc/ ...)
~/.local/bin/taro-wine                 # A
taro                                   # B
```

A `ja_JP.UTF-8` locale and an X display are required. Japanese input goes through XIM
(for example `uim-xim` with `XMODIFIERS=@im=uim`).

## What had to be done

**A (current Wine)**

- Report Windows XP (or 2000). With the default Windows 10 the installer refuses to run; with 98/Me it
  divides by zero in its free-space check, because `INT21_Fat32` returns success without setting carry
  for unimplemented AX=73xx functions.
- Run under a Japanese locale so that the ANSI code page is 932.
- Copy the JL fonts from the jwd part of the CD-ROM and map MS UI Gothic / MS Gothic / MS Mincho to
  them through `HKCU\Software\Wine\Fonts\Replacements`.

**B (bundled jwd)**

- i386 multiarch libraries, plus `libstdc++5` (bookworm) and `libgif4` (jessie), fetched over HTTPS
  and checked against the SHA256 values in Debian's package indices.
- `jwdrun`: sets `ADDR_COMPAT_LAYOUT | READ_IMPLIES_EXEC` from a 32-bit process and
  `GLIBC_TUNABLES=glibc.rtld.execstack=2`.
- `jwdshim.so` (`LD_PRELOAD`): honours the low-address mmap hint used for DOS memory, serves a small
  fake `/proc/meminfo`, and extends each thread's 4096-byte signal stack upwards so that today's
  signal frames (about 3.7 KB with AVX-512) no longer overwrite the TEB below it.

## Status

Verified on a Debian 13.7 userland: installation, start-up, Japanese input with uim-xim + anthy, and
saving and reopening `.jtd` files, on both routes (A with Wine 10.0, win32 and win64 prefixes). A was
also checked on two WSL2 + WSLg machines. Not verified: printing, help, Wine 11 and later.

[docs/jwd-vs-current-wine.md](docs/jwd-vs-current-wine.md) (Japanese) compares the 620 ChangeLog
entries of the JustSystems fork with Wine 10.0 and 11.21, and lists six changes that current Wine
does not have. `tools/jwd-probe.c` is the small Windows program used for the measurements.

## Development

```sh
sudo apt install gcc-multilib shellcheck gcc-mingw-w64-i686
make lint && make check && make probe
```

`make check` exercises `jwdrun` and `jwdshim.so` without the product.

## Acknowledgement

The code and documents in this repository were written and verified with the assistance of Claude,
Anthropic's AI. The installation and behaviour checks in a chroot and the source comparison between
jwd and current Wine were done by Claude; the checks on WSL were done by the author.

## License

BSD 3-Clause, see [LICENSE](LICENSE). Ichitaro for Linux and the jwd runtime it contains
(based on Wine, LGPL) are covered by their own terms. "Ichitaro" and "ATOK" are registered
trademarks of JustSystems Corporation.
