# 210f — Changing what a port is

Sixth child of [210](210-input-port-record.md). It branches off
[210b](210b-the-port-record.md) rather than following the concurrency
line, and can be built alongside it.

## Current behavior

Converting a port between kinds destroys and rebuilds. Both conversion
paths in the source do the same three things: free the cell array,
null the pointer, flip the tag.

Two consequences follow, and both are worse than they look.

**A buffer that is exactly the right size is thrown away.** Making a
ring port into a static frees cells sized to the very type that port
will still be carrying if it ever becomes a ring again — at which
point they are allocated afresh.

**Values in flight are discarded silently.** Whatever a producer had
already handed over and that nobody had claimed yet is freed with the
array. Nothing is said. A person turning a port from a wire into a
constant loses however many values happened to be waiting, and finds
out never.

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

- A port that has been a static, is turned into a ring, and is turned
  back into a static: does it still hold the static value it had
  before, or must one be written again? Keeping it is consistent with
  cells surviving; requiring a fresh write is consistent with *none*
  existing as the honest way to say a port has no source. Whichever it
  is, the dump has to be able to say it.

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
