# libs/json/json.c — public surface

Small DOM JSON parser written for SoraMech, not vendored. Single
file plus header; arena-allocated tree, single-threaded parse, no
mutation API on parsed trees. Two halves: a parser (implemented)
and a streaming writer (declared; body deferred to when issue 311
needs it).

## Lifecycle

- `json_arena_t *json_arena_create(void)` — empty arena; NULL on OOM.
- `void json_arena_destroy(json_arena_t *a)` — frees every chunk.
- `size_t json_arena_bytes(const json_arena_t *a)` — diagnostic.

## Parser

- `json_node_t *json_parse(arena, src, *err_offset, *err_msg)` —
  parse a NUL-terminated string. NULL on syntax error;
  `*err_offset` is the byte offset, `*err_msg` is a static string.
- `json_node_t *json_parse_file(arena, path, *err_line, *err_msg)`
  — convenience: reads the file and parses. Reports 1-based line
  numbers on error.

## Accessors

- `json_kind(n)` — `JSON_NULL` / `JSON_BOOL` / `JSON_NUMBER` /
  `JSON_STRING` / `JSON_ARRAY` / `JSON_OBJECT`.
- `json_is_null(n)` — convenience.
- `json_bool_value(n)`, `json_number_value(n)`, `json_string_value(n)`.
- `json_array_size(n)`, `json_array_at(n, i)`.
- `json_object_size(n)`, `json_object_key(n, i)`,
  `json_object_value(n, i)`, `json_object_get(n, key)`.

All accessors return sensible defaults on the wrong kind: `0`,
`0.0`, `NULL`, no crash. The graph loader is expected to
validate before reading.

## Supported JSON

- Per RFC 8259 plus a few tightenings:
  - No comments.
  - No trailing commas.
  - No unescaped control characters in strings.
  - No embedded NULs in strings.
- UTF-8 strict. `\uXXXX` for BMP; surrogate pairs handled.
- Numbers are `double`; integer-shaped fields read via
  `(int)json_number_value(n)`.

## Writer (declared; deferred)

Signatures in `json.h`: `json_writer_init`, `json_writer_object`,
`json_writer_array`, `json_writer_end`, `json_writer_key`,
`json_writer_string`, `json_writer_int`, `json_writer_number`,
`json_writer_bool`, `json_writer_null`, `json_writer_finish`.

Current implementation: every entrypoint sets `w->err` and
`json_writer_finish` returns -1 if the writer was touched. The
real body lands alongside the JSONL run-log writer (issue 311).

## Related

- Issue 314 — design.
- Issue 305 — graph loader, the first consumer.
- Issue 311 — JSONL run-log writer.
- Issue 309 — Makefile discovers `libs/json/*.c` via wildcard.
