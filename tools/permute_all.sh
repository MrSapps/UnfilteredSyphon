#!/bin/bash
# Run decomp-permuter on every NON_MATCHING function for $1 seconds each (default 600).
#   tools/permute_all.sh 600 [file.c ...]
# Results: permute/<func>/output-*/ ; summary in permute/summary.txt
cd "$(dirname "$0")/.."
. .venv/bin/activate
SECS=${1:-600}; shift
FILES=${@:-$(grep -l "#ifdef NON_MATCHING" src/*/*.c)}
mkdir -p permute/bin
ln -sf "$PWD/tools/bin/mipsel-linux-gnu-objdump" permute/bin/mips-linux-gnu-objdump
export PATH="$PWD/permute/bin:$PATH"
for f in $FILES; do
    for fn in $(python3 - "$f" <<'PY'
import re, sys
t = open(sys.argv[1]).read()
for m in re.finditer(r"#ifdef NON_MATCHING\n(?:/\*.*?\*/\n)?(?:static )?[^\n(]*?\b(\w+)\(", t, re.S):
    print(m.group(1))
PY
    ); do
        [ -d "permute/$fn/output-0" ] && continue
        python3 tools/permute_setup.py "$f" "$fn" >/dev/null 2>&1 || { echo "$fn: setup failed" >> permute/summary.txt; continue; }
        timeout "$SECS" python3 tools/decomp-permuter/permuter.py "permute/$fn" -j4 --stop-on-zero >"permute/$fn/log.txt" 2>&1
        best=$(grep -o "new best score! ([0-9]*" "permute/$fn/log.txt" | tail -1 | grep -o "[0-9]*$")
        base=$(grep -o "base score = [0-9]*" "permute/$fn/log.txt" | head -1 | grep -o "[0-9]*$")
        echo "$f $fn base=${base:-?} best=${best:-none}" >> permute/summary.txt
    done
done
