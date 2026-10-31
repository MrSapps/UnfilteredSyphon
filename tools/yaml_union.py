#!/usr/bin/env python3
"""Resolve git conflict hunks in config/scus_942.40.yaml from parallel data migrations.

    tools/yaml_union.py conflicted.yaml > out.yaml

Each hunk holds subsegment lines from two branches. They are unioned and sorted by
address (rom offset for `[0x.., type, name]`, vram for `{ ... vram: 0x.. }`). When both
sides have an entry at the same address, a TU's dotted entry (`.data`, `.sbss`, ...)
wins over a blob piece. A `start:` key on the first line of a section is kept.
"""
import re
import sys

HUNK = re.compile(r"<<<<<<< [^\n]*\n(.*?)=======\n(.*?)>>>>>>> [^\n]*\n", re.S)


def key(line):
    m = re.search(r"vram:\s*(0x[0-9A-Fa-f]+)", line) or re.search(r"\[\s*(0x[0-9A-Fa-f]+)", line)
    return int(m.group(1), 16) if m else None


def dotted(line):
    return bool(re.search(r"type:\s*\.|,\s*\.\w+,", line))


def resolve(m):
    lines = [l for l in (m.group(1) + m.group(2)).splitlines(keepends=True) if l.strip()]
    starts = [l for l in lines if "start:" in l]
    by_addr = {}
    for l in lines:
        k = key(l)
        if k is None:
            sys.exit(f"unparsable line: {l!r}")
        cur = by_addr.get(k)
        if cur is None or (dotted(l) and not dotted(cur)):
            by_addr[k] = l
    out = [by_addr[k] for k in sorted(by_addr)]
    # keep `start:` on whichever entry now comes first
    if starts:
        start = re.search(r"start:\s*0x[0-9A-Fa-f]+", starts[0]).group(0)
        out = [re.sub(r"\s*start:\s*0x[0-9A-Fa-f]+,", "", l) for l in out]
        out[0] = out[0].replace("{ ", "{ " + start + ", ", 1)
    return "".join(out)


text = open(sys.argv[1]).read()
sys.stdout.write(HUNK.sub(resolve, text))
