# 305 — C graph loader (replaces 003-loader.lua)

## Status
open

## Current behavior
The phase 2 loader at `src/003-loader.lua` reads a map directory's
`boxes/*.json`, `data/*.json`, and metadata, decodes JSON via dkjson,
runs schema validation, builds the in-memory graph table, and detects
entry boxes. It is Lua. The synchronous executor consumes its output.

The phase 3 pool runner (issue 301) cannot embed a Lua state for
loading — Lua appears in the runner only as a per-box language runtime,
never as the runner's own scripting layer. The loader is therefore
ported to C and lives inside (or alongside) `src/008-pool-runner.c`.

## Concept

The C graph loader is the first phase of the pool runner's startup. It:
1. Walks the map directory.
2. Parses every JSON file.
3. Validates schema and connection topology.
4. Builds a single `graph_t` struct that the pool runner and dispatch
   layer consume.

The loader is a one-shot at startup. It crashes the program on any
validation failure (per the hard-crash policy in issue 303). There is
no recovery, no skip-bad-boxes mode.

## Inputs

The map directory layout (unchanged from phase 2):

```
maps/<name>/
    meta.json          ← entry box, src_dirs, run-level metadata
    boxes/*.json       ← one file per box
    data/*.json        ← static data values (literal inputs)
    src/*              ← box function source files (resolved via meta.src_dirs)
```

The loader is given the map directory path as a C string.

## Output: `graph_t`

```c
typedef struct {
    int           n_boxes;
    box_t        *boxes;            // array of box descriptors
    int           n_connections;
    connection_t *connections;
    int           n_size_classes;
    int          *size_classes;     // distinct slot sizes the graph uses
    int           n_entry_boxes;
    int          *entry_box_ids;    // indices into boxes[]
    int           n_languages;
    const char  **languages;        // distinct language names used
} graph_t;

typedef struct {
    char         *id;               // user-defined box id ("classifier", "stamp", …)
    char         *kind;             // "call" | "data"
    char         *lang;             // "lua" | "c" | "bash" | ...; NULL for data and iterator/comparator
    char         *ref;               // file path; NULL for data/iterator/comparator
    char         *fn;                // function name; NULL for data/iterator/comparator
    int           n_inputs;
    input_decl_t *inputs;
    int           output_capacity;  // bytes; 0 if variable-size (uses large-value heap)
    int           is_comparator;
    int           is_iterator;
    int           n_iter_outputs;
    char        **iter_output_names;
    // …
} box_t;

typedef struct {
    int   from_box_id;
    char *from_branch;              // NULL for plain call; "lt"/"eq"/"gt" for comparator;
                                    // iterator output name otherwise
    int   to_box_id;
    char *to_input_name;
    int   to_input_index;
} connection_t;
```

Strings are owned by the graph (allocated from a single arena attached
to the graph; freed when the graph is destroyed at the end of the run).

## Loading phases

### 1. Directory walk
Open the map directory, enumerate `boxes/`, `data/`, and the optional
`meta.json`. Each entry is queued for parsing.

### 2. JSON parse
Use the vendored JSON parser (issue 301 lists cJSON / jsmn / hand-rolled
as candidates). Each file becomes a parsed JSON tree. Parse errors are
fatal — print the file path and the parser's error message, abort.

### 3. Schema validation
Box kinds and what they mean at runtime:

- **`call`** is a verb — a box that runs a function (or, for iterator
  and comparator variants, a dispatch-layer routing primitive). Each
  call box becomes a `dispatch_task_t` in the pool when its inputs
  are ready.
- **`data`** is a noun — a literal value embedded in the map. It has
  no function and produces no task in the pool. Its output slot is
  allocated and filled at startup by the loader and remains filled
  for the duration of the run; downstream consumers read it like any
  other slot.

Required fields per kind:
- `id`, `kind` always required.
- For `kind == "call"`: either (a) `lang`, `ref`, `fn` (plain call
  box), or (b) `iterator_outputs` (iterator box, no `ref`/`fn`), or
  (c) `comparator` field set (comparator box, no `ref`/`fn`).
