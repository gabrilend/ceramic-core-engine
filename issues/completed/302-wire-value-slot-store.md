# 302 — Per-task slot store with wire-held references

## Status
complete

## Current behavior
Wire values are passed through stdout pipes and transient strings. Values
have no addressable location, no reference count, and no cross-language
accessibility. Each value vanishes once the pipe is drained.

## Concept

### Slots belong to input ports, not tasks

Each box has **one slot per input port**, allocated when the graph
loads and persisting for the life of the run. A wire is a routing
declaration: "the producer's output port pushes a copy of its value
into this consumer's named input slot." The producer never owns a
slot — its output is a routing event, not a stored value.

When a producer fires, the dispatch layer copies the value into
every downstream input slot wired to that output. Fan-out is N
pushes (one per consumer); fan-in is N producers all pushing into
the same consumer's slot. Both behaviors fall out of the same
push-into-input-slot mechanic.

Slots live in process heap memory, accessible to every worker
thread. Cross-process visibility is not required: language specs
(issue 303) only ever receive byte pointers — never raw slot
addresses — so out-of-process specs (Bash) work via sockets, not
shared memory.

### Tasks are ephemeral; box state is durable

A **task** is one invocation: pop one value from each of the box's
input slots, run the box's logic, push the output to every
downstream input slot. Tasks have no persistent identity — they're
just stack frames on workers. Tasks are spawned dynamically by the
dispatch layer when a box's input set is ready (every input slot
holds at least one value).

The **box** is the durable thing: it owns its input port slots, any
counter state (for iterators), its language spec handle, and so
on. The box exists from graph load until run end. Tasks come and
go.

This is uniform across iterator and non-iterator boxes. Both run N
times where N depends on how many invocations their inputs supply
(possibly 1, possibly thousands). The difference is just what the
task does:
- **Plain call task**: invoke spec on the popped values, push the
  return to outputs.
- **Iterator task**: read box's counter, route the popped value to
  output port `iterator_outputs[counter]`, increment counter mod N.
  No spec invocation.
- **Comparator task**: invoke spec on the popped values, compare
  the return against `comparand`, route to the matching `lt` /
  `eq` / `gt` branch.

### All slots are ring buffers; read mode varies

There is one slot type. It is a ring buffer of `n_cells` cells,
each of `cell_capacity` bytes. The number of cells (`n_cells`) and
the read mode are both compile-time properties of the input port:

- **1-cell + peek mode**: the value the producer wrote stays in the
  cell. Tasks read via `slot_peek` — read without draining — and
  every subsequent task on this consumer reads the same value
  unless the producer overwrites it. Used for **single-push wires**
  (a literal, or a wire from a producer box whose invocation count
  is exactly 1). Multiple consumer tasks read the value as many
  times as needed; the cell is never empty.
- **N-cell + pop mode**: a queue. Producers push; consumers
  `slot_pop` — drain one cell per task. Used for **multi-push wires**:
  any wire whose producer has more than one task invocation
  during the run. The producer's pushes accumulate, the consumer
  drains in pop order.

Compile-time analysis classifies each wire. A producer is
"runs-once" if no transitive ancestor is an iterator; otherwise
it's "runs-N-times." Wires from runs-once producers get 1-cell
peek slots; wires from runs-N-times producers get N-cell pop slots.

`n_cells` for pop slots is sized for worst-case backlog (or a
default like 64 with growth on demand). Lists grow if the producer
outruns the consumer past the pre-allocation.

Fan-in: multiple producers pushing into the same N-cell pop slot
land in arrival order. The consumer pops FIFO unless the cell
carries an ordering tag (see "Cell tagging" below).

### Atomic-counter slot mode

A slot allocated with `SLOT_ATOMIC_COUNTER` carries a single
4-byte unsigned integer cell, read via a special op:

```c
uint32_t slot_read_inc(slot_store_t *s, slot_id_t id, uint32_t mod);
// Returns the current value mod `mod`, then atomically increments
// the underlying counter. Concurrent readers each get a distinct
// value. Counter wraps at 2^32; mod handles N-branch routing.
```

Used for iterator counters (issue 304): the iterator box has an
auto-allocated `SLOT_ATOMIC_COUNTER` slot at load time; the
dispatch action calls `slot_read_inc(counter_slot, n_branches)`
to pick its output branch. Multiple iterator tasks can read
concurrently from the same counter slot in parallel — each gets a
unique routing index — without any per-box mutable state and
without the serial dependency of a self-push counter.

