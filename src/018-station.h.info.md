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
| kind | `unsigned char` | Ring buffer (0), static (1), or no source yet (2). Stored, never inferred. |
| elem_size | `int` | Bytes per value; exactly the parameter's size. |
| storage | `void *` | The ring's cells. Allocated at placement whatever the kind, and never freed until the map is. Reallocated on growth; the slot itself never moves. |
| capacity | `int` | Cells allocated, all of them usable. Starts at ten unless the port was told otherwise. |
| stride | `int` | Bytes from one cell to the next: a value, its state, and padding to keep the next value aligned. |
| read_hint, write_hint | `int` | Where a reader and a writer each start looking. Hints, not positions. |
| held | `int`, atomic | Cells ready right now. |
| static_id | `int` | Static only: statics-table entry (phase 4). Kept when the port is converted away, so a port that goes static, buffer, static reads the same entry. |
| growths, high_water | `int` | How many doublings, and the deepest backlog — phase 7's reading. |

**The three kinds, and what each one answers when readiness asks
whether the port holds a value.** A *buffer* answers yes when
anything is waiting in it. A *static* always answers yes, and is
never consumed. A port with *no source yet* always answers no — so
the station holding it can never run, which is what lets a station
exist before anybody has finished wiring it. That last one is a
state, not a value: no null is invented and nothing is ever handed to
a box.

**The cells outlive the kind.** A port keeps its buffer whatever it is
currently for, so changing a port's source is a field write rather
than an allocation, and values already waiting in it are still there
afterwards. A port that is a static all its life carries cells it
never uses; that is paid once, at startup.

**A cell says what is happening to it**: nothing here, a writer is
filling me, the bytes have landed, a reader is emptying me. Every move
between those names the state it starts from and is one atomic swap,
so two threads can never own one cell and a move out of a state a cell
is not in is refused. That is the mutual exclusion, per cell rather
than per port, and it is what lets a reader *look* for a usable cell
instead of computing where one must be — which in turn is what lets
the copying leave the station's lock, and what will let a buffer grow
by adding a page rather than copying.

Cells are never cleared when released. Every write covers the full
element size, so a stale value is always completely overwritten; the
promise is not that a cell was cleaned but that its bytes are never
read unless its state says ready.

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

**map_slot_start_depth(map, station, slot, cells)** — tell one port
how deep its buffer should start. A hint, not a setting: growth covers
being wrong, so nobody has to be right. Refuses a port that already
holds values, and refuses fewer than two cells (one is always spare).
Worth using when an author already knows one input side outruns its
siblings and would rather not watch it grow thirteen times to find
out.

**map_slot_convert(map, station, slot, kind)** — change what a port
is. The tag changes and nothing else: cells are not freed, not
cleared, not drained, so values waiting in a buffer are still waiting
if it becomes a buffer again. Becoming a static is refused here
because it needs a value this call has no room for — bind through the
statics table instead. Takes the station's mutex, so no readiness
walk sees a port mid-change.

The honest cost of losing nothing: a port converted away and back may
serve a value that arrived before the conversion after values that
arrived during it. Arrival order is not promised, so nothing that was
still being offered is lost here.

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
