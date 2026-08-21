# 206 — The task struct

## Current behavior

Built, as the pool header's task struct grown to its real shape: the
shim to call, the producing station, the iterator's port (inert until
phase 5), and the claimed inputs and output landing place — all in
one allocation laid out struct, pointer array, input bytes, output
bytes, sized exactly for the box and freed with one call by the
worker that ran it. Construction happens after the station's mutex is
released, on the assembling thread's own time; the values inside are
copies, so a task depends on nothing another thread can change.
Phase 1's synthetic tests still embed the struct at the head of
larger payloads, which the pool cannot tell apart — the "replace the
bare task throughout the pool" step cost nothing because the pool
reads only the call field. Copy fidelity is proven by the parcel
tests riding issues 202 and 205.

## Intended behavior

One invocation, made concrete. A task is created the moment a station's
inputs are all present and destroyed by the worker that ran it.

**Fields:**

| Field | Type | Meaning |
|---|---|---|
| call | function pointer | The shim to run |
| station | `int` | Which station produced it, so delivery knows where to look |
| port | `int` | For an iterator, which port was assigned. Inert until phase 5. |
| in | array of value buffers | One claimed value per input port |
| out | value buffer | Where the return value lands |

**Sized exactly, not maximally.** A task for a two-int box holds two
ints and an int. Not a maximum-sized buffer, not a union of every type
in the program. In this phase the sizes come from the hand-written box
declarations; from phase 3 the generator knows them and can emit the
exact size per box.

**Allocated fresh per invocation, freed by the worker that ran it**,
after that worker has delivered its output. The allocation is not on
the contended path: it happens after the station's mutex has been
released, so a slow allocator delays one task rather than blocking
every other thread trying to reach that station.

Replacing this later with a free list is invisible to everything
outside the allocate and free calls, so it is not a decision this issue
owes.

**The values inside a task are copies.** This is the property that
makes concurrent invocations of one station safe — see issue 204. It
also means a task is entirely self-contained: once built, it depends on
nothing that another thread can change.

**The port field is filled at enqueue time, not at run time.** An
iterator's cursor advances during the readiness check while the mutex
is held, and the port it landed on is recorded here. The box function
never sees it. Two tasks assembled a moment apart therefore carry
different ports and cannot collide. This is inert until phase 5, but
the field belongs here so that phase 5 is a change to routing rather
than a change to this structure.

## Suggested implementation steps

1. Define the task struct with all five fields.
2. Construction, called after the readiness check releases the mutex,
   taking the claimed values and the station.
3. Destruction, called by the worker after delivery.
4. Replace phase 1's bare task with this one throughout the pool. The
   pool still treats it as opaque — it moves the pointer and calls
   `call`, and knows nothing about the rest.
5. A test that a task built from a station holds byte-identical copies
   of the claimed values and that mutating the station's buffers
   afterwards does not change them.

## Related

- [003 — Delivery](../../docs/003-datapath-delivery.md)
- Issue 204 — where the values come from
- Issue 302 — where the shim pointer comes from
- Issue 504 — the port field's eventual use
