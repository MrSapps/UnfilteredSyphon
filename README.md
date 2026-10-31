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

### Original toolchain (Psy-Q 4.3)

The same sources also build byte-identical with the original compiler, assembler and
linker (ccpsx/cc1psx 2.7.2.SN32, aspsx 2.77, psylink 2.73, the real 4.3 libraries) under
wine. This needs the Psy-Q SDK in `/home/snake/dev/psyq_sdk` and `wine`, but not the
modern build:

```sh
python3 configure.py --psyq         # runs splat, writes build_psyq.ninja (build.ninja untouched)
ninja -f build_psyq.ninja           # builds build/psyq/scus_942.40 + build/psyq/bin/*.ovl, checks SHA1
```

See [docs/psyq_build.md](docs/psyq_build.md).

## Layout

- `config/scus_942.40.yaml`, `config/ovl/*.yaml` - splat configs (overlay ones from `tools/gen_ovl_yaml.py`)
- `config/symbols/<module>.txt` - symbols, seeded from the linker map by `tools/map2syms.py`;
  hand-maintained from then on (names, types, sizes). `overrides.txt` resolves symbols that
  are ambiguous between overlays sharing an address range.
- `src/`, `include/` - decompiled C and headers
- `asm/`, `assets/`, `build/` - generated, not committed
