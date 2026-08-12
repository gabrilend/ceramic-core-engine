# 018-station.h — stations, slots, and maps, from outside

A map is a flat table of stations. A station is one placement of a
box: the buffers where its input values wait, the mutex guarding
them, and the ports its output leaves through. Wires name stations by
index — a 32-bit number, never a pointer — so nothing dangles when
buffers grow.

## Data structures

**slot** — one input's waiting place.
| field | type | meaning |
|---|---|---|
| kind | `unsigned char` | Ring buffer (0) or static (1). Stored, never inferred. |
| elem_size | `int` | Bytes per value; exactly the parameter's size. |
| storage | `void *` | The ring's cells. Reallocated on growth; the slot itself never moves. |
| capacity | `int` | Cells allocated. One always spare, so usable is one less. |
| head, tail | `int` | Oldest value / next free cell. Equal means empty. |
| static_id | `int` | Static only: statics-table entry (phase 4). |
| growths, high_water | `int` | How many doublings, and the deepest backlog — phase 7's reading. |

**destination** — one landing place: `{station int32, slot int32}`,
linked. **port** — one exit: a linked list of destinations, itself
linked to the station's next port.

**station** — fixed-size record; everything variable hangs off
pointers so the table stays indexable and no station ever moves.
Fields: mutex, call (the shim), kind (plain 0 / comparator 1 /
iterator 2), slots + n_slots, ports + n_ports, cursor (iterator's one
memory), out_size (`int`, bytes of return value, 0 = sink).

**map** — stations + count + the pool delivery pushes into.

## Functions

**map_create(station count) → map** — the one flat allocation.

**map_place(map, station index, shim, kind, slot count, element
sizes array, output size)** — put a box at a station: one ring-buffer
slot per element size. Scaffolding until the loader takes over
(phase 6); sizes come from the registry from phase 3.

**map_connect(map, from station, port index, to station, to slot)** —
draw a wire. Ports are created in order, no gaps; repeat a port to
fan out. Destination must already be placed. Fails loudly on any
nonsense (slot out of range, wiring from a sink, gaps in ports).

**map_start(map, worker count)** — create the pool with delivery as
its finish hook. Seed values before releasing `map->pool`; release
and join through the pool interface.

**map_deliver_value(map, station, slot, value pointer)** — deliver
one value: lock, write, readiness check, claim if complete, unlock,
then build and push a task if one became due. Both the interior of
the delivery walk and the way tests seed a map.

**map_deliver(context, task)** — the delivery walk itself; the pool
calls it after every task. Sinks skip it; a port wired nowhere
discards.

**map_slot_depth(map, station, slot) → int** — values waiting right
now. For demos and diagnostics only.

**map_destroy(map)** — tear everything down, pool included.
