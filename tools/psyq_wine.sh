#!/bin/sh
# Run an original Psy-Q tool (ccpsx, cc1psx, aspsx, psylink, psylib ...) headlessly
# under wine with a private prefix, so it never touches another wine session.
#
#   PSYQ_SDK=/home/snake/dev/psyq_sdk/psyq_4.3 tools/psyq_wine.sh ccpsx -O2 -G8 -c f.c -of.obj
#
# Env:
#   PSYQ_SDK         SDK root with bin/ lib/ include/ (default psyq_4.3, which the exe uses)
#   PSYQ_BIN         override the directory the tool .exe is taken from (e.g. an aspsx/2.81 dir)
#   PSYQ_WINEPREFIX  wine prefix to use (default build/wineprefix in the repo; created
#                    on first use, ~10 s).  Shut down only that prefix's server with
#                    WINEPREFIX=<prefix> wineserver -k
# Unix paths work as arguments (wine maps / to Z:).
#
# Safe to run many at once (ninja -j): the prefix is created once under a lock
# (<prefix>.lock, flock) and psyq.ini is replaced atomically.  ccpsx itself is NOT
# safe to run in parallel (fixed temp file C:/users/<u>/Temp/PQ2); tools/psyq_cc.py
# avoids it by running cpppsx (ccpsx -E -o) and cc1psx separately.
set -e
tool=$1
shift
here=$(cd "$(dirname "$0")/.." && pwd -P)
sdk=${PSYQ_SDK:-/home/snake/dev/psyq_sdk/psyq_4.3}
bin=${PSYQ_BIN:-$sdk/bin}
export WINEPREFIX="${PSYQ_WINEPREFIX:-$here/build/wineprefix}"
export WINEDEBUG=-all
export WINEDLLOVERRIDES="mscoree,mshtml="
unset DISPLAY WAYLAND_DISPLAY
mkdir -p "$WINEPREFIX"

# First use: create the prefix once, even when several jobs start together.
if [ ! -f "$WINEPREFIX/.psyq_ready" ]; then
    (
        flock 9
        if [ ! -f "$WINEPREFIX/.psyq_ready" ]; then
            wineboot -i > "$WINEPREFIX/wineboot.log" 2>&1 || true
            wineserver -w
            [ -f "$WINEPREFIX/system.reg" ] || { echo "psyq_wine: could not create $WINEPREFIX" >&2; exit 1; }
            touch "$WINEPREFIX/.psyq_ready"
        fi
    ) 9> "$WINEPREFIX.lock"
fi

winpath() { echo "Z:$1" | tr / '\\'; }
winsdk=$(winpath "$sdk")

# ccpsx refuses to run without a psyq.ini, which it finds through PSYQ_PATH.  Write one
# with absolute paths into the prefix instead of into the (read-only) SDK tree; replace
# it atomically, as another job may be reading it.
inidir="$WINEPREFIX/psyq_ini/$(basename "$sdk")"
mkdir -p "$inidir"
ini=$(mktemp "$inidir/psyq.ini.XXXXXX")
cat > "$ini" <<EOF
[ccpsx]
stdlib=libgs.lib libgte.lib libgpu.lib libspu.lib libsnd.lib libetc.lib libapi.lib libsn.lib libc.lib libcd.lib libcard.lib
compiler_path=$winsdk\\bin
assembler_path=$winsdk\\bin
linker_path=$winsdk\\bin
library_path=$winsdk\\lib
c_include_path=$winsdk\\include
cplus_include_path=$winsdk\\include
EOF
if cmp -s "$ini" "$inidir/psyq.ini"; then rm -f "$ini"; else mv -f "$ini" "$inidir/psyq.ini"; fi
PSYQ_PATH=$(winpath "$inidir")
export PSYQ_PATH
exec wine "$bin/$tool.exe" "$@"
