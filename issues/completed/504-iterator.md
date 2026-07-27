# 504 — The iterator

## Current behavior

Built. The cursor advances inside the readiness path, under the
station's mutex, at the moment a task becomes due — the chosen port
is written into the task and the box never sees any of it, exactly
as this issue specified. The routing row on the way out just reads
the recorded port. The cursor is an index into the port list, walked
per delivery, consistency over cleverness as decided here. Proven by
twenty-four hundred values from eight concurrent feeders landing
exactly eight hundred on each of three ports — the no-two-tasks-
one-port-per-cycle property made arithmetic. Arrival order remains
unpromised, and the phase 5 demo shows the disorder beside the
fairness: a spreader, not a funnel.

## Intended behavior

Any number of output ports. Each run sends its value down the next one,
wrapping at the end.

**The cursor lives on the station**, and it is the one piece of memory
a station keeps across invocations.

**It is safe because of *when* it is touched.** The cursor advances
during the readiness check, while the station's mutex is held, and the
port it landed on is written into the task struct. The box function
never sees it. Two tasks assembled a moment apart therefore carry
different ports, decided by the enqueuing thread under the lock, and no
two invocations can collide over it.

This is not really an exception to "a box cannot remember." It is the
station remembering on the box's behalf, at a moment when only one
thread can be looking.

**An iterator distributes fairly but does not deliver in order.** The
port is chosen when the task is created, not when it finishes. If the
task holding port one takes longer than the one holding port two, port
two receives first.

That is correct for what an iterator is for. It is a spreader, not a
funnel — "send this line to a language model and that one to a mail
server," not "write line one then line two." Anything wanting order
between destinations wants something else, and building that something
else is not in scope.

**The cursor is an index, not a pointer into the port list**, even
though the list is linked and advancing a node pointer would avoid
walking. Everything else in the engine addresses by index — slots,
stations, statics — and the walk over a handful of ports is free.
Consistency is worth more than the walk.

## Suggested implementation steps

1. Fill the iterator row of issue 501's dispatch: read the port
   recorded in the task struct, which was chosen at enqueue.
2. In the readiness check, when the station is an iterator, advance the
   cursor and record the port in the task being built — inside the
   mutex, alongside the value claiming.
3. Extend the construction calls from issue 207 to place an iterator
   with several ports.
4. A test that N successive values reach N ports in order, then wrap.
5. A test with many concurrent invocations asserting every port
   received its fair share and no two tasks were assigned the same port
   in one cycle — the property the mutex placement exists to guarantee.
6. A test with deliberately uneven box durations, asserting the
   distribution is still fair even though arrival order is not.

## Related

- [005 — Routing](../docs/005-routing.md)
- Issue 204 — the readiness check where the cursor advances
- Issue 206 — the task field the port is recorded in
- Issue 501 — the dispatch this fills