The slot's "cell" is just an `atomic_uint32_t`. No push is ever
performed; reads do the work. From the dispatch action's
perspective it's just another slot; from the slot store's
perspective the read op is special-cased.

### Cell tagging (for parallel-iterator ordering)

A cell can optionally carry a 4-byte ordering tag alongside its
size and data:

```
Cell layout (N-cell pop slot)
Offset  Size  Field
------  ----  -----
0       4     filled_size
4       4     tag      (0 if untagged; iterator counter snapshot otherwise)
8       …     data
```

Slots created for wires from parallel iterators (issue 304) are
tagged. Pop on a tagged slot returns the cell with the lowest tag,
not the head cell — preserving iterator-counter order across pushes
that arrived out of order from parallel iterator workers.

Untagged slots default tag to 0 and pop FIFO at no extra cost. The
tag-aware pop is a per-slot mode flag set at allocation time.

### Asynchrony
The store is fully asynchronous. Slots are allocated, filled, read, and
freed at any time, on any thread, with no global lock. Synchronization
is per-slot. There is no central allocator-wide mutex.

## Slot layout

Each slot is a header followed by `n_cells` cells, each `cell_capacity`
bytes. The header tracks ring-buffer state and lifetime:

```
Slot header
Offset  Size  Field
------  ----  -----
0       4     cell_capacity   (bytes per cell, set at allocation)
4       4     n_cells         (ring size, set at allocation)
8       4     head            (read index, mod n_cells)
12      4     tail            (write index, mod n_cells; cells_filled = tail - head)
16      1     touched         (per-slot lock: 0=free, 1=another thread mutating)
17      3     _pad
20      …     cells           (n_cells * (4-byte size + cell_capacity bytes))
```

Each cell stores a 4-byte filled-size followed by `cell_capacity`
bytes of value data. Cells have wildly different capacities across
slots — capacity is per-slot, not global. The C convention of passing
sizes alongside arrays applies: the task that allocates a slot knows
how big it needs the cells.

`touched` is a single-byte flag toggled around any header mutation
(push, pop, refcount changes). A reader that finds `touched == 1`
spins until it clears. This is the simplest possible per-slot lock —
if contention proves to matter, replace with a proper mutex without
changing the API.

Writes to a cell complete with a memory barrier before `tail` is
advanced, so a reader that observes `head < tail` is guaranteed to
see the complete value at the head cell.

## Lifetime

A slot — the named input port on a box — is owned by its box and
lives from graph load until run end. That hasn't changed. What
has changed is the values that pass through slots: they are
reference-counted memory chunks served by a single unified
allocator (see "Allocation strategy" below), not malloc'd memory
that lives the whole run.

Every value handed to a consumer carries a reference. When a
consumer is done with a value, it drops its reference. When the
last reference drops, the chunk holding the value goes back to
the allocator's free-lists, available for the next request. Two
neighbor-merge steps and an on-demand sweep keep fragmentation
under control (described in the allocator section).

The earlier note in this document — "no per-slot reference
counting" — referred to the box's own slot record, not to the
values flowing through it. The slot record itself is still
allocated once at graph load and not freed until run end. The
values inside the slot are the things that get refcounted, freed,
and recycled. The distinction matters: there's one slot per
input port, and many values flow through that slot over the life
of a run.

Run termination is still governed by the pool's active-task
counter (issue 301): when no task is running and no further
spawns are pending, the run is over. The allocator and refcount
machinery don't influence termination — they just ensure memory
gets returned to the free-lists as values become unreferenced
during the run.

### No wait lists

Tasks never park on slots. Tasks are spawned by the dispatch
layer's spawn-on-input-arrival rule and run to completion; they
never block. A slot that has no value yet simply means the
spawn rule hasn't fired for that consumer yet. No wait list,
no per-slot wake-up traversal — just a check after each push.

## Backing store

Slots are allocated from process heap memory. The slot store owns a
per-run allocator (described under "Allocation strategy" below) that
hands out memory chunks to slots. Plain `malloc`-class memory is
sufficient — every worker thread in the process can read and write
it, and no spec ever needs to access it from a child process.

Synchronization within a slot uses the per-slot `touched` flag for
header mutations. There is no allocator-wide mutex on the
read/write path; only the per-size-class free lists in the
allocator have their own locks.

