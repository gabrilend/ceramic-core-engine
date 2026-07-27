# SoraMech — Architecture

Docs 001–006 describe SoraMech from the outside: what it is, what
a map looks like, how to run one, how to write a box. This doc is
the inside view — the components, what each one owns, and the path
a single value takes from a JSON file on disk to a byte written
downstream.

Read this if you're changing the runtime. If you're writing maps or
boxes, 002 and 005 are the docs you want.

## Three programs, one format

Nothing in SoraMech is a framework that the other parts plug into.
There are three independent programs that never call each other,
and a directory format all three can read:

| Program | Lives in | Speaks to |
|---|---|---|
| **The editor** | `assets/` — canvas, inspector, API client | HTTP, to the server |
| **The file server** | `src/005-http-server.lua`, `src/006-server-main.lua` | The filesystem |
| **The pool runner** | `src/008`–`src/020` + `libs/` | The filesystem, then the CPU |

The editor never knows whether a runner is active. The runner never
knows whether the editor is open. The server is a file proxy and
understands neither. **The map directory is the entire interface
between them** — if a program can read and write those JSON files,
it is a first-class participant.

That constraint is the load-bearing design decision in the project.
It is why the map format is plain JSON-per-box rather than a binary
graph or an in-memory object model: a format only one program can
read would collapse the three programs into one.

A fourth participant exists and is being retired: the **phase 2
synchronous runner** (`src/003-loader.lua`, `src/004-executor.lua`,
`src/007-runner-main.lua`). It executes the same map directories
single-threaded. Its retirement is tracked on the phase 3 progress
page.

## The map directory is the program

```
my-map/
  meta.json        — name, description, entry_box_id (vestigial), src_dirs
  boxes/*.json     — one file per box; wires live on the producer's
                     connections[] array
  src/*            — box source, in whatever languages the map uses
  data/*.json      — persistent map-owned data
  tmp/             — symlink to RAM; run logs and scratch
```

There is no file that lists the boxes. The directory listing is the
box list, and the graph is reconstructed by resolving every
`connections[]` entry against the box ids found on disk. Adding a
box is creating a file.

Format details are in `002-map-model.md`. One caveat that belongs
here rather than there: `meta.json`'s `entry_box_id` does **not**
select where a run starts. Entry boxes are derived from topology —
see "Starting a run" below, and issue 206 (entry box designation),
which owns retiring the field.

## The pool runner, bottom to top

Every module is one file in `src/` with a companion `.info.md`
describing its public surface. The stack:

```
        008-pool-runner.c        main(): owns everything below
                 │
    ┌────────────┼────────────┬──────────────┐
    │            │            │              │
012-dispatch  010-graph-   011-spec-    014-event-queue
    │          loader       registry          │
    │            │            │        013-jsonl-events
    │            │            │
    │            │      langs/<lang>/spec.so  ← dlopen'd plugins
    │            │            │
    │            │      020-sentinels    (shared with the plugins)
    │            │
009-slot-store ──┘
    │
016-unified-allocator
    │
libs/task-pool/pool.c        libs/json/json.c
```

What each one owns:

- **`libs/task-pool/pool.c`** — the worker threads and the work
  queue. It knows nothing about boxes; it runs opaque actions with
  a priority. One worker per CPU by default, `SORAMECH_WORKERS=N`
  to override.
- **`libs/json/json.c`** — the project's own C JSON parser. Written
  in-tree rather than vendored so the runtime has no external
  dependency in its load path.
- **`016-unified-allocator.c`** — the one runtime heap. Pre-warms a
  free-list per output size the graph declares, so an allocation
  during a run is a list pop rather than a `malloc`. Frees are
  refcount drops that eagerly fuse with adjacent free chunks.
  Everything hot allocates here: slot cells, value payloads, and
  the task structs themselves.
- **`009-slot-store.c`** — the memory for values in flight. One
  slot per input port, each a small ring of cells. This is where
  the two input methods live: a *consuming* port pops one delivery
  per fire, a *referencing* port peeks the same value forever.
- **`010-graph-loader.c`** — map directory in, `graph_t` out.
  Parses, validates per-box schema, resolves every wire endpoint to
  an integer index, rejects illegal cycles, derives the entry set,
  classifies each edge as same-language or cross-language, sizes
  the slots, and resolves each call box to a language spec.
- **`011-spec-registry.c`** — scans `langs/<name>/spec.so`,
  `dlopen`s each, reads the exported `soramech_lang_spec` symbol.
  This is the entire language-plugin mechanism.
- **`012-dispatch.c`** — the action a worker runs for one box fire:
  read the inputs, invoke the box through its language spec, pick
  the output branch per the routing kind, push into downstream
  slots, and re-spawn whatever the push made ready.
