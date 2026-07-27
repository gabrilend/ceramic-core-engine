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
| 104 — termination by last sleeper | in progress | |
| 105 — phase 1 demo | not started | |

Notes for the phase: the machinery for 101–104 proved to be one
function with four aspects rather than four functions, and landed as a
unit in the 101 commit; the issues were then proven and closed one at
a time by their tests. The first-pass report carries the lesson.
