#!/bin/sh
# Link the game with the original psylink 2.73 from the command file written by
# tools/gen_psylink.py, convert the CPE to a PS-X EXE and compare everything with
# the originals.  See docs/psyq_build.md.
#
#   tools/gen_psylink.py && tools/psyq_link.sh [OUTDIR]     (default build/psyq)
#
# Uses tools/psyq_wine.sh (honours PSYQ_WINEPREFIX).  Exit status 0 = all identical.
set -e
here=$(cd "$(dirname "$0")/.." && pwd -P)
out=${1:-$here/build/psyq}
cd "$out"
rm -f ./*.ovl siphon.cpe siphon.exe siphon.map siphon.sym
"$here/tools/psyq_wine.sh" psylink /c /m @siphon.lnk,siphon.cpe,siphon.sym,siphon.map > psylink.log 2>&1 || true
if ! grep -q "^0 error(s)" psylink.log; then
    grep -i -m 40 "error" psylink.log
    exit 1
fi
# cpe2exe in the 4.3 SDK bin is CPE2PSX v2.0 (Gemini/J.Ingram); -CJ gives the
# "for Japan area" licence string the original header carries
"$here/tools/psyq_wine.sh" cpe2exe -CJ siphon.cpe > cpe2exe.log 2>&1
bad=0
cmp -s siphon.exe "$here/orig/scus_942.40" && echo "OK    scus_942.40" || { echo "DIFF  scus_942.40"; bad=1; }
for f in "$here"/orig/bin/*.ovl; do
    n=$(basename "$f")
    if cmp -s "$n" "$f"; then echo "OK    $n"; else echo "DIFF  $n: $(cmp "$n" "$f" 2>&1 | head -1)"; bad=1; fi
done
exit $bad
