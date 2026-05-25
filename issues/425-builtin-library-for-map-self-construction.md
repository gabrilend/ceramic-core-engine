# 425 — Built-in library for map self-construction (rolled-back prior attempt)

## Rolled back — superseded by phase 4

The implementation this issue described shipped and then was
reverted before the phase 3 release candidate. The design didn't
converge: open questions about wire-as-target vs id-as-input
semantics, ids-as-editor-only vs runtime-fresh creation, and how
the new box's id flows out of the utility-box without a second
output port remained unresolved.

The redesigned approach lives under phase 4 — see
[`issues/419-runtime-graph-mutation.md`](419-runtime-graph-mutation.md)
for the new architectural ground rules and the open questions
that still need resolution before implementation restarts. The
historical-behavior text below describes what the rolled-back
implementation did; the runtime no longer contains any of this
code.

## Status
rolled back · phase 4 · this file documents the prior attempt
that shipped under phase 3 and was reverted before the
release candidate. Renumbered from 319 during the phase-4
consolidation. The historical-behavior text below describes
what the rolled-back implementation did.

The (now-historical) status line as the implementation landed:

complete — every Q1–Q5 design question carries a **RESOLVED**
marker with the chosen shape, and the implementation lands in
six sub-issues 319a / 319b / 319c / 319d / 319e / 319f, all
shipped and themselves moved to completed/. Runtime
self-construction works end-to-end: a Lua box calls create_box
and connect inside its own invoke, and the dispatch fans the
trigger's return value into the freshly-created downstream slot
which then fires under its own language spec. The integration
suite's four runtime-create fixtures (319d-runtime-create,
319e-c-create, 319-box-kind-create, 319-cross-lang-create)
all pass.

**Bug postscript** (originally reported here as a latent
visibility race, now resolved at the source): runtime-create
tests intermittently failed with `dst->n_inputs` and
`dst->input_slot_ids` reading as their calloc'd zero values
even after `runtime_create_box` had populated them. Diagnosed
via the issue-311 push events (`result=to-input-out-of-range`
visible in the JSONL transcript) plus a one-off stderr trace at
the skip site: the dst pointer was valid (`dst->id` read
correctly) but field reads landed at wrong offsets. **Root
cause was a partial-rebuild trap in the Makefile**, not a
memory ordering issue: changing fields on `box_t` or `routing_t`
(issue 248 added n_outputs/outputs, issue 243 added
n_thresholds/thresholds) didn't trigger rebuilds of every .o
file that included the header, so different translation units
saw different struct layouts and field offsets disagreed. A
clean rebuild lined them up; the "layout-dependent timing" we
saw earlier was whichever .o files Make happened to remake from
incidental other touches. The Makefile now compiles with
`-MMD -MP` and `-include`s the generated .d files so a header
change propagates to every dependent .o automatically — the
partial-rebuild trap can't recur.

A small dispatch-side hardening also landed alongside the fix:
`push_branch` and `push_to_downstream` now read `b->n_connections`
and `b->connections` with explicit atomic-acquire (paired with
the existing atomic-release in `box_add_connection`). That isn't
load-bearing for the recurrence-prevention — the bug was at the
build layer — but it's the correct paradigm for the
atomic-stored fields and rules out a real cross-thread split
between a concurrent connect and a concurrent fan-out.

## Concept

A map should be able to build more of itself while running.
Two built-in primitives, exposed by the runtime to every language
spec, are enough to bootstrap that capability:

- **`create_box(filepath, function_name) -> box_handle`**
  Spawn a new box on the currently-running map. The first argument
  is the file holding the function the box will run; the second is
  the name of that function inside the file. The runtime does any
  build-pipeline work that's needed (compile the source, register
  the spec, allocate the box's input port slots) before returning
  a handle the caller can use to wire the box up.

- **`connect(from_box, to_box_a, to_box_b, ...) -> bool`**
  Attach a wire from the single output of `from_box` to each of
  the destination boxes. Variadic on the destinations, since fan-out
  from a single producer is the common pattern. Returns true if
  every connection succeeded, false otherwise.