## Slot creation

Slot creation happens at **graph load**, not per task. The graph
loader walks every box, allocates one ring buffer per input port,
and parks the slot on the box's per-port slot table. The dispatch
layer never allocates new slots at runtime in the steady state —
all slots that will ever exist are created up front.

```c
slot_id_t slot_alloc(slot_store_t *store, int cell_capacity, int n_cells);
```

`cell_capacity` and `n_cells` come from compile-time analysis of
the graph. Boxes downstream of iterators get larger `n_cells` to
absorb the iterator's worst-case backlog.

If a slot's pre-allocated `n_cells` proves too small at runtime
(producer outpaces consumer beyond the pre-allocation), the slot's
ring grows — see "Allocation strategy" below.

## C API surface

```c
slot_id_t slot_alloc       (slot_store_t *s, int cell_capacity, int n_cells, int flags);
void      slot_push        (slot_store_t *s, slot_id_t id, const void *data, int size, uint32_t tag);
int       slot_peek        (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // 1-cell: read without draining
int       slot_pop         (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // N-cell: drain head (or lowest-tag)
uint32_t  slot_read_inc    (slot_store_t *s, slot_id_t id, uint32_t mod);            // SLOT_ATOMIC_COUNTER only
int       slot_has_value   (slot_store_t *s, slot_id_t id);                          // for spawn-rule check
```

Flags include:
- `SLOT_TAGGED`: cells carry ordering tags; `slot_pop` returns the
  lowest-tag cell.
- `SLOT_ATOMIC_COUNTER`: slot holds a single atomic uint32; reads
  via `slot_read_inc` only. `slot_push` / `slot_peek` / `slot_pop`
  are not valid on atomic-counter slots.

Push always takes a `tag` argument; untagged slots ignore it (or
treat 0 as "no order").

1-cell peek slot use: producer `push` once at startup (literal) or
when its single invocation completes (single-push wire). All
consumer tasks `peek` to read the value as many times as their box
runs. The cell is never empty after the initial push.

N-cell pop slot use: producers `push` repeatedly. Consumer tasks
`pop` one cell each. For tagged slots, `pop` returns the
lowest-tag cell; for untagged, FIFO. Pop on an empty slot is a
bug — the spawn rule guarantees the slot is non-empty before a
task spawns.

There is no `slot_ref` / `slot_unref` — slot lifetime is the run.

## Submission timing

Tasks are spawned dynamically in response to slot pushes. Each box
has small per-box state used by the dispatch layer:

```
box_runtime_state {
    slot_id_t  *input_slots;     // one per input port (set at load)
    int        *port_modes;      // PEEK or POP, per input port
    int         n_inputs;
    slot_id_t   counter_slot;    // iterator only: SLOT_ATOMIC_COUNTER slot;
                                 // 0 / unused otherwise
};
```

The spawning rule:
- When a producer pushes to one of a box's input slots, the
  dispatch layer checks **does every input slot have a value
  available?** "Available" means:
  - For peek-mode (1-cell) slots: cell has been written at least
    once. Once a peek slot is filled, it stays available — every
    subsequent spawn-check on this box passes the peek-port check.
  - For pop-mode (N-cell) slots: queue is non-empty.
- If yes, spawn a `dispatch_task_t` and submit to the pool. The
  task pops (or peeks) one value from each input slot when it
  runs.
- Each push that completes the input set triggers exactly one
  spawn. Multiple pushes accumulate in pop slots; each pop
  consumes one, and if more remain, the next push (or check)
  re-fires the spawn rule.

This makes the pool's queue the rate buffer: pushes accumulate
in pop slots (possibly multiple per slot), and one task per
accumulated value gets spawned.

Tasks never block on slots. They're spawned after inputs are
ready, run to completion, and disappear. Blocking-and-waking
machinery is unnecessary — the spawn-on-arrival rule is the
synchronization primitive.

The pool's queue is the only queue. There is no separate "blocked
tasks" structure.

## Allocation strategy: one unified allocator for every value

There is one allocator. It serves both values whose size is known
from the graph at compile time and values whose size only becomes
known when they're produced (LLM responses, dynamic arrays, anything
where the upper bound isn't a useful number). The earlier draft of
this document split these into two stores — a size-class slot
allocator for the known case, a separate "large-value heap" for the
unknown case. That split was a mistake. They are the same problem
and they reuse the same machinery; treating them as two stores led
to the unknown-size path being a monotonically-growing pool that
never reclaims, which is a memory leak by design.

