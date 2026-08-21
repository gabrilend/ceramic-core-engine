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
