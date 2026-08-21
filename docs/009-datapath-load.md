# 009 — Datapath: loading and starting

The binary holds a registry of boxes and no map. The map file holds a
map and no code. This is what happens in the moment between.

**Reading a file has no privileges**, and that is the shape of this
document now. It used to describe a sequence only one mechanism could
perform — count the stations, allocate the table once, place, wire,
validate in a phase nothing else could enter, seed, release — with a
state called *still loading* that nothing else could be in. Every one
of those has become an ordinary operation
([212](../issues/completed/212-one-way-to-build-a-program.md)). What is left of
reading a file is a reader: each line becomes calls anybody could
make, and then it asks for the program to be brought up like anybody
would.

## Two passes, for one reason

Declaration order in a map file does not matter — an arrow may point at
a station declared further down. That requires reading the file twice.

**The two-pass structure survives only as that sentence**: resolve
names after every station exists. It is not two kinds of pass any
more, and neither pass can do anything the construction surface does
not offer.

**First pass: create every station.** For each station line, look its
box function up in the registry. That gives the shim pointer, the
parameter count, and each parameter's type and size. Allocate the
station's port array with one ring-buffer port per parameter — the
default — each sized exactly `sizeof` its parameter, and record the
station's name — through the operation that names one, **as the
station is created**, so that the name lands on the program itself.

The loader used to keep a lookup table of its own for resolving
arrows, and it does not any more: the names are on the program, so the
program is what gets asked. That is one fewer allocation and one
larger thing — every refusal raised from here onwards can say which
station it is about in the word the file's author typed, instead of
numbering it.

A comparator gets one extra port on the end, typed to match the box's
return value.

Then apply the input lines. Each one is a call on the port
configuration operation: a starting depth where the line gave one,
then a source — a bare dash for none, or text for a constant, copied
into the port at the port's own type.

**Second pass: resolve the arrows.** By now every station exists and
can be found by name. For each output line, turn the destination name
into an index — the one thing here that is a fact about the file
rather than about the program — and then draw the wire through the
ordinary wiring operation.

**The wire check belongs to that operation, not to loading.** Reading
a file used to perform a width check of its own before connecting, on
the grounds that this is the first moment both ends are known. It is,
and the wiring operation is reached at exactly that moment, so the
second copy bought nothing and cost something: while it existed, the
first one could have a hole that no program read from a file would
ever meet. It had one. A station with no input ports at all — a box
that takes nothing and returns a value — skipped the width question
entirely, and only a program built by calling the surface could have
found out.

## Then the program is brought up, which is a separate act

Some things are only visible once every station and arrow exists, and
asking about them is no longer part of reading a file. A caller
assembles a program by whatever route — reading a file, calling the
surface, or both — and then says it is finished. That is when the
whole-program checks run and when everything that can start does.

**It is repeatable**, which is the point rather than a convenience: a
station added to a running program is checked and started by the next
call, and one already started is not started twice. There is no
end-of-file moment to hang a whole-program question on, because
several files can build one program — but there is still a starting
gate, and that is where such a question can honestly be asked.

What it checks:

- **An arrow landing on a port that is not a buffer**, which would
  have nowhere to put its value. Fatal, and now caught earlier still —
  as the wire is drawn, since wiring applies every rule at any moment.
- **A port with no source.** Reported, not refused: that is the
  ordinary state of a station nobody has finished wiring, and being
  able to sit in it is what lets a program be assembled a piece at a
  time.
- **Unreachable stations.** A station with buffered inputs that
  nothing ever writes to will never run. Reported — unless it is a
  declared entrance, which is precisely a station something outside
  delivers into.

Two checks used to live here and can no longer be stated, because
nothing is pulled: a cycle among gather links, and a port fanning out
to both a gatherer and a ring buffer. See
[056](implementation-notes/056-no-pull-path.md). A cycle in the push
direction remains legal — it is how anything repeats — and needs a
finite companion input to ever stop.

## The seed

Nothing in the engine searches for work. A station is discovered only
by something writing into it, which means that at the instant the
program starts, nothing can happen — every station is waiting for a
delivery, and there is nobody to deliver.

So the station table is swept, at that gate, and this is the one place
in the engine that walks it looking for work. Everywhere else a
station is reached by index, through a wire.

**Enqueue every station that has no ring-buffer inputs.**

*No ring-buffer inputs* means the station has nothing that can ever be
written into it, so it will never be discovered by delivery. If it is
going to run at all, it must be started here.

**And the sweep is really the writes finishing.** Binding a static from
the file is a write, and a write runs the readiness check on the
station holding it ([004](004-datapath-statics.md)). A station with only
statics is ready the moment its last one is bound, so what looks like a
separate startup step is the ordinary mechanism arriving at the end of
construction. Nothing here is special-cased; the same thing happens
when a station is added to a program that is already running.

After the sweep, the pool is running and the map propagates on its
own — until somebody adds a station and brings the program up again,
which sweeps once more and starts only what it has not already
started.

## Termination, from the map's point of view

The program ends when the pool has nothing left to run and no worker is
mid-task — see [006](006-datapath-scheduling.md). For a map, that
means every value that entered has finished travelling and no station
is waiting on one that will never come.

A map that should run indefinitely keeps itself alive by looping: a
value that comes back around into a ring buffer is a new delivery, and
a new delivery is new work. Such a map is expected to check a stop
condition somewhere in the loop, because nothing else will stop it.

## Related

- [008 — Map file format](008-map-file-format.md), the input
- [007 — The build path](007-datapath-build.md), the registry this is read against
- [006 — Scheduling](006-datapath-scheduling.md), what the seed pushes into