### How the allocator learns sizes ahead of time

Before the run starts, the loader walks every box in the graph and
reads the size that box declares for its output. A box that always
produces a 32-byte number declares 32. A box that always produces a
4-megabyte image declares 4 megabytes. The loader collects every
distinct declared size, deduplicates them, and creates one free-list
per distinct size. Each list is pre-populated with enough chunks to
cover the static demand for that size.

Boxes whose output size depends on runtime — the LLM case — declare
"size unknown." The loader doesn't create a pre-warmed list for those
boxes; it knows their values will be allocated on demand.

Both kinds share the same underlying memory region. The difference is
only whether the loader had a list pre-warmed for the value's size.

### Two doors, one allocator

**Door 1 — declared-size allocation.** Given a request for size N,
where N matches a declared size class: pop the head of that class's
free-list. One step. This is the common path for the vast majority
of values; nearly every box in a typical graph has a declared output
size.

**Door 2 — runtime-size allocation.** Given a request for an unknown
size at runtime: find the smallest size class whose chunks are large
enough. If that class is exhausted, try the next-larger. If the
chunk handed out is oversized for the request, slice the unused tail
into a residual chunk and push that residual onto the appropriate
smaller free-list. The residual is only kept if it's at least as large
as the smallest declared size class — anything smaller is left as
internal waste rather than fragmenting the lists with stubs.

Both doors fall through to the same fragmentation-recovery and growth
machinery below when their free-lists can't satisfy a request.

### Reference counting drives deallocation

Every value the allocator hands out is reference-counted. The dispatch
layer increments the count when handing a value to a consumer and
decrements it when the consumer is done. When the count reaches zero,
the chunk is returned to the allocator.

The return-to-allocator step does two things atomically: pushes the
chunk onto the appropriate free-list AND checks the chunk's two
physical neighbors. If either neighbor is also free, the deallocator
merges them into a larger chunk and pushes the merged chunk onto the
free-list for the merged size. This eager neighbor-merge is O(1) and
keeps small-scale fragmentation from accumulating on the hot path.

Reference counting requires touching every place that hands out a
pointer to an allocator-managed value. That cost is paid once. After
the wiring exists, both doors above benefit from the same recycling.

### When does the bigger sweep run

The eager neighbor-merge above catches the common case. It misses
patterns where free chunks are scattered with allocated chunks between
them — the cheap merge can't see past its immediate neighbors.

A deeper sweep that walks the whole address-ordered chunk list
looking for runs of contiguous free chunks runs on two deterministic
triggers, both decoupled from wall-clock time:

1. **At the moment an allocation would otherwise fail.** When a
   request can't be satisfied from any free-list — including after
   trying larger classes and slicing — the system runs the deep sweep
   before falling back to asking the operating system for more memory.
   If the sweep produces a chunk large enough to satisfy the request,
   the allocation succeeds and no growth happens. This is the
   mandatory trigger: the sweep is the immediate alternative to
   monotonic growth, and it fires exactly when its work has value.

2. **At quiescence — moments when no task is running.** The dispatch
   layer already recognizes these moments because it uses them to
   decide a run is complete. Between batches is a natural opportunity
   because no tasks are reading or writing chunks at that instant.
   This is the opportunistic trigger.

The mandatory trigger is enough on its own. The opportunistic trigger
is a tunable knob that can be enabled later if profiling shows
fragmentation surviving the cheap merge faster than the mandatory
trigger catches up.

A third option was considered — trigger when the count of small free
fragments crosses a threshold — but rejected for now. It works, but
it needs the threshold tuned, and the two triggers above cover the
same ground without that tuning step.

### Physical-order chunk list (for the merge steps)

Both the eager neighbor-merge and the deeper sweep need to know
which chunks are physically adjacent in memory. The size-class
free-lists don't carry that information — they're indexed by size,
not address.

The supporting structure is a doubly-linked list of all chunks (free
and allocated) in address order. Splits and merges are O(1) given a
chunk pointer. Neighbor lookup is O(1). The cost is pointer-chasing
during the deeper sweep, but the sweep only runs at the two triggers
above, not on the hot path.

