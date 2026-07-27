# 004 — Datapath: gathering

Delivery ([003](003-datapath-delivery.md)) is a push: a box finishes,
and its value is carried forward into whoever was waiting for it.
Gathering is the one place the engine runs backwards. A slot with no
buffer reaches upstream at the moment it is needed and pulls a value
into existence.

It exists for one reason: some values should be fresh at the moment
they are used, not fresh at the moment they were produced. A file's
contents, a clock, an environment variable. Pushing those means they
were correct once, some time ago.

## What a gatherer is

**A gatherer is a station with no ring-buffer slots.** Every one of its
slots is either static or itself gathered, or it has none at all.

This is not a rule imposed on top of the design; it falls out of it. A
station with no ring buffer has nothing that can ever be written into
it, so no delivery can ever discover it, so being pulled is the only
way it could ever run.

The same fact decides which end of a wire is which. A box whose output
feeds a gatherer slot is pulled. A box whose output feeds a ring buffer
is pushed. A box whose output fans out to both is neither coherently,
and the loader rejects it by name.

## The path

This happens inside step 8 of delivery — the task struct is being
assembled, the station's mutex has already been released, and one of
the slots turns out to be a gatherer.

**1. Read the slot's `source` field** to get the upstream station's
index.

**2. Assemble that station's arguments.** Its slots are static or
gathered. Statics are read from the table. Gathered ones recurse into
this same path.

**3. Call its shim** on the worker's own stack. No task struct is
pushed, no pool is involved, no mutex is taken on the upstream station
— there is nothing on it to guard, because it has no buffers.

**4. Copy the return value straight into the task struct** being
assembled.

The upstream station is never touched in any way that persists. The
value exists only inside the task struct that asked for it.

## Consequences worth knowing

**This is the one exception to "a box only runs when a worker picks it
up from the pool."** A gathered box runs inline, on a thread that is in
the middle of doing something else. It is worth naming the exception
rather than letting it hide, because it is where the surprises will
come from.

**A gatherer must be safe to run from several threads at once.**
Ordinary boxes have their values handed to them, and the guarantee that
two invocations do not collide comes from the values being claimed under
a mutex. A gatherer reaches out to the world instead — a file, a clock —
and two workers can be inside it at the same instant. Opening a file,
reading it, and closing it is fine. Keeping an open file handle
somewhere and seeking in it is not.

**A gatherer runs once per task assembled, not once.** A station
enqueued a million times pulls from its gatherer a million times. That
is precisely what "fresh at the moment it is used" asks for, but it
means a gatherer that touches a slow disk is a cost paid on the
delivery path, over and over.

**A gatherer cannot fail by declining.** The task struct has a slot
waiting for bytes and no way to represent their absence. A read box
pointed at a file that is not there stops the program and says so.

**Gatherers may chain.** A gatherer's slot may itself be gathered, and
the chain is walked inline. Depth is a static property of the map, so
the cost is bounded and knowable.

**A cycle in a gather chain is fatal.** Gathering is a function call
that has not returned yet, so a loop recurses until the stack dies —
which surfaces as a segfault with no message and no indication that two
boxes in a map point at each other. Cycles are therefore checked when a
connection is made, not when it is traversed. See below.

## Cycle checking

The check is cheap because of one fact: **if the graph is acyclic
before an edge is added, any cycle that edge creates must run through
it.** So there is never a reason to scan the graph. Start at the new
wire's destination, walk forward along gather links, and see whether
you arrive back at its source.

Start from an empty map, refuse every edge that closes a loop, and the
map is acyclic forever by induction. Cost is the length of one chain,
paid once per connection — a few hundred at load, occasionally at
runtime if the map is ever edited while running. The same walk yields
the chain's depth for free, which is the worst-case inline work a
worker will do while assembling a task.

**The check applies only to gather wires.** A cycle in the push
direction is legal and useful. Since a box cannot remember anything, a
loop through a ring buffer is the only way to build a counter: the
value goes out, comes back around, and arrives as the next run's input.
A blanket cycle check would forbid the sole mechanism the engine has
for carrying state.

The difference is that a push cycle passes through a buffer and the
call ends; a gather cycle is a call that never returns.

## Related

- [002 — Stations and slots](002-stations-and-slots.md), the slot kinds
- [003 — Delivery](003-datapath-delivery.md), step 8 is where this runs
- [009 — Loading](009-datapath-load.md), where the cycle check happens first
