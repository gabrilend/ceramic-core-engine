# 501 — Routing dispatch

## Current behavior

Built. The three-entry dispatch table stands at step one of delivery
and nowhere else — the plain row moved in unchanged, the comparator
row compares and returns sign-plus-one, the iterator row reads the
port its task was assigned at enqueue. Nothing else in the engine
asks what kind a station is. Port creation became kind-aware in the
same stroke: a plain box may wire only port zero, a comparator up to
three, an iterator any number — and for the kinds where an index
carries meaning, intermediate ports are created empty rather than
refused, because wiring only some outcomes is legitimate and an
empty port discards, which is what an unwired outcome should do.
This replaced phase 2's no-gaps rule, which turned out to encode a
plain-only assumption. The plain path's behavior is unchanged, its
proof being the entire earlier suite still passing untouched.

## Intended behavior

**Box kind is purely a routing policy.** All three kinds execute
identically — pop their inputs the same way, build the same task, call
their shim the same way. They differ only in which output port a
returned value goes down.

So the kind is consulted at exactly one moment, step 1 of delivery, and
nowhere else in the engine. Nothing else ever asks what kind a station
is. This issue is the three-entry dispatch table at that one moment;
issues 502 through 504 fill in the rows.

**A table, not a chain of conditionals.** The station's kind indexes
into a function that returns which port to use. Adding a fourth kind
later should be a row rather than a new branch, and referring to a
function by index is cheaper than walking a chain of comparisons to
reach the same place.

**Ports become a real list.** A plain station has one port, a
comparator has exactly three, an iterator has as many as its map gives
it. Issue 201 placed the port list already; this is where more than one
entry in it starts mattering.

The distinction to keep straight: a **port** is one exit from a
station, and a port holds a **list of destinations**. Fan-out is what
one port with several destinations does — it is not a routing kind. All
three kinds fan out identically, because fan-out happens after the port
has been chosen.

## Suggested implementation steps

1. The dispatch table, with the plain row moved into it unchanged.
2. Leave the comparator and iterator rows failing loudly until 502 and
   504 fill them, so a map using a kind before it works stops rather
   than routing everything one way.
3. Confirm the delivery walk is untouched below the port choice — every
   kind shares the destination walk, the mutex, the write, and the
   readiness check.
4. A test that the plain path is byte-for-byte unchanged in behaviour
   from phase 2.

## Related

- [005 — Routing](../../docs/005-routing.md)
- [003 — Delivery](../../docs/003-datapath-delivery.md), step 1
- Issue 205 — the call site this fills
- Issues 502 and 504 — the other two rows
