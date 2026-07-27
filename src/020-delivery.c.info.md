# 020-delivery.c — the moving half, from inside

The usable interface is in `018-station.h.info.md`. This file is the
hot path: everything here happens between a box returning and the
next box being queued.

## The shape of one delivery

Lock the destination station → memcpy the value into its ring (grow
first if the tail would catch the head) → walk every slot asking "do
you hold a value?" → if all do, pop one value per ring slot into a
stack buffer → unlock → build the task (one malloc holding struct,
pointer array, input bytes, output bytes) → push it into the pool.
The contended section is copies and index arithmetic only; user code
never runs under a station's lock.

## The dispatch tables

Three tables, each indexed by a kind, each a row per kind rather
than a branch per case:

- **slot_filled[slot kind]** — ring: head differs from tail;
  gatherer and static: always yes.
- **slot_claim_locked[slot kind]** — ring: pop under the mutex; a
  null row means "resolved during task construction, outside the
  lock" (gatherers and statics, phase 4).
- **route_choose[station kind]** — consulted at exactly one moment,
  on the way out: plain returns its only port; comparator and
  iterator rows fail loudly until phase 5 fills them.

## Growth (issue 203)

Doubles the storage a slot points at — never the slot, never the
station — with the wrapped portion copied so contents read
contiguously from cell zero. Counted per slot, high water tracked on
every write, both for phase 7 to report.
