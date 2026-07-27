# 403 — Gatherer slots and inline gathering

## Current behavior

Every value in the engine is pushed: a box finishes and its result is
carried forward to whoever was waiting. A value that should be current
at the moment it is *used* — a file's contents, a clock, an environment
variable — can only be current at the moment it was *produced*, which
may be long before.

## Intended behavior

The third slot kind, and the one place the engine runs backwards. A
slot with no buffer reaches upstream at the moment it is needed and
pulls a value into existence.

**A gatherer is a station with no ring-buffer slots.** Every one of its
slots is static or itself gathered, or it has none at all.

This is not a rule imposed on the design; it falls out of it. A station
with no ring buffer has nothing that can ever be written into it, so no
delivery can ever discover it, so being pulled is the only way it could
ever run.

The same fact decides which end of a wire is which. A box whose output
feeds a gatherer slot is pulled. A box whose output feeds a ring buffer
is pushed. A box whose output fans out to both is neither coherently,
and the loader rejects it — see issue 604.

**Where gathering happens.** Inside task-struct construction, after the
station's mutex has been released. That placement is deliberate: the
contended section stays short, the mutex is not held while user code
runs, and everyone upstream is unblocked before any gathering begins.

**The path:** read the slot's source station index; assemble that
station's arguments, which are static or recursively gathered; call its
shim on the worker's own stack; copy the return value straight into the
task struct being built. No task is pushed, no pool is involved, no
mutex is taken on the upstream station — there is nothing on it to
guard, because it has no buffers.

## Consequences that must be written into the source as comments

**This is the one exception to "a box only runs when a worker picks it
up from the pool."** A gathered box runs inline, on a thread in the
middle of doing something else. Naming the exception is worth more than
hiding it, because it is where the surprises will come from.

**A gatherer must be safe to run from several threads at once.**
Ordinary boxes have their values handed to them, and the guarantee that
two invocations do not collide comes from the values being claimed
under a mutex. A gatherer reaches out to the world instead, and two
workers can be inside it at the same instant. Opening a file, reading
it, and closing it is fine. Keeping an open handle in a static and
seeking in it is not.

**A gatherer runs once per task assembled.** A station enqueued a
million times pulls a million times. That is exactly what "fresh at the
moment it is used" asks for, and it means a gatherer touching a slow
disk is a cost paid on the delivery path over and over.

**A gatherer cannot decline.** The task struct has a place waiting for
bytes and no way to represent their absence. A read box pointed at a
file that is not there stops the program and says so.

## Suggested implementation steps

1. Populate the gatherer row of the readiness dispatch tables: always
   filled, and "give me a value" means run the upstream box.
2. The gathering call, invoked during task construction, outside the
   mutex.
3. Extend the map construction calls from issue 207 to place a gatherer
   slot.
4. A read box written as an ordinary function taking a path and
   returning contents, to prove no engine support is required — the
   original design called for a dedicated read box type and it
   dissolved.
5. A test that a gatherer runs once per task and that its value differs
   between two tasks when the underlying file changed in between.
6. A test that runs one gatherer from many threads at once and asserts
   every result is correct.

## Related

- [004 — Gathering](../docs/004-datapath-gather.md)
- Issue 404 — chains and cycles
- Issue 604 — the load-time rules this creates
- Issue 605 — why the startup sweep must skip these
