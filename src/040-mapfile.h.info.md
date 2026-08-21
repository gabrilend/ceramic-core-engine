# 040-mapfile.h — the map file surface, from outside

A program is a directory of C functions and a text file. This is the
text file's door.

## Data structures

**map_description** — what the file said, nothing constructed:
stations (name, box name, kind, input overrides, output arrows, all
with line numbers), the statics entries as raw text, the file path.
Public so the parser can be tested without building anything.

## Functions

**mapfile_parse(path) → description** — read a map file. Line
oriented; first word dispatches (`statics`, `in`, `out`, station
line); `#` comments; indentation meaningless. Any malformed line is
fatal naming file, line, and what was expected.

**mapfile_free(description)**

**map_load_file(path, worker count) → map** — the whole journey, and
every structural step of it is a call anybody could make (issues 210g,
212): parse; first pass (a station per station line, named as it is
created, its box placed by name, its door marked, one configuration
call per port line); second pass (each arrow's destination name turned
into an index — the one thing here that is a fact about the file
rather than about the program — then the ordinary wiring operation,
which applies the width check and every other rule); pool started with
workers parked; then the program brought up, which is where the
whole-program checks live and where the first tasks are made. Caller
releases the pool, joins it, destroys the map.

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

**map_seed_count(map) → int** — how many stations the bring-up
started; one when the author expected ten means a wiring mistake.

**map_load_last_timing** — where the most recent load's seconds went,
in four stages: parse, first pass, second pass, bring-up. There were
two more and both stopped being stages rather than getting faster —
*validation* timed a naming sweep that no longer happens, and *seed*
timed a phase only the loader could enter.
