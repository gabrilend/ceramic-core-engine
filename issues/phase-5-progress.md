# Phase 5 progress — routing kinds

Phase 5's goal: comparators and iterators — a map that decides,
built as variations on one step of delivery and nothing else.

| Issue | State | In one line |
|---|---|---|
| 501 — routing dispatch | complete | Three rows at one moment; port creation became kind-aware. |
| 502 — comparator | complete | Threshold as last slot, three outcome ports, unwired outcomes discard. |
| 503 — three-way comparison | complete | Resolved at placement, one call and a sign switch on delivery. |
| 504 — iterator | complete | Cursor under the mutex at enqueue; exactly even under any crowd. |
| 505 — phase 5 demo | complete | Sorting network live, six operators from arrows, exact spreading, the byte lie. |
| 506 — boxes with several outputs | **retired** | The parity argument for it dissolved; a program's several outputs are several stations. |

Phase 5 is finished: maps decide, and every decision is wiring. It is
also the only phase that has *shrunk* — its one open issue was retired
rather than built.

## The sentence this phase owns

**A port is a choice of destination, not a set of results.** A
comparator's three ports and an iterator's many all carry the *same*
value, routed to one place; which port is used is a property of the
station, not of the box. That is why a box returning one value and a
station having several ports are both true at once, and it is what
retired the multi-output issue: a program with several outputs has
several output *stations*, so a box never had to catch up to anything.

The efficiency case for multi-output boxes survives and is not worth a
mechanism — a box computing two related results from shared work has to
be two boxes computing the shared part twice. If that ever shows up in
a measurement rather than in an argument, issue 506 still holds the
design, and the field tables it would need are already emitted.

Notes for the phase: the port-gap rule from phase 2 had quietly
assumed every station was plain; comparators made port indexes mean
outcomes, and the rule became per-kind. Recorded for the report.

An unwired comparator outcome **discards**, and that turned out to be
load-bearing far outside this phase. It is the reason a port wired to
nothing cannot buffer by default — an unwired branch is the normal
case, not an oversight — which is why the one place values are held
instead is a program's own results, and why that is stated as an
exception rather than as the rule.
