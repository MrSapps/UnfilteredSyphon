#!/bin/sh
# Set up the local toolchain without root: python venv, mipsel binutils, decomp tools.
set -e
cd "$(dirname "$0")/.."

python3 -m venv .venv
.venv/bin/pip install -q --upgrade pip
.venv/bin/pip install -q "splat64[mips]" spimdisasm rabbitizer ninja pycparser colorama watchdog python-Levenshtein cxxfilt

if [ ! -x tools/binutils/usr/bin/mipsel-linux-gnu-as ]; then
    tmp=$(mktemp -d)
    (cd "$tmp" && apt-get download binutils-mipsel-linux-gnu)
    dpkg -x "$tmp"/*.deb tools/binutils
    rm -rf "$tmp"
fi

[ -d tools/m2c ] || git clone -q --depth 1 https://github.com/matt-kempster/m2c.git tools/m2c
[ -d tools/asm-differ ] || git clone -q --depth 1 https://github.com/simonlindholm/asm-differ.git tools/asm-differ
# maspsx ships a wine-based ASPSX.EXE test suite; drop it so editors never run it
[ -d tools/maspsx ] || { git clone -q --depth 1 https://github.com/mkst/maspsx.git tools/maspsx && rm -rf tools/maspsx/aspsx tools/maspsx/tests; }

echo "Copy the original files into orig/: scus_942.40, siphon.map, bin/*.ovl"
echo "Then: . .venv/bin/activate && python3 tools/map2syms.py && python3 configure.py && ninja"
