# 212a — One table, and a program is a receipt

## Current behaviour

**Built.** One station table; everything is wirable to everything.

Starting a program beside another is retired, along with the borrowed-
pool flag it needed. What it offered — a second thing you can end
without touching the first — is what pruning a receipt now offers,
without the restriction that came with it.

**A program is a kept receipt.** The list of stations one placing
created used to be handed to the caller and freed as soon as wiring was
done; it is kept now, because ending a program is pruning exactly that
list. One sweep cuts every wire naming any member, interior wires
included.

**Two maps in one process is untouched.** Nothing here reintroduces a
process-wide pointer to an active map, and the proof that two
independently built programs do not interfere stands as it was. What
went is the *restriction*, not the coexistence.

**A double free came out of it**, reachable the moment a program could
be ended by pruning: destroying a map walked its stations before
sweeping the scrapyard, and a removed station's shim — which is what
the walk tests — is not cleared until the sweep runs. So its ports were
freed and then reclaimed a second time. Everything that removed a
station before this happened to sweep first.

## Intended behavior

**One station table. Everything is wirable to everything.** The rule
that two stations must share a table stays true by there being only one.

**A program is a receipt** — the list of station indices that placing a
description created. That list is already what instantiation hands back
([217a](217a-one-receipt-for-a-box-and-a-map.md)); what changes is that
it is kept rather than discarded once wiring is done, because it is now
a program's identity.

**Ending a program is pruning its stations**, in one sweep
([216a](216a-removing-several-stations-at-once.md)). Cutting every wire
that names any of them is what the existing removal already does; doing
it for a set is the same walk done once.

**Starting a program beside another is retired**, along with the
borrowed-pool flag and the per-program closing flag it needed. What it
offered — a second thing you can end without touching the first — is
what pruning a receipt now offers, without the restriction.

**Two programs in one process is untouched.** Nothing here re-introduces
a process-wide pointer to an active map, and the proof that two
independently-built programs on separate pools do not interfere stays
exactly as it is ([908](completed/908-two-maps-in-one-process.md)). What
goes is the *restriction*, not the coexistence.

## Suggested implementation steps

1. Retire starting a program beside another: the call, the borrowed-pool
   flag, and the test that proves the two cannot be wired.
2. Keep the receipt alive past wiring, and give the caller a way to hold
   several.
3. Add ending a program: prune every station in a receipt in one sweep,
   then release the receipt.
4. Decide what a closing program means now that closing was per-table —
   winding down is a property of the pool, and refusing new outside work
   is a property of a marked port, so the per-program flag may simply go.
5. Tests: two descriptions placed into one program and wired to each
   other, which the old rule forbade; one of them pruned while the other
   keeps running; the pruned one's stations reused by a later placement.

## Related

- [212 — One way to build a program](completed/212-one-way-to-build-a-program.md),
  whose starting-beside half this retires
- [216a — Removing several stations at once](216a-removing-several-stations-at-once.md),
  the sweep this needs
- [217a — One receipt for a box and a map](217a-one-receipt-for-a-box-and-a-map.md),
  which makes the receipt universal
- [908 — Two maps in one process](completed/908-two-maps-in-one-process.md),
  which stays true and is not what this changes
- [090 — One station table per processor](../docs/implementation-notes/090-one-table-per-processor.md),
  which this note has to answer to
