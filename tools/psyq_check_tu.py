#!/usr/bin/env python3
"""Compile main-exe TUs with the ORIGINAL Psy-Q ccpsx (via tools/psyq_wine.sh) and
compare their .text against orig/scus_942.40 with relocated fields masked.

usage: tools/psyq_check_tu.py [--keep DIR] [--flags "-O2 -G8"] egshog egstex ...

The source is staged as s/<tu>.c so __FILE__ matches the original ("s/<tu>.c").
-DM2CTX is passed so INCLUDE_ASM/SDATA_LOCAL (GAS-only constructs) compile away;
TUs that still contain INCLUDE_ASM functions will therefore come out short.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
from psyq_obj import read_lib  # noqa: E402
from psyq_libmatch import masks_for  # noqa: E402

EXE_DELTA = 0x8000F800      # vram - file offset for scus_942.40


def tu_range(name):
    """(file offset start, end) of the c segment <name> in the main yaml."""
    lines = open(os.path.join(ROOT, "config/scus_942.40.yaml")).read().splitlines()
    for i, l in enumerate(lines):
        m = re.match(r"\s*- \[(0x[0-9A-Fa-f]+), c, %s\]" % re.escape(name), l)
        if m:
            nxt = re.match(r"\s*- \[(0x[0-9A-Fa-f]+),", lines[i + 1])
            return int(m.group(1), 16), int(nxt.group(1), 16)
    raise SystemExit("no c segment %s" % name)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tus", nargs="+")
    ap.add_argument("--flags", default="-O2 -G8")
    ap.add_argument("--keep", help="directory for the .obj files (default: a temp dir)")
    args = ap.parse_args()
    work = args.keep or tempfile.mkdtemp(prefix="psyqtu")
    os.makedirs(work, exist_ok=True)
    if not os.path.exists(os.path.join(work, "s")):
        os.symlink(os.path.join(ROOT, "src/main"), os.path.join(work, "s"))
    orig = open(os.path.join(ROOT, "orig/scus_942.40"), "rb").read()
    bad_total = 0
    for tu in args.tus:
        obj = os.path.join(work, tu + ".obj")
        cmd = [os.path.join(TOOLS, "psyq_wine.sh"), "ccpsx"] + args.flags.split() + [
            "-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "src"),
            "-DM2CTX", "-c", "s/%s.c" % tu, "-o" + obj]
        if os.path.exists(obj):
            os.unlink(obj)
        r = subprocess.run(cmd, cwd=work, capture_output=True, text=True)
        if not os.path.exists(obj):
            print("%-10s COMPILE FAILED\n%s%s" % (tu, r.stdout[-2000:], r.stderr[-2000:]))
            bad_total += 1
            continue
        t = read_lib(obj)[0].obj.section_named(".text")
        start, end = tu_range(tu)
        w = struct.unpack("<%dI" % (len(t.data) // 4), t.data)
        e = struct.unpack("<%dI" % ((end - start) // 4), orig[start:end])
        m = masks_for(t)
        diffs = [i for i in range(min(len(w), len(e))) if (w[i] & m[i]) != (e[i] & m[i])]
        status = "OK" if not diffs and len(w) == len(e) else "DIFF"
        print("%-10s %-4s text 0x%x vs orig 0x%x, %d differing words%s" % (
            tu, status, len(t.data), end - start, len(diffs),
            "" if not diffs else " (first @0x%x = %08x)" % (start + 4 * diffs[0] + EXE_DELTA, e[diffs[0]])))
        bad_total += status != "OK"
    sys.exit(1 if bad_total else 0)


if __name__ == "__main__":
    main()
