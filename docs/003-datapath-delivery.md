# 003 — Datapath: delivery

The central path of the engine. It answers one question: a box function
has just returned a value — what happens between that moment and the
moment some other box runs because of it?

## The path

A worker has just executed a task. It holds the return value in the task
struct's output field, and the task records which station produced it.

**1. Choose a port.** The station's kind decides which output port this
value goes down. A plain box has one. A comparator compares the value
against its threshold port and picks one of three. An iterator uses the
port its task was assigned when it was enqueued. This is the whole of
[005](005-routing.md).

**2. Walk that port's destination list.** Each `{station, port}` pair is
delivered to independently. A port wired to a hundred destinations means
a hundred deliveries by this one worker before it goes back for more
work — acceptable, because each delivery may unblock a station, so the
worker is manufacturing parallelism for everyone else.

**3. Write the value into the port, taking no lock.** Search for an empty
slot from where the port's write hint points, sweeping forward until it
wraps; take the first that will move from empty to reserved, `memcpy`
`elem_size` bytes into it, and publish it as ready. If the sweep finds
nothing the buffer grows first, which *is* one of the rare operations
that takes the mutex — see [002](002-stations-and-ports.md).

**Nothing is excluded here and nothing needs to be.** Reserving a slot is
a single compare-and-swap, so two writers reaching for the same one
cannot both win; and once a slot says *reserved* it belongs to that
writer alone. Deliveries into one station never serialize against each
other. Only task construction does.

The hint is read once and the sweep is bounded by it: at most every slot,
exactly once each. Re-reading a hint that other workers keep pushing
forward would let a searcher chase it, and a search that can be outrun is
a search with no bound.

**4. Take the station's mutex.** The only lock a delivery takes, and what
it covers is the slot *states* — never the value bytes.

**5. Run the readiness check.** Walk every port and ask whether it holds
a value. A ring buffer does if its count of ready slots is above zero. A
static always does. A port with no source never does, so a station
carrying one waits forever — which is what lets a station exist before
anybody has finished wiring it.

This same check is what a **write to a static** triggers
([004](004-datapath-statics.md)). It arrives from a different direction
and answers the same way, which is why nothing extra is needed to make a
static write run a station, and why a write can never run one whose ring
port is empty.

If any port is empty, release the mutex and move on. The value sits in
the buffer waiting for its siblings.

**6. If every port is occupied, take one slot from each — and copy
nothing.** The same sweep as step 3 run the other way, looking for a
ready slot and moving it to claimed. That is all that happens under the
lock: a search and one state write per port. Taking the slot is what
makes the value spoken for, since a slot moves out of ready exactly once.
Which value a port yields is not promised to be the oldest, and
[058](058-guarantees.md) says why.

**Check every port before taking from any of them.** Nothing can remove a
ready slot in between: other claimers are excluded by this mutex, and a
writer only ever *adds* availability. So either every port answers and
the claim succeeds outright, or one does not and nothing was taken —
which is why there is no undo path and why two workers cannot each end up
holding half a claim.

A static is the exception in both directions: peeked rather than taken,
and its bytes copied **here**, inside the lock, because nothing owns a
static and the mutex is all that stands between this copy and somebody
writing that constant.

If the station is an iterator, its cursor advances now and the port it
landed on is recorded.

**7. Release the mutex.** The contended section contained no `memcpy` at
all.

**7a. Copy the claimed values out, and release the slots.** With no lock
held. Each slot is in *claimed*, which means it belongs to this worker,
so the copy needs no exclusion — several workers can be doing this on one
station while another holds the lock doing its takes. This is where the
expense of a delivery lives, a two-hundred-byte struct per port, and it
runs fully in parallel.

**8. Build the task struct.** Allocated fresh, sized exactly for this
box. It receives the values claimed in step 6, the station's shim
pointer, the station's index, and for an iterator the port chosen in step
6.

**9. Push the task onto the pool** and continue to the next destination.

When every destination has been handled, the worker frees its own task
struct and goes back to the pool.

## Why the values are spoken for before the mutex is released

Step 6 is why two invocations of one station can run at once without
interfering. By the time the mutex is released, this invocation's slots
are *claimed* and nothing else may touch them. A second thread arriving
immediately afterward finds different slots and builds a different task.

**It is the exclusive claim that does the work, not the copying.** Once a
slot says claimed its bytes are private property, which is why the copy
happens outside, in parallel across every worker doing the same thing to
the same station.

This is also why a box may not remember anything. The station is guarded,
but the box function runs long after the mutex was dropped, on whichever
worker picks the task up, and two of them can be inside the same box
function at the same instant. The only state a station keeps across
invocations is the iterator's cursor, touched only in step 6, under the
mutex, by the enqueuing thread.

## Why nothing polls

No part of this path searches for work. The readiness check runs on
exactly one station: the one just written to. A station whose inputs have
not changed cannot have become ready.

The consequence is that a station with no ring-buffer inputs is never
discovered this way. Such a station runs when one of its statics is
**written** — which happens when the program is built, and any time a
wire delivers into a static port or somebody turns a knob
([004](004-datapath-statics.md)). That is also what starts a program:
binding the statics is a write, and the writes that build it are the
writes that set it going.

## Boxes that return nothing

A box whose function has no return value is a sink — a file write, a
print, an effect. Delivery is skipped and the worker goes straight to
freeing its task. Sinks fall out of a function declared `void`.

## Related

- [002 — Stations and ports](002-stations-and-ports.md)
- [005 — Routing](005-routing.md), which port step 1 chooses
- [006 — Scheduling](006-datapath-scheduling.md), what step 9 pushes into
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md), which is why step 8 has nothing to resolve