Together these let a running map grow new sub-graphs in response
to runtime conditions: a planner box decides what sub-pipeline to
build, calls `create_box` for each node, calls `connect` to wire
them, and the dispatch layer picks up the new region on the next
push.

## Why this shape works (and the parts that need to be checked)

**Single-output-per-box is already the law.** Issue 218 collapsed
boxes to one output. Variadic destinations on `connect` is just
fan-out across that single output — no new concept on the wire
side. The variadic part of the API matches the data model.

**`filepath + function_name` matches box JSON.** That's already how
language specs locate the code a box runs. The API takes the same
two strings the box JSON would hold.

**Handle-as-return mirrors `data` boxes' encapsulation contract
(issue 248).** Encapsulated maps already expose typed handles
to externally-supplied inputs. A `create_box` handle is the same
kind of thing — a reference the caller uses to identify the box
in follow-up calls.

## Open design questions

These are real forks in the design. Each one changes what the
implementation looks like; none have a default answer that's
obviously right.

### Q1 — When does the new box exist? — **RESOLVED: runtime growth, unified store**

**Resolution.** The slot store grows at runtime. There is no
"static region" versus "dynamic region" distinction — every box
is a dynamic box, allocated through the same mechanism, whether
it was declared in `boxes/<id>.json` at graph load or created
by a `create_box` built-in call mid-run. Graph load becomes the
first user of the growth primitive rather than a separate code
path.

**Why this is the right shape (not a compromise).**

The slot store *already* needs dynamic memory at runtime for an
unrelated reason: a user may push 200 values into an input slot
before the consuming box has run even once. Ring buffer cells
have to be growable on the push path regardless of whether the
graph itself is growing. Once dynamic allocation is in the slot
store's hot path, gating new *box* allocation behind a separate
"static-only" pathway is artificial — we'd be solving the same
problem in two places.

The encapsulated-map feature (248) and the same-language-merge
research (313) both want to *unzip* a `.map` file into the
runtime graph at the same level as ordinary boxes — the entire
point is that the runtime doesn't spawn a nested thread pool for
the inner graph. Unzipping requires growing the box population
at runtime. A unified growable store covers all three callers
(graph load, `create_box`, map unzipping) with one mechanism.

**The structure: two-level chunked-append index table, no
static ceilings.**

The slot store keeps a top-level array of "chunk pointers." Each
chunk is a fixed-size block of slot pointers. Slot bodies
themselves are allocated separately and pointed to from chunk
entries. Three layers of growth, each cheaper than the layer
above:

- **Layer 1 — appending a slot inside a chunk** (most common).
  Atomic-increment the chunk's in-use counter; publish the slot
  pointer at that index. Lock-free, single-instruction-ish.
- **Layer 2 — adding a new chunk** (uncommon). Write a new chunk
  pointer into the top-level array at the next free top-level
  index; bump the atomic "chunks in use" counter. Lock-free.
- **Layer 3 — growing the top-level array** (rare; logarithmic in
  total slots across the run). When the top-level array itself
  fills, use the double-buffer-with-swap pattern: allocate a new
  top-level array of 2× size, copy existing chunk pointers
  (pointers only; chunks themselves stay put), atomically swap
  the top-level array pointer, defer-free the old array via
  refcount or epoch-based reclamation. Workers caching the old
  top-level pointer can finish their lookups against it — every
  chunk and every slot the old array references is still alive.

**No static ceilings anywhere.** Layer 1's chunk capacity is a
tuning constant (cache-friendly size, e.g. 64), not a limit on
total slots. Layer 2's top-level capacity grows via layer 3.
Layer 3 has no upper bound. The user pays one extra indirection
on every slot access (top-level → chunk → slot), and a rare
double-buffer swap whose cost amortizes to zero across the run.

