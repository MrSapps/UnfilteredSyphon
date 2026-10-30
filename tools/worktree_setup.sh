#!/bin/sh
# Make a git worktree buildable by linking the untracked toolchain and originals
# from the main checkout:  tools/worktree_setup.sh /path/to/main/checkout
set -e
MAIN=$1
[ -d "$MAIN/orig" ] || { echo "usage: $0 <main checkout>"; exit 1; }
for p in orig .venv tools/binutils tools/compilers tools/maspsx tools/m2c tools/asm-differ; do
    [ -e "$p" ] || ln -s "$MAIN/$p" "$p"
done
