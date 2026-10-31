#!/usr/bin/env python3
"""Write the .rdata/.text subsegments of config/scus_942.40.yaml from config/tu_main.txt.

Each TU's .rdata starts at its `rodata=` address, or else at the lowest .rdata label
referenced by its functions. Cross-TU .rdata references are reported, since they
usually mean a misplaced boundary.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from map2syms import Section, code_refs  # noqa: E402
from tu_analyze import functions  # noqa: E402

BASE = 0x80010000
RDATA_END = 0x80014408
TEXT_END = 0x800FC8E4
ROM = lambda a: a - BASE + 0x800  # noqa: E731
YAML = Path("config/scus_942.40.yaml")

# Data placed in .text by hand-written asm, as (address, symbol); each becomes a textbin.
TEXT_ISLANDS = [
    (0x80076A98, "gLOSFace"), (0x80076C14, "gTestHit"), (0x80076C18, "gTestRay"),
    (0x800C289C, "gEmergencyRA"), (0x800CD2F8, "gSortInfo"),
    (0x800CDAAC, "lowColor"), (0x800CDAEC, "gFlashlightShift"), (0x800CDAF0, "gFlashlightIntensity"),
    (0x800CDAF4, "gFlashIntensity"), (0x800CDAF8, "gAmbientLight"), (0x800CDAFC, "gCollidePolys"),
    (0x800D0970, "gTotal"), (0x800D0974, "gIn"),
]
ISLAND_END = {0x80076C1C, 0x800C28A0, 0x800CD2FC, 0x800CDB00, 0x800D0978}


def parse_tus():
    tus = []
    for line in Path("config/tu_main.txt").read_text().splitlines():
        line = line.split("#")[0].split()
        if not line:
            continue
        addr, name, kind = int(line[0], 16), line[1], line[2]
        rod = None
        for opt in line[3:]:
            k, _, v = opt.partition("=")
            if k == "rodata":
                rod = "none" if v == "none" else int(v, 16)
        tus.append(dict(addr=addr, name=name, kind=kind, rodata=rod))
    for a, b in zip(tus, tus[1:]):
        a["end"] = b["addr"]
    tus[-1]["end"] = TEXT_END
    return tus


def rdata_labels():
    text = "\n".join(p.read_text() for p in Path("asm/main").rglob("*.s"))
    labels = {int(m.group(1), 16) for m in re.finditer(r"dlabel \w*?_?([0-9A-F]{8})\b", text)}
    return {a for a in labels if BASE <= a < RDATA_END}


def main():
    data = Path("orig/scus_942.40").read_bytes()[0x800:]
    img = (BASE, data)
    tus = parse_tus()
    funcs = functions()
    labels = rdata_labels()
    starts = {a for _, a in funcs}

    for tu in tus:
        if tu["addr"] not in starts and tu["kind"] != "textbin":
            print(f"warning: {tu['name']} start {tu['addr']:08X} is not a function", file=sys.stderr)
        refs = set()
        for _, a in funcs:
            if tu["addr"] <= a < tu["end"]:
                nxt = min((b for b in starts if b > a), default=tu["end"])
                refs |= {r for r in code_refs(img, Section(a, nxt, "text", ".text")) if r in labels}
        if tu["name"] != "main":
            refs.discard(BASE)  # `lui 0x8001` noise
        tu["refs"] = refs

    # choose .rdata starts (explicit, else lowest reference), then check monotonicity
    last = BASE - 1
    for tu in tus:
        if tu["rodata"] is None and tu["refs"]:
            cand = [r for r in tu["refs"] if r > last]
            tu["rodata"] = min(cand) if cand else "none"
        if isinstance(tu["rodata"], int):
            if tu["rodata"] <= last:
                print(f"error: {tu['name']} rodata {tu['rodata']:08X} not after {last:08X}", file=sys.stderr)
            last = tu["rodata"]
    ro = [t for t in tus if isinstance(t["rodata"], int)]
    for a, b in zip(ro, ro[1:]):
        a["rodata_end"] = b["rodata"]
    if ro:
        ro[-1]["rodata_end"] = RDATA_END
        assert ro[0]["rodata"] == BASE, "first TU must own the start of .rdata"
    for t in ro:
        bad = sorted(r for r in t["refs"] if not t["rodata"] <= r < t["rodata_end"])
        if bad:
            print(f"note: {t['name']} references .rdata outside its block: "
                  + " ".join(f"{r:08X}" for r in bad[:6]), file=sys.stderr)

    lines = []
    for t in ro:
        kind = ".rodata" if t["kind"] == "c" else "rodata"
        lines.append(f"      - [0x{ROM(t['rodata']):X}, {kind}, {t['name']}]")
    for t in tus:
        if t["kind"] == "textbin":
            lines.append(f"      - [0x{ROM(t['addr']):X}, textbin, {t['name']}]")
            continue
        starts_on_island = any(a == t["addr"] for a, _ in TEXT_ISLANDS)
        if not starts_on_island:
            lines.append(f"      - [0x{ROM(t['addr']):X}, {t['kind']}, {t['name']}]")
        for a, sym in TEXT_ISLANDS:
            if t["addr"] <= a < t["end"]:
                assert t["kind"] == "asm", f"data island {sym} inside C file {t['name']}"
                lines.append(f"      - [0x{ROM(a):X}, textbin, {sym}]   # data in .text")
        # asm continues after each run of islands
        for e in sorted(ISLAND_END):
            if t["addr"] < e < t["end"]:
                cont = t["name"] if starts_on_island and e == min(x for x in ISLAND_END if x > t["addr"]) else f"{t['name']}_{e:08X}"
                lines.append(f"      - [0x{ROM(e):X}, asm, {cont}]")
    lines.sort(key=lambda l: int(re.search(r"\[0x([0-9A-F]+)", l).group(1), 16))

    y = YAML.read_text()
    head, rest = y.split("    subsegments:\n", 1)
    _, tail = rest.split("      - [0xED0E4, data, data]\n", 1)
    YAML.write_text(head + "    subsegments:\n" + "\n".join(lines) + "\n      - [0xED0E4, data, data]\n" + tail)
    print(f"{len(tus)} TUs, {len(ro)} with .rdata")


if __name__ == "__main__":
    main()
