# Phase 2 progress — stations and the push path

Phase 2's goal: the first phase where a graph runs. Stations in one
flat table, ring-buffer slots with exact cell sizing and growth, the
delivery path, output ports with fan-out, and the real task struct —
with maps hand-built and shims hand-written as deliberate scaffolding.

| Issue | State | In one line |
|---|---|---|
| 201 — station table | complete | Flat array of fixed-size records; nothing moves, everything indexed. |
| 202 — ring-buffer slots | complete | Tagged slots, exact-size cells, write/pop as pure memcpy. |
| 203 — slot buffer growth | complete | Doubling under the mutex with unwrap; growth count and high water kept for phase 7. |
| 204 — readiness check | complete | Dispatch-table walk at the tail of every write; claims popped under the mutex. |
| 205 — delivery walk | complete | Port choice, destination walk, sinks free; the pool's finish hook. |
| 206 — task struct | complete | One exact-size allocation: shim, station, port, input copies, output. |
| 207 — hand-built maps | complete | Construction calls kept irritating on purpose; hand shims carry their own death notes. |
| 208 — phase 2 demo | not started | |

Notes for the phase: the station layer landed as two files —
structure and motion — with the phase's mechanisms built together and
proven issue by issue, same pattern as phase 1.