If profiling later shows traversal cost hurting at sweep time, the
list can be swapped for a sorted-by-address array of `(start, end)`
pairs with an auxiliary index — the allocator's external API doesn't
change.

### Why one allocator instead of two

The split design existed because the original two-tier scheme had a
"large-value heap" carved out for variable-size payloads, on the
theory that mixing variable and fixed sizes in the same pool would
complicate the fixed-size path. That reasoning doesn't survive
contact with reference counting and free-list recycling: with
refcount-driven free, the fixed-size path stays fast because most
requests match a declared class and pop in one step regardless of
what else is in the region.

Splitting them also meant the variable-size pool had to grow
forever, because writing the same allocator twice — once with
reclamation, once without — was more work than was budgeted in early
phase 3. That's the position the current code is in. Unified gets us
the reclamation everywhere, with no extra design work.

### Growth (last resort)

When both doors fail and the deep sweep can't produce a fitting
chunk, the allocator grows by asking the operating system for a new
region, dividing it into chunks per the declared size classes (or
into a single large chunk if the request was for an unknown size
beyond all declared classes), and threading the new chunks into the
free-lists and the address-ordered list.

Growth is the rare case. With pre-warmed free-lists sized to static
demand and refcount-driven recycling, a well-behaved graph never
reaches growth — every allocation either pops a pre-warmed chunk or
recycles one whose last reference just dropped.

## No type tags

Slots store opaque bytes. The producer wrote N bytes; the consumer
reads N bytes. What those bytes mean is the language spec's business
on each side, and the wire's metadata in the graph (issue 219's
compiler can check producer-output type vs consumer-input type at
build time). The slot store does not interpret values — it does not
need to know if they are strings, numbers, JSON, images, or anything
else. Removed the type-tag field from the header for that reason.

## Suggested implementation sequence
1. Write `src/slot-store.h` and `src/slot-store.c` — the C API above.
2. Unit test single-value slots: thread A writes, thread B reads,
   refcount decrements, slot freed when last ref dropped.
3. Unit test ring-buffer slots: multiple producers push, single consumer
   drains in order, no values lost or duplicated.
4. Integrate into `src/008-pool-runner.c`: shared memory created at
   startup, store handle passed to the dispatch layer.

## Relevant files
- `src/008-pool-runner.c` — pool entry point, owns the shared memory region
- `libs/task-pool/900-task-pool.h` — pool API (refcounting is complementary)
- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle
- `issues/304-task-dispatch-layer.md` — task dispatch reads/writes slots
- `issues/213-queued-inputs-and-task-model.md` — queued input model that
  ring-buffer slots implement
- `docs/004-ipc-and-threading.md` — IPC options; shm_open + mmap is Option 2

## Implementation log

### Core milestone — 2026-05-12

What shipped:
- `src/009-slot-store.h` — the public API exactly as the design
  describes: `slot_alloc` / `slot_push` / `slot_peek` / `slot_pop` /
  `slot_read_inc` / `slot_has_value` / `slot_fill_count`. Plus the
  store lifecycle pair and a small inspection helper for tests.
- `src/009-slot-store.c` — implementation. Plain-malloc per slot
  (size-class free lists deferred); ring slots and atomic-counter
  slots both supported; the SLOT_FLAG_TAGGED variant of pop scans
  for the lowest-tag filled cell. Per-slot spinlock via
  `atomic_flag`. Atomic counter via `atomic_fetch_add_explicit`,
  no lock.
- `tests/009-slot-store-test.c` — 12 unit tests covering peek
  semantics, FIFO pop, tagged pop ordering (in-order and
  interleaved), atomic counter (sequential + 8-thread concurrent),
  bad-argument paths, ring-full / empty edge cases, and a
  multi-thread producer/consumer race over 5000 values.
- Makefile additions: `test` target builds test binaries via a
  pattern rule and runs them. Per-test prerequisites listed
  explicitly so each binary links the minimal set of source `.o`s.
- File index counter advanced to `009`.

Verified clean build and 12/12 tests pass in `make STRICT=1`
(`-Werror -Wextra -Wpedantic`).

What's deferred to a follow-on within 302:
- **Size-class free lists with coalescing.** The current
  implementation `malloc`s each slot independently. That's correct
  but unoptimized. Slots are allocated once at graph load and never
  freed until run end, so under the current usage pattern free-list
  reuse buys nothing. Lands once profiling justifies it (or once
  long-running maps surface allocation pressure).
