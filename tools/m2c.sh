#!/bin/sh
# Decompile one function with m2c:  tools/m2c.sh <tu> <function> [extra m2c args]
# Context comes from the TU's headers so m2c knows types/prototypes.
set -e
cd "$(dirname "$0")/.."
tu=$1; fn=$2; shift 2
ctx=build/m2c_ctx.c
printf '#include "common.h"\n#include "game.h"\n' > build/m2c_ctx_in.c
for h in include/*.h; do printf '#include "%s"\n' "$(basename "$h")" >> build/m2c_ctx_in.c; done
cpp -P -undef -D__GNUC__=2 -Dmips -DM2CTX -Iinclude build/m2c_ctx_in.c > "$ctx"
asm=$(find asm -path "*nonmatchings/$tu/$fn.s" | head -1)
.venv/bin/python3 tools/m2c/m2c.py --target mipsel-gcc-c --context "$ctx" "$@" "$asm"
