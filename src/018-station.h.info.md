# 018-station.h — stations, ports, and maps, from outside

A map is a flat table of stations. A station is one placement of a
box: the buffers where its input values wait, the mutex guarding
them, and the ports its output leaves through. Wires name stations by
index — a 32-bit number, never a pointer — so nothing dangles when
buffers grow.

## Data structures

**port** — one input's waiting place.
| field | type | meaning |
|---|---|---|
| kind | `unsigned char` | Ring buffer (0), static (1), or no source yet (2). Stored, never inferred. |
| elem_size | `int` | Bytes per value; exactly the parameter's size. |
| pages | `in_port_page_t *` | The ring's slots, in a list of equal-sized pages. The first is allocated at placement whatever the kind; growth appends. **Nothing already there ever moves**, which is what lets a worker copy out of a slot it owns while holding no lock. Never freed until the map is. |
| page_slots | `int` | Slots per page, the same for every page of this port, and the same number the first page was given. Raising a port's starting depth therefore makes every page large rather than making a long chain of small ones. |
| capacity | `int` | Total slots across every page, all of them usable. Starts at ten unless the port was told otherwise. A sum rather than one allocation's size, which is what the buffer report speaks. |
| stride | `int` | Bytes from one slot to the next: a value, its state, and padding to keep the next value aligned. |
| read_hint, write_hint | `int` | Where a reader and a writer each start looking. Hints, not positions. |
| held | `int`, atomic | Slots ready right now. |
| constant | `void *` | The static's value, `elem_size` bytes, allocated at placement like the slots. Kept when the port is converted away, so a port that goes static, buffer, static reads the value it read before. |
| constant_string | `char *` | A string constant's characters, since the value is a pointer that has to point at something the port owns. |
| constant_set | `int` | Whether anybody has written one. |
| growths, high_water | `int` | How many doublings, and the deepest backlog — phase 7's reading. |

**The three kinds, and what each one answers when readiness asks
whether the port holds a value.** A *buffer* answers yes when
anything is waiting in it. A *static* always answers yes, and is
never consumed. A port with *no source yet* always answers no — so
the station holding it can never run, which is what lets a station
exist before anybody has finished wiring it. That last one is a
state, not a value: no null is invented and nothing is ever handed to
a box.

**The slots outlive the kind.** A port keeps its buffer whatever it is
currently for, so changing a port's source is a field write rather
than an allocation, and values already waiting in it are still there
afterwards. A port that is a static all its life carries slots it
never uses; that is paid once, at startup.

**A slot says what is happening to it**: nothing here, a writer is
filling me, the bytes have landed, a reader is emptying me. Two threads
can never own one slot. That is the mutual exclusion, per slot rather
than per port, and it is what lets a reader *look* for a usable slot
instead of computing where one must be — which in turn is what let the
copying leave the station's lock, and what lets a buffer grow by adding
a page rather than copying.

**Only one of the four moves is a compare-and-swap**, and knowing which
is the point. Empty → reserved is where two writers genuinely race for
the same slot, so the loser must be told it lost. The other three have
exactly one possible mover: an owner moving a slot it holds, or a
claimer under the station's mutex taking a ready one, which nothing
else can touch. Those are a load and a store, with acquire and release
ordering so the bytes travel with the state. The distinction is not
cosmetic — the claim's search asks this of every candidate it walks
past, so a read-modify-write per candidate was measurable where a load
is not.

Slots are never cleared when released. Every write covers the full
element size, so a stale value is always completely overwritten; the
promise is not that a slot was cleaned but that its bytes are never
read unless its state says ready.

**destination** — one landing place: `{station int32, port int32}`,
linked. **port** — one exit: a linked list of destinations, itself
linked to the station's next port.

**station** — fixed-size record; everything variable hangs off
pointers so the table stays indexable and no station ever moves.
Fields: mutex, call (the shim), kind (plain 0 / comparator 1 /
iterator 2), box_name (the literal its placement function wrote, null
when placed by hand), in_ports + n_in_ports, out_ports + n_out_ports,
cursor
(the iterator's one memory), out_size (`int`, bytes of the return
value, 0 = sink).

