# 314 — A small C JSON parser, written for SoraMech

## Status
open

## Current behavior

The phase 3 design assumes a vendored JSON parser sits under
`libs/json/` — issues 305 (graph loader), 311 (JSONL run output),
and 309 (build system) all reference it. The original plan was to
drop in cJSON / jsmn / similar.

After thinking about it: a vendored generic parser is one more
external surface to learn and to debug, with idioms that don't
match the rest of the runtime, and complications around large
inputs / streaming / arena allocation that we'd want to special-case
anyway. SoraMech needs JSON in two specific places:

1. **Reading** map files (`meta.json`, `boxes/*.json`,
   `data/*.json`) at graph load. Inputs are kilobyte-scale, parsed
   once, validated immediately. The output is a tree that the
   graph loader walks once.
2. **Writing** run-log events into `tmp/last-run.jsonl` (issue
   311). Output is per-event, line-oriented, ASCII-safe; the writer
   thread serialises one event struct per call.

Both are narrow enough that a small, opinionated parser tailored to
the project beats a general-purpose library on every dimension that
matters to us.

## Intended behavior

A C library under `libs/json/`, vendored as plain source, that
SoraMech owns end to end. Two halves:

### Parser (DOM, arena-allocated)

```c
typedef struct json_arena json_arena_t;
typedef struct json_node  json_node_t;

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_NUMBER,
    JSON_STRING, JSON_ARRAY, JSON_OBJECT
} json_kind_t;

json_arena_t *json_arena_create(void);
void          json_arena_destroy(json_arena_t *a);

/* Parse a NUL-terminated string into a tree owned by the arena.
 * Returns NULL on syntax error; *err_offset and *err_msg are filled
 * with the position and a short diagnostic. */
json_node_t *json_parse(json_arena_t *a,
                        const char *src,
                        int *err_offset, const char **err_msg);

/* Parse a file. Convenience over json_parse + a file read. */
json_node_t *json_parse_file(json_arena_t *a,
                             const char *path,
                             int *err_line, const char **err_msg);

/* Inspect. */
json_kind_t   json_kind        (const json_node_t *n);
int           json_is_null     (const json_node_t *n);
int           json_bool_value  (const json_node_t *n);
double        json_number_value(const json_node_t *n);
const char   *json_string_value(const json_node_t *n);   /* NUL-terminated */
int           json_array_size  (const json_node_t *n);
json_node_t  *json_array_at    (const json_node_t *n, int i);
int           json_object_size (const json_node_t *n);
const char   *json_object_key  (const json_node_t *n, int i);
json_node_t  *json_object_value(const json_node_t *n, int i);
json_node_t  *json_object_get  (const json_node_t *n, const char *key);
```

All strings, arrays, and objects are arena-allocated. The whole
parse tree lives in one `json_arena_t`; destroying the arena
frees every node and every string in one go. No per-node `free`.

`json_object_get` is the common path for "pull the `id` field off
this box" — linear scan over the object's keys; object sizes in
this project are < 20, so the simplicity wins over a hash map.

### Writer (streaming, no DOM)

```c
typedef struct {
    char  *buf;
    int    cap;
    int    used;
    int    err;       /* 0 ok; nonzero = truncation / OOM */
} json_writer_t;

void json_writer_init    (json_writer_t *w, char *buf, int cap);
void json_writer_object  (json_writer_t *w);   /* '{' */
void json_writer_end     (json_writer_t *w);   /* '}' or ']' */
void json_writer_array   (json_writer_t *w);
void json_writer_key     (json_writer_t *w, const char *key);
void json_writer_string  (json_writer_t *w, const char *s);
void json_writer_int     (json_writer_t *w, long long v);
void json_writer_number  (json_writer_t *w, double v);
void json_writer_bool    (json_writer_t *w, int v);
void json_writer_null    (json_writer_t *w);

/* Returns the number of bytes written (excludes NUL terminator),
 * or -1 if w->err is set. */
int  json_writer_finish  (json_writer_t *w);
```

The writer maintains a small state stack (depth ≤ 16 is plenty
for our event records) so it knows when to emit commas between
elements. The writer never allocates — the caller hands in a fixed
buffer, the writer reports overflow via `w->err`. This matches the
JSONL event-writer design (issue 311): one record per fixed-size
struct, serialised into a per-write buffer of bounded size.

### What the parser supports

- JSON per RFC 8259: objects, arrays, strings, numbers (parsed as
  `double`), `true`, `false`, `null`.
- UTF-8 strict. Escape sequences `\"`, `\\`, `\/`, `\b`, `\f`,
  `\n`, `\r`, `\t`. `\uXXXX` for BMP code points; surrogate pairs
  handled. Decoded strings are NUL-terminated; embedded NULs in
  source strings are rejected (we don't need them and they make
  the strings interaction with the rest of the C code awkward).
- Strict syntax. Trailing commas, comments, unquoted keys: all
  errors with a precise message.

### What it does NOT support

- **Streaming / SAX**. Our inputs are small and parsed once. If
  some future use case wants streaming (a 10-MB LLM response that
  is itself JSON), revisit then.
