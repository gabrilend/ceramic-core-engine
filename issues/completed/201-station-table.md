# 201 — The station table

## Current behavior

Built, in the station layer under `src/`, split into a structural
file (allocation, placement, wiring, teardown) and a motion file
(everything that moves a value), so an error in one is findable
without reading the other. The table is one flat allocation of
identical fixed-size records; slots, ports, and destinations hang off
pointers; every cross-reference is an index. Ports and destinations
append at their list tails so wiring order is preserved, which the
loader's round-trip will later rely on. The kind field and iterator
cursor are placed and inert as planned. Proven by a test that floods
one slot through seven doublings and confirms every station's address
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
many input slots they have, how many output ports, how large each
buffer is. If any of that were stored inline the array would hold
variable-length records and stop being indexable. So everything that
varies hangs off a pointer, and the struct itself is uniform.

The same decision has a second payoff: **a station never moves.** Its
buffers grow by reallocating the storage a slot points at, not the
station. This is what lets a wire hold a station index forever without
fixing up.

**Fields:**

| Field | Type | Meaning |
|---|---|---|
| mutex | `pthread_mutex_t` | Guards the input slots. Held during delivery and the readiness check. |
| call | function pointer | The shim for this station's box. Hand-written until phase 3. |
| kind | `unsigned char` | Plain, comparator, or iterator. Consulted only on the way out. |
| slots | pointer to an array | Input slots, in the box function's parameter order. |
| n_slots | `int` | How many. |
| ports | pointer to a linked list | Output ports. |
| cursor | `int` | Which port an iterator sends to next. Unused by other kinds. |

The kind field and the cursor are placed now and left inert until phase
5, so that adding routing is a change to the delivery path rather than
a change to this structure.

## Suggested implementation steps

1. Create the station struct and the table in `src/`, with creation
   taking a station count and allocating the array once.
2. Slot array allocation per station, sized by the number of inputs.
3. The output port list, with a port holding a linked list of
   destinations, each destination a pair of 32-bit numbers — which
   station, which slot. Both numbers are needed: the delivery path
   takes the destination station's mutex and examines all of its slots,
   so it must be able to name the station, not merely land inside it.
4. Teardown that frees slots, ports, and destination lists.
5. A test that builds a table by hand, walks it by index, and confirms
   that growing a slot's storage leaves every station's address
   unchanged.

## Related

- [002 — Stations and slots](../docs/002-stations-and-slots.md)
- Issue 202 — what goes in the slots array
- Issue 207 — how a table gets built before map files exist
