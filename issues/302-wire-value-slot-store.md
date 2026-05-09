# 302 — Per-task slot store with wire-held references

## Status
open

## Current behavior
Wire values are passed through stdout pipes and transient strings. Values
have no addressable location, no reference count, and no cross-language
accessibility. Each value vanishes once the pipe is drained.

## Concept

### Slots belong to tasks, not wires
A **slot** is the memory region that holds a task's output. Slots are
created when tasks are created. A box that runs once produces one slot;
a box that runs ten times (an iterator) produces ten slots, one per
invocation. Slots live in process heap memory, accessible to every
worker thread in the process. Cross-process visibility is not required:
language specs (issue 303) only ever receive byte pointers — never
raw slot addresses — so out-of-process specs (Bash) work via sockets,
not shared memory.

A **wire** is the relationship between two tasks: it says "task B's input
N reads from task A's output M." Wires hold references to slots. They
are the timing mechanism — a task does not become ready until every
input wire reports its source slot is filled. They are also the lifetime
mechanism — a slot is freed only after every wire that referenced it has
released it.

The wire is the arbiter. The slot is just memory.

### All slots are ring buffers
There is one slot type. It is a ring buffer of `n_cells` cells, each
of `cell_capacity` bytes. The `n_cells = 1` case is the trivial ring
holding a single value; the `n_cells = N` case is a queue.

A single-output box allocates a 1-cell ring. The producer pushes once,
consumers read the head. An iterator that produces 10 outputs over its
lifetime allocates 10 separate 1-cell rings (one per invocation, per
the box-not-wire-is-the-slot model below).

A box with a queued input allocates a multi-cell ring as the input's
holding pen — many upstream wires push into it, the consumer drains it
in arrival order.

Same data structure, same API, same per-slot lock. The dispatch layer
parameterizes `n_cells` based on whether the slot is being used for
fan-out or fan-in.

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
0       4     refcount        (atomic int32; lifetime counter)
4       4     cell_capacity   (bytes per cell, set at allocation)
8       4     n_cells         (ring size, set at allocation; 1 for single-value)
12      4     head            (read index, mod n_cells)
16      4     tail            (write index, mod n_cells; cells_filled = tail - head)
20      1     touched         (per-slot lock: 0=free, 1=another thread mutating)
21      3     _pad
24      …     cells           (n_cells * (4-byte size + cell_capacity bytes))
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

## Reference counting protocol

Every party that may still touch a slot holds a reference. The slot is
freed when the count reaches zero. The references themselves are how
we track liveness — there is no separate "is this producer / consumer
still alive" mechanism.

### Single-output (1-cell) slot
1. Task allocates its output slot with `refcount = 0`.
2. For each fan-out wire reading from the slot, `slot_ref` is called.
   The wire holds that reference until its consumer is done reading.
3. After the consumer reads (peek), it calls `slot_unref`.
4. When refcount reaches zero, the unrefing task frees the slot.

### Queued (N-cell) slot
The consumer's input queue. Producers and the consumer all hold refs:
1. The consumer task allocates the queue slot with one ref for itself.
2. Each producer that may push into the queue increments the refcount
   when it is constructed; the ref is dropped when that producer's
   box function ends.
3. The consumer drops its ref when its own box function ends (i.e.
   when the iterator stops re-spawning because no more inputs are
   coming).
4. When the last ref drops, the queue is freed.

The producer-side ref is what makes "no more inputs are coming"
detectable: when the queue's refcount equals 1 (only the consumer
holds it) and the queue is empty, no upstream task can push again,
so the iterator's natural end-of-stream condition fires.

### Wait list
Each slot also owns a wait list — pointers to tasks parked on the
slot waiting for a value. When a producer pushes, the slot walks the
wait list and tells the pool to re-run those tasks. The wait list is
attached to the slot, not to the producer or the consumer.

## Backing store

Slots are allocated from process heap memory. The slot store owns a
per-run allocator (described under "Allocation strategy" below) that
hands out memory chunks to slots. Plain `malloc`-class memory is
sufficient — every worker thread in the process can read and write
it, and no spec ever needs to access it from a child process.

Synchronization within a slot uses the per-slot `touched` flag for
header mutations and a per-slot wait list (above) for blocked tasks.
There is no allocator-wide mutex on the read/write path; only the
per-size-class free lists in the allocator have their own locks.

## Slot creation

Slot creation happens inside the dispatch layer at task submission. The
caller specifies the cell capacity (bytes per value) and the ring size
(number of cells, 1 for single-value):

```c
slot_id_t slot_alloc(slot_store_t *store, int cell_capacity, int n_cells);
```

The slot is created with `refcount = 0`, `head = tail = 0`. The
dispatch layer then increments the refcount once per consumer (one ref
per fan-out wire for a 1-cell slot; one ref for the consumer task on a
queued-input slot) before the task is exposed.

## C API surface

```c
slot_id_t slot_alloc (slot_store_t *s, int cell_capacity, int n_cells);
void      slot_push  (slot_store_t *s, slot_id_t id, const void *data, int size);
int       slot_peek  (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // non-destructive: read head, leave it
int       slot_pop   (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // destructive: read head, advance head
void      slot_wait  (slot_store_t *s, slot_id_t id); // blocks until tail > head
void      slot_ref   (slot_store_t *s, slot_id_t id);
void      slot_unref (slot_store_t *s, slot_id_t id); // frees backing memory when refcount → 0
```

Single-value (1-cell) slot use: producer `push` once, each consumer
`peek` once, then `unref`. Cells are not popped — fan-out consumers
all read the same value, and the slot is freed when the last consumer
unrefs.

Queued-input (N-cell) slot use: producers `push` repeatedly, the
consumer `pop`s in arrival order. The consumer holds the only ref.
When the consumer task is done with its queue, it `unref`s and the
slot is freed.

## Submission timing

The dispatch layer builds the task struct as soon as the graph asks
for it — output slots allocated, all known data filled in, input wire
references attached. It is then submitted directly to the pool. The
pool's existing block-and-wake machinery handles the rest:

- If any input slots are unfilled at the moment of submission, the
  dispatch action immediately `ACT_BLOCK`s on the first unfilled
  input. The pool parks the task on that slot's wait list.
- When the upstream producer pushes, it walks the wait list and wakes
  the parked tasks. The action re-runs from the top, checks the next
  input, blocks again if needed.
- When all inputs are present, the action proceeds to invoke the box.

There is no separate "blocked tasks" set held by the dispatch layer.
The pool is the only thing that tracks blocked tasks. This avoids
duplicating the wait-list machinery the pool already has.

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
