# Migrating a TU's data into its C file

Goal: every main-exe TU defines its own `.rdata/.data/.sdata/.sbss/.bss`, so the
same source builds byte-identical objects with both toolchains:

* the modern build (cc1 2.7.2 + maspsx + GNU as/ld, splat linker script), and
* the original Psy-Q 4.3 toolchain (`ccpsx` = cc1psx 2.7.2.SN32.3.7 + aspsx 2.77,
  later psylink 2.73).

Two facts force this. aspsx addresses small data through `$gp` only when the
symbol is defined (or `.comm`'d) in the same file, so `extern` + `SDATA_LOCAL`
no longer works. And psylink lays out each section object by object, in link order,
so a TU's data has to come from that TU's object.

## How the compiler places data (cc1 2.7.2, `-O2 -G8`, aspsx)

| C definition                                  | section                       | order |
|-----------------------------------------------|-------------------------------|-------|
| initialised, size <= 8 (including `= 0`/`NULL`) | `.sdata`                      | definition order |
| initialised, size > 8                         | `.data` (`.rdata` if `const`) | definition order |
| string literal, size <= 8                     | `.sdata` (!)                  | first use, while the function is compiled |
| string literal, size > 8                      | `.rdata`                      | first use |
| `static`, uninitialised, size <= 8            | `.sbss` (`.lcomm`)            | definition order |
| `static`, uninitialised, size > 8             | `.bss` (`.lcomm`)             | definition order |
| global, uninitialised                         | common (`.comm`)              | **not in the TU's sections**, see below |

* cc1 2.7.2 does not move explicitly zero-initialised variables to bss. `static int x = 0;`
  is `.sdata`, while `static int x;` is `.sbss`.
* File-scope variables are emitted where they are defined. A function's literals are
  emitted while that function is compiled, so variables defined above the first
  function come before every literal in `.sdata`. Unreferenced statics are still
  emitted.
* A symbol listed in `orig/siphon.map` is a global. If it isn't listed, it is
  `static`. Keep the `D_<addr>` name for statics you can't name yet:
  `tools/psyq_check_tu.py` checks those addresses.
* **Commons.** psylink appends every common after the `.sbss`/`.bss` of all
  objects, in its own hash order. That is why the tail of `.sbss`
  (0x80110230–0x801104A0) and of `.bss` (from about 0x80127C58) look shuffled. Commons
  belong to no TU's sections. A TU should still give a *tentative definition*
  (`void *gHogHeader;`) for each small common it reads through `$gp`, otherwise aspsx
  emits `lui/lw`. Use the real map name: the hash order presumably depends on it (not yet verified).
  Commons that the original reached with `lui`/`addiu` stay `extern`.
* Game TUs (not the egs* engine files) start their `.rdata` with a 16-byte
  `{1, 1, 1, 0}` const. It is defined at file scope as
  `static const s32 D_xxx[4] = {1, 1, 1, 0};` before the first function. Its origin
  (probably a header) is still unknown.

### Pitfalls

* **`__LINE__`.** `ASSERT` bakes the line number into `.text`. Keep every line
  above the last ASSERT where it is: when you replace the `SDATA_LOCAL` and `extern`
  blocks, keep the same number of lines.
* **Builtins.** The original compiled without `-fno-builtin`, but the modern build
  passes it. Write inlined library calls explicitly, for example
  `__builtin_strcpy(buf, "CDERR")`. That puts the literal back into `.sdata` and gives
  the same block copy under both toolchains.
* **TU boundaries in the yaml can be wrong.** Check them before you trust them. One
  case found so far: `gEGSMathIdentityMatrix` was in egsanim's `.rodata`, but it starts
  egsmath's. Another: sprite's texture-name strings sit inside joystick.c's
  `INCLUDE_RODATA` blob.

## Finding the TU's data

```
tools/data_owner.py egstex             # every data address egstex's .text touches
tools/data_owner.py --section .sdata   # a whole section, each address with its users
```

Each lui/lo and `$gp` access in the original `.text` is attributed to the TU whose
code contains it. A TU's data is the contiguous run that only that TU touches,
lying between its neighbours' runs in link order. Unreferenced items at the edges,
such as stream's `cdInt` or the two `""` strings after "T", need judgement. Note
the choice in a comment.

## Placing it: splat yaml

The C object supplies a section through a *dotted* subsegment named after the TU,
exactly like the existing `.rodata` entries. The splat data blob is split around it,
and each leftover piece is named after its start (rom offset for data, vram for bss):

```yaml
      - [0xED0E4, data, data]
      - [0xF72D8, .data, stream]          # stream.c.o(.data)
      - [0xF72EC, data, data_F72EC]
      ...
      - [0x100710, .sdata, egsdebug]      # egsdebug.c.o(.sdata)
      - [0x100720, .sdata, egstex]
      - [0x100728, sdata, sdata_100728]
      - { start: 0x100768, type: sbss, vram: 0x8010FF68, name: sbss }
      - { type: .sbss, vram: 0x80110220, name: egsanim }
      - { type: sbss, vram: 0x80110228, name: sbss_80110228 }
      - { type: bss, vram: 0x801104A0, name: bss }
      - { type: .bss, vram: 0x80122A38, name: egstex }
      - { type: .bss, vram: 0x80122C90, name: egsmisc }
      - { type: bss, vram: 0x80125210, name: bss_80125210 }
```

If a C object has a non-empty section with no dotted entry, splat's automatic sibling
placement drops it at the start of that section, and the sha1 check fails. In that
case, add the missing entry.

`configure.py` runs maspsx with `--use-comm-section`. Without it, maspsx gives each
`.comm` a private copy in the object, and accesses bind to that copy. With it, they
stay real commons and GNU ld resolves them to splat's `.sbss`/`.bss` labels.

## Compiling under ccpsx

`ccpsx` predefines `_PSYQ`. Under `_PSYQ`, `include/common.h` makes `INCLUDE_ASM`,
`INCLUDE_RODATA` and `SDATA_LOCAL` no-ops and skips `include_asm.h`, because aspsx
can't parse `.include`, `.pushsection` or the maspsx markers. `include/scratch.h`
(`.macro`/`.set push`) still needs its own `_PSYQ` variant for the TUs that use it.

## Checklist for one TU

1. `tools/data_owner.py <tu>` gives the address ranges per section. Confirm the
   neighbours and decide how to treat unreferenced edge items.
2. In the C file, replace the `SDATA_LOCAL`/`extern` lines with definitions in address
   order, using initial values from `asm/main/data/*.s`. Keep the line count. Use
   `static` unless the symbol is in `orig/siphon.map`. Write small string literals
   inline instead of `D_` externs.
3. Add tentative definitions for the commons the TU reaches through `$gp`.
4. Add dotted yaml entries and split the blobs. Run `python3 configure.py`, then
   `ninja`, check for `FAILED: [code=`, and run `sha1sum --quiet -c orig/SHA1SUMS.build`.
5. Run `tools/psyq_check_tu.py <tu>`. Every section must be `OK`, with no "unplaced"
   section and no `symbol ... expected` lines.

## Status

| TU       | sections from C                     | modern sha1 | ccpsx (all sections) |
|----------|-------------------------------------|-------------|----------------------|
| egsdebug | .rdata .text .sdata                 | OK          | OK |
| egstex   | .rdata .text .sdata .bss            | OK          | OK |
| egsmisc  | .rdata .text .sdata .bss            | OK          | OK |
| egsanim  | .rdata .text .data .sdata .sbss     | OK          | OK |
| stream   | .rdata .text .data .sdata + commons | OK          | OK |