**Same machinery serves three callers:** graph load (the first
batch of slots), `create_box` (one slot at a time during dispatch),
and `.map` unzipping for encapsulated maps (a burst of slots when
a sub-map is expanded into the runtime). All three look identical
to the slot store — append, append, append.

The allocator-wide-lock-free property is preserved. Only the
in-use counters and the top-level swap use atomics; everything
on the hot read/write path stays per-slot.

**Removal — refcount, not tombstone.** Issue 315 (compiled
artifact reference counting) is the safety mechanism. A box's
refcount goes to zero only when no other box references it as
producer or consumer; any worker about to use the box must hold
a reference that keeps the count positive. Therefore "refcount
reached zero" is a proof that no worker holds a stale pointer.
Slot indices freed at that moment are safe to reuse.

No tombstoning required, no generation counters on handles, no
ABA hazard — 315's refcount discipline subsumes all three.

**What this resolution does NOT decide:** the precise API of the
growth primitive on the slot store, the cache-line-aware chunk
size choice, and whether ring buffer *cell* growth uses
double-buffer-with-swap under the per-slot lock (the suggested
shape, since each slot already holds a lock during growth).
Those are implementation details for the slot store work itself,
not the `create_box` / `connect` design.

### Prerequisite — remove the dispatch input cap before `create_box` ships

Today `src/012-dispatch.c` allocates five parallel stack arrays
of size 16 (`bufs`, `buf_chunks`, `datas`, `sizes`,
`input_native`) inside the per-call invocation path. A box with
more than 16 inputs hits a hard error from the dispatcher. This
contradicts the "no static ceilings" principle resolved in Q1.

**Fix:** convert the five arrays to VLAs (`char *bufs[n];` etc.,
C99). Cost is zero — VLAs adjust the stack pointer by
`n * 5 * sizeof(void*)` with no allocator call. Keep a sanity
guard at a generous ceiling (e.g. 4096 inputs) so a runaway
loader can't blow the stack; the existing error-return path
stays in place above that.

**Related but lower priority:** the `branch[24]` stack buffers
at dispatch.c:629/647/678 hold a `snprintf("out_%u", ...)`
result. Worst-case formatted length is 15 bytes; 24 is
arbitrary-but-safe and `snprintf` truncates without overflowing.
Change to a named constant (`MAX_BRANCH_NAME = 16`) for hygiene
when the VLA work lands. Not a real bug.

### Q1.5 — The shape of the `create_box` argument: box JSON schema as the contract

`create_box`'s argument is **one structured value whose shape is
exactly the box JSON schema**. In Lua, a table; in C, either a
struct or a JSON string built by the caller; in Bash, a JSON
string. The runtime converts to JSON internally (same path as the
universal wire format), validates against the box schema, and
instantiates the box record. Anything you can write into
`boxes/<id>.json` on disk, you can pass to `create_box` at
runtime.

**Why this shape (not a fat positional signature, not key/value
varargs, not a builder pattern):**

- The box JSON schema **is** the source of truth on disk. Having
  `create_box` mirror that schema means there is no parallel
  "create_box parameter list" to maintain — adding a new schema
  field works in `create_box` automatically.
- New customization toggles (priority, debug-instrumentation
  flags, custom translation specs per port, emoji labels for tap
  mode, etc.) land in the schema and become available to
  `create_box` callers in the same change.
- Languages without rich native structured types (Bash) can use
  JSON strings; languages with rich types (Lua tables, C structs)
  can use native shapes that the spec converts.

**Structural metadata vs runtime values — the split this design
enforces.** The argument carries **structural metadata** (id,
kind, lang, ref, fn, routing, inputs/outputs schema, position).
Per-invocation **runtime values** (the data flowing through
wires) keep flowing through input slots as before. They do not
mix. Asking "should priority arrive on an input slot?" is the
wrong question — priority is metadata, not a value flowing
through the dataflow. Mixing the two would make a box's *shape*
depend on runtime state, which the slot store and the dispatcher
both assume is fixed once the box exists.

### Box customization surface — what `create_box` accepts (audit, 2026-05-23)