- **`013-jsonl-events.c` / `014-event-queue.c`** — the transcript.
  The queue is a multi-producer ring the workers push onto without
  blocking; a dedicated thread drains it to the JSONL file, so no
  worker ever waits on disk.
- **`020-sentinels.c`** — the shared vocabulary for values that
  cannot cross a language boundary intact (a Lua closure, a file
  handle). Linked into the language plugins, not into the core.

### One module is orphaned

`015-large-value-heap.c` is compiled into the binary and has a
passing unit test, but **no production code includes its header**.
The unified allocator (016) took over variable-size payloads and
015 was never removed; its own test is what keeps it alive. Treat
its `.info.md` as describing a component that is no longer wired
in. Removing it is unclaimed work.

## The path of one value

This is the whole runtime in one sequence. A worker is running box
`A`, which is wired to box `B`'s `x` port.

1. **Load.** `graph_load` builds `graph_t`. Every read box's bytes
   are read once and cached on its box record. Every consumer gets
   a per-port list of its read-box predecessors.
2. **Slots.** `graph_attach_runtime` allocates one slot per input
   port, sized from the producers' declared output capacities, and
   picks each port's input method — referenced for a typed-in
   literal with no wire feeding it, consuming for everything else.
3. **Literals.** The dispatch's startup pass delivers every
   typed-in constant to its port exactly once.
4. **Entry.** Every derived entry box is spawned as an initial
   task.
5. **Gather.** A worker picks up `A`'s task and reads its inputs.
   For each port it takes a queued value from the slot; if the slot
   is empty and the port has read-box predecessors, it pulls the
   cached bytes directly instead.
6. **Invoke.** The dispatch calls `A`'s language spec's `invoke`,
   asking for either native bytes or JSON depending on how the
   loader classified the outgoing edge.
7. **Route.** `A` returns one value. The routing kind decides which
   of `A`'s wires receive it — all of them for `plain`, one branch
   for the six selective kinds.
8. **Push.** The value is written into `B`'s `x` slot.
9. **Wake.** The push calls the readiness check on `B`. If every
   required port of `B` now has a value, `B` spawns as a new task
   and the sequence repeats from step 5.
10. **Quiescence.** When the queue is empty, no task is in flight,
    and no slot holds a queued value, the run is done. For a batch
    map that means exit; for a self-feeding map it means waiting.

Steps 5 through 9 are the entire execution model. Everything else
in the runtime exists to make those five steps cheap or observable.

### Starting a run

The entry set is derived, not declared. A box qualifies when it is
a `call` or `write` box (a `read` box never runs as a task at all)
and every non-optional input port has zero non-read feeders. A
zero-input call box qualifies vacuously. A map can therefore have
many entry boxes, and usually does.

### Concurrency

Every box is multi-spawn: the runtime never gates re-entry, and one
box may be running on several workers at once. The guarantee is
that each fire owns its input values and owns a unique return slot
for its output. Thread safety inside a box function is the box
author's responsibility — `005-writing-boxes.md` carries the
per-language contract.

## The language plugin boundary

A language is a directory under `langs/` containing a `spec.so`
that exports one `soramech_lang_spec` struct. The registry finds
it, `dlopen`s it, and the dispatch calls through it. Nothing about
Lua, C, or Bash is special-cased in the core — the three shipped
languages use exactly the interface a fourth would.

A spec owns four things: per-worker `init` (each worker gets its
own interpreter state, so Lua's `lua_State` is per worker rather
than per box), `invoke`, a declaration of which languages it can
serialise values for, and — optionally — a report of what form it
actually wrote, for the one case where a spec's output diverges
from the form the dispatch asked for.

An undeclared cross-language pair is a fatal load error naming the
pair, not a best-effort coercion. That is the fallbacks-are-errors
rule applied at the language boundary.

## Known structural debt

Recorded here because it is architectural rather than a bug in any
one place:

- **`entry_box_id` is mandatory and inert** — issue 206.
- **`015-large-value-heap.c` has no callers** — see above.
- **The standalone Lua validator is stranded** — it speaks a
  retired schema dialect and nothing invokes it. Issue 103.

## What's next

- [`docs/002-map-model.md`](002-map-model.md) — the format this
  architecture loads.
- [`docs/004-runtime.md`](004-runtime.md) — running a map, the
  transcript, the compile pipeline.
- [`docs/005-writing-boxes.md`](005-writing-boxes.md) — the box
  author's contract, including thread safety.
- [`docs/006-test-coverage-map.md`](006-test-coverage-map.md) —
  what is tested and what runs the tests.
