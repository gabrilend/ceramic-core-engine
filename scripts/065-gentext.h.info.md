# 065-gentext.h — the generator's support machinery

Four things a C program needs before it can do what a scripting
language does for free, and nothing that knows what a box is.

- **`arena_t`** — a bump allocator over a list of blocks. Hands out
  zeroed, aligned memory; frees everything at once. `arena_alloc`,
  `arena_strdup`, `arena_strndup`. It dies rather than returning null,
  because a generator that cannot allocate has no partial answer worth
  returning and the alternative is a failure path in every caller to
  an exit that is already certain.
- **`buf_t`** — a string that grows by doubling. `buf_add`,
  `buf_addstr`, `buf_addch`, `buf_addf`, and `buf_line`, which appends
  a formatted string and a newline because every caller in the emitter
  writes whole lines. Formatting asks `vsnprintf` how much room it
  needs rather than guessing; a truncated line of generated C is a
  compile error somebody else has to diagnose.
- **`vec_t`** — an array of fixed-size elements that grows by
  doubling. `vec_push` returns a zeroed slot, **invalidated by the
  next push**, so a caller fills it immediately and re-reads by index.
  That sharp edge is deliberate: the elements are contiguous so the
  emitter can walk them in order.
- **`gt_add_c_string`** — a span of arbitrary bytes appended as a C
  string literal broken one piece per line, with the opening quote, the
  closing quote and the semicolon all written here so a caller cannot
  get half of it right. One literal per line keeps a generated file
  readable and never approaches a compiler's limit on how long a single
  literal may be — a limit the engine's own source would meet. There is
  one of these rather than two because there were briefly two: the box
  sources carried their own copy of the loop, and a second copy of an
  escaping rule is a second definition of the format it produces.
- **String helpers** — `gt_normalize_type` collapses whitespace and
  settles pointer spelling so `const char*` and `const  char  *` are
  one name, which matters because those strings are the emitted file's
  type names and two spellings would be two types. `gt_mangle` turns a
  type name into an identifier fragment. `gt_line_of`, `gt_trim`,
  `gt_all_space`, `gt_is_ident`.

Why it is its own unit: everything above it assumes it is correct, and
a bug here surfaces as garbled C hundreds of lines away in a file
nobody reads. Proven on its own by `tests/071-test-gentext.c`.
