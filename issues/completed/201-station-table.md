# 201 — The station table

## Current behavior

**Built, and the one flat allocation is becoming shelves.**

Under [212](../212-one-way-to-build-a-program.md), adding a station is
the only way one ever comes into existence, so the table starts empty
and grows while a program is read. It grows by adding another
allocation and a pointer to it, never by reallocating
([211](211-growing-the-station-table.md)) — because this issue put
the station's mutex inside the station record, and a mutex is
identified by where it lives. Move one and every thread parked on it
waits at an address nobody will unlock.

**Which is to say the invariant this issue wrote down is not being
traded away — it is the reason for the shape.** *Growing a buffer must
never move a station* was proved here by flooding one port through
seven doublings and finding every address unchanged. It now has to hold
for growing the *table* as well, and shelves are what make that
possible.

Two other decisions here are load-bearing and unchanged: every
cross-reference is an **index rather than a pointer**, which is what
lets the table grow at all, and ports and destinations **append at
their tails so wiring order is preserved**, which the dump's round trip
depends on and which [214](214-destinations-without-a-lock.md) has
to keep when it replaces the destination list with an array.

The remainder describes it as built.

Built, in the station layer under `src/`, split into a structural
file (allocation, placement, wiring, teardown) and a motion file
(everything that moves a value), so an error in one is findable
without reading the other. The table is one flat allocation of
identical fixed-size records; ports, ports, and destinations hang off
pointers; every cross-reference is an index. Ports and destinations
append at their list tails so wiring order is preserved, which the
loader's round-trip will later rely on. The kind field and iterator
cursor are placed and inert as planned. Proven by a test that floods
one port through seven doublings and confirms every station's address
and every neighbour's contents are untouched.

## Intended behavior

One flat array holding every station in a loaded map, allocated once
and not resized while the program runs. A station is addressed by its
position in that array — a 32-bit index, never a pointer.

**Why an index.** It is half the size of a pointer. It survives being
written to a log or dumped to the screen, where an address is noise.
And it leaves room for the array to be reallocated when runtime map
editing arrives in phase 7, without every wire in the program becoming
a dangling reference.

**Why the struct is fixed-size.** Stations vary enormously — in how
many input ports they have, how many output ports, how large each
buffer is. If any of that were stored inline the array would hold
variable-length records and stop being indexable. So everything that
varies hangs off a pointer, and the struct itself is uniform.

The same decision has a second payoff: **a station never moves.** Its
buffers grow by reallocating the storage a port points at, not the
station. This is what lets a wire hold a station index forever without
fixing up.

**Fields:**

| Field | Type | Meaning |
|---|---|---|
| mutex | `pthread_mutex_t` | Guards the input ports. Held during delivery and the readiness check. |
| call | function pointer | The shim for this station's box. Hand-written until phase 3. |
| kind | `unsigned char` | Plain, comparator, or iterator. Consulted only on the way out. |
| ports | pointer to an array | Input ports, in the box function's parameter order. |
| n_in_ports | `int` | How many. |
| ports | pointer to a linked list | Output ports. |
| cursor | `int` | Which port an iterator sends to next. Unused by other kinds. |

The kind field and the cursor are placed now and left inert until phase
5, so that adding routing is a change to the delivery path rather than
a change to this structure.

## Suggested implementation steps

1. Create the station struct and the table in `src/`, with creation
   taking a station count and allocating the array once.
2. Port array allocation per station, sized by the number of inputs.
3. The output port list, with a port holding a linked list of
   destinations, each destination a pair of 32-bit numbers — which
   station, which port. Both numbers are needed: the delivery path
   takes the destination station's mutex and examines all of its ports,
   so it must be able to name the station, not merely land inside it.
4. Teardown that frees ports, ports, and destination lists.
5. A test that builds a table by hand, walks it by index, and confirms
   that growing a port's storage leaves every station's address
   unchanged.

## Related

- [002 — Stations and ports](../../docs/002-stations-and-ports.md)
- Issue 202 — what goes in the ports array
- Issue 207 — how a table gets built before map files exist
