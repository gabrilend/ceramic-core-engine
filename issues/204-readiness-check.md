# 204 — The readiness check and claiming values

## Current behavior

Values can be written into a station's slots, but nothing notices when
a station has everything it needs.

## Intended behavior

This is the engine's one rule made real:

> A station runs when, and only when, every one of its input slots
> holds a value.

**Nothing polls.** The check runs as the tail end of a write, on
exactly one station — the one just written to. A station whose inputs
have not changed cannot have become ready, so there is nothing else to
look at. This is why the engine has no scheduler thread and no scan:
the act of finishing is the act of scheduling.

**The check, holding the station's mutex:**

1. Walk every slot. A ring buffer holds a value when head and tail
   differ. A gatherer slot always holds one, because its value is
   produced on demand. A static slot always holds one, because its
   value is simply there. The last two arrive in phase 4; the walk
   should dispatch on the slot's tag from the start rather than assume
   every slot is a buffer.
2. If any slot is empty, release and stop. The value sits in the buffer
   waiting for its siblings.
3. If all are occupied, **claim one value from each.** Ring buffer
   slots are popped — copied out and the head advanced — so the value
   is now spoken for and no other thread can take it.
4. Release the mutex.

**Steps 3 and 4 are the whole reason two invocations of one station can
run at once.** By the time the mutex is released, this invocation's
values have been copied out of the station entirely. A second thread
arriving immediately after finds different values, claims those, and
the two never meet.

This is also why a box may not remember anything. The station is
guarded, but the box function runs long afterwards, on whichever worker
picks the task up, and two of them can be inside the same box function
at the same instant.

**The contended section is deliberately short** — a walk over a handful
of slots, a few `memcpy`s, some index arithmetic. Nothing that can
block, nothing that calls into user code, no allocation. Building the
task struct happens after the release, in issue 206.

**The slot walk is a dispatch table**, not a chain of conditionals: the
slot's tag indexes into "is it filled?" and "give me a value." Adding a
fourth slot kind later should be a row rather than a new branch in two
functions that must be kept in agreement.

## Suggested implementation steps

1. The two dispatch tables, with only the ring-buffer row populated;
   the gatherer and static rows return "always filled" and are wired up
   in phase 4.
2. The check itself, called at the end of every write.
3. Claiming into a values buffer the caller supplies, so this function
   allocates nothing.
4. A test with a station of several inputs, filling them in every
   order, asserting a task is produced exactly once and only when the
   last one arrives.
5. A test that hammers one station from several threads and asserts
   the number of claims equals the number of complete input sets, with
   no value claimed twice and none lost.

## Related

- [003 — Delivery](../docs/003-datapath-delivery.md)
- Issue 205 — what calls this
- Issue 206 — what happens after the mutex is released
