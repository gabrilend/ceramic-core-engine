# 101 — The task queue: a ring of pointers that doubles

## Current behavior

Built, in the pool library under `libs/`. The queue is a ring of task
pointers guarded by one mutex, doubling when the tail would land on
the head, with the wrapped portion unwrapped during the copy so the
contents stay in first-in-first-out order. Push and pop never block;
pop returns nothing on empty and the sleeping decision lives with the
workers. The queue additionally records its high-water occupancy and
growth count, placed early so the phase 1 demo could report measured
numbers rather than constants. Covered by a growth-order test seeded
with a wrapped ring and a many-threads push/pop test.

## Intended behavior

A first-in-first-out queue holding pointers to task structs. It is the
only channel between the thread that discovers work and the thread that
performs it, and it is the foundation everything else in phase 1 sits
on.

**The queue holds pointers, not tasks.** Each entry is an address of a
task struct sitting on its own in the heap. This distinction is what
makes the rest of the design safe: nothing anywhere in the program
holds a pointer *into* the ring, so the ring's storage may be moved
underneath every thread without producing a single dangling reference.

**Fields:**

| Field | Type | Meaning |
|---|---|---|
| slots | pointer to an array of pointers | The ring itself |
| capacity | `int` | How many entries the array holds |
| head | `int` | Where the oldest task sits |
| tail | `int` | Where the next task goes |
| mutex | `pthread_mutex_t` | Guards all four of the above |

Both indices wrap at capacity. Order is first-in-first-out, so a task
that has been waiting cannot be starved by newer arrivals. A stack
would be faster — the last value written is still in cache — but it can
delay an old task indefinitely under sustained load, and the engine has
no way to express urgency to compensate.

**When the ring fills, it doubles.** If advancing the tail would land
it on the head, the storage is reallocated to twice its size, the
wrapped-around portion is copied so the queue reads contiguously again,
and both indices are corrected. All of this happens while the mutex is
held, so no other thread is inside.

This is reachable and not theoretical: one station wired to a hundred
destinations produces a hundred tasks during a single delivery, so the
queue can go from nearly empty to overflowing in one worker's turn.

**Rejected alternative:** a worker that finds the queue full runs a
task itself and then retries its enqueue. It is deadlock-free, but it
delays the enqueue by however long an arbitrary user function takes.
Growth delays it by a bounded memory copy. Same shape, wildly different
worst case.

**Task struct lifetime.** Allocated fresh by whoever enqueues it,
freed by the worker that ran it, after that worker has delivered its
output. Swapping in a free list later is invisible to everything
outside the allocate and free calls, so it is not a decision this issue
owes.

## Suggested implementation steps

1. Create the pool library under `libs/` with the queue struct, its
   creation, and its destruction.
2. Push and pop, both taking the mutex, both operating on the indices.
   Pop returns nothing when the queue is empty rather than blocking —
   the sleeping decision belongs to issue 103, not here.
3. The growth path, inside push, triggered when advancing the tail
   would collide with the head. Reallocate, unwrap, fix indices.
4. A test that fills a small queue past its capacity several times over
   and asserts that everything pushed comes back out, in order, once.
   The unwrap step is the part that will be wrong first: seed the test
   with a queue whose contents have wrapped before the growth happens.
5. A test that pushes and pops from several threads at once and
   asserts nothing is lost or duplicated.

## Related

- [006 — Scheduling](../../docs/006-datapath-scheduling.md)
- Issue 102 — the workers that pop from this
- Issue 103 — what a worker does when the pop comes back empty
