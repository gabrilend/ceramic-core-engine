# 305 — C graph loader (replaces 003-loader.lua)

## Status

reopened 2026-07-26 — the loader shipped complete and is otherwise
healthy, but one of its load-time passes computes a fact the
project no longer recognises. There is no longer any such thing as
a single-spawn box: **every box is multi-spawn, unconditionally.**
`propagate_multi_spawn` and the three slot decisions that branch on
its result have to come out.

Everything else this issue delivered stands: the parse, the schema
validation, endpoint resolution, cycle rejection, entry-box
derivation, spec resolution, per-edge language classification, and
the `scan_input_feeders` consolidation.

## Current behavior
The C loader lives at `src/010-graph-loader.{c,h}` and replaces the
phase 2 Lua loader. It opens a map directory, parses every JSON file
under it through the project's JSON parser, validates per-box schema,
resolves connection endpoints to integer indices, rejects non-iterator
cycles, derives the entry-box set, and (via `graph_attach_runtime`)
allocates per-port input slots sized to fit declared producer output
capacities, resolves each call box's language spec, and enumerates
the distinct languages and slot size classes a map uses. Per-edge
same-language classification (issue 312) also runs at load time.

Three small refactors landed in this iteration:
- A single `scan_input_feeders` helper replaces three duplicated
  reverse-scan loops that walked all producers feeding a given
  `(box, port)`. Native-edge classification, large-value detection,
  per-port slot sizing, size-class enumeration, and entry-box
  detection all collect their facts from the same walk so any
  future change to "what counts as a feeder" has one place to land.
- All six routing kinds now parse uniformly (plain, comparator,
  iterator, randomizer, weighted, distributor).
- The `_routing_needs_counter_slot` set in `graph_attach_runtime`
  now lists all four counter-using kinds (iterator, randomizer,
  weighted, distributor) instead of three.

Future refactoring opportunities noted but not taken in this
iteration (each would route currently-duplicated logic through a
single pathway, no behavior change):
- The four `n_outputs`-from-JSON blocks in `parse_routing` are
  near-identical and could share a tiny `parse_n_outputs` helper.
- The two string-array parse blocks (`link_libs`, `headers`) are
  near-identical and could share a `parse_string_array` helper.
- `count_json_files_in_dir` + `load_boxes` both open the same dir;
  a single-pass loader that grows the boxes array would remove the
  double-walk.
- `graph_box_index`, `graph_box_by_id`, and the inner lookup in
  `resolve_topology` all do linear scans by id; a small hashmap or
  sorted id list would collapse the O(n²) topology pass to O(n log
  n) without changing observable behaviour.

**Non-conformant: the marker walk still runs.**
`graph_attach_runtime` calls `propagate_multi_spawn`, a fixed-point
forward BFS that seeds every iterator-routing call box with
`multi_spawn = 1` and spreads the flag to everything reachable.
Three slot decisions then branch on the result — cell count
(`MULTI_SPAWN_RING_CELLS` = 16 vs 1), mode (`SLOT_MODE_POP` vs
`SLOT_MODE_PEEK`), and flags (`SLOT_FLAG_TAGGED` vs none) — with a
per-port override that pins a wire-less literal back to a 1-cell
untagged peek slot.

## Intended behavior

Delete `propagate_multi_spawn` and the `multi_spawn` field on
`box_t`. Every box is multi-spawn, so a per-box marker carries no
information.

The three slot decisions survive, but their basis moves from the
box to the port — which is where issue 324 (multi-fire boxes
consume their literal inputs) already said it belonged in its
design ruling. That ruling stated the two
slot kinds are not loop bookkeeping but the two **input methods**:
a value is either consumed on use or referenced on use. The loader
kept the box-level marker as the default and layered the port rule
on top as an exception. With the marker gone, the port rule is
simply the whole rule:

- A port carrying a typed-in literal and **no** wire feeders is
  **referenced** — 1 cell, peek mode, untagged, no dual ring.
  Startup delivers it once and every fire re-reads it.
- Every other port is **consumed** — N-cell tagged pop ring.
  Wire-fed ports want a fresh delivery per lap; a port carrying
  both a literal and a wire treats the literal as a seed.

Note the existing iterator exception in that override
(`!box_is_iterator(b)`) — a literal typed into an iterator's
intake is the first delivery on the conveyor, not configuration.
That exception is about the *port's* meaning on a routing box, not
about spawn category, so it survives the deletion unchanged.

Because every port that can be POP now is POP, the conservative
note at the bottom of the "Per-input slot modes" log entry below
resolves itself: there is no longer a finer-grained per-wire
classification to want, since no box-level status can make a wire
less multi-push than it is.

### Suggested implementation steps

1. `src/010-graph-loader.c` — delete `propagate_multi_spawn` and
   its call in `graph_attach_runtime`.
2. `src/010-graph-loader.c` — invert the slot-mode block: start
   from consumed (N-cell, POP, TAGGED) and let the
   literal-and-no-feeders test select referenced, instead of
   deriving the default from `b->multi_spawn`.
