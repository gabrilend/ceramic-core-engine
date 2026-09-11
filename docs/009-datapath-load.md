# 009 — Datapath: loading and starting

The binary holds boxes and no map. The map file holds a map and no code.
This is what happens in the moment between.

**Nothing here reads the map.** A description is *compiled* into the
calls it describes, and those calls are made, so no program built with
this engine carries a parser.

## From a description to a program, before anything runs

**One command.** `cerac` takes a description and the C functions it
names and hands back an executable:

```
cerac accumulate.map arithmetic.c        ->  ./accumulate
```

The program lands beside the description and is named after it. It takes
the description's arguments from its command line and prints its results,
one value per line in port order, so a description that says everything
about a program is a program.

`cerac` carries the engine's own source inside it, which is why nothing
else has to be on the machine. What it hands the C compiler is one piece
of text built in memory — the header, the engine, the construction code,
the `main` — with nothing written to disk except the program itself. See
[the compiler](../scripts/144-cerac.c.info.md).

## From a file to a running program

Four movements, and only the last one is the engine's. This is the other
case: a description arriving at a program that is already running.

**One — the compiler is asked what the description references.** A
program that grew and then wrote itself down names boxes that arrived
after it started, and whoever reads it back may never have heard of them.
Anything missing is found and compiled in first. This is the ordinary
path rather than a rescue, because a description written by a running
program is the normal kind of description.

**Two — the description is compiled.** Its text goes to `cerac`, which
turns each station line into a call that adds a station, names it, builds
it from its box, and configures its ports — including which of them are
the map's arguments and which are its results — and turns each arrow into
a call that draws a wire, and then compiles that.

**Finding `cerac` is the only question a relocated program has.** It is
looked for beside the program, then on the path, and `CERAMIC_COMPILER`
answers outright. A program that is never handed a new description never
asks: it carries no engine source, invokes no compiler, and runs on a
machine with no toolchain at all.

**What is compiled is the description and nothing else.** The boxes it
names are already in the process, so what comes out declares the
functions that build stations from them and binds to the ones the program
published, rather than carrying a second copy of anything. Measured on a
real one: a whole program's structure in seventeen kilobytes, defining no
box code and asking the program for three station-builders.

**Three — the result is loaded and its build function is called.** These
are the same calls a person writing C would make, so a description and a
hand-written program are one path with two authors rather than two paths
that must agree.

**Four — the program is brought up**, which is the same act any caller
performs and is described below.

**Declaration order does not matter.** An arrow may point at a station
declared further down, because the generator emits every station first
and every wire afterwards.

## What this costs, and who pays

Two `cerac` invocations, about a tenth of a second each: one asking what
the description wants, one building it. It used to be four — a generator
and a compiler for each half — with an emitted C file written to the
scratch tier so the compiler had something to read. There is no such
file now.

**A program built from its own descriptions pays none of it**, because
the build already did the compiling and the built function is simply in
the binary. The cost falls on a program being handed a description it was
not built for, which is the only case where there is genuinely new code
to make.

**And it buys a fault caught earlier.** A box name answering to nothing,
and an arrow pointing at a station nobody declared, are both refused
while the description is still text, so the complaint names the line.

## Bringing a program up is a separate act

Some things are only visible once every station and arrow exists. A
caller assembles a program by whatever route — reading a file, calling
the surface, or both — and then says it is finished. That is when the
whole-program checks run and when everything that can start does.

**It is repeatable**, which is the point rather than a convenience: a
station added to a running program is checked and started by the next
call, and one already started is not started twice.

What it checks:

- **An arrow landing on a port that is not a buffer**, which would have
  nowhere to put its value. Fatal, and caught earlier still — as the wire
  is drawn, since wiring applies every rule at any moment.
- **A port with no source.** Reported, not refused: that is the ordinary
  state of a station nobody has finished wiring, and being able to sit in
  it is what lets a program be assembled a piece at a time.
- **Unreachable stations.** A station with buffered inputs that nothing
  ever writes to will never run. Reported — unless one of those ports is
  marked as an argument, which is precisely a port something outside
  delivers into.
- **A gap or a repeat in the numbering.** Fatal. Two ports both claiming
  to be argument one is refused, and so is argument two with no argument
  one — on the result side as well.

A cycle in the push direction is legal — it is how anything repeats — and
needs a finite companion input to ever stop.

## The seed

Nothing in the engine searches for work. A station is discovered only by
something writing into it, which means that at the instant the program
starts, nothing can happen — every station is waiting for a delivery, and
there is nobody to deliver.

So the station table is swept, at that gate, and this is the one place in
the engine that walks it looking for work. Everywhere else a station is
reached by index, through a wire.

**Enqueue every station that has no ring-buffer inputs**, because such a
station has nothing that can ever be written into it and will never be
discovered by delivery. If it is going to run at all, it must be started
here.

**And the sweep is really the writes finishing.** Binding a static from
the file is a write, and a write runs the readiness check on the station
holding it ([004](004-datapath-statics.md)). A station with only statics
is ready the moment its last one is bound, so what looks like a separate
startup step is the ordinary mechanism arriving at the end of
construction. The same thing happens when a station is added to a program
that is already running.

After the sweep, the pool is running and the map propagates on its own —
until somebody adds a station and brings the program up again, which
sweeps once more and starts only what it has not already started.

## Termination, from the map's point of view

The program ends when the pool has nothing left to run and no worker is
mid-task — see [006](006-datapath-scheduling.md). For a map, that means
every value that entered has finished travelling and no station is
waiting on one that will never come.

A map that should run indefinitely keeps itself alive by looping: a value
that comes back around into a ring buffer is a new delivery, and a new
delivery is new work. Such a map is expected to check a stop condition
somewhere in the loop, because nothing else will stop it.

## Related

- [008 — Map file format](008-map-file-format.md), the input
- [007 — The build path](007-datapath-build.md), where a description is
  turned into calls
- [006 — Scheduling](006-datapath-scheduling.md), what the seed pushes
  into