- **No integer / no big-number type**. Numbers are `double`. The
  graph loader's integer-looking fields (`output_capacity`,
  `n_outputs`) are read via `(int)json_number_value(n)`. Documented
  in the loader, not in the parser.
- **No pretty-print**. The writer emits compact JSON only (one
  whitespace token between primitives, no spaces, no newlines).
  JSONL needs compact lines anyway.
- **No mutation API on parsed trees**. Trees are read-only; if a
  caller wants to construct JSON, it uses the writer.

### Errors

`json_parse` returns NULL on syntax error and fills `*err_offset`
with the byte offset where parsing failed, and `*err_msg` with a
short static-string description ("expected ':' after key",
"unterminated string", "invalid escape sequence"). The graph loader
turns this into a precise diagnostic ("maps/<name>/boxes/<id>.json:
line 12: unterminated string").

OOM during parsing (arena out of memory) is a fatal error — the
arena uses a chunked-buffer allocator that calls `malloc` when it
grows; if that fails, the parser reports `*err_msg = "out of
memory"` and returns NULL. The hard-crash policy of the rest of
the runtime (issue 303) applies upstream.

## Why now

The graph loader (issue 305) is the immediate consumer. The JSONL
writer (issue 311) is the next one. Building the parser ourselves
keeps the dependency surface to zero and gives us idioms (arenas,
error reporting style, allocation policy) that match the slot
store and the dispatch layer.

## Suggested implementation sequence

1. **Arena allocator** (`libs/json/arena.c` + `.h`). Chunked
   bump allocator: each chunk is a `malloc`'d buffer; allocate by
   advancing an offset; chain chunks when one fills. Free chunks
   in `json_arena_destroy`.
2. **Parser** (`libs/json/json.c` + `.h`). Hand-rolled recursive
   descent. ~600 lines. One internal struct per kind, packed into
   a `json_node_t` union with a small tag.
3. **Writer** (in `libs/json/json.c` alongside). State stack for
   comma placement; bounded buffer; clean error reporting via
   `w->err`.
4. **Unit tests** (`tests/314-json-test.c`). Conformance cases:
   nesting, escapes, surrogate pairs, error positions, writer
   bracket-comma correctness, round-trip for a meta.json-shaped
   input.
5. **Wire into the Makefile.** `libs/json/*.c` is already in the
   wildcard from issue 309; no Makefile edit needed beyond
   confirming it picks up.

## Relevant files

- `libs/json/` — vendored sources (to be created here, not
  imported from upstream).
- `issues/305-c-graph-loader.md` — the immediate consumer.
- `issues/311-integration-tests-and-run-output.md` — the writer's
  consumer (JSONL events).
- `issues/309-build-system.md` — Makefile already discovers
  `libs/json/*.c` via wildcard.
- `docs/001-architecture.md` — references the JSON parser in the
  phase 3 runtime layout.

## Open questions

- **Comments.** RFC 8259 forbids them; cJSON optionally allows
  `//` and `/* */`. Box files are produced by the editor, not
  hand-written, so we don't need them. Stay strict.
- **`json_node_t` storage layout.** Tagged union (`json_kind_t` +
  union of payloads) is simplest. Could also pack the kind into
  low bits of a pointer for size — defer until profiling shows
  node-count pressure.
- **Object key lookup performance.** Linear scan is fine for
  objects with 2–20 keys (every object in a SoraMech map). If
  data files ever grow to thousands of keys, add an optional
  hash-table mode.

## Implementation log

### Parser milestone — 2026-05-12

What shipped:
- `libs/json/json.h` — full public surface for both halves.
- `libs/json/json.c` — arena allocator (chunked bump, 16 KB
  default chunk), recursive-descent parser, every accessor. The
  parser handles every JSON kind, all escape sequences including
  BMP `\uXXXX` and surrogate pairs, with precise error positions.
  Numbers go through `strtod` after a small NUL-terminated scratch
  buffer (heap fallback for literals over 64 bytes — exceptionally
  rare). Arrays and objects build via a temporary linked list in
  the arena, finalized to fixed-size pointer arrays at close time;
  the link nodes are abandoned (recovered when the arena is
  destroyed).
- `tests/314-json-test.c` — 14 unit tests: every primitive,
  string escapes, U+00E9 single-escape, U+1F600 surrogate pair,
  empty / non-empty / nested arrays and objects, whitespace
  insensitivity, six distinct error-position cases, a realistic
  meta.json-shaped parse, file loading with line-numbered errors,
  arena growth across the 16 KB chunk boundary, and accessor
  safety on the wrong kind.
- Makefile per-test dependency line for `314-json-test`.

Verified `make STRICT=1` (`-Werror -Wextra -Wpedantic`) builds
cleanly; 14/14 parser tests and the 12 slot-store tests all pass.

What's deferred:
- **Stricter leading-zero rule.** RFC 8259 disallows `0\d+`; we
  currently accept `01` as `1`. None of our inputs produce that
  shape, so it's a tightening that lands when something needs it.

### Writer half — 2026-05-12

Filled in the writer bodies (object/array open+close+key+
primitives) on top of the depth-stack design declared in the
header. Eight new writer tests cover comma placement, escape
emission, empty and non-empty containers, nesting, overflow
detection, and a round-trip back through the parser. Done.
