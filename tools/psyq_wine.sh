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

winpath() { echo "Z:$1" | tr / '\\'; }
winsdk=$(winpath "$sdk")

# ccpsx refuses to run without a psyq.ini, which it finds through PSYQ_PATH.  Write one
# with absolute paths into the prefix instead of into the (read-only) SDK tree.
inidir="$WINEPREFIX/psyq_ini/$(basename "$sdk")"
mkdir -p "$inidir"
cat > "$inidir/psyq.ini" <<EOF
[ccpsx]
stdlib=libgs.lib libgte.lib libgpu.lib libspu.lib libsnd.lib libetc.lib libapi.lib libsn.lib libc.lib libcd.lib libcard.lib
compiler_path=$winsdk\\bin
assembler_path=$winsdk\\bin
linker_path=$winsdk\\bin
library_path=$winsdk\\lib
c_include_path=$winsdk\\include
cplus_include_path=$winsdk\\include
EOF
PSYQ_PATH=$(winpath "$inidir")
export PSYQ_PATH
exec wine "$bin/$tool.exe" "$@"