Maintained list of every per-box customization that currently
exists or is on a known open issue. When a new toggle is added
anywhere in SoraMech, **append to this list** in the same change.
The list is a contract reminder: `create_box`'s structured
argument must cover all of these because they are all valid
fields in the box JSON schema.

**Working today (in shipped box JSON):**

- `id` — required, unique within the map.
- `kind` — call / read / data / write / comparator / iterator /
  randomizer / weighted / distributor / map (the encapsulated
  kind from 248).
- `lang` — lua / c / bash / map (issue 303 spec registry).
- `ref` — file path to the source containing the box's function.
- `fn` — function name within `ref`.
- `inputs[]` — per-port: `name`, `type`, `value` (default),
  `optional`.
- `value` (read-box literal source).
- `path` (data and write boxes — disk file).
- `comparand` (comparator boxes).
- `routing.kind` — plain / comparator / randomizer / weighted /
  iterator / distributor.
- `routing.weights` (weighted), `routing.n_outputs` (iterator,
  randomizer).
- `iterator_outputs[]` (iterator branch names).
- `output_capacity` (ring buffer cell size hint).
- `connections[]` — outgoing wires: `from_branch`, `to_box`,
  `to_input`. (May be omitted in `create_box` calls if the caller
  will follow up with `connect()`.)
- `ui.x`, `ui.y` (canvas position).

**Coming from open issues (must be supported by `create_box`
when they land):**

- `external` block on `data` boxes — issue 248 (externally-supplied).
- `external` block on `write` boxes — issue 248 amended (externally-consumed).
- `outputs[]` array — issue 248, encapsulated boxes only, exception
  to issue 218's single-output-per-box rule.
- `custom_translation_spec` per input port — issue 246.
- `encap_mode` (`inline` | `runtime`) — issue 248, on `kind: "map"` boxes.
- Four-emoji label for tap mode — issue 249.
- Debug instrumentation flags per box — issue 247.
- `priority` — not yet specced; would feed the pool's
  task-scheduling priority. Worth its own issue when introduced;
  goes into the schema and into this list.

**Derived (NOT settable via `create_box`):**

- Wire color — derived from producer/consumer language pairing
  (issue 245). The compiler colors wires; the user doesn't.
- Per-edge native flag — derived by the graph loader at compile
  time from input/output language pairs (issue 312). Not a
  user-set field.

### Q2 — Which input port does `connect` target? — **RESOLVED: connection-entry shape from box JSON schema**

**Resolution.** `connect`'s argument is **one (or many)
structured value(s) whose shape is exactly a connection entry
from the box JSON schema's `connections[]` array**:

```
connect({from_box=A, from_branch=null, to_box=B, to_input="x"})
```

Variadic for multiple connections at once:

```
connect({from_box=A, from_branch=null, to_box=B, to_input="x"},
        {from_box=A, from_branch="gt",  to_box=C, to_input="value"},
        {from_box=A, from_branch="lt",  to_box=D, to_input="value"})
```

The "which port" question is answered by **`to_input` being
explicit in every connection entry**. No implicit-first-free
magic, no pair-syntax that has to be remembered separately from
the JSON shape, no per-port sub-handles on the box handle. The
shape on disk and the shape in the API call are identical.

**Why this shape (consistent with Q1.5).** The schema is the
API. Every place SoraMech needs a "way to describe a box" or a
"way to describe a wire," the same JSON shape we already have on
disk is what the API accepts. Two artifacts (the schema, the
API) collapse to one artifact, and every future schema change
updates the API for free.

**Construction in each language — `native_to_json` is the answer.**

Each language spec already implements `native_to_json` for the
dual-ring wire format (issue 312) — that's how a Lua box's
return value crosses into a C consumer. The *same callback*
converts the user's spec-side value into the JSON shape the
runtime ingests. No new spec contract; no new callback; adding
a new language gives self-construction for free.

