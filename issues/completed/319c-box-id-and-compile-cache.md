# 319c — Box id generator and compile cache

## Status
complete

## Parent issue
Sub-issue of 319. Provides the box id generator that
`create_box` (319d) returns when the caller doesn't supply an
explicit `id`.

## Reduced scope — compile cache not needed as a separate module

Issue 319's Q5 resolution called for a content-hash-keyed
compile cache under `tmp/` that shells out to
`scripts/soramech-compile.sh` on miss. Investigation while
starting this sub-issue found that **the language specs already
do this work internally**:

- `langs/c/spec.c` has `maybe_lazy_compile` (line 228) that
  detects a `.c` source path, compiles it to `.so` on first
  use, and caches the resulting handle in the per-worker dlopen
  table.
- Lua and Bash have no compile step — `langs/lua/spec.c` loads
  via `luaL_loadfile` and caches the Lua module; Bash's spec
  hands the source path to a persistent subprocess.

So `create_box(spec)` for any language can resolve via the
existing per-spec invocation path. No separate cache layer
preserves more concept-purity than it saves work — the runner
already stays ignorant of the compiler because each language
spec encapsulates its own loading discipline. The Q5
"compile cache" idea is therefore folded into existing spec
machinery rather than implemented as a new module.

If a future need surfaces for a unified content-hash cache
above the spec layer (e.g. for pre-warming or shared
artifacts across workers), revisit; for now, defer.

## Current behavior

There is no box id generator. The graph loader currently expects
every box's `id` field to be supplied by the caller (from JSON
on disk). `create_box` callers (issue 319d) need a way to
produce unique ids when they don't want to name a box themselves.

## Intended behavior

A small utility module that exposes one function:

```c
int box_id_generate(char *buf, size_t buf_size);
```

Fills `buf` with a string of the form `auto_<8 hex chars>` (a
process-wide atomic counter rendered as hex). Returns 0 on
success, -1 if `buf_size` is insufficient (the required size
is 14 bytes: 5 prefix chars + 8 hex chars + null terminator).

Uniqueness is guaranteed within a single process run via the
atomic counter. Cross-run uniqueness is not a goal — ids are
runtime-only constructs; persistent ids come from the on-disk
box JSON files.

## Suggested implementation

1. Create `src/017-box-id.h` — declaration of `box_id_generate`.
2. Create `src/017-box-id.c` — atomic counter + snprintf
   formatting.
3. Create `src/017-box-id.info.md` — function listing per the
   project's documentation convention.
4. Create `tests/017-box-id-test.c` — verify format, uniqueness
   across many calls, rejection on too-small buffer, and
   uniqueness under concurrent access from multiple threads.
5. Add Makefile dependency line for the new test.
6. Initialize `file-index-counter` at 17.

## Relevant files

- `src/017-box-id.{h,c}` — new module.
- `tests/017-box-id-test.c` — new test.
- `Makefile` — add per-test dependency line.
