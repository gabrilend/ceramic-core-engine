# 020-delivery.c — the moving half, from inside

The usable interface is in `018-station.h.info.md`. This file is the
hot path: everything here happens between a box returning and the
next box being queued.

## The shape of one delivery

Lock the destination station → search the ring for an empty slot and
take it (grow first if the search comes back empty-handed) → memcpy the
value in and publish the slot as ready → walk every port asking "do you
hold a value?" → if all do, search each ring port for a ready slot,
take it, and copy it into a stack buffer → unlock → build the task (one
malloc holding struct, pointer array, input bytes, output bytes) → push
it into the pool. The contended section is copies and searching only;
user code never runs under a station's lock.

**Both searches are the same function.** A reader looking for a ready
slot and a writer looking for an empty one differ in which transition
they attempt and which hint they start from, and in nothing else. Each
starts where its hint points, sweeps forward, wraps, and stops where it
began — reading the hint exactly once, which is what bounds the sweep
to every slot one time each. A hint that another worker has moved in
the meantime is not wrong, only less lucky.

## The dispatch tables

Three tables, each indexed by a kind, each a row per kind rather
than a branch per case:

- **in_port_filled[port kind]** — ring: its count of ready slots is above
  zero; static: always yes; no source yet: always no, so the station
  never runs.
- **in_port_claim_locked[port kind]** — ring: pop under the mutex; no
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

## Growth (issues 203, 210e)

**Adds one page of slots to the end of a list.** Nothing is copied and
no slot that already exists moves, so there is no window to get right.
Growth still takes the station's mutex, as one of the rare structural
operations, so two threads meeting a full buffer add one page between
them rather than one each. Counted per port, high water tracked on
every write, both for phase 7 to report — and the count now means
*pages added beyond the first* rather than doublings, which is why the
report says pages.

**It used to double and copy, and that could not be made safe.** The
live values had to be carried across: copy first and then publish, and
a value taken from the old array mid-copy exists in both places and is
delivered twice; publish first and then copy, and a reader sees an
empty buffer while it fills. Neither order works on its own. What made
it safe was that the station's mutex was held for the whole copy, so
nothing else could happen at all — and that is precisely the
protection issue 210d spends when the value copies leave the lock. A
worker copying out of a slot it has claimed holds no lock, because a
claimed slot belongs to it alone, and relocating that slot underneath
it is the one thing ownership does not cover. So the copy had to stop
existing rather than be ordered correctly.

**The walk takes no lock and copies nothing** (issue 214). A port's
destinations are one immutable array behind a pointer; reading that
pointer once yields something nobody will ever modify, and what a
rewire replaced is filed rather than freed, so a walker already inside
a set is not walking freed memory. It used to snapshot the destination
list onto the walker's own stack under the station's mutex, because a
rewire could free a node under its feet — a lock acquisition and a copy
proportional to fan-out on every value the engine moved.