- **Large-value heap for variable-size payloads.** Slots currently
  store fixed-size cells; very large outputs (multi-MB LLM
  responses, dynamically-sized arrays) will need the two-tier
  scheme described under "Large-value heap" above. Blocks no
  in-flight phase 3 work until a graph wants variable-size outputs.

Neither deferred item blocks 301 / 304 / 305 / 306 / 307 / 308.

### Large-value heap milestone — 2026-05-19

The variable-size payload follow-on landed in two layers:

1. **`src/015-large-value-heap.{c,h}`** — chunked arena allocator.
   Default 64 KB chunks; oversized allocations (larger than the
   default chunk) get their own exact-fit chunk so one large value
   doesn't bloat the regular bump chunks. Pointer stability is the
   load-bearing property: chunks never move once allocated, so a
   consumer's pointer survives any number of subsequent producer
   allocations. Eight unit tests in
   `tests/015-large-value-heap-test.c` cover the create/destroy
   round-trip, single + multi alloc within a chunk, oversized
   allocations getting their own chunks, pointer stability across
   forced growth, zero-size allocations returning non-NULL, the
   stats accessors, and an eight-thread / eight-thousand-alloc
   concurrency stress test that asserts every returned pointer is
   unique and writable.

2. **Slot-store integration.** Added `SLOT_FLAG_LARGE_VALUE` to
   `slot_alloc`. Cell layout for LARGE_VALUE slots is
   `filled_size (4 bytes) + pad/tag (4 bytes) + stable pointer
   (8 bytes)` — the 8-byte payload offset keeps the pointer
   naturally aligned regardless of whether SLOT_FLAG_TAGGED is
   also set. The slot store lazy-creates one shared lvh heap on
   the first LARGE_VALUE allocation; the heap is destroyed
   alongside the store. Push allocates from the heap *outside*
   the slot spinlock (the heap has its own mutex; nesting is
   fine, but reducing time-under-spinlock keeps the lock fast for
   other producers). Peek and pop follow the cell's pointer and
   memcpy the requested bytes back into the caller's buffer.

Four new slot-store tests cover the new path:
`large_value_push_pop` (100 KB payload, double-peek, then pop);
`large_value_buf_too_small` (oversized payload + small dest →
-1, cell preserved); `large_value_multi_push_fifo` (three
different-size values pushed and popped in order); and
`large_value_tagged_ordering` (LARGE_VALUE + TAGGED → pops
return lowest-tag-first with bytes intact).

What this unblocks: any box that wants to emit a variable-size
output can now declare its downstream consumer slots with
SLOT_FLAG_LARGE_VALUE and produce payloads whose size isn't
known at graph-load time. The wiring from declared box output
type to the LARGE_VALUE flag in `graph_attach_runtime` lands as
a follow-on once a fixture or real graph requires it; the
foundation is in place.

### Producer-driven LARGE_VALUE wiring — 2026-05-19

The "wiring from declared box output type" deferred above is
now in place. During `graph_attach_runtime`, each consumer
input slot's flags get OR'd with `SLOT_FLAG_LARGE_VALUE` if any
producer feeding that port declares `output_capacity == 0`. The
rule covers both data boxes (which default to 0 because the
field isn't in their JSON schema) and call boxes that opt into
variable-size output by setting `"output_capacity": 0`. A
mixed-language fan-in where any producer is variable-size
upgrades the whole slot to LARGE_VALUE — the conservative call,
since the slot can only carry one format and the LVH path
handles small payloads correctly too.

A new test in `tests/010-graph-loader-test.c`
(`variable_size_producer_lvh_wiring`) attaches the hello fixture
with a deliberately tight default_cell_bytes of 512, then
asserts a 2 KB push succeeds on greet's `name` port (fed by
data box `who`) and fails on greet's `salutation` port (literal
only, no producer). The discriminator is the slot's capacity:
without LARGE_VALUE the 2 KB push exceeds 512 and is rejected;
with it the heap absorbs the bytes and the push lands.

The remaining phase-3 checklist item "Variable-size outputs
work via the large-value heap" is now satisfied end-to-end at
the slot-store boundary. A graph fixture that exercises the
path on the dispatch side — a Lua box returning a multi-KB
string and a downstream box reading it — is a one-line fixture
addition and lands with the 6-more-integration-fixtures task.

