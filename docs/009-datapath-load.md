# 009 — Datapath: loading and starting

The binary holds boxes and no map. The map file holds a map and no
code. This is what happens in the moment between.

**Nothing here reads the map.** That is the shape of this document
now, and it is the second time this document has been cut down for the
same reason: the special thing it described stopped being special.

First, reading a file lost its privileges. It used to be a sequence
only one mechanism could perform — count the stations, allocate the
table once, place, wire, validate in a phase nothing else could enter,
seed, release — with a state called *still loading* that nothing else
could be in. Every one of those became an ordinary operation
([212](../issues/completed/212-one-way-to-build-a-program.md)), and
what was left was a reader making calls anybody could make.

Then the reader itself went
([311d](../issues/311d-the-map-becomes-code.md)). A description is
**compiled** into the calls it describes, and those calls are made. So
there is no reading step to describe at all, and no program built with
this engine carries a parser.

## What happens instead

A description on disk becomes a running program in four movements, and
only the last one is the engine's.

**One — the compiler is asked what the description references.** A
program that grew and then wrote itself down names boxes that arrived
after it started, and whoever reads it back may never have heard of
them. Anything missing is found and compiled in first. This is the
ordinary path rather than a rescue, because a description written by a
running program is the normal kind of description.

**Two — the description is compiled.** Its text goes to the generator,
which turns each station line into a call that adds a station, names
it, builds it from its box, marks its door and configures its ports,
and turns each arrow into a call that draws a wire. Then the C
compiler that built this binary compiles that.

**What is compiled is the description and nothing else.** The boxes it
names are already in the process — so what comes out declares the
functions that build stations from them and binds to the ones the
program published, rather than carrying a second copy of anything.
Measured on a real one: a whole program's structure in seventeen
kilobytes, defining no box code and asking the program for three
station-builders.

**Three — the result is loaded and its build function is called.** The
calls are made. They are the same calls a person writing C would make,
so a description and a hand-written program are one path with two
authors rather than two paths that must agree.

**Four — the program is brought up**, which is the same act any caller
performs and is described below.

## What this costs, and who pays

Two compiler invocations, about a tenth of a second each: one asking
what the description wants, one building it.

**A program built from its own descriptions pays none of it**, because
the build already did the compiling and the built function is simply
in the binary. The cost falls exactly where it should — on a program
being handed a description it was not built for, which is the only
case where there is genuinely new code to make.

**And it buys a fault caught earlier.** A box name answering to
nothing, and an arrow pointing at a station nobody declared, are both
refused while the description is still text, so the complaint names
the line. The engine could never do that: by the time it looked, there
were no lines left.

**Declaration order still does not matter**, and it costs nothing to
say so any more. An arrow may point at a station declared further
down, which used to require reading the file twice. The generator
emits every station first and every wire afterwards, which is the same
requirement expressed once, at build time, in the order the calls come
out.

**The wire check belongs to the wiring operation**, not to loading.
Reading a file used to perform a width check of its own before
connecting, on the grounds that this is the first moment both ends are
known. It is, and the wiring operation is reached at exactly that
moment, so the second copy bought nothing and cost something: while it
existed, the first one could have a hole that no program read from a
file would ever meet. It had one. A station with no input ports at all
— a box that takes nothing and returns a value — skipped the width
question entirely, and only a program built by calling the surface
could have found out.

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
- **A program that never says where its results come from.** Fatal.
  Bringing a program up is a caller declaring it finished, and a
  finished program that has not said what it produces has not said
  what it is for. A station marked as the way out **with nothing
  wired into it** satisfies this completely — the declaration is the
  interface, and what flows through it is a separate matter. See
  [008](008-map-file-format.md).

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
- [007 — The build path](007-datapath-build.md), where a description is turned into calls
- [311d — The map becomes code](../issues/311d-the-map-becomes-code.md), which removed the reader
- [006 — Scheduling](006-datapath-scheduling.md), what the seed pushes into
