# NON_MATCHING helpers

Scratch tools for working on `#ifdef NON_MATCHING` functions. Outputs go to `build/nm/`.

- `score.py [src.c ...]` — compile each TU with `-DNON_MATCHING` (same flags as `build.ninja`,
  per-TU `gsize`/`origname`) and print `difflines total name file` per NON_MATCHING function
  (register/immediate fields masked); sorted list in `build/nm/scores.txt`.
- `d.py src.c func [ctx [variants.txt idx]]` — show only the differing regions (ours | original),
  optionally for variant `idx` of a variants file.
- `vt.py src.c func variants.txt` — try source variants (`old\n----\nnew`, several per variant
  joined by `~~~~\n`, variants separated by `====\n`); prints a score per variant. Variants are compiled from a temp copy; `src/` is never written.
- `regswap.py [src.c ...]` — list functions whose diff is registers only.
- `alloc.py src.c func [variants.txt idx]` — gcc 2.7.2 global-alloc priority table (`cc1 -dl -dg` dumps).

The scores are a guide; only the full build + `sha1sum -c orig/SHA1SUMS.build` is authoritative.