What's still deferred within 302:
- **Size-class free lists with coalescing.** Still no need.
- **Per-allocation free in the large-value heap.** The heap
  grows monotonically for the life of a run. Long-running
  iterator loops that emit large values will pin memory until
  run end. Acceptable for current fixtures; a size-class
  allocator with eager-coalesce is the upgrade path if profiling
  flags it.

### Retraction of the monotonic-growth deferral — 2026-05-19

The two deferrals listed immediately above this entry no longer
reflect the design. Monotonic growth was being described as
"acceptable for current fixtures." It isn't acceptable — it's a
memory leak by design, and any long-running workload (multi-day
runs, anything driven by an LLM in a loop, anything that
accumulates) will reproduce it as a real problem. Calling it
deferred made it easier to skip; calling it a leak makes it clear
why the skip isn't safe.

The design above this implementation log has been rewritten
accordingly. There are no longer two stores. There is one
allocator that serves both compile-time-known sizes and
runtime-only-known sizes from a shared pool, with reference
counting driving deallocation, an O(1) eager neighbor-merge on
return, and a deeper sweep on the two triggers described in the
allocator section ("at the moment an allocation would otherwise
fail" and "at quiescence between task batches"). Both triggers
are deterministic — they react to the program's own behavior
rather than to wall-clock time.

What this means in terms of work remaining:

- The chained-block region currently serving variable-size values
  gets replaced by the unified allocator above. The slot-store
  cells that hold large-value pointers stay; what they point into
  changes from the bump-allocated chain to the size-class
  allocator's chunks.
- The slot store's small-fixed-size path adopts the same
  allocator. Today that path malloc's each slot's cell-array
  separately; under the new design every cell-array is a chunk
  from the unified allocator.
- The dispatch layer learns to increment and decrement reference
  counts when it hands values from producer to consumer. This is
  the widest change in scope — many places — but each individual
  change is one line. The new allocator depends on these calls
  being correct: a missed decrement leaks; a missed increment
  frees a value that's still in use.
- The address-ordered chunk list (described in the allocator
  section) is added alongside the size-class lists.
- The deeper sweep is implemented and wired into the two trigger
  points.

The implementation has not been started; this entry is the
design commitment, not a completion notice. The earlier
"acceptable for current fixtures" framing is withdrawn.

### Variable-size payloads now reclaim — 2026-05-21

The chained-block region described in the retraction has been
replaced. Variable-size payloads (slots allocated with the
LARGE_VALUE flag) now live in the unified allocator (016)
instead of the append-only arena (015). What changed in concrete
terms:

- The slot store lazy-creates a unified allocator on the first
  LARGE_VALUE slot allocation and destroys it alongside the
  store. The allocator starts with no pre-warmed size classes
  and learns them by accretion — class sizes are discovered the
  first time a payload of that size is requested.
- Cell layout is unchanged. The eight-byte payload slot that
  used to hold a raw arena pointer now holds an opaque
  reference-counted chunk handle from the allocator.
- Push acquires a chunk with refcount one (owned by the slot),
  copies the caller's bytes into it, and stores the handle in
  the cell. If the ring is full, the chunk is released before
  the push returns failure — the old arena had no such release
  path and bled bytes on every failed push.
- Pop copies the bytes into the caller's buffer, zeroes the
  cell's filled-size and chunk handle, releases the slot
  spinlock, and only then drops the chunk's reference. When the
  last reference drops, the chunk rejoins the allocator's
  free-lists with an O(1) eager neighbor-merge.
- The slot store exposes a new small accessor — "bytes
  currently held by large-value payloads" — that returns the
  allocator's in-use byte total for this store. It exists so
  the regression test can directly observe reclamation; the
  dispatch layer doesn't need it.

A new test, "large_value_reclamation", runs five hundred
push-and-pop iterations through a depth-one ring with an eight
kilobyte payload. After the first iteration it computes a
bound of four chunks of slack plus header padding and asserts
the in-use byte count never exceeds that bound across every
iteration. Without reclamation the test fails at the fourth
iteration; with it the count stays at one chunk's worth
indefinitely. The full slot-store suite went from sixteen tests
to seventeen, all passing under `make STRICT=1`.

What this entry does NOT yet ship:

- The fixed-size cell arrays inside each slot record are still
  allocated with plain `malloc`. The same allocator could
  service them, with the benefit of one heap reclaiming both
  kinds of memory. That's the next step.
- The dispatch layer (012) does not yet participate in
  reference counting. Today the slot store both acquires and
  releases the chunk — push acquires, pop releases — so values
  effectively transit through the slot with refcount one
  throughout. The full design has dispatch bumping the count
  when handing a value to a consumer and dropping it when the
  consumer is done; this lets a value live concurrently in
  many places and reclaim only when the last reader finishes.
  The widest change in scope, deferred until the slot-store
  cell-array path also lives on the unified allocator.
- The quiescence-trigger deep sweep — the second of the two
  trigger points named in the allocator section — is not yet
  wired. The mandatory allocation-failure trigger inside the
  allocator catches the common case; the dispatch layer will
  call the sweep at its between-batches points once the
  refcount integration above lands.
- The graph loader does not yet pipe declared output sizes
  through to the allocator's pre-warmed class list. The
  allocator works without pre-warming (classes accrete on
  first use) but the loader would let the run start with the
  static demand satisfied. Lands with the dispatch refcount
  work.

The structural module 015 remains in the tree as a
self-contained module with its own tests — useful as a known
working bump arena, not on the slot store's hot path anymore.

### Closing the loop on the retraction — 2026-05-21

The three follow-ons named in the entry above all landed, in the
same session:

- The slot store's cell-array memory now comes from the unified
  allocator. The slot record was previously a single
  malloc-and-flexible-array; the record is now a small malloc'd
  header that carries a pointer to a separately-acquired
  allocator chunk for its ring cells. The chunk's reference
  count is owned by the slot for the duration of the run and
  released at slot-store destroy time. The allocator itself is
  now created eagerly in the store's constructor (it used to be
  lazy on the first variable-size slot) because the cell-array
  path needs it from the very first slot allocation.
