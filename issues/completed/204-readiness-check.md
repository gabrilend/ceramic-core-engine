# 204 — The readiness check and claiming values

## Current behavior

**Built, and being reached from a second direction while losing its
lock.**

Three changes, none of which touch what the check *means*.

**It is no longer only the tail end of a write.** Writing a **static**
now runs it too ([004](../../docs/004-datapath-statics.md)), which is
what replaced the pull path and what starts a program at all. The check
answers identically either way — an empty ring port still says no,
because the engine will not invent a value — so nothing here is
special-cased. It simply has two callers.

**The dispatch tables lose a row and stop answering with absences.**
The gatherer row goes with the pull path. The null entries in the claim
table, which meant "resolved later, outside the mutex," become explicit
cases, because a decision written as a hole is one a reader has to
already know how to interpret ([210](210-input-port-record.md)).

**And the mutex goes.** The claim becomes a walk of the ports in
ascending index order, flipping a ready slot to claimed at each and
rolling back if any port has nothing — with the fixed order being what
stops two threads grabbing crosswise and livelocking.

What this issue decided and what everything above still obeys: **user
code must never run under a station's mutex**, because one slow box
would freeze every thread delivering into that station. That is why the
claim table had null rows at all, and the reasoning outlived the rows.

The remainder describes it as built.

Built, as the tail end of every write. The check walks the ports
through a dispatch table keyed on the port's kind — ring buffers
answer by head-differs-from-tail, gatherer and static rows answer
"always" from the start, exactly as planned. Claiming is a second
table: ring values are popped into a caller-supplied stack buffer
under the mutex, while the gatherer and static rows are deliberately
null, meaning "resolved during task construction, outside the lock" —
a refinement of this issue's text made so user code can never run
under a station's mutex; the delivery-file comments carry the
reasoning. Proven by a three-input box fed in all six arrival orders
firing exactly once per set, and eight threads hammering one station
for sixteen thousand claims with nothing lost, torn, or doubled.

## Intended behavior

This is the engine's one rule made real:

> A station runs when, and only when, every one of its input ports
> holds a value.

**Nothing polls.** The check runs as the tail end of a write, on
exactly one station — the one just written to. A station whose inputs
have not changed cannot have become ready, so there is nothing else to
look at. This is why the engine has no scheduler thread and no scan:
the act of finishing is the act of scheduling.

**The check, holding the station's mutex:**

1. Walk every port. A ring buffer holds a value when head and tail
   differ. A gatherer port always holds one, because its value is
   produced on demand. A static port always holds one, because its
   value is simply there. The last two arrive in phase 4; the walk
   should dispatch on the port's tag from the start rather than assume
   every port is a buffer.
2. If any port is empty, release and stop. The value sits in the buffer
   waiting for its siblings.
3. If all are occupied, **claim one value from each.** Ring buffer
   ports are popped — copied out and the head advanced — so the value
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
of ports, a few `memcpy`s, some index arithmetic. Nothing that can
block, nothing that calls into user code, no allocation. Building the
task struct happens after the release, in issue 206.

**The port walk is a dispatch table**, not a chain of conditionals: the
port's tag indexes into "is it filled?" and "give me a value." Adding a
fourth port kind later should be a row rather than a new branch in two
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

- [003 — Delivery](../../docs/003-datapath-delivery.md)
- Issue 205 — what calls this
- Issue 206 — what happens after the mutex is released
