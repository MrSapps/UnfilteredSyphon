#!/usr/bin/env python3
"""Tag splat's nonmatching asm so each file can be included in two passes.

cc1 2.7.2 (-G8) writes data straight out but buffers function bodies until the end
of the file; jump tables travel with the function bodies, and constants addressed
from static initializers are deferred to the very end. So one object's .rodata is:

    [consts + strings, source order][jump tables + deferred consts]

Everything in a TU's .rodata before its first jump table is pass 1, the rest pass 2.
INCLUDE_ASM includes each file twice (see include/common.h):

  INC_PART == 1  at file scope   -> data-stream rodata
  INC_PART == 2  inside a dummy  -> code, jump tables, deferred rodata
"""
import re
import sys
from pathlib import Path

TAG = "# split_parts\n"
LABEL = re.compile(r"(?:dlabel|jlabel|glabel)\s+\w*?_?([0-9A-F]{8})\b")


def blocks_of(text):
    header, blocks, cur = [], [], None
    for line in text.splitlines(keepends=True):
        if line.strip().startswith(".section"):
            cur = [line]
            blocks.append(cur)
        elif cur is None:
            header.append(line)
        else:
            cur.append(line)
    return header, blocks


def block_addr(block):
    for line in block:
        m = LABEL.search(line)
        if m:
            return int(m.group(1), 16)
    return None


def transform(text, first_jtbl):
    header, blocks = blocks_of(text)
    part1, part2 = [], []
    for b in blocks:
        sect = b[0].split()[1].rstrip(",").strip('"')
        addr = block_addr(b)
        data_stream = sect in (".rodata", ".sdata", ".data") and (
            first_jtbl is None or addr is None or addr < first_jtbl
        )
        (part1 if data_stream else part2).extend(b)
    # text before any .section directive is code (function files often start with it)
    part2 = header + part2
    out = [TAG]
    out += [".if INC_PART == 1\n"] + part1 + [".endif\n"]
    out += [".if INC_PART == 2\n"] + part2 + [".endif\n"]
    return "".join(out)


# Once a function is C, splat stops emitting its .s and with it the jump table,
# but the table still sits in the TU's rodata. Every table ever seen is kept here.
JTBLS = Path(__file__).parent.parent / "config/jtbls.txt"


def load_jtbls():
    known = set()
    if JTBLS.exists():
        for line in JTBLS.read_text().splitlines():
            if line and not line.startswith("#"):
                known.add(tuple(line.split()))
    return known


def main():
    n = 0
    known = load_jtbls()
    seen = set(known)
    for root in sys.argv[1:] or ["asm"]:
        mod = Path(root).name
        for tu in Path(root).glob("nonmatchings/*"):
            texts = {p: p.read_text() for p in tu.glob("*.s")}
            for t in texts.values():
                for m in re.findall(r"dlabel jtbl_([0-9A-F]{8})", t):
                    seen.add((mod, tu.name, m))
            files = [p for p in texts if not texts[p].startswith(TAG)]
            if not files:
                continue
            jtbls = [int(a, 16) for m, t, a in seen if m == mod and t == tu.name]
            first = min(jtbls) if jtbls else None
            for p in files:
                p.write_text(transform(texts[p], first))
                n += 1
    if seen != known:
        JTBLS.write_text(JTBLS.read_text().split("\n", 1)[0] + "\n"
                         + "".join(" ".join(k) + "\n" for k in sorted(seen)))
    print(f"split_parts: tagged {n} files")


if __name__ == "__main__":
    main()
