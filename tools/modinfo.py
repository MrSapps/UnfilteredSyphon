"""Where a module's built/original images, ELF and symbols live."""
import re
from pathlib import Path


def module(name):
    if name == "main":
        return dict(
            built="build/scus_942.40", orig="orig/scus_942.40", elf="build/scus_942.40.elf",
            base=0x80010000 - 0x800, syms="config/symbols/main.txt",
        )
    yaml = Path(f"config/ovl/{name}.yaml").read_text()
    vram = re.search(r"vram: (0x[0-9A-Fa-f]+)", yaml)
    return dict(
        built=f"build/bin/{name}.ovl", orig=f"orig/bin/{name}.ovl", elf=f"build/bin/{name}.ovl.elf",
        base=int(vram.group(1), 16), syms=f"config/symbols/{name}.txt",
    )


def module_of_tu(tu):
    hits = list(Path("src").glob(f"*/{tu}.c"))
    return hits[0].parent.name if hits else "main"


def symbol_addrs(path):
    out = {}
    if Path(path).exists():
        for line in Path(path).read_text().splitlines():
            m = re.match(r"(\S+) = 0x([0-9A-Fa-f]+);", line)
            if m:
                out[m.group(1)] = int(m.group(2), 16)
    return out
