# UnfilteredSyphon

Matching decompilation of the **Syphon Filter Nov 11 1998 prototype** (SCUS-94240):
the main executable `SCUS_942.40` and its overlays in `\BIN\*.OVL`.

## Setup

```sh
tools/setup.sh                      # venv, mipsel binutils, m2c, asm-differ, maspsx (no root)
# copy from the disc: scus_942.40, siphon.map -> orig/ ; bin/*.ovl -> orig/bin/
. .venv/bin/activate
python3 configure.py                # runs splat, writes build.ninja
ninja                               # builds build/scus_942.40 + build/bin/*.ovl, checks SHA1
```

## Layout

- `config/scus_942.40.yaml`, `config/ovl/*.yaml` - splat configs (overlay ones from `tools/gen_ovl_yaml.py`)
- `config/symbols/<module>.txt` - symbols, seeded from the linker map by `tools/map2syms.py`;
  hand-maintained from then on (names, types, sizes). `overrides.txt` resolves symbols that
  are ambiguous between overlays sharing an address range.
- `src/`, `include/` - decompiled C and headers
- `asm/`, `assets/`, `build/` - generated, not committed
