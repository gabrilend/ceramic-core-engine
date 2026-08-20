# 216 — Removing a station

A station can be taken out of a running program and its place reused.
This overturns a standing guarantee, and it overturns it into a
stronger one rather than a weaker one.

## Current behavior

**A station cannot be removed**, and guarantee
[S2](../docs/058-guarantees.md) says so: *a wire is an index, never a
pointer; a wire written down today is valid forever* — held by
*stations can never be removed or reordered, only added to a table that
has room.*

The reason is mechanical. A wire is a destination record living on some
station's output port:

```c
typedef struct destination {
    int32_t station;   /* index into map.stations */
    int32_t slot;      /* which input port of it  */
    struct destination *next;
};
```

Remove station 2 from a four-station map and there are two bad
outcomes. Leave a hole and the slot is dead forever. Shift everything
down and station 3 becomes station 2, so **every destination record
naming a station above 2 is now wrong** — pointing past the end, or at
whatever moved into its place, silently.

**And a program that cannot remove anything leaks.** A long-running
program that adds stations as work arrives and never takes any away
holds every station it ever made. That is the real cost, and it is not
acceptable for a system whose point is being reconfigured while it
runs.

## Intended behavior

**A station is removed by removing the wires that name it first.** Then
nothing stale can exist, and the slot is free to reuse with no tag, no
version, and no cost anywhere on the delivery path.

The sequence:

1. **Take the rewiring lock.** Removing and wiring must not interleave,
   or somebody draws a wire to a station that is going away.
2. **Find every wire that names it.** A wire exists only as a
   destination record on some station's output port, so walking every
   station, every output port, every destination finds all of them.
   Nothing else in the engine names a station.
3. **Remove those records.** Nothing can deliver into it now.
4. **Remove its own outgoing destinations**, so whatever it has already
   produced goes nowhere.
5. **Wait until no worker is inside it.** The only subtle step: a task
   built from this station may already be running and will touch its
   counters and output ports when it finishes, and a delivery may be
   mid-flight into its buffers.
   [310](310-boxes-compiled-at-runtime.md) already designs the
   mechanism — one counter per worker, bumped at the start and end of a
   whole task, swept until every worker has crossed a boundary.
6. **Free its ports, its buffers, its name.**
7. **Mark the slot free** for the next station to take.

**Every station slot is the same size, so any free one fits any new
station.** The record is fixed-size on purpose — the source says so:
*the array of these must stay indexable, and growing a buffer must
never move a station.* Everything variable is pointed to rather than
stored inline. So reuse is a matter of finding any free slot, not a
matter of finding one large enough.

### Why no version tag is needed

The usual solution to reusing a slot is a **generation counter**: a
number on the slot that increments each time it is reused, carried
alongside the index in every wire, and checked on delivery. It exists
to catch a *stale* wire — one written before the slot changed hands —
because otherwise an old wire silently delivers into whatever took the
place, which is a wrong answer that looks right.

**Step 3 makes stale wires impossible.** Removal is what removes them,
so there is never a wire naming a station that isn't there, so there is
nothing to catch. That saves four bytes on every wire and a comparison
on every single delivery, forever, in exchange for a walk during an
operation that happens rarely.

### The guarantee gets sharper, not weaker

**Was:** a wire written down today is valid forever, because nothing is
ever removed.

**Becomes:** a wire never names a station that is not there, because
removing a station is what removes the wires to it.

The second is a stronger statement about the running engine and it
costs nothing on the hot path. What is given up is only the *reason*
the old one held — the promise that the set of stations grows and never
shrinks, which was never worth anything by itself.

### Reaching a program from outside it

The one thing step 2 cannot find is a **caller outside the map**. The
input station accepts deliveries from outside, several callers into one
port, and the engine deliberately does not remember who called
([213](213-the-input-station.md)) — so there is nothing to walk.

**That is declared undefined rather than solved.** A program is reached
through its input and output stations, and holding on to anything else
across a removal is outside what the engine defines. This becomes a
stated guarantee rather than an assumption, so that a caller doing
something exotic knows it is doing something exotic.

## Suggested implementation steps

1. The wire-finding walk: every station, every output port, every
   destination, collecting those that name the target. On its own,
   testable without removing anything.
2. Removal as an operation on the construction surface
   ([212](212-one-way-to-build-a-program.md)), taking the rewiring lock
   and performing steps 2 through 4.
3. The quiescence wait, shared with
   [310](310-boxes-compiled-at-runtime.md)'s box unloading rather than
   built twice — it is the same question asked about a different thing.
4. Freeing, and the free slot list.
5. Placement prefers a free slot before growing the array
   ([211](211-growing-the-station-table.md)).
6. A test that a station is removed while values are in flight through
   its neighbours, and nothing tears.
7. A test that a slot is reused and the new station receives only what
   was wired to it — the proof that no stale wire survived.
8. A test that removing a station some wire still names is refused and
   names the wire, if refusal is chosen over silent disconnection; see
   the open question.

## Open questions

- **Does removal disconnect wires silently, or refuse when any exist?**
  Disconnecting is what makes the sequence above work and is the
  obvious reading of "remove this station." Refusing — *station 7 is
  still fed by station 3; disconnect it first* — is louder and matches
  the project's habit of refusing rather than doing something helpful
  on your behalf. The second makes removal a two-step act for the
  caller and makes a self-modifying program wordier. Undecided.
- **Should a removed station's index be reused immediately, or held
  back for a while?** Immediate reuse is simplest and is safe by the
  argument above. Holding an index back for some period would catch a
  *caller's own* stale reference — code outside the engine that kept an
  index it was handed — but that is the undefined case named above, and
  buying half a defence against it may be worse than none.

## Related

- [211 — Growing the station table](211-growing-the-station-table.md),
  which this makes the other half of — a table that grows and shrinks
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  whose surface gains this operation
- [214 — Destinations without a lock](completed/214-destinations-without-a-lock.md),
  whose retire-sweep-free is the same lifetime problem in miniature
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  which designs the quiescence sweep and whose box unloading becomes
  far more useful once a station can be removed
- [213 — The input station](213-the-input-station.md), the door that
  outside callers use and the reason they cannot be walked
- [058 — Guarantees](../docs/058-guarantees.md), where S2 is restated
  and the interface guarantee is added
