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

**map_seed_count(map) → int** — how many stations the bring-up
started; one when the author expected ten means a wiring mistake.

**map_load_last_timing** — where the most recent load's seconds went,
in four stages: parse, first pass, second pass, bring-up. There were
two more and both stopped being stages rather than getting faster —
*validation* timed a naming sweep that no longer happens, and *seed*
timed a phase only the loader could enter.
