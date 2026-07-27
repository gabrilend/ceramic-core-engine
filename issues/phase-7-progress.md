# Phase 7 progress — seeing inside it

Phase 7's goal: everything that makes the engine legible while it
runs. None of it required for correctness, all of it required for
confidence.

| Issue | State | In one line |
|---|---|---|
| 701 — buffer growth reporting | complete | Both piles named; periodic observer; loud shutdown word. |
| 702 — station statistics | complete | Counts always on; timing compiles in and out; gather charged to the puller. |
| 703 — map dump | complete | The live table as a map file; round trip byte-identical. |
| 704 — runtime rewiring | complete | Check and change under one lock; refusals return, they do not kill. |
| 705 — HTML documentation | in progress | Generator and 78-page site stand; two widgets and deep links remain. |
| 706 — phase 7 demo | not started | |

Notes for the phase: reporting, dumping, and rewiring landed as
three modules behind one header. Rewiring required delivery to
snapshot destination lists under the station mutex — a retrofit the
second pass should design in from phase 2.