3. `src/010-graph-loader.h` — remove the `multi_spawn` field
   (line 262) and fix the slot-shape comment (line 358) that
   explains the choice in terms of spawn category.
4. `src/008-pool-runner.c` line 330 — duplicates the 16-vs-1
   decision off the same marker; it must follow the same
   port-level rule or be deleted in favour of the loader's.
5. `src/010-graph-loader.info.md` — check and update the
   public-surface description.
6. `tests/012-dispatch-test.c` line 310 asserts
   `iter_box->multi_spawn == 1` on a field that will not exist.

`MULTI_SPAWN_RING_CELLS` = 16 should be renamed rather than kept
— it is now just the ring depth for a consuming port, and the
name will otherwise outlive the concept it refers to.

The dispatch half of this change — the spawn guard itself — is
issue 304 (task dispatch layer). Neither issue is complete
without the other; landing this one alone leaves a marker nobody
sets still branching slot allocation.

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

`graph_t` is opaque; the structs it holds (`box_t`, `connection_t`,
`input_decl_t`, `routing_t`) are public so the dispatch layer can walk
them without going through accessors. Box kinds are an enum:
`BOX_CALL` (verb — runs a function), `BOX_READ` (value source — either
an inline `value` literal or a file at `path`), `BOX_WRITE` (file sink
— consumes one input). Routing is a tagged union on `routing_kind_t`
(plain / comparator / iterator / randomizer / weighted / distributor)
carrying the per-kind parameters in the same struct.

Each connection holds both its original string endpoint refs (for
diagnostics) and the resolved integer indices (`to_box_idx`,
`to_input_idx`). The topology pass fills the indices in; the dispatch
layer uses only the indices.

The graph also exposes:
- `n_languages` / `languages[]` — distinct language names across call
  boxes, filled in by `graph_attach_runtime`.
- `entry_box_id` — single name from `meta.json` (the multi-entry set
  described below is the work this iteration adds).
- `map_dir` — the directory the graph was loaded from, kept so the
  dispatch layer can resolve relative paths inside read / write boxes.

Strings are owned by the graph (allocated from a single JSON arena
attached to the graph; freed when the graph is destroyed at the end
of the run). The box / connection / input arrays are plain malloc and
freed in `graph_destroy`.

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

- **`call`** is a verb — a box that runs a function. Each call box
  becomes a dispatch task in the pool when its inputs are ready. The
  branching primitives that used to be separate box kinds
  (comparator / iterator) are now expressed as a `routing` field on a
  call box; the dispatch layer reads `routing.kind` to pick the
  outgoing branch.
- **`read`** is a value source. It can carry an inline `value`
  literal or a `path` that the dispatch layer reads at run time.
  When `value` is set the canvas hides the `path` input port. A
  read box has no function to invoke — it just produces a value
  downstream consumers can read like any other slot.
- **`write`** is a file sink. It consumes one input and emits the
  string `"true"` downstream after a successful write so it can be
  chained.

Required fields per kind:
- `id`, `kind` always required.
- For `kind == "call"`: `ref` (source file) plus a `routing` object
  (`{ "kind": "plain" | "comparator" | "iterator" | ... }` with the
  per-kind parameters). `lang` and `fn` are optional — `lang` falls
  back to the file-extension lookup in the spec registry, `fn`
  defaults to the box id at dispatch time.
- For `kind == "read"`: at least one of `value` (inline literal) or
  `path` (file). Either can also arrive at run time on the implicit
  `path` input port — the run-time check lives in dispatch.
- For `kind == "write"`: no extra fields beyond `inputs` (the input
  carries the bytes to write and the path).

Box ids must be unique across the map. Connection endpoints must
reference valid box ids and valid input port names.

### 4. Topology validation
- Every connection's `from_box` and `to_box` exist.
- Every connection's `to_input_name` matches a declared input on the
  destination box.
- No non-iterator cycles. The validator runs DFS; any cycle that does
  not pass through an iterator box is rejected.
- Every required input has at least one connection feeding it (or is
  fed by a `read` box, the value-source kind).

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
An "entry box" is one the pool runner submits as an initial task. A
box qualifies when it is a `call` or `write` box (the kinds that
actually run as tasks) AND either:
- it has zero inputs, or
- every one of its non-optional inputs is fed only by `read` boxes
  — literal-or-file value sources with no upstream computation.
  Inputs flagged `optional` in the schema are ignored when checking
  this condition, so a box that has all its computed inputs satisfied
  by literals still qualifies even if an optional port is wired to
  another call box (it just won't fire until the optional value
  arrives).

Read boxes never qualify — they aren't tasks; their value is staged
into their downstream consumers' input slots at dispatch time.

A map can have any number of entry boxes. The runner submits all of
them at once and the graph propagates from there; there is no
singular `main`. The single `entry_box_id` field on `meta.json`
remains supported as a hint but is not the canonical answer.

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

> Superseded 2026-07-26 by the ruling in "Intended behavior"
> above: the spawn category this pass computes no longer exists.
> The per-port slot modes it introduced survive; the box-level
> propagation that selected them does not. Kept as the record of
> where the marker came from.

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
