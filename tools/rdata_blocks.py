#!/usr/bin/env python3
"""Cut main .rdata into per-TU blocks and list which functions reference each block.

Block starts: the per-TU `{1,1,1,0}` header constant and the `s/<file>.c` strings
(the latter only matter where no marker precedes them). Each function is assigned to
the block holding most of its .rdata references.
"""
import re
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from map2syms import Section, code_refs  # noqa: E402
from tu_analyze import functions  # noqa: E402

BASE = 0x80010000
RDATA_END = 0x80014408
TEXT_END = 0x800DD000  # start of Psy-Q libraries (approx.)
MARKER = struct.pack("<4I", 1, 1, 1, 0)


def main():
    data = Path("orig/scus_942.40").read_bytes()[0x800:]
    rd = data[: RDATA_END - BASE]
    files = {BASE + m.start(): m.group(1).decode() for m in re.finditer(rb"s/([a-z0-9_]+)\.c\0", rd)}
    markers = [BASE + m.start() for m in re.finditer(re.escape(MARKER), rd) if m.start() % 4 == 0]
    extra = [int(a, 16) for a in sys.argv[1:] if not a.startswith("-")]
    starts = sorted(set([BASE] + markers + extra))
    labels = {
        int(m.group(1), 16)
        for m in re.finditer(r"label \w*?_?([0-9A-F]{8})\b", "\n".join(
            p.read_text() for p in Path("asm/main").rglob("*.s") if "rodata" in p.name or "nonmatchings" in str(p)))
        if RDATA_END > int(m.group(1), 16) >= BASE
    }
    labels |= set(files)

    def block(addr):
        i = 0
        while i + 1 < len(starts) and starts[i + 1] <= addr:
            i += 1
        return i

    funcs = functions()
    ends = [f[1] for f in funcs[1:]] + [TEXT_END]
    per = {}
    for (name, start), end in zip(funcs, ends):
        if start >= TEXT_END:
            break
        refs = [r for r in code_refs((BASE, data), Section(start, end, "text", ".text")) if r in labels]
        if start > 0x800148A0:
            refs = [r for r in refs if r != BASE]  # lui 0x8001 noise
        if not refs:
            continue
        b = Counter(block(r) for r in refs).most_common(1)[0][0]
        per.setdefault(b, []).append((start, name))

    # keep the longest chain of functions whose block index never decreases
    seq = sorted((a, n, b) for b, fl in per.items() for a, n in fl)
    best, prev = [1] * len(seq), [-1] * len(seq)
    for i in range(len(seq)):
        for j in range(i):
            if seq[j][2] <= seq[i][2] and best[j] + 1 > best[i]:
                best[i], prev[i] = best[j] + 1, j
    i = max(range(len(seq)), key=lambda k: best[k])
    keep = []
    while i >= 0:
        keep.append(seq[i])
        i = prev[i]
    dropped = sorted(set(seq) - set(keep))
    per = {}
    for a, n, b in sorted(keep):
        per.setdefault(b, []).append((a, n))
    if "-v" in sys.argv:
        for a, n, b in dropped:
            print(f"# outlier {a:08X} {n} -> block {starts[b]:08X}")

    if "--funcs" in sys.argv:
        keepset = {(a, n): b for a, n, b in keep}
        for (name, start), end in zip(funcs, ends):
            if start >= TEXT_END:
                break
            b = keepset.get((start, name))
            print(f"{start:08X} {end-start:5X} {name:45s} {'' if b is None else f'{starts[b]:08X}'}")
        return

    for i, s in enumerate(starts):
        e = starts[i + 1] if i + 1 < len(starts) else RDATA_END
        fs = [files[a] for a in files if s <= a < e]
        fn = per.get(i, [])
        span = f"{fn[0][0]:08X}..{fn[-1][0]:08X} {fn[0][1]} .. {fn[-1][1]} ({len(fn)})" if fn else "-"
        print(f"{s:08X}-{e:08X} {'/'.join(fs) or '?':10s} {span}")


if __name__ == "__main__":
    main()
