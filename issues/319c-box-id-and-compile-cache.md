# 319c — Box id generator and compile cache

## Status
open — planning stub. Full design in parent issue 319's "Q4" and
"Q5" resolutions.

## Parent issue
Sub-issue of 319. Combines two small independent utilities that
both gate `create_box`:

- **Box id generator** — auto-generates UUID-style box_id strings
  for `create_box` callers who don't supply an explicit `id`.
  Design: 319 Q4.
- **Compile cache** — content-hash-keyed lookup under `tmp/`,
  shell-out to `scripts/soramech-compile.sh` on cache miss,
  dlopen the resulting artifact. Preserves the runner-doesn't-know-
  about-compiler separation. Design: 319 Q5.

## Intended behavior

- A `box_id_generate()` utility returns a string like
  `auto_8f3c2a1e9b4d`. Collision-free within a run.
- A `compile_cache_resolve(source_path, lang_spec)` utility
  returns a dlopen-ready artifact path. Cache hit returns
  immediately; cache miss invokes the compile script and waits.

## Suggested implementation

To be expanded when picked up. Both utilities are independent of
the slot store work and can land before 319b if convenient.
