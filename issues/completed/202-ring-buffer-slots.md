# 202 — Ring-buffer input slots

## Current behavior

**Built, and the two indices are going.**

Head and tail are *positions*, taken modulo the capacity — which is
what forces growth to physically move every value back into order when
the capacity changes, and what makes a ring buffer the one thing in the
engine that cannot grow by simply adding more room.
[210](../210-input-port-record.md) replaces them: each cell carries its
own state, a reader scans from a bookmark that is allowed to be wrong,
and nothing anywhere computes a location from the capacity. Then a
buffer grows by adding a page, nothing is copied, and the ordering
hazard around the copy stops existing rather than being handled.

What that costs is this issue's proven property: **values no longer
leave a port in the order they arrived.** The test here — several
hundred deliveries of a struct-and-int pair, every pair byte-identical
and in order — is correct about the engine as it exists and stops being
right the moment the new claim lands. Retiring it is a step inside that
change, not a bug being tolerated.

What survives is the thing that made this fast in the first place:
**cells are exactly the size of the parameter the port feeds**, so a
write is a memory copy into a fixed offset with no allocation anywhere
on the path. That is untouched, and everything above is arranged so it
stays true.

The remainder describes it as built.

Built, in the motion half of the station layer. A slot carries its
one-byte kind tag from the start, with the ring buffer as the only
populated row; cells are exactly the element size handed to placement,
so every write is a memcpy into a fixed offset with no allocation on
the hot path. Write advances the tail, pop advances the head, and
occupancy is head-differs-from-tail — answerable under the station's
mutex alone. Growth on collision went in alongside (issue 203) rather
than the planned loud failure, since both were built in one sitting.
Proven by a test pairing a padded 40-byte struct with an int through
several hundred deliveries: every pair byte-identical, in order.

## Intended behavior

The ordinary kind of input slot: a ring buffer where values arrive by
being written and wait their turn.

Two other slot kinds exist in the design — gatherers and statics — and
both arrive in phase 4. The slot carries a one-byte tag from the start
so that adding them is a new case rather than a new field. The tag is
**stored, never inferred**: asking "is the station upstream of me an
input-less one?" on every readiness check would mean chasing an index
into another station to answer a question that cannot change while the
program runs.

**Fields, of which only the first four matter in this phase:**

| Field | Type | Meaning |
|---|---|---|
| kind | `unsigned char` | Ring buffer, gatherer, or static |
| elem_size | `int` | Bytes per value |
| storage | `void *` | The cells |
| capacity | `int` | How many cells |
| head | `int` | Where the oldest value sits |
| tail | `int` | Where the next one goes |
| source | `int` | Gatherer only — phase 4 |
| static_id | `int` | Static only — phase 4 |

**Cells are exactly `elem_size` bytes.** Not a maximum, not a union of
every type in the program — the exact size of the parameter this slot
feeds. In this phase the size is supplied by hand alongside the
hand-written shims; from phase 3 it comes from the registry, derived
from `sizeof` the real C type.

The payoff is that a write is a `memcpy` into a fixed offset with no
allocation anywhere on the delivery path, which is the hottest path in
the engine.

**A slot holds a value when head and tail differ.** That is the whole
of the readiness test for this kind, and it must be answerable while
holding only the station's mutex.

## Suggested implementation steps

1. Add the slot struct to `src/`, with the tag defaulting to ring
   buffer.
2. Slot initialization taking an element size and a starting capacity,
   allocating the cells once.
3. Write: `memcpy` into the cell at the tail, advance the tail. Growth
   when the tail would collide with the head is issue 203; until then,
   fail loudly rather than overwrite.
4. Pop: copy out of the cell at the head, advance the head.
5. Occupancy test.
6. A test that writes and pops values of several different sizes,
   including a struct larger than a machine word, and confirms the
   bytes come back identical.

## Related

- [002 — Stations and slots](../docs/002-stations-and-slots.md)
- Issue 203 — growth
- Issue 401 — static slots, the second tag value
- Issue 403 — gatherer slots, the third