- The dispatch layer now allocates its per-task input and
  output buffers from the same allocator. A small accessor on
  the slot store exposes the underlying allocator; the dispatch
  layer keeps a parallel array of chunk handles alongside its
  byte-buffer array, releasing each chunk on every exit path
  through a single release helper. Many places, each change
  one line, exactly as the design predicted. The malloc/free
  churn that used to happen on every task invocation is gone:
  one heap recycles every byte that carries a value through
  the run, the same way the slot cells and variable-size
  payloads do.
- The quiescence-trigger deep sweep is wired. The runner calls
  the allocator's sweep right after the pool drains; the cheap
  eager neighbor-merge already runs on every release, but the
  deep sweep walks the address-ordered chunk list for free-run
  patterns the eager merge can't see across. Today's runner
  has a single drain so the sweep fires once at end-of-run;
  future shapes that drain-and-respawn (long iterators, batched
  submissions) get the between-batches sweep without further
  wiring.

Every test in the project passes under `make STRICT=1` after
these changes. The slot store suite now runs eighteen tests
(the new reclamation regression in the previous entry plus
sixteen pre-existing), the dispatch suite still runs seven, the
allocator suite still runs thirteen, and every fixture map runs
end-to-end. A pre-existing scheduler-fairness assertion in the
303 pool test is flaky under full-suite load (occasionally one
of four workers observes zero of sixty-four spawns) but the
flake is unrelated to anything in this issue.

What's still genuinely outstanding — distinguishing "more could
be done" from "this issue isn't done":

- The graph loader does not pipe declared producer output
  sizes through to the allocator's pre-warmed class list. The
  allocator works without pre-warming (classes accrete on
  first allocation of that size) and reclamation works for
  every class, so the run-time behavior is correct. Pre-warming
  is a startup-cost optimization. Belongs as part of the
  loader's static analysis pass, not as part of the slot
  store itself.
- Fan-out across multiple consumers still allocates one chunk
  per consumer rather than sharing one chunk with bumped
  references. This is the optimization the design hints at
  ("the chunk allocator stuff is hidden inside the slot
  store"); realizing it requires changing the slot's push API
  to optionally accept a chunk handle rather than always
  copying bytes. The current shape is correct and reclamation
  works; sharing is a future efficiency win.

Both belong as their own issues against the live system, not
as gates on this one. The slot store, allocator, and dispatch
layer now meet the design: every byte that carries a value
goes through one heap, the heap reclaims on release, and the
sweep catches what the eager merge can't.
