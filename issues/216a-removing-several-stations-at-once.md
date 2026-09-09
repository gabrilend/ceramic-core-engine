# 216a — Removing several stations at once

## Current behavior

**Removing one station sweeps the whole table.** A wire exists in
exactly one place — an entry in the destination set on the producing
station's output port — and an input port has no field naming what feeds
it. So finding every wire that points at a station means asking every
station in the table.

For one station that is correct and cheap. For a *set* of them it is the
same walk repeated: pruning a fifty-station program from a five-thousand
station table means fifty sweeps of five thousand stations, each taking
and releasing a mutex per station visited.

**The one-at-a-time shape also leaves the graph inconsistent in the
middle.** Between the first station going and the last, the program
being removed is half there — some of its interior wires cut, others
still live, and values still moving along the ones that remain.

## Intended behavior

**One call takes a set of stations and does one sweep.** Every wire that
names any station in the set is cut, in one pass over the table, with
each destination set rebuilt once rather than once per removed station it
happened to name.

**All of them are marked before any wires are cut**, so the whole set
stops starting new work at the same moment rather than one at a time.
That is what the single-station path already does for one station,
widened.

**The interior wires go too, without being special.** A wire from one
removed station to another is named by a station in the set, so it is cut
by the same pass. Nothing has to know it was interior.

**The cost of not adding a back-reference is accepted here rather than
paid off.** An input port could carry a list of what feeds it, which
would make removal ask only the sources instead of the whole table. It is
not worth it: every wire operation would then maintain two structures
that can disagree, and the destination set's entire safety argument is
that it is immutable and swapped whole, so a second structure would need
the same discipline for a question asked almost never. Delivery — the
hot path, run constantly — only ever asks *where does this value go*.
Removal is rare. One sweep of a table is the right price.

## Suggested implementation steps

1. Add a call taking an array of station indices and a count.
2. Mark every one as removed first, under each station's own mutex, so
   nothing new starts from any of them while wires are being cut.
3. One pass over the table: for each station, for each output port, if
   its destination set names any station in the set, build one
   replacement set with all of them dropped and swap it in.
4. Retire the removed stations' ports and buffers through the scrapyard,
   as the single-station path already does.
5. Keep the single-station call as the set-of-one case, so there is one
   path rather than two.
6. Tests: a set whose members are wired to each other and to outsiders,
   all cut in one call; a worker mid-walk on a replaced set finishing on
   the old one; the freed slots reused by a later placement.

## Related

- [216 — Removing a station](completed/216-removing-a-station.md), whose
  walk this widens
- [214 — Destinations without a lock](completed/214-destinations-without-a-lock.md),
  the immutable-and-swapped discipline this must keep
- [212a — One table, and a program is a receipt](212a-one-table-and-a-program-is-a-receipt.md),
  which is the caller this exists for
