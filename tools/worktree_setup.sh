#!/bin/sh
# Make a git worktree buildable by linking the untracked toolchain and originals
# from the main checkout:  tools/worktree_setup.sh /path/to/main/checkout
set -e
MAIN=$1
[ -d "$MAIN/orig" ] || { echo "usage: $0 <main checkout>"; exit 1; }
for p in .venv tools/binutils tools/compilers tools/maspsx tools/m2c tools/asm-differ; do
    [ -e "$p" ] || ln -s "$MAIN/$p" "$p"
done
# orig/ exists in a worktree (SHA1SUMS is tracked), so link its contents one by one
mkdir -p orig
for f in "$MAIN"/orig/*; do
    n=$(basename "$f")
    [ -e "orig/$n" ] || ln -s "$f" "orig/$n"
done
