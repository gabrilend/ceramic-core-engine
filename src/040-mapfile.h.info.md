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

**map_load_file(path, worker count) → map** — the whole journey:
parse; first pass (stations from the registry, ring slots by
default, comparator threshold appended, statics bound); second pass
(arrows resolved by name and type-checked — "adder -> printer.0: box
returns int, slot takes float"); whole-map validation (collected,
printed together);
pool started with workers parked; seed swept. Caller releases the
pool, joins it, destroys the map.

**map_seed_count(map) → int** — how many stations the seed enqueued;
one when the author expected ten means a wiring mistake.