**map** — stations + count + the pool delivery pushes into. It used to
carry a numbered table of shared constants too; that is gone, and with
it the process-wide pointer naming one map as *the* map, so any number
of maps can run in one process without seeing each other.

## Functions

**map_create(station count) → map** — N places reserved up front.
**map_create_empty() → map** — no places at all, grown one at a time,
which is what reading a file does now.
**map_add_station(map) → index** — one more place, reusing a removed
one before growing. The table is shelves, so growing never moves
anything already placed.
**map_station(map, n) → station** — one shift, one mask, one
dereference.

**map_place(map, station index, shim, kind, port count, element
sizes array, output size)** — put a box at a station: one ring-buffer
port per element size. Scaffolding until the loader takes over
(phase 6); sizes come from the registry from phase 3.

**map_configure_port(map, station, port, source, text) → NULL or a
sentence** — **the one operation that says where a port's values come
from.** A station, a port, a source, and — when the source is a value
— the value itself as text. Binding a constant, taking a source away,
and giving a port back to the arrows were three calls with three
shapes; they are cases of this one. Text distinguishes the two ways to
become a constant: given some, the port takes that value; given none,
it goes back to the value it held before, and having never held one is
refused.

The tag changes and nothing else: slots are not freed, not cleared,
not drained, so values waiting in a buffer are still waiting if it
becomes a buffer again. Takes the station's mutex, so no readiness
walk sees a port mid-change.

The honest cost of losing nothing: a port converted away and back may
serve a value that arrived before the conversion after values that
arrived during it. Arrival order is not promised, so nothing that was
still being offered is lost here.

**It returns a refusal rather than stopping**, which is what lets a
caller reading a file collect every mistake in it and present them
together. Reading a file is one of its two callers; a program editing
another program, through the construction boxes, is the other.

**map_in_port_start_depth(map, station, port, slots) → NULL or a
sentence** — tell one port how deep its buffer should start, which
also sets the size of every page it will ever add. A hint, not a
setting: growth covers being wrong, so nobody has to be right. Refuses
a port that already holds values, and refuses fewer than one slot.
Worth using when an author already knows one input side outruns its
siblings and would rather not watch it grow thirteen times to find
out. It used to stop the program instead of refusing, and was the last
port operation that did.

**map_in_port_convert(map, station, port, kind)** — the same
operation under the name callers already say, for the cases with no
text: it stops the program on refusal rather than handing one back.

**map_bring_up(map) → NULL or a collected complaint** — a caller
declaring a program finished: the whole-program checks run, and
everything that can run without waiting for an arrival is started.
Repeatable, and nothing is started when anything is wrong. What it
refuses: an arrow landing on a port that is not a buffer, and **a
program that never says where its results come from** — one station
has to be marked as the way out, though nothing need be wired into it.
What it warns about without refusing: a port with no source, and
buffered inputs no arrow feeds.

**map_check_sources(map) → NULL or a sentence** — every parameter with
nowhere to get a value, named and counted. Asked when somebody says
the program is finished, not while it is being assembled, because a
port with no source is the ordinary state of a station nobody has
finished wiring. No exceptions, ever.

**map_wire(map, from station, port, to station, to port) → NULL or a
sentence** — **the one wiring operation**, legal at any moment,
applying every rule: the source is placed and is not a sink, the port
index means something for that station kind, the destination port
exists and is a buffer, and the two widths agree. Ports are created in
order, no gaps; repeat a port to fan out.

**map_connect(map, from station, port index, to station, to port)** —
the same operation, for a caller that wants a refusal to stop the
program.

**map_start(map, worker count)** — create the pool with delivery as
its finish hook. Seed values before releasing `map->pool`; release
and join through the pool interface.

**map_deliver_value(map, station, port, value pointer)** — deliver
one value: lock, write, readiness check, claim if complete, unlock,
then build and push a task if one became due. Both the interior of
the delivery walk and the way tests seed a map.

**map_deliver(context, task)** — the delivery walk itself; the pool
calls it after every task. Sinks skip it; a port wired nowhere
discards.

**map_in_port_depth(map, station, port) → int** — values waiting right
now. For demos and diagnostics only.

**map_destroy(map)** — tear everything down, pool included.
