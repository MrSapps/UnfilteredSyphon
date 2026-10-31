#!/usr/bin/env python3
"""Work out which Psy-Q library modules are linked into the main exe and in what
order psylink placed them, and write config/psyq_libs.txt for tools/gen_psylink.py.

    tools/psyq_liborder.py [-o config/psyq_libs.txt]

psylink appends library modules in the order it resolves them and lays out every
section of a module in that same order.  A module's position is found from the
symbols it exports: orig/siphon.map gives their addresses, so address - offset is
where the module's section starts.  Modules are then ordered by .text address, and
modules without code (tables such as libgte CSTBL, libsn SNDEF) by their .data
address relative to the code modules' data.  Each placement is checked against the
exe bytes (relocated fields masked).

Libraries: the 4.3 SDK, with libcd from the 4.3.1 patch (psyq_4.4/lib/patches).
Only the main exe's range is considered (the movie overlay's libpress/libmcrd/libcard
modules are separate).
"""
import argparse
import glob
import os
import re
import struct
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
from psyq_obj import read_lib  # noqa: E402
from psyq_libmatch import masks_for  # noqa: E402

SDK = "/home/snake/dev/psyq_sdk/psyq_4.3/lib"
PATCHES = {"libcd": "/home/snake/dev/psyq_sdk/psyq_4.4/lib/patches/libcd.lib"}
EXE_DELTA = 0x8000F800
MAIN = (0x80010000, 0x801377E0)          # main exe incl. bss
FILE_END = 0x8010FF68                    # end of loaded data (.sdata end)
TEXT = (0x800DD908, 0x800FC8E4)          # library code
DATA_START = 0x80107BA4                  # library .data (libsn SNMAIN's)
# ccpsx's psyq.ini stdlib order (tools/psyq_wine.sh)
STDLIB = ["libgs", "libgte", "libgpu", "libspu", "libsnd", "libetc", "libapi", "libsn", "libc",
          "libcd", "libcard"]
SECTIONS = [".rdata", ".text", ".data", ".sdata", ".sbss", ".bss"]


def lib_paths():
    out = {}
    for f in sorted(glob.glob(os.path.join(SDK, "*.lib"))):
        n = os.path.splitext(os.path.basename(f))[0]
        out[n] = PATCHES.get(n, f)
    return out


def map_symbols():
    out = {}
    for l in open(os.path.join(ROOT, "orig/siphon.map")):
        m = re.match(r"\s([0-9A-F]{8})\s\s(\w+)$", l)
        if m:
            out[m.group(2)] = int(m.group(1), 16)
    return out


def matches(sec, exe, addr):
    if addr + len(sec.data) > FILE_END:
        return False
    n = len(sec.data) // 4
    w = struct.unpack("<%dI" % n, sec.data[:n * 4])
    e = struct.unpack("<%dI" % n, exe[addr - EXE_DELTA:addr - EXE_DELTA + n * 4])
    return all((a & m) == (b & m) for a, b, m in zip(w, e, masks_for(sec)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", default=os.path.join(ROOT, "config/psyq_libs.txt"))
    a = ap.parse_args()
    exe = open(os.path.join(ROOT, "orig/scus_942.40"), "rb").read()
    syms = map_symbols()
    found = {}                   # (lib, module) -> {section: address}
    mods = {}
    for lib, path in lib_paths().items():
        for m in read_lib(path):
            mods[(lib, m.name)] = m
            o = m.obj
            bases = {}
            for _, sid, off, name in o.xdefs:
                if name in syms and MAIN[0] <= syms[name] < MAIN[1]:
                    bases.setdefault(o.sections[sid].name, set()).add(syms[name] - off)
            if not bases:
                continue
            pos = {}
            ok = True
            for sname, bs in bases.items():
                if len(bs) != 1:
                    ok = False
                    break
                pos[sname] = bs.pop()
                sec = o.section_named(sname)
                if sec.data and not matches(sec, exe, pos[sname]):
                    ok = False
            if ok:
                found[(lib, m.name)] = pos
            else:
                print("note: %s/%s exports map symbols but does not fit" % (lib, m.name), file=sys.stderr)
    # the same module in two libraries (libc/libc2 MEMMOVE, libapi/libcard C112): keep
    # the one psylink finds first, in the ccpsx stdlib search order
    by_addr = {}
    for k, p in found.items():
        if ".text" in p:
            by_addr.setdefault(p[".text"], []).append(k)
    for ks in by_addr.values():
        ks.sort(key=lambda k: STDLIB.index(k[0]) if k[0] in STDLIB else len(STDLIB))
        for k in ks[1:]:
            del found[k]
    # code of modules none of whose code symbols are in the map: unique masked match
    # in the library code range, outside the code already placed
    taken = sorted((p[".text"], p[".text"] + len(mods[k].obj.section_named(".text").data))
                   for k, p in found.items() if ".text" in p)
    for k in list(found):
        t = mods[k].obj.section_named(".text")
        if ".text" in found[k] or t is None or len(t.data) < 8:
            continue
        hits = [h for h in range(TEXT[0], TEXT[1] - len(t.data) + 1, 4)
                if not any(lo < h + len(t.data) and h < hi for lo, hi in taken)
                and matches(t, exe, h)]
        if len(hits) == 1:
            found[k][".text"] = hits[0]
            taken.append((hits[0], hits[0] + len(t.data)))
    taken.sort()
    end = TEXT[0]
    for lo, hi in taken:
        if lo != end:
            print("warning: library code gap %08x..%08x" % (end, lo), file=sys.stderr)
        end = max(end, hi)
    # order: by .text address; code-less modules by .data, .bss, ... address,
    # interleaved with the code modules' positions in that section
    code = sorted((p[".text"], k) for k, p in found.items() if ".text" in p)
    seq = [k for _, k in code]
    # code-less modules: walk the library .data in code order (each module's .data
    # rounded to 4) and slot each one in where its .data address comes up
    rest = sorted((p[".data"], k) for k, p in found.items() if ".text" not in p and ".data" in p)
    others = [k for k, p in found.items() if ".text" not in p and ".data" not in p]
    if others:
        print("warning: not placed: %s" % others, file=sys.stderr)

    def dsize(k):
        d = mods[k].obj.section_named(".data")
        return (len(d.data) + 3) & ~3 if d is not None else 0
    out = []
    cursor = DATA_START
    for k in seq:
        while rest and rest[0][0] <= cursor and dsize(k):
            addr, r = rest.pop(0)
            if addr != cursor:
                print("warning: %s/%s .data at %08x, expected %08x" % (r[0], r[1], addr, cursor),
                      file=sys.stderr)
            out.append(r)
            cursor = addr + dsize(r)
        if ".data" in found[k] and dsize(k) and found[k][".data"] != cursor:
            print("warning: %s/%s .data at %08x, walk says %08x" % (k[0], k[1], found[k][".data"], cursor),
                  file=sys.stderr)
            cursor = found[k][".data"]
        out.append(k)
        cursor += dsize(k)
    seq = out + [k for _, k in rest]
    with open(a.o, "w") as f:
        f.write("# Psy-Q library modules of SCUS_942.40 in psylink order (tools/psyq_liborder.py)\n"
                "# lib module  section:address of each section that exports a map symbol\n")
        for k in seq:
            f.write("%s %s  %s\n" % (k[0], k[1], " ".join(
                "%s:%08x" % (s, found[k][s]) for s in SECTIONS if s in found[k])))
    print("wrote %s: %d modules (%d with code)" % (a.o, len(seq), len(code)))


if __name__ == "__main__":
    main()