- **Lua:** table literal. Idiomatic, terse.
  ```lua
  soramech.connect({from_box = "a", from_branch = nil,
                    to_box = "b", to_input = "x"})
  ```
- **C:** designated-initializer struct (`connection_t c = {.from_box="a", ...};`)
  for the common case; varargs JSON-string helper for dynamic
  fields. The struct definition lives alongside the C spec's
  header.
- **Bash:** JSON string. Bash has no native dict; JSON strings
  are how it talks to everything anyway.

**Benefits that stack from machinery reuse:**

- No new callback on the language spec — `native_to_json` is
  already required for wire crossings.
- One validator in the runtime — the box JSON schema validator
  used for `boxes/<id>.json` files at graph load handles
  runtime-constructed connections as-is.
- Persistence is trivial — if `create_box` ever grows an "also
  write to disk" option, the JSON is already in hand.
- The connections inside a `create_box` call's structured value
  (the box's own `connections[]` field) and standalone `connect()`
  calls produce the same internal records.

**What this resolution does NOT decide:** the precise validation
error policy when a `to_input` names a non-existent port (likely
hard-crash per Q3 territory), and whether `connect()` accepts
the `from_box` as a handle from `create_box`, as the box's
string id, or both (likely both, since both can be resolved to
the same internal record). These fall under Q3 and Q4
respectively.

### Q3 — Bool return versus hard-crash — **RESOLVED: hard-crash with reason**

**Resolution.** `connect` returns nothing (`void`). On any
failure it aborts the program with a plain-English message
naming the boxes, the attempted wire, and what went wrong:

```
soramech: cannot connect 'a' → 'b'.in_x
  reason: 'b' has no input port named 'in_x'
  available ports: 'count', 'label'
```

Matches the project-wide error policy in CLAUDE.md ("prefer
error messages and breaking functionality over fallbacks";
"nil checks are just asking for errors"). A bool return invites
silent ignoring — if the user is calling `connect` they
intend the wire to exist; any reason it doesn't is a bug worth
surfacing immediately, not a normal-flow result to branch on.

The corresponding `create_box` rule: failure to instantiate a
box (missing file, malformed schema, unknown spec language,
duplicate id) is also a hard crash with reason. No box handle
is returned for a failed creation — the program aborts before
any caller has the chance to mis-use a null handle.

### Q4 — What is the "box handle" actually? — **RESOLVED: auto-generated box_id string**

**Resolution.** The handle returned by `create_box` is **a
string — the box's `id` field from the box JSON schema**. If
the caller's spec value supplied an explicit `id`, that string
is returned. If `id` was omitted, the runtime mints an
auto-generated UUID-style string (e.g.
`auto_8f3c2a1e9b4d`) and uses it as the box's id.

The handle is therefore identical to the box_id used everywhere
else in the system: in `boxes/<id>.json` filenames, in the
`from_box` / `to_box` fields of connection entries, in dispatch
log lines, in the run-output JSONL. There is no separate handle
type, no fast in-process pointer that has to be unwrapped at
wire crossings, no struct carrying both an id and a pointer.

**Why a string id (not an in-process pointer, not the function
name).**

- *Not an in-process pointer*: the handle has to survive being
  passed to `connect()` from any worker, being persisted to
  `tmp/last-run.jsonl`, and potentially being carried as a value
  on a wire to another box. The cross-language wire format
  explicitly forbids handles that back-route to a specific spec
  (see `feedback_cross_language_pitfalls`). A string id has
  none of those problems.
- *Not the function name*: multiple boxes can run the same
  function with different wiring, different input slot widths,
  different external bindings. The id distinguishes them.
- *Why UUID-style auto-generation*: when a runtime caller
  constructs a box dynamically, they often don't have a
  meaningful name in mind. Auto-generation removes the need to
  invent one and removes the risk of collision with an
  existing id. Callers who *do* want a meaningful name (for
  log readability) can pass `id = "my_planner_step_3"`
  themselves.

**Internal lookup cost.** The dispatcher resolves
box_id → box record via a hash table (already needed for the
existing static graph load path). One hash lookup per `connect`
call, amortized to ~zero for the steady-state wire-push hot
path (which holds a direct pointer cached at graph-build time).
Box destruction (315) removes the hash entry as part of the
refcount-zero teardown.

### Q5 — Does `create_box` trigger the build pipeline? — **RESOLVED: both, via a compile cache in tmp/**

**Resolution.** Both runtime and compile-time paths exist, and
the runner stays ignorant of the compiler's internals by treating
the compiler as an external shell-out tool (which it already is:
`scripts/soramech-compile.sh`).

**The compile cache.** A directory under `tmp/` holds compiled
artifacts keyed by a content hash of (source file + spec
language + relevant compile flags). When `create_box` resolves
a `ref` + `fn`:

1. Compute the cache key from the source's contents and the
   language spec.
2. Look up the key in the cache.
3. **Hit:** `dlopen` (or equivalent for the language) the cached
   artifact and proceed.
4. **Miss:** shell out to `scripts/soramech-compile.sh` with the
   source path and target artifact path; wait for completion;
   verify exit status; dlopen the freshly built artifact.

The runner never links the compiler, never imports
compiler-internal modules, never duplicates compiler logic. It
only knows "if artifact X exists in the cache, use it; otherwise
run this script." The three-program separation in
`project_soramech` (server / runner / editor) extends cleanly to
"runner treats compiler as external tool, same as it treats
language-spec subprocess binaries."

**Compile-time precompilation as optimization.** When the
compile pipeline can statically determine that some
`create_box(spec)` calls have constant `ref` + `fn` (e.g. the
spec is a literal table in the caller's source), it can
pre-populate the cache during the compile step. Runtime calls
to `create_box` with those refs hit the cache immediately, no
shell-out latency. This is purely an optimization — correctness
doesn't depend on it. Defer the analysis pass until profiling
shows the runtime compile-on-demand path is a bottleneck.

**Cache invalidation.** Cache key includes source content hash,
so changing a source file naturally produces a new key (and the
old entry becomes garbage to be collected during run teardown).
No invalidation logic needed; content hashing is the
invalidation.

**Why not pure runtime / pure compile-time:**

- *Pure runtime*: every `create_box` call would pay shell-out
  latency even for code that hasn't changed since the previous
  run. Wasteful in tight loops or hot planner boxes.
- *Pure compile-time*: dynamic `create_box` calls (where `ref`
  comes from a runtime value, e.g. a planner box choosing what
  pipeline to construct based on LLM output) can't be analyzed
  ahead of time. Forbidding them gives up the use case that
  motivates self-construction in the first place.

The cache makes both paths cheap when they apply and removes the
need to pick one.

## Suggested implementation steps (after Q1–Q5 are resolved)

1. Resolve the open design questions; update this issue with the
   chosen shape before any code is written.
2. Add the two built-ins to the language-spec contract
   (`lang_spec_t` — the runtime exposes them via the spec's
   built-in table; each language spec wraps them in its native
   calling convention).
3. Extend the slot store with the growth primitive Q1 selects
   (if Q1 picks (a) or (c)).
4. Add dispatch-layer logic for newly-created boxes — they need to
   participate in the same push-on-arrival mechanism as static
   boxes.
5. Tests under `tests/`: a small map whose first box uses
   `create_box` + `connect` to build a downstream pipeline, then
   runs end-to-end and validates outputs.
6. Update `docs/004-ipc-and-threading.md` to describe the
   self-construction contract and the slot-store growth shape.
7. Update phase-3-progress.md.

## Related issues and documents

- 248 — encapsulated map as box (the function-like abstraction
  these primitives could be used to construct ad-hoc).
- 302 — wire value slot store (the structure that needs the growth
  primitive in Q1).
- 218 — single output per box (justifies the variadic-on-destinations
  shape).
- 217–222 — compile pipeline (relevant to Q5).
- `docs/004-ipc-and-threading.md` — current threading model these
  primitives must fit inside.
