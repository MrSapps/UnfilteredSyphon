#!/usr/bin/env python3
"""Generate splat configs for each overlay from the section table in siphon.map."""
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from map2syms import parse_map  # noqa: E402

TEMPLATE = """\
name: {name}.ovl
sha1: {sha1}
options:
  basename: {name}
  target_path: orig/bin/{name}.ovl
  base_path: ../..
  platform: psx
  compiler: PSYQ

  asm_path: asm/{name}
  src_path: src/{name}
  build_path: build
  asset_path: assets/{name}
  ld_script_path: build/{name}.ld
  ld_dependencies: True
  elf_path: build/{name}.elf

  symbol_addrs_path:
    - config/symbols/{name}.txt
    - config/symbols/{name}.manual.txt
    - config/symbols/main.ext.txt
  undefined_funcs_auto_path: config/undefined_funcs_auto.{name}.txt
  undefined_syms_auto_path: config/undefined_syms_auto.{name}.txt

  gp_value: 0x8010F600
  use_legacy_include_asm: False
  migrate_rodata_to_functions: True
  asm_jtbl_label_macro: jlabel
  section_order: [".rodata", ".text", ".data", ".sdata", ".sbss", ".bss"]
  ld_bss_is_noload: True
  ld_align_segment_vram_end: False
  ld_align_section_vram_end: False
  create_asm_dependencies: True
  subalign: 4
  disasm_unknown: True
  string_encoding: ASCII
  data_string_encoding: ASCII
  rodata_string_guesser_level: 2
  data_string_guesser_level: 2
segments:
{segments}
"""


def main():
    sections, _ = parse_map("orig/siphon.map")
    groups = {}
    for s in sections:
        if s.group in ("text", "bss", "load"):
            continue
        groups.setdefault(s.group, []).append(s)

    out = Path("config/ovl")
    out.mkdir(parents=True, exist_ok=True)
    for name, secs in groups.items():
        data = Path(f"orig/bin/{name}.ovl").read_bytes()
        org = min(s.start for s in secs)
        rom = lambda a: a - org  # noqa: E731
        lines = []
        if name == "tcache":
            lines.append(f"  - {{ start: 0x0, type: bin, name: {name}, vram: 0x{org:08X} }}")
            lines.append(f"  - [0x{len(data):X}]")
        else:
            subs = []
            bss = None
            for s in sorted(secs, key=lambda s: s.start):
                k = s.kind
                if k == "header":
                    subs.append(f"      - [0x{rom(s.start):X}, rodatabin, header]")
                elif k == "rdata":
                    subs.append(f"      - [0x{rom(s.start):X}, rodata, rdata]")
                elif k == "text":
                    subs.append(f"      - [0x{rom(s.start):X}, asm, text]")
                elif k == "data":
                    subs.append(f"      - [0x{rom(s.start):X}, data, data]")
                elif k == "bss":
                    bss = s
            end = rom(bss.start) if bss else len(data)
            lines.append(f"  - name: {name}")
            lines.append("    type: code")
            lines.append("    start: 0x0")
            lines.append(f"    vram: 0x{org:08X}")
            if bss:
                lines.append(f"    bss_size: 0x{bss.end - bss.start:X}")
            lines.append("    subsegments:")
            lines += subs
            if bss:
                lines.append(
                    f"      - {{ start: 0x{end:X}, type: bss, vram: 0x{bss.start:08X}, name: bss }}"
                )
            lines.append(f"  - [0x{end:X}]")
        sha1 = hashlib.sha1(data).hexdigest()
        (out / f"{name}.yaml").write_text(
            TEMPLATE.format(name=name, sha1=sha1, segments="\n".join(lines))
        )
        Path(f"config/symbols/{name}.manual.txt").touch()
        Path(f"config/symbols/{name}.txt").touch()
        print(name, hex(org), hex(len(data)))


if __name__ == "__main__":
    main()
