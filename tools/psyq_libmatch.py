#!/usr/bin/env python3
"""Identify which Psy-Q SDK library objects are linked into a binary.

Every module's .text from each SDK's lib/*.lib is searched for in the binary
(word aligned) with relocated fields masked out.  For each hit the versions
whose copy of that module also matches at that address are listed, so you can
tell per object which SDK release(s) it is consistent with.

usage: psyq_libmatch.py BIN FILE_OFS_START FILE_OFS_END VRAM_OF_START [SDK_DIR ...]
  e.g. psyq_libmatch.py orig/scus_942.40 0xCE108 0xED0E4 0x800DD908
SDK_DIR defaults to /home/snake/dev/psyq_sdk/psyq_4.{3,4,5}
"""
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psyq_obj import read_lib  # noqa: E402


def masks_for(sec):
    n = len(sec.data) // 4
    m = [0xFFFFFFFF] * n
    for off, t, _ in sec.relocs:
        i = off // 4
        if i >= n:
            continue
        if t == 0x10:            # 32-bit word
            m[i] = 0
        elif t == 0x4A:          # jal/j 26-bit target
            m[i] &= 0xFC000000
        else:                    # hi16 / lo16 / gprel16 ...
            m[i] &= 0xFFFF0000
    return m


def load_sdk(d):
    out = {}                     # (lib, module) -> (words, masks)
    libdir = os.path.join(d, "lib") if os.path.isdir(os.path.join(d, "lib")) else d
    for f in sorted(glob.glob(os.path.join(libdir, "*.lib")) + glob.glob(os.path.join(libdir, "*.obj"))):
        lib = os.path.splitext(os.path.basename(f))[0].lower()
        for m in read_lib(f):
            t = m.obj.section_named(".text")
            if not t or len(t.data) < 8:
                continue
            w = list(struct.unpack("<%dI" % (len(t.data) // 4), t.data[:len(t.data) // 4 * 4]))
            out[(lib, m.name)] = (w, masks_for(t), [x[3] for x in m.obj.xdefs])
    return out


def find_all(binw, words, masks, lo, hi):
    """Return word indices in [lo,hi) where words match binw under masks."""
    n = len(words)
    k = next((i for i, mm in enumerate(masks) if mm == 0xFFFFFFFF), None)
    hits = []
    if k is None:
        return hits
    anchor = words[k]
    for j in range(lo + k, hi - n + k + 1):
        if binw[j] != anchor:
            continue
        s = j - k
        if all((binw[s + i] & masks[i]) == (words[i] & masks[i]) for i in range(n)):
            hits.append(s)
    return hits


def main():
    a = sys.argv[1:]
    if len(a) < 4:
        print(__doc__)
        sys.exit(1)
    path, start, end, vram = a[0], int(a[1], 0), int(a[2], 0), int(a[3], 0)
    sdks = a[4:] or ["/home/snake/dev/psyq_sdk/psyq_%s" % v for v in ("4.3", "4.4", "4.5")]
    data = open(path, "rb").read()
    base = start & ~3
    seg = data[base:end]
    binw = list(struct.unpack("<%dI" % (len(seg) // 4), seg[:len(seg) // 4 * 4]))
    vers = [os.path.basename(d.rstrip("/")) for d in sdks]
    sdkmods = [load_sdk(d) for d in sdks]

    hits = {}                    # word index -> {(lib,mod): (size, set(vers), exports)}
    for vi, mods in enumerate(sdkmods):
        for key, (w, m, ex) in mods.items():
            for s in find_all(binw, w, m, 0, len(binw)):
                e = hits.setdefault(s, {}).setdefault(key, [len(w) * 4, set(), ex])
                e[1].add(vers[vi])

    covered = 0
    pos = 0
    allv = set(vers)
    for s in sorted(hits):
        addr = vram + (base - start) + s * 4
        # prefer the longest module found at this address
        items = sorted(hits[s].items(), key=lambda kv: -kv[1][0])
        (lib, mod), (size, vs, ex) = items[0]
        if s * 4 < pos:
            continue             # nested inside an earlier, longer match
        if s * 4 > pos:
            print("  -- gap %08x..%08x (0x%x bytes unidentified)" % (vram + pos, addr, s * 4 - pos))
        tag = "all" if vs == allv else ",".join(sorted(vs))
        also = ""
        if len(items) > 1:
            also = "  (also: %s)" % " ".join("%s/%s" % k for k, _ in items[1:4])
        print("%08x %5x %-8s %-9s [%s] %s%s" % (addr, size, lib, mod, tag, " ".join(ex[:4]), also))
        covered += size
        pos = s * 4 + size
    if pos < len(binw) * 4:
        print("  -- gap %08x..%08x" % (vram + pos, vram + len(binw) * 4))
    print("covered 0x%x of 0x%x bytes" % (covered, len(binw) * 4))


if __name__ == "__main__":
    main()
