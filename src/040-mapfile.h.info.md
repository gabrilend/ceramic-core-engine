# 040-mapfile.h — a description becoming a program, from outside

A program is a directory of C functions and a text file. This is the
door the text file comes in through — **and it comes in by being
compiled, not by being read** (issue 311d).

The text goes to the generator, which turns it into the construction
calls it describes; the compiler that built this binary compiles them;
the result is loaded and called. So there is exactly one way a
description becomes a program, and everything here is a caller of that
way rather than a second implementation of it.

The parser itself lives with the compiler now
([099-mapparse.h](../scripts/099-mapparse.h.info.md)) and is not part
of any program built with this engine.

**What it costs.** Reading a description while a program runs spends
two compiler invocations — one asking what the description references,
so anything missing can be fetched and compiled first, and one
building it. Roughly a tenth of a second each. A program built from
its own descriptions never reads one and never pays it.

## Functions

**map_load_file(path, worker count) → map** — the whole journey, and
every structural step of it is a call anybody could make (issues 210g,
212): the description is compiled into a function that builds it, and
that function creates each station, names it, places its box, marks
its door, configures each port and then draws every arrow — calling
the same construction surface a person writing C would. Then the pool
is started with its workers parked, and the program is brought up,
which is where the whole-program checks live and where the first tasks
are made. Caller releases the pool, joins it, destroys the map.

Two refusals happen earlier than they used to and say more: a box name
that answers to nothing, and an arrow pointing at a station nobody
declared, are both caught while the description is still text, so the
complaint names the line.

This caller adds exactly one policy of its own: a *file* somebody
asked to be run that starts nothing and declares no entrance is
refused, because it would do nothing at all.

**map_instantiate_file(map, path) → instance** — **a description
brought inside a program that already exists.** Reading a file into a
fresh program is this with the program fixed at "a new empty one",
which is what it always was.

A description is a **template being instantiated**, not a program
being merged: new stations are built for its stations and wired the
way it says, and one description can be instantiated as many times
into one program as anybody likes with nothing shared between the
copies. Nothing that already exists is renumbered.

What the description says and where its stations land are related by a
**table**, not an offset: adding a station hands back a freed place
before it grows the table, so a program that has had removals gets
whatever holes exist, in whatever order. The offset is what the
translation degenerates to when nothing has been removed.

**map_instance_entrance / map_instance_result(map, instance, nth) →
int** — the nth door facing that way, or -1. This is the whole of what
a parent is entitled to know about something it brought inside itself;
everything that is not a door belongs to the description's author to
rename or restructure.

**map_instance_free(instance)** — the handle goes, the stations stay.

**map_add_part(map, name, out part) → NULL or a refusal** — **adding a
box and adding a map are one operation.** A map is a list of boxes and
the wiring between them; a box is a list of one.

A **part** is where values go in and where they come out. For a map
those are the stations it declared as doors; for a single box they are
the same station, because a box's own input ports are its way in and
its own output port is its way out — a box is a map of one station
whose doors are itself.

Which kind a name refers to is resolved rather than guessed: a box
lives in the binary and a description lives on disk, both are looked
for, and finding both or neither is refused naming what was searched.

**map_connect_parts(map, from part, port, to part, port) → NULL or a
refusal** — a wire from one part's way out to another's way in. For
two single boxes this is the ordinary wire. Port numbers are the ones
a wire has always had, so a comparator's three outcomes are reachable
exactly as before.

Nothing takes a part apart. Everything that consumes one takes it
whole, which is what keeps a handle from needing accessors.

**map_seed_count(map) → int** — how many stations the bring-up
started; one when the author expected ten means a wiring mistake.

**map_load_last_timing** — where the most recent load's seconds went,
in four stages: parse, first pass, second pass, bring-up. There were
two more and both stopped being stages rather than getting faster —
*validation* timed a naming sweep that no longer happens, and *seed*
timed a phase only the loader could enter.
