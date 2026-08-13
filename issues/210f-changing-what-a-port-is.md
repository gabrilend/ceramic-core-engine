# 210f — Changing what a port is

Sixth child of [210](210-input-port-record.md). It branches off
[210b](210b-the-port-record.md) rather than following the concurrency
line, and can be built alongside it.

## Current behavior

**Conversion no longer destroys anything, and half of this is built.**

The destruction is gone from every path. Binding a static used to free
the cell array and null the pointer; it writes the tag and nothing
else now, so a buffer sized to the very type the port carries is no
longer thrown away and reallocated, and values a producer had already
handed over are no longer discarded silently. That was the part that
was worse than it looked: a person turning a port from a wire into a
constant lost however many values happened to be waiting, and found
out never.

**One conversion operation exists, naming a station, a port, and the
tag it is becoming** — but it reaches two of the three tags. A port
can be made a buffer or unconfigured, in any direction, while the
program runs. Becoming a static is refused there and still goes
through the statics binding call, because what a port needs to become
a static is a *value*, and where that value lives is
[401](401-static-slots.md)'s question. So the operation has no room to
carry one yet.

**Blocked, therefore:** the static-facing third of the conversion
matrix, and both tests below that cycle a port through all three tags.
The ring-to-unconfigured pair is tested — a station is held still by
an unconfigured port with values waiting at its other ports, then runs
when the port is given a source, with the waiting values intact.

## Intended behavior

**Conversion is a field write.** The tag changes; storage does not
move. [210b](210b-the-port-record.md) is what makes this possible by
giving every port its cells at instantiation regardless of what the
port is currently for, so there is never a moment when the storage a
tag needs is absent.

**Values survive a change of source.** Switching a port's tag away
from ring leaves its cells exactly as they are — not freed, not
cleared, not drained. They are waiting if the port becomes a ring
again. Discarding them would throw away values a producer already
handed over, invisibly, which is worse than serving them slightly
late.

That "slightly late" is the honest cost and should be said plainly: a
port turned into a static and back may deliver a value that arrived
before the conversion, after values that arrived after it. Given that
[210d](210d-the-claim-takes-no-lock.md) already gives up arrival order
as a guarantee, this costs nothing that was still being promised — but
it is a second reason for the same non-guarantee, and a reader
deserves to know it is not only rollback that opens gaps.

**Conversion happens under the station's mutex**, as one of the four
rare structural operations, so no readiness walk sees a port
mid-change.

**All three tags are reachable from all three.** A ring becomes a
static; a static becomes a ring; either becomes *none*, which is a
port whose source has been taken away and not yet replaced, and a
station holding one stops being able to run without anything being
destroyed.

## Suggested implementation steps

1. One conversion operation naming a station, a port, and the tag it
   is becoming, replacing both existing paths.
2. Leave the cells alone on every path through it. This is the whole
   change and it is mostly deletions.
3. A test that a port cycles through all three tags while the program
   runs, with a station upstream delivering throughout, and nothing
   tears.
4. A test that values waiting in a ring port when it becomes a static
   are still there, and still delivered, when it becomes a ring again.
5. Note the second source of out-of-order delivery in
   [058](../docs/058-guarantees.md) beside the first, so the
   non-guarantee lists both reasons rather than one.

## Open questions

**Answered:**

- *A port that has been a static, is turned into a ring, and is turned
  back into a static: does it still hold the static value it had
  before, or must one be written again?* **It keeps the value.** The
  rule this issue is built on is that conversion destroys nothing, and
  a static's binding is storage like the cells are storage — carving
  out an exception for it would mean the issue's one sentence had a
  second clause nobody could derive from the first.

  It also keeps *none* honest. If a port coming back from a ring
  arrived unconfigured, then *none* would mean two different things:
  "nobody has said yet" and "somebody said, then said something else."
  Those want different answers from a person looking at a dump. Keeping
  the value means *none* is only ever reached by asking for it, which
  is what makes it a state rather than a leftover.

  The dump says the binding it finds, so a port that has cycled reads
  identically to one that never moved — which is correct, because they
  are in the same state.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210b — The port record](210b-the-port-record.md), whose standing
  buffer is the entire reason this is cheap
- [210g — One way to build a station](210g-one-way-to-build-a-station.md),
  which offers this operation as part of one surface
- [405 — Changing a static while it runs](405-statics-mutation.md),
  which becomes one case of this
- [704 — Rewiring while it runs](completed/704-runtime-rewiring.md),
  the same live-editing discipline on the wires
- [058 — Guarantees](../docs/058-guarantees.md), which gains a second
  reason for a non-guarantee it already carries
