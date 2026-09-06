# Phase 1 progress — the pool

Phase 1's goal: a thread pool that knows nothing about boxes — the
task queue as a ring of pointers, workers that sleep rather than spin,
and termination decided by the last worker to fall asleep. Everything
above it in the project stands on this.

| Issue | State | In one line |
|---|---|---|
| 101 — task queue ring | complete | FIFO ring of pointers that doubles when full; order survives growth and many threads. |
| 102 — workers and run loop | complete | Fixed threads behind a starting gate; run-deliver-free loop; per-worker identity. |
| 103 — sleeping and waking | complete | Condition-variable sleep, wake-all on push, exact sleeper count under the one mutex. |
| 104 — termination by last sleeper | complete | Last sleeper's final look decides; broadcast shutdown; outside submitters registered. |
| 105 — phase 1 demo | complete | Four measured scenes: growth, termination tail, idle cost, throughput ceiling. |
| [106 — stopping on purpose](completed/106-stopping-on-purpose.md) | complete | Three signals answered by a thread that *waits* for one rather than handling it, so the reports may take locks; the pool's own ending arrives at the same waiting point as one more signal. A refusal is fatal rather than ignorable. |
| [107 — several queues, and a station may name one](107-several-queues-a-station-may-name.md) | open | The single ring becomes several, and a station may say which one its tasks are delivered into. **A station that says nothing changes nothing** — one default queue, served by everybody, in today's order. A queue is a *destination* and the engine is deliberately kept ignorant of what one is, so that later it can be another socket, a graphics processor, a machine across a network, or something that is not a computer. Queues may overlap in who serves them, which is how specializations and priority lanes layer without knowing about each other. Weakens first-in-first-out from global to per-queue plus a source order. |
| [108 — choosing where a box runs](108-choosing-where-a-box-runs.md) | open | A station may name the processors it will run on — cores, packages, NUMA nodes, cache domains, workers — joined by *and*, *or* and *not*, on an `on` line in the map file. Builds on 107: a placement is more entries in the worker's queue array, not an arrangement of its own. Workers grow homes they **announce themselves**, since there is no main thread to hand them one. This is where U1 is genuinely sold, because widening a placement is what a placement forbids. |

**Phase 1 reopened at the far end, and closed again.** Everything
built here still stands untouched — the new work added no mechanism to
the run loop and changed no lock. It only said what happens when a
program is told to stop, or finds an instruction it refuses to
continue past, neither of which the pool had an answer for. The happy
ending is exactly as 104 built it, and the polite shutdown reuses it
unmodified.

What the pool gained is three small things it ferries without
understanding: a signal to raise when it decides the work has run out,
a way to be told to stop starting new things, and one number per
worker saying which station that worker is inside. The last is a
number rather than a pointer on purpose, because the report written
while a lock is held forever must not dereference anything — a stale
integer is a wrong answer, a stale pointer is a crash inside the thing
that exists to explain a crash.

**And the phase learned that a guarantee can be nominal.** The promise
that a second interrupt always works could not have been kept as
designed: with every signal blocked, a second one arriving during a
report would sit pending behind a report that never finishes, so the
escape would have opened only when nobody needed it. It works now, and
it took the only signal handler in the engine to make it true.

**It also learned that its own termination rule has a window in front
of it.** A program that seeds nothing has an empty queue and nobody
promising anything between the gate opening and the first delivery, so
the last sleeper declares it finished before it has begun. That is the
rule working exactly as 104 wrote it, and 104 provided the clause that
answers it — make the standing promise before opening the gate. It
took three test scenes passing for the wrong reason to notice.

The pool moves opaque work across every core, sleeps for free, and
knows when it is done. Nothing in it mentions a station, which is what
phase 2 is for.

Notes for the phase: the machinery for 101–104 proved to be one
function with four aspects rather than four functions, and landed as a
unit in the 101 commit; the issues were then proven and closed one at
a time by their tests. The first-pass report carries the lesson.

**And it is open again, this time at the mechanism rather than around
it.** Two issues, and the order between them was worked out after both
were written. 107 splits the one queue into several and lets a station
name the one its tasks are delivered into. 108 asks the pool to obey a
placement, and turns out to be a way of *computing* which servers a
queue has from a description of hardware — a notation over 107 rather
than a mechanism beside it, which is why it takes the higher number
despite having been thought of first.

**The default is the whole design.** A station names nothing, and
nothing changes: one queue, served by every worker, in today's order,
on today's mutex. An earlier draft made a worker deliver into its own
queue by default, on a cache-locality argument, and that was wrong —
it assumes a destination is a thread in this process, which is exactly
the assumption the structure exists to avoid. A queue is a
**destination**, and a destination should be free to become another
socket, a graphics processor, a machine across a network, or something
that is not a computer at all. Locality is one thing a destination can
happen to express, opted into, never assumed.

**What it costs is global first-in-first-out**, which becomes
per-queue order plus the order of each server's source array. And
because queues may overlap in who serves them, a specialization and a
priority lane and a socket are the same structure with different
membership — none of them needs its own code, and none of them has to
know the others exist.

**The interesting cost appears only when somebody uses it**: a server
may sleep while a queue it does not subscribe to holds work. In 107
that never happens by default, since the default queue is served by
everybody. In 108 it is unavoidable, because widening a placement is
precisely what a placement forbids — so that is where the guarantee is
actually sold.

**Neither 107 nor 108 has been started, and neither should be without
asking first.** Both carry a note at the top saying so. They are
blueprints under discussion rather than plans agreed on: 107 has
already had its central default reversed once and its vocabulary is
still unsettled, and 108 has had two decisions reversed and leaves
seven questions open. Nothing in either has been built — every file
touched while writing them is a document.

## How it came to be this way

These are the turns the design actually took, lifted out of the source
comments where they had been sitting. They describe states the engine is
no longer in, which is why they are here rather than beside the code: a
comment is for what is true now.

### The lost-wakeup race turned out to have no window

The pool was designed around a re-scan by the last worker to fall
asleep, closing a race where a worker preempted between deciding to
sleep and registering itself would never be woken. Under the lock
discipline that was actually built — the queue check and the sleep
registration inside one hold of the mutex, and the wait releasing that
same mutex atomically — the window never opens, and the race proved
unreproducible. The re-scan stayed, as the last sleeper's final look at
the queue before declaring the program finished.
