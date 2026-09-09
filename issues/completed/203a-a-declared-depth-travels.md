# 203a — A declared depth travels downstream

## Current behavior

**Built.** Saying how deep one port should be says it for the chain
below it.

Two moments carry it, and between them they cover both a file and a
program that grows while it runs:

- **Declaring a depth.** `in 0 x100` in a map file, or the call it
  becomes, sizes the stations that port feeds, and the ones they feed.
- **Drawing a wire.** A new wire carries the backlog its source can
  hold, so a station added at minute ten is as deep as one added at
  minute zero. This is also what makes a file size itself, at no extra
  cost — loading draws every wire it names.

**Nothing runs on the delivery path.** Both are construction-time
operations already holding the rewire mutex.

### What each kind passes on

| kind | what its destinations get |
|---|---|
| plain | the whole backlog, to **every** wire on port zero — fan-out duplicates a value rather than dividing it |
| iterator | the backlog divided by its exits, rounded up, because the exits are taken in turn |
| comparator | **nothing.** One of three exits per run, chosen by the data |
| a box returning void | nothing to send |

**A station passes on the minimum over the ports that gate it**, because
it runs when every input port holds a value: fed a hundred from one side
and ten from the other it runs ten times and produces ten. Static ports
are skipped, since a static is always full and gates nothing; a port
with no source at all answers zero, because the station never runs.

**The fixed point is "already deep enough".** A port that is big enough
is left alone, and that is what makes a cycle safe — an accumulator's
output feeds its own input, the walk arrives back where it started, and
the second visit finds the port at the size the first visit gave it. No
visited set, no depth limit, nothing to keep in step with the graph
changing.

**The ordinary case costs one comparison.** Two stations fresh out of
the box are both at the default depth, so wiring them propagates that
default and changes nothing.

## Why it was worth doing

Growth is cheap in itself — it appends a page and copies nothing — but
it happens **on the delivery path, under the station's own mutex**,
which is the lock the readiness check also wants. A burst of a hundred
values into a ten-slot buffer takes that lock about nine extra times,
for a reason nothing in the map shows.

An author who knows the burst is coming could already say so on the
first port. What they could not do was say it once: before this, the
depth had to be repeated on every station below, and a station added
later left a hole in the middle of the chain.

## The comparator, and why it stops rather than sizing everything

A comparator's three exits are less, equal and greater, and which one a
value takes is decided by the data. So any one of them could take the
whole backlog.

Two answers were available. **Size all three** for the whole backlog —
honest about the worst case, three times the memory for a guess. **Size
none** and let whichever turns out busy grow a page at a time — the
status quo on that branch, costing time rather than memory.

The cheaper mistake is the one that only costs time, so the walk stops
at a comparator. Its own input port is still sized: the doubt is about
which exit, not about what arrives.

## What was refused

**Inferring it at runtime from what is currently queued.** It would mean
re-walking the graph every time a queue grows, on the path that runs
constantly, to avoid an allocation that appends a page and copies
nothing. The walk would also be holding a station's mutex while it ran.

**A load-time-only pass.** It would size a file and miss every station
a program added while running, which is most of what runtime rewiring
is for. Carrying the backlog on the wire covers both with one rule.

## Related

- [203 — Port buffer growth](completed/203-port-buffer-growth.md), whose
  page-adding growth this reduces the frequency of
- [210b — The port record](completed/210b-the-port-record.md), where the
  starting depth and its `x64` spelling came from
- [502 — Comparator](completed/502-comparator.md) and
  [504 — Iterator](completed/504-iterator.md), the two kinds that route
  rather than pass through
- [008 — Map file format](../docs/008-map-file-format.md), which
  documents the depth an author writes
