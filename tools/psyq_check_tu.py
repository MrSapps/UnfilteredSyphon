#!/usr/bin/env python3
"""Compile main-exe TUs with the ORIGINAL Psy-Q ccpsx (via tools/psyq_wine.sh) and
compare every section of the resulting SN OBJ against orig/scus_942.40.

usage: tools/psyq_check_tu.py [--keep DIR] [--flags "-O2 -G8"] egshog egstex ...

Where each section of a TU lives is taken from config/scus_942.40.yaml: the `c`
subsegment gives .text, and dotted subsegments named after the TU (`.rodata`,
`.data`, `.sdata`, `.sbss`, `.bss`) give the data sections the C object provides
(see docs/data_migration.md).  Contents are compared with relocated fields
masked; sizes are compared after rounding the OBJ section up to 4 (psylink
aligns each object's section).  A section the OBJ has but the yaml does not
place is reported as "unplaced" (its bytes are still in a splat data segment).

The source is staged as s/<tu>.c so __FILE__ matches the original ("s/<tu>.c").
ccpsx defines _PSYQ, which include/common.h uses to turn INCLUDE_ASM into markers;
tools/psyq_cc.py runs ccpsx -S, expands them with tools/psyq_asm.py, then aspsx.
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

import yaml

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
from psyq_obj import read_lib  # noqa: E402
from psyq_libmatch import masks_for  # noqa: E402

EXE_DELTA = 0x8000F800      # vram - file offset for scus_942.40
YAML_TO_SN = {"c": ".text", ".rodata": ".rdata", ".data": ".data", ".sdata": ".sdata",
              ".sbss": ".sbss", ".bss": ".bss"}
SECTIONS = [".rdata", ".text", ".data", ".sdata", ".sbss", ".bss"]


def main_subsegments():
    """[(type, name, vram)] of the main segment, plus a final end marker."""
    cfg = yaml.safe_load(open(os.path.join(ROOT, "config/scus_942.40.yaml")))
    main = next(s for s in cfg["segments"] if isinstance(s, dict) and s.get("name") == "main")
    out = []
    for s in main["subsegments"]:
        if isinstance(s, list):
            start, typ = s[0], s[1]
            name = s[2] if len(s) > 2 else None
            vram = start + EXE_DELTA
        else:
            typ, name = s["type"], s.get("name")
            vram = s["vram"] if "vram" in s else s["start"] + EXE_DELTA
        out.append((typ, name, vram))
    out.append(("end", None, 0x801377DC))   # end of .bss (orig/siphon.map)
    return out


def tu_layout(tu):
    """{SN section name: (vram start, vram end)} for the parts of <tu> its C object provides."""
    subs = main_subsegments()
    lay = {}
    for i, (typ, name, vram) in enumerate(subs[:-1]):
        if name == tu and typ in YAML_TO_SN:
            lay[YAML_TO_SN[typ]] = (vram, subs[i + 1][2])
    return lay


def known_addresses():
    """{name: vram} from orig/siphon.map (globals) and config/symbols."""
    out = {}
    for f in sorted(os.listdir(os.path.join(ROOT, "config/symbols"))):
        for l in open(os.path.join(ROOT, "config/symbols", f)):
            m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+);", l)
            if m:
                out[m.group(1)] = int(m.group(2), 16)
    for l in open(os.path.join(ROOT, "orig/siphon.map")):
        m = re.match(r"\s([0-9A-F]{8})\s\s(\w+)$", l)
        if m:
            out[m.group(2)] = int(m.group(1), 16)
    return out


def check_symbols(o, lay, known):
    """Data symbols whose OBJ position disagrees with their known (or D_<addr>) address."""
    bad = []
    syms = [(sec, off, name) for _, sec, off, name in o.xdefs] + list(o.locals)
    for sec, off, name in syms:
        sname = o.sections[sec].name
        if sname == ".text" or sname not in lay:
            continue
        m = re.match(r"D_([0-9A-F]{8})$", name)
        want = int(m.group(1), 16) if m else known.get(name)
        if want is not None and lay[sname][0] + off != want:
            bad.append("%s at 0x%08X, expected 0x%08X" % (name, lay[sname][0] + off, want))
    return bad


def compare(sec, orig, lo, hi):
    """(status, detail) for one OBJ section against orig bytes [lo, hi)."""
    size = len(sec.data) or sec.bss
    want = hi - lo
    aligned = (size + 3) & ~3
    if aligned != want and size != want:
        return "DIFF", "size 0x%x vs orig 0x%x" % (size, want)
    if not sec.data:
        return "OK", "size 0x%x" % size
    n = len(sec.data) // 4
    off = lo - EXE_DELTA
    w = struct.unpack("<%dI" % n, sec.data[:n * 4])
    e = struct.unpack("<%dI" % n, orig[off:off + n * 4])
    m = masks_for(sec)
    diffs = [i for i in range(n) if (w[i] & m[i]) != (e[i] & m[i])]
    tail = sec.data[n * 4:] != orig[off + n * 4:off + len(sec.data)]
    if diffs or tail:
        first = 4 * diffs[0] if diffs else n * 4
        return "DIFF", "%d differing words (first @0x%x = %08x)" % (
            len(diffs) + tail, lo + first, e[first // 4] if diffs else 0)
    return "OK", "size 0x%x" % size


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("tus", nargs="+")
    ap.add_argument("--flags", default="-O2 -G8")
    ap.add_argument("--keep", help="directory for the .obj files (default: a temp dir)")
    args = ap.parse_args()
    work = os.path.abspath(args.keep or tempfile.mkdtemp(prefix="psyqtu"))
    os.makedirs(work, exist_ok=True)
    if not os.path.exists(os.path.join(work, "s")):
        os.symlink(os.path.join(ROOT, "src/main"), os.path.join(work, "s"))
    orig = open(os.path.join(ROOT, "orig/scus_942.40"), "rb").read()
    known = known_addresses()
    bad_total = 0
    for tu in args.tus:
        obj = os.path.join(work, tu + ".obj")
        cmd = [sys.executable, os.path.join(TOOLS, "psyq_cc.py")] + args.flags.split() + [
            "-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "src"),
            "-c", "s/%s.c" % tu, "-o" + obj]
        if os.path.exists(obj):
            os.unlink(obj)
        r = subprocess.run(cmd, cwd=work, capture_output=True, text=True)
        if not os.path.exists(obj):
            print("%-10s COMPILE FAILED\n%s%s" % (tu, r.stdout[-2000:], r.stderr[-2000:]))
            bad_total += 1
            continue
        o = read_lib(obj)[0].obj
        lay = tu_layout(tu)
        lines, ok = [], True
        for name in SECTIONS:
            sec = o.section_named(name)
            have = sec is not None and (len(sec.data) or sec.bss)
            if name not in lay:
                if have:
                    ok = False
                    lines.append("%-6s unplaced (0x%x bytes, not in the yaml)" % (name, len(sec.data) or sec.bss))
                continue
            lo, hi = lay[name]
            if not have:
                ok = ok and hi == lo
                lines.append("%-6s %s missing (orig 0x%x)" % (name, "OK" if hi == lo else "DIFF", hi - lo))
                continue
            st, detail = compare(sec, orig, lo, hi)
            ok = ok and st == "OK"
            lines.append("%-6s %-4s %s" % (name, st, detail))
        for b in check_symbols(o, lay, known):
            ok = False
            lines.append("symbol " + b)
        commons = sorted(c[3] for c in o.commons)
        print("%-10s %s" % (tu, "OK" if ok else "DIFF"))
        for l in lines:
            print("    " + l)
        if commons:
            print("    commons: " + " ".join(commons))
        bad_total += not ok
    sys.exit(1 if bad_total else 0)


if __name__ == "__main__":
    main()
