#!/usr/bin/env python3
"""Compare two psylink maps: section layout and symbol addresses.

    tools/psyq_mapdiff.py [build/psyq/siphon.map [orig/siphon.map]] [--all]

Prints section lines that differ and every symbol whose address differs (or that
only one map has), in address order of the original.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read_map(path):
    secs, syms = [], {}
    for l in open(path):
        m = re.match(r"\s([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8}) ([0-9A-F]{8}) (\S+)\s+(\S+)", l)
        if m:
            secs.append((m.group(5), m.group(6), int(m.group(1), 16), int(m.group(3), 16)))
            continue
        m = re.match(r"\s([0-9A-F]{8})\s\s(\S+)$", l)
        if m:
            syms.setdefault(m.group(2), set()).add(int(m.group(1), 16))
    return secs, syms


def main():
    args = [x for x in sys.argv[1:] if not x.startswith("--")]
    mine = args[0] if args else os.path.join(ROOT, "build/psyq/siphon.map")
    orig = args[1] if len(args) > 1 else os.path.join(ROOT, "orig/siphon.map")
    s1, y1 = read_map(mine)
    s2, y2 = read_map(orig)
    d1 = {(g, n): (a, l) for g, n, a, l in s1}
    for g, n, a, l in s2:
        if d1.get((g, n)) != (a, l):
            got = d1.get((g, n))
            print("section %-8s %-12s orig %08x+%x  here %s" % (
                g, n, a, l, "%08x+%x" % got if got else "missing"))
    for g, n, a, l in s1:
        if not any(g == x and n == y for x, y, _, _ in s2):
            print("section %-8s %-12s only here %08x+%x" % (g, n, a, l))
    bad = []
    for n, addrs in y2.items():
        if n not in y1:
            bad.append((min(addrs), n, "missing here"))
        elif y1[n] != addrs:
            bad.append((min(addrs), n, "here %s" % " ".join("%08x" % x for x in sorted(y1[n]))))
    for n, addrs in y1.items():
        if n not in y2 and "--all" in sys.argv:
            bad.append((min(addrs), n, "only here"))
    for a, n, what in sorted(bad):
        print("%08x %-32s %s" % (a, n, what))
    print("%d symbols differ" % len(bad), file=sys.stderr)


if __name__ == "__main__":
    main()