- For `kind == "data"`: a `value` field; no `inputs` or `connections`
  (other than outgoing wires from this box's output slot).

Box ids must be unique across the map. Connection endpoints must
reference valid box ids and valid input names.

Schema rules mirror `src/001-schema.lua`. The C validator is a port of
that file's logic, simplified by the absence of port-named branching
(removed in issue 108 cleanup).

### 4. Topology validation
- Every connection's `from_box` and `to_box` exist.
- Every connection's `to_input_name` matches a declared input on the
  destination box.
- No non-iterator cycles. The validator runs DFS; any cycle that does
  not pass through an iterator box is rejected.
- Every required input has at least one connection feeding it (or is
  fed by a `data` box).

Validation failure aborts the program with a precise error message —
which box, which connection, what was wrong.

### 5. Slot size class enumeration
Walk every box and queued-input declaration to enumerate the distinct
slot sizes the graph will need. Each unique size becomes a size class
in the slot allocator (issue 302). Output:

```c
graph->size_classes      = { 64, 256, 1024, 65536 };  // example
graph->n_size_classes    = 4;
```

The slot allocator pre-populates each class's free list with
`n_workers × (count of boxes whose output is this size)` slots. This
covers the worst case — every worker concurrently producing an output
at this size class — without overshooting. Demand beyond this triggers
growth (issue 302); the per-thread multiplier is the right ceiling for
the static count, while leaving headroom for iterators that allocate
per-invocation slots.

### 6. Entry-box detection
A box is an entry box if either:
- It has zero inputs, or
- All of its inputs are connected only to `data` boxes (literal values
  with no upstream computation).

Entry boxes are submitted to the pool first; everything else fires from
post-action routing in the dispatch layer.

A map can have any number of entry boxes. The user lays out the
initial program state via `data` boxes and entry-point call boxes; the
runner submits all entries at once, and the graph propagates from
there. There is no singular `main`.

### 7. Language enumeration
Collect the distinct `lang` values across all plain call boxes. The
pool runner uses this set to decide which language specs (issue 303)
to load and which per-worker handles to initialize.

Iterator and comparator boxes have no `lang` and contribute nothing
here.

### Note on built-in primitives vs shipped libraries

There are two distinct categories of "things the user gets out of the
box," and they live in different places:

- **Dispatch-layer primitives** are graph-topology operations whose
  semantics are routing, not data transformation. Iterator and
  comparator are the two so far. They have no `lang` / `ref` / `fn` —
  the dispatch layer implements their behavior directly. Splitter
  / fan-out is already free via wire fan-out and does not need its
  own box kind. Possible future primitives: a "gate" (forward only
  when a control input is truthy), a "merge" (forward whichever
  input arrives first). These are not on the roadmap; flagged here
  so the door stays open.
- **Shipped libraries** are data-transformation utilities — JSON
  parse/serialize, file read/write, string concat/split, regex,
  shell-out. These are language-specific (string semantics differ
  between Lua, C, and Bash) and ship as `libs/<name>.<ext>` per
  language. They are not built into the dispatch layer; they are
  ordinary call-box source files that users wire into their maps.
  `libs/ollama.lua` is an existing example.

The distinction matters: dispatch-layer primitives change the graph
model; shipped libraries don't. A dispatch primitive can do things a
function call cannot (like routing on a counter); a library is just
code that happens to be vendored.

## Error policy

Any failure in any phase aborts the program. The loader prints:
- The map path
- The phase that failed (parse / schema / topology / etc.)
- The specific file or box that caused the failure
- The parser's or validator's diagnostic message

Then `exit(1)`. There is no partial-load mode.

## Open questions

- JSON library choice: cJSON (heaviest, easiest, well-maintained),
  jsmn (tiny, no allocations, manual tree walking), or hand-rolled
  (smallest dependency footprint, more code). Decision deferred to
  implementation time.
### String storage

A "string arena" is a single `malloc`-backed buffer the graph loader
uses to store every string it owns (box ids, file paths, function
names, branch tags) packed end-to-end. Each string is a `char *`
pointing into the arena. Allocating a string is just bumping the
arena's offset; freeing happens once when the arena is destroyed at
the end of the run, in a single `free(arena_buffer)`.

This is faster and simpler than calling `malloc` per string: one
`free` for thousands of strings, no fragmentation, and lookup is
just a pointer.

The arena starts small and grows on demand by `realloc`-ing to a
larger size when the current buffer fills up. Because every stored
string is referenced by a `char *` into the arena, a `realloc` that
moves the buffer would invalidate all the pointers — so the loader
either uses a series of fixed-size chunks (a chunk list rather than a
single growable buffer) or pre-sizes the arena from the file count
to avoid mid-load realloc. Chunk-list is the standard answer.

## Suggested implementation sequence

1. Vendor the chosen JSON parser into `libs/json/`.
2. Define `graph_t`, `box_t`, `connection_t`, `input_decl_t` in
   `src/008-pool-runner.c` (or a sibling header).
3. Implement phase 1 (directory walk) and phase 2 (parse). Smoke test:
   load `maps/hello`, print the parsed tree.
4. Implement phase 3 (schema validation) one rule at a time. Each
   rule has a unit test against a malformed map fixture.
5. Implement phase 4 (topology validation), including the cycle
   detector. Test against `maps/branch-test` (valid) and a fixture
   with a non-iterator cycle (invalid).
6. Implement phase 5 (size class enumeration). Output should match
   the box outputs declared in `maps/hello` and `maps/classify-demo`.
7. Implement phases 6 (entry boxes) and 7 (languages). Verify against
   known maps.
8. Wire the loader into `src/008-pool-runner.c`'s `main` so the pool
   runner starts by loading and validating before doing anything
   else.

## Relevant files

- `src/003-loader.lua` — phase 2 loader, ported by this issue
- `src/001-schema.lua` — phase 2 schema validator, also ported
- `src/008-pool-runner.c` — pool runner that owns and uses the loader
- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle that
  consumes the graph
- `issues/302-wire-value-slot-store.md` — slot allocator that consumes
  the size class list
- `issues/303-language-runtime-spec.md` — spec registry that consumes
  the language list
- `issues/304-task-dispatch-layer.md` — dispatch layer that consumes
  the box and connection arrays
- `issues/219-map-compiler.md` — the compiler is the other consumer of
  the same validation logic; the C loader and the compiler should
  share validation code if possible

## Implementation log

### Phase 1 + 2 + early phase 3 — 2026-05-12

What shipped:
- `src/010-graph-loader.h` — public types and API. `graph_t` is
  opaque; the box / connection / input declaration shapes are
  public so the dispatch layer (issue 304) can walk them directly.
  Routing kinds align with issue 233 — `routing_t` with `kind`
  plus per-kind parameters, no legacy `comparand` /
  `iterator_outputs` top-level fields.
- `src/010-graph-loader.c` — implementation. Pipeline: open the
  map dir, parse `meta.json` via the project JSON parser
  (issue 314), walk every `.json` under `boxes/`, parse each into
  a `box_t`, enforce kind-specific required fields (ref/routing
  for call; path for data; inputs only for file_write), validate
  id uniqueness across boxes. Errors return a malloc'd
  `<file>:<line>: <message>` diagnostic via the `**err`
  out-parameter; the pool runner's main is expected to print and
  abort.
- `tests/maps/hello/` and `tests/maps/branching/` — fixture maps
  exercising the call/data combo, comparator routing (lt/eq/gt
  branches all present), and iterator routing (n_outputs=3).
- `tests/010-graph-loader-test.c` — 7 unit tests covering
  successful loads of both fixtures, missing map directory,
  NULL argument, unknown box kind, missing routing, and
  duplicate box id. Malformed-shape tests use mkdtemp scratch
  dirs so the tracked fixtures stay valid.
- Memory model: one `json_arena_t` per loaded graph holds every
  string and every parsed tree from every file; the box,
  connection, and input declaration arrays are plain malloc.
  `graph_destroy` releases all of it.
- `.gitignore` anchored to `/maps/` so `tests/maps/` is tracked.
- Makefile per-test dependency line for `010-graph-loader-test`
  linking the loader and the JSON parser.

Verified `make STRICT=1` builds cleanly; 33 tests across three
suites (slot store 12, graph loader 7, JSON 14) all pass.

What's deferred to follow-on iterations within 305:
- **Phase 4 — topology validation.** Connection endpoints store
  string refs only. The next pass resolves `to_box` to integer
  indices, verifies endpoints exist, and detects non-iterator
  cycles via DFS.
- **Phase 5 — slot size class enumeration.** The loader needs to
  walk every box output and produce the distinct `cell_capacity`
  values the slot store's allocator pre-populates.
- **Phase 6 — entry-box detection.** A box with zero inputs or
  with all inputs fed only by `data` boxes is an entry. The pool
  runner spawns initial tasks for these.
- **Phase 7 — language enumeration.** Distinct `lang` values
  across call boxes, handed to the spec registry (issue 303).
- The remaining routing kinds from issue 233 (randomizer,
  weighted, distributor) — the parser currently rejects them with
  a precise message.

None of those block 301 / 303 / 306. Topology and language
enumeration land before 304 (the dispatch layer) since the
dispatch action needs both.

### Topology endpoint resolution — 2026-05-12

After `load_boxes` succeeds, a `resolve_topology` pass walks every
outgoing connection on every box and resolves the `to_box` string
to its index in `graph->boxes`, and `to_input` to its index in
the target box's `inputs[]`. Both string refs are preserved on
`connection_t` for diagnostics. Misses surface as
`box 'X' has a connection to nonexistent box 'Y'` or
`... nonexistent input 'Z'`. Two new fixture-based failure tests
plus an assertion on the resolved indices in the happy-path
`load_hello` test. The `branching` fixture grew three more box
files (`small`, `exact`, `big`) so its three comparator branches
have real targets now.

Still deferred within 305: slot size class enumeration, entry-box
detection, language enumeration.

### Non-iterator cycle detection — 2026-05-12

Standard DFS with white/gray/black coloring; the only twist is
that we skip outgoing edges from iterator-routing boxes, since
cycles passing through an iterator are legitimate (the iterator's
input queue eventually empties and the loop terminates). Two new
fixture-based tests cover the reject case (`a → b → a`, no
iterator) and the allowed case (`a → iter → b → a`).

### Runtime attach (slot allocation + spec resolution) — 2026-05-12

`graph_attach_runtime(g, slots, specs, default_cell_bytes, err)`
walks every box and:
- allocates one 1-cell peek slot per declared input port via
  `slot_alloc`, storing the slot id in `box->input_slot_ids[i]`;
- resolves the spec for each call box by looking up `box->lang`
  in the registry (or, as a fallback, by matching the file
  extension of `box->ref`), storing the result on
  `box->spec_idx`.

The graph also remembers its `map_dir` so the dispatch layer can
resolve relative paths for data boxes and file_write boxes.

What's still deferred:
- **Language enumeration on the graph.** We resolve per-box but
  don't expose the distinct set of languages a map uses, so
  workers init every spec rather than only the ones in use.

### Per-input slot modes + multi-spawn propagation — 2026-05-12

`graph_attach_runtime` now picks the slot mode per input port:
- A new BFS pass (`propagate_multi_spawn`) seeds every iterator-
  routing call box as `multi_spawn = 1` and propagates the flag
  forward through the connection graph. Every box reachable from
  an iterator is now flagged.
- For multi-spawn boxes, each input slot is allocated as a
  16-cell pop ring. For single-spawn boxes, the legacy 1-cell
  peek slot is used.
- `box->input_slot_modes[]` carries the per-port mode
  (`SLOT_MODE_PEEK` / `SLOT_MODE_POP`) so the dispatch action
  picks the right read op.

This is the compile-time wire classification the architecture
doc described, with a single-step forward propagation rule. A
proper per-wire classification (an iterator-rooted wire is
multi-push regardless of the immediate consumer's status) would
allow finer-grained mode picks; the current rule is conservative
in the right direction (slots that *could* be POP are POP).
