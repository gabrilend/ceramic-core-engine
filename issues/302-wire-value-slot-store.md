# 302 — Per-task slot store with wire-held references

## Status
open

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

Slots are owned by the box that holds the input port. They live
from graph load until run end. There is no per-slot reference
counting and no end-of-stream propagation — the slot is freed
when the run ends, alongside its owning box.

Run termination is governed entirely by the pool's active-task
counter (issue 301): when no task is running and no further spawns
are pending, the run is over. There's nothing the slot store needs
to track to make this work.

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
slot_id_t slot_alloc (slot_store_t *s, int cell_capacity, int n_cells, int flags);
void      slot_push  (slot_store_t *s, slot_id_t id, const void *data, int size, uint32_t tag);
int       slot_peek  (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // 1-cell: read without draining
int       slot_pop   (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // N-cell: drain head (or lowest-tag)
int       slot_has_value(slot_store_t *s, slot_id_t id);                       // for spawn-rule check
```

Flags include `SLOT_TAGGED` (cells carry ordering tags; pop returns
lowest-tag cell). Push always takes a `tag` argument; untagged
slots ignore it (or treat 0 as "no order").

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
    int         counter;         // iterator only; 0 otherwise
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

## Allocation strategy: size-class free lists with coalescing fallback

The slot store uses a segregated free-list allocator. The map is
analyzed at compile time to enumerate every distinct slot size the
graph will need (each call box declares its output slot size; each
queued-input ring declares its total size from cell capacity × queue
depth). The allocator builds one free list per distinct size class.

Variable-size and unknown-size payloads (a long LLM response, an
array whose length is not known until runtime) are handled by a
separate **large-value heap** described below — not by allocating
huge slots up front.

### The lists
Each size class is an array (or linked list) of free slots of exactly
that size. At startup, each list is pre-populated with enough slots
to cover the static box count for that size. Lists grow during the
run if demand exceeds the pre-allocation.

Each list has its own mutex — allocation and deallocation in different
size classes never contend with each other.

### Allocation
Given a request for size N bytes:
1. Find the smallest size class with `class_size ≥ N`.
2. If that class has a free slot, pop it. Done.
3. Otherwise, try the next larger size class. Repeat.
4. If a larger class hits, the slot is oversized for the request.
   Try to slice the unused tail into a residual slot. If the residual
   is at least as large as the smallest size class, push it onto that
   class's list. Otherwise skip the slice — oversize the allocation
   and live with the waste until the slot is freed.
5. If no size class has a free slot, attempt to coalesce: walk the
   address-ordered chunk list looking for adjacent free chunks that
   together meet the requested size. If found, merge them and return
   the combined chunk.
6. If coalescing fails, grow the region (see "Growth" below) and
   append the new slots to the appropriate lists. Then try again.
7. After steps 5 or 6 complete, queue a deferred cleanup task on the
   pool. Cleanup runs on a worker thread later and walks the
   chunk list to opportunistically coalesce contiguous free chunks
   into the largest possible chunks. Cleanup is best-effort — it
   does not block any other thread. If memory fragments faster than
   cleanup can keep up, the next allocation grows the heap; that's
   fine.

### Deallocation
When refcount drops to zero, the slot is pushed onto its size class's
free list. The deallocator also performs a cheap eager-coalesce step:
check the slot's two physical neighbors; if either is free, merge
them and push the combined chunk onto the appropriate size class.
This is O(1) given the doubly-linked chunk list (see below) and keeps
small-scale fragmentation in check without needing the deferred
cleanup task to run.

### Physical-order chunk list (for coalescing)
Coalescing needs to know which chunks are physically adjacent in
memory. The size-class free lists don't carry that information.

Two viable representations:

- **Doubly-linked list** of all chunks (free and allocated) in
  address order. Splits and merges are O(1) given the chunk pointer.
  Neighbor lookup is O(1). Pointer chasing is the cost.
- **Two parallel arrays** (`start[i]`, `end[i]`) sorted by start
  address. Lookup by binary search is O(log N), but inserts and
  deletes are O(N) without an additional index. Cache-friendly to
  scan.

The linked list is simpler and has the right asymptotic behavior for
splits/merges, which dominate. Start there. If profiling later shows
list traversal is hot, switch to the array form with an auxiliary
index — the API doesn't change.

### Large-value heap (variable-size payloads)

For values whose size is not known at compile time — long LLM
responses, dynamically-sized arrays, anything where the upper bound
is not a useful number to allocate — the slot does not hold the value
directly. Instead, the slot holds a small fixed-size handle:

```
{ uint32_t size; uint32_t lvh_offset; }
```

`lvh_offset` points into a separate **large-value heap**, a
plain `malloc`-backed region used only for variable-size payloads.
When a box produces a 4 MB LLM response, the dispatch layer:
1. Allocates 4 MB in the large-value heap.
2. Writes the bytes there.
3. Stores the handle (size = 4 MB, offset = …) in the slot.

The consumer reads the handle from the slot, follows the offset,
copies the bytes (or works with them in place). When the slot's
refcount hits zero, the slot's deallocator also frees the
large-value-heap region.

The slot allocator is unchanged — it always sees fixed-size slots.
The two-tier approach keeps the hot path simple while still
supporting unbounded outputs.

A box opts into large-value output by declaring its output as
"variable-size" in the graph. Compile-time-known outputs use the
fixed-size path with no indirection.

### Growth
When all size-class lists are exhausted and coalescing cannot satisfy
a request, the allocator grows by calling `malloc` for a new region,
dividing it into slots, and appending them to the relevant size-class
lists. Plain heap memory is fine — no spec needs cross-process access
to slot memory.

### Why this design
- Compile-time enumeration makes every common allocation a single
  pop from a list. No size-comparison loops, no walks of free space.
- Slicing residuals cannot fragment beyond the granularity of the
  smallest declared size class, since smaller residuals are simply
  not split.
- Coalescing is the safety valve for unusual allocation patterns,
  and it costs nothing on the common path.
- Growth is rare — if pre-allocation matches static demand, it never
  fires.

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
