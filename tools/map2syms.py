#!/usr/bin/env python3
"""Parse the Psy-Q linker map (siphon.map) into per-module splat symbol files.

Overlays share address ranges, so each map symbol is assigned to the module(s)
whose sections contain it. Text symbols in overlapping overlays are resolved by
checking for a function boundary (preceding `jr $ra` + delay slot) in each
candidate overlay image.
"""
import argparse
import re
import struct
from collections import defaultdict
from pathlib import Path

LINKER_SYM = re.compile(r"^_+\w*?_(org|obj|orgend|objend|size)$|^__(c|d)tors")
JR_RA = 0x03E00008


class Section:
    def __init__(self, start, end, group, name):
        self.start, self.end, self.group, self.name = start, end, group, name

    def contains(self, addr):
        return self.start <= addr < self.end

    @property
    def kind(self):
        n = self.name.split(".")[-1].lstrip("$")
        if self.name.startswith("$"):
            return "header"
        if self.group == "load":
            return "hog"
        return n


def parse_map(path):
    sections, symbols = [], []
    mode = None
    for line in Path(path).read_text().splitlines():
        if "Section name" in line:
            mode = "sec"
            continue
        if "Names in address order" in line:
            mode = "sym"
            continue
        if "Names alphabetically" in line or "Program entry" in line:
            mode = None if "Program" in line else "skip"
            continue
        p = line.split()
        if mode == "sec" and len(p) == 6:
            start, end = int(p[0], 16), int(p[1], 16) + 1
            sections.append(Section(start, end, p[4], p[5]))
        elif mode == "sym" and len(p) == 2:
            symbols.append((int(p[0], 16), p[1]))
    return sections, symbols


def module_of(sec):
    return "main" if sec.group in ("text", "bss", "load") else sec.group


def load_images(orig):
    imgs = {}
    exe = (orig / "scus_942.40").read_bytes()
    imgs["main"] = (0x80010000, exe[0x800:])
    return imgs


def word_at(img, addr):
    base, data = img
    off = addr - base
    if off < 0 or off + 4 > len(data):
        return None
    return struct.unpack_from("<I", data, off)[0]


def code_refs(img, text):
    """Addresses referenced by jal or lui/lo16 pairs in a module's text section."""
    base, data = img
    refs = set()
    hi = {}
    for addr in range(text.start, min(text.end, base + len(data)), 4):
        w = word_at(img, addr)
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        imm = w & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 3:
            refs.add((addr & 0xF0000000) | ((w & 0x3FFFFFF) << 2))
        elif op == 0x0F:
            hi[rt] = imm << 16
        elif rs in hi and op in (0x09, 0x0D, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x29, 0x2B):
            lo = imm if op == 0x0D else simm
            refs.add((hi[rs] + lo) & 0xFFFFFFFF)
    return refs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default="orig/siphon.map")
    ap.add_argument("--orig", default="orig")
    ap.add_argument("--out", default="config/symbols")
    args = ap.parse_args()

    sections, symbols = parse_map(args.map)
    orig = Path(args.orig)
    imgs = load_images(orig)
    groups = defaultdict(list)
    for s in sections:
        groups[module_of(s)].append(s)
    for g, secs in groups.items():
        if g == "main":
            continue
        start = min(s.start for s in secs)
        imgs[g] = (start, (orig / "bin" / f"{g}.ovl").read_bytes())

    refs = {}
    for g, secs in groups.items():
        refs[g] = set()
        for s in secs:
            if s.kind == "text":
                refs[g] |= code_refs(imgs[g], s)
            elif s.kind in ("data", "rdata", "sdata"):
                refs[g] |= {word_at(imgs[g], a) for a in range(s.start, s.end - 3, 4)}

    # hand-resolved ambiguous symbols: "ADDR name module"
    overrides = {}
    ovr_path = Path(args.out) / "overrides.txt"
    if ovr_path.exists():
        for line in ovr_path.read_text().splitlines():
            p = line.split("#")[0].split()
            if len(p) == 3:
                overrides[(int(p[0], 16), p[1])] = p[2]

    out = defaultdict(list)
    unresolved = []
    for addr, name in symbols:
        if LINKER_SYM.match(name) or addr < 0x80000000:
            continue
        cands = [s for s in sections if s.contains(addr)]
        if not cands:
            # end-of-section labels such as `_end`
            unresolved.append((addr, name, "none"))
            continue
        mods = {module_of(s): s for s in cands}
        if len(mods) > 1:
            texts = {m: s for m, s in mods.items() if s.kind == "text"}
            good = [
                m
                for m, s in texts.items()
                if addr == s.start or word_at(imgs[m], addr - 8) == JR_RA
            ]
            if len(good) == 1:
                mods = {good[0]: mods[good[0]]}
            else:
                users = [m for m in mods if addr in refs[m]]
                if len(users) == 1:
                    mods = {users[0]: mods[users[0]]}
                else:
                    # a data symbol can't sit mid-function in another overlay
                    plausible = [m for m in mods if m not in texts or m in good]
                    if len(plausible) == 1:
                        mods = {plausible[0]: mods[plausible[0]]}
        if len(mods) > 1 and (addr, name) in overrides:
            m = overrides[(addr, name)]
            mods = {m: mods[m]}
        if len(mods) > 1:
            unresolved.append((addr, name, ",".join(sorted(mods))))
            continue
        (mod, sec), = mods.items()
        kind = sec.kind
        attrs = ""
        if kind == "text":
            attrs = " // type:func"
        out[mod].append((addr, name, attrs))

    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)
    for mod, syms in sorted(out.items()):
        with open(outdir / f"{mod}.txt", "w") as f:
            for addr, name, attrs in sorted(syms):
                f.write(f"{name} = 0x{addr:08X};{attrs}\n")
        print(f"{mod:10s} {len(syms)}")
    with open(outdir / "unresolved.txt", "w") as f:
        for addr, name, why in unresolved:
            f.write(f"{addr:08X} {name} {why}\n")
    print("unresolved", len(unresolved))


if __name__ == "__main__":
    main()
