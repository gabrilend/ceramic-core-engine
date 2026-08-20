# 020-delivery.c — the moving half, from inside

The usable interface is in `018-station.h.info.md`. This file is the
hot path: everything here happens between a box returning and the
next box being queued.

## The shape of one delivery

Lock the destination station → search the ring for an empty cell and
take it (grow first if the search comes back empty-handed) → memcpy the
value in and publish the cell as ready → walk every slot asking "do you
hold a value?" → if all do, search each ring slot for a ready cell,
take it, and copy it into a stack buffer → unlock → build the task (one
malloc holding struct, pointer array, input bytes, output bytes) → push
it into the pool. The contended section is copies and searching only;
user code never runs under a station's lock.

**Both searches are the same function.** A reader looking for a ready
cell and a writer looking for an empty one differ in which transition
they attempt and which hint they start from, and in nothing else. Each
starts where its hint points, sweeps forward, wraps, and stops where it
began — reading the hint exactly once, which is what bounds the sweep
to every cell one time each. A hint that another worker has moved in
the meantime is not wrong, only less lucky.

## The dispatch tables

Three tables, each indexed by a kind, each a row per kind rather
than a branch per case:

- **slot_filled[slot kind]** — ring: its count of ready cells is above
  zero; static: always yes; no source yet: always no, so the station
  never runs.
- **slot_claim_locked[slot kind]** — ring: pop under the mutex; no
  source yet: a named function that stops the program, because
  reaching it means the readiness walk and the claim walk disagreed
  about the same port. The static row is still a null meaning
  "resolved during task construction, outside the lock" (so the
  statics table's lock never nests inside a station's) — issue 210b
  wanted that hole given a name too, and issue 401 changes what the
  name would say, so it waits rather than being written twice.
- **route_choose[station kind]** — consulted at exactly one moment,
  on the way out: plain returns its only port; comparator and
  iterator rows fail loudly until phase 5 fills them.

## Growth (issue 203)

Doubles the storage a slot points at — never the slot, never the
station. Every value is claimed out of the old cells and republished
into the front of the new ones, one at a time rather than in a block
copy: a cell carries its state interleaved with its bytes, and the
fresh run is being *built* rather than moved, so carrying the old
states across would be wrong. Claiming on the way out is also what
identifies which cells held anything, since only a ready cell will
move. Counted per slot, high water tracked on every write, both for
phase 7 to report.

**The walk takes no lock and copies nothing** (issue 214). A port's
destinations are one immutable array behind a pointer; reading that
pointer once yields something nobody will ever modify, and what a
rewire replaced is filed rather than freed, so a walker already inside
a set is not walking freed memory. It used to snapshot the destination
list onto the walker's own stack under the station's mutex, because a
rewire could free a node under its feet — a lock acquisition and a copy
proportional to fan-out on every value the engine moved.
