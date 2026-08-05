# 009 — Datapath: loading and starting

The binary holds a registry of boxes and no map. The map file holds a
map and no code. This is what happens in the moment between, and it is
the only moment in the program's life when the station table is walked
from end to end.

## Two passes

Declaration order in a map file does not matter — an arrow may point at
a station declared further down. That requires reading the file twice.

**First pass: create every station.** For each station line, look its
box function up in the registry. That gives the shim pointer, the
parameter count, and each parameter's type and size. Allocate the
station's slot array with one ring-buffer slot per parameter — the
default — each sized exactly `sizeof` its parameter, and record the
station's name in a lookup table that is thrown away when loading ends.

A comparator gets one extra slot on the end, typed to match the box's
return value.

Then apply the input lines, converting the named ports from ring
buffers to statics and copying each one's value in.

**Second pass: resolve the arrows.** By now every station exists and
can be found by name. For each output line, look up the destination
station and slot, and append a `{station, slot}` pair to that port's
destination list.

This is also where every wire is type-checked, because it is the first
moment both ends are known. The registry knows the source box's return
type and the destination box's parameter type, both derived from the C
that will actually run, so the check needs nothing from the file.

## Then the checks that need the whole map

Some errors are only visible once every station and arrow exists:

Two checks used to live here and can no longer be stated, because
nothing is pulled: a cycle among gather links, and a port fanning out
to both a gatherer and a ring buffer. See
[056](implementation-notes/056-no-pull-path.md). A cycle in the push
direction remains legal — it is how anything repeats — and needs a
finite companion input to ever stop.
- **Unreachable stations.** Not fatal, but worth reporting: a station
  with ring-buffer inputs that nothing ever writes to will never run.

## The seed

Nothing in the engine searches for work. A station is discovered only
by something writing into it, which means that at the instant the
program starts, nothing can happen — every station is waiting for a
delivery, and there is nobody to deliver.

So the station table is swept exactly once, here, and never again.

**Enqueue every station that has no ring-buffer inputs and whose output
feeds a ring buffer.**

Both halves of that sentence are load-bearing.

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

After the sweep, the pool is running and the map propagates on its own.
The sweep is also the last time anything iterates the station table;
from here on, every station is reached by index, through a wire.

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
