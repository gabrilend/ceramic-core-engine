# 703 — Dumping the loaded map

## Current behavior

A map file goes in and a running program comes out. There is no way to
see what the loader actually built, only what the file said.

## Intended behavior

Write the in-memory station table back out **in the map file format**,
so that loading a map and dumping it produces a file equivalent to the
one that went in.

**Round-tripping is the point.** If the dump and the original ever
disagree, one of them is wrong, and the disagreement is a loader bug
that nothing else would catch. A dump that renders in a private format
proves nothing; a dump that reads back as a map is a continuous
self-check.

**It is also how runtime editing becomes safe.** Once phase 7 allows
wires to change while the program runs, the file on disk stops
describing the program. The dump is then the only accurate description
of what is actually running, and the only way to save a modified map.

**Dump from the table, never from the parsed description.** The
description is what the file said; the table is what exists. Every
value the loader derived — element sizes from the registry, the
comparator's extra slot, resolved indices — should be visible in the
dump, since those are exactly the places where the file and reality can
part company.

**Include what the file cannot say**, as comments: each slot's resolved
type name and element size, each station's index, the gather chain
depth from issue 404. The file format deliberately carries no types,
and this is where a reader gets to see the ones the registry supplied.

## Suggested implementation steps

1. The writer, walking the station table and emitting the format from
   the map file document.
2. Derived facts as comments, clearly separated from the lines that
   would parse.
3. A round-trip test: load a map, dump, load the dump, dump again, and
   assert the two dumps are identical.
4. A round-trip test over every example map used by the phase demos.
5. Make it callable at any moment, not only at startup, so a running
   program can be asked what it currently is.

## Related

- [008 — Map file format](../docs/008-map-file-format.md)
- Issue 704 — the reason this matters most
