# 703 — Dumping the loaded map

## Current behavior

**Built, and about to need something it has been getting for free.**

The dump prints a static's value by repeating the **original string**
the file gave it, because a statics entry keeps both the text and the
parsed bytes. Once a static's value lives on the port that reads it
([401](401-static-slots.md)) there is no entry and no retained
string — only bytes and a field table describing their shape. So the
dump needs a formatter that walks a field table and produces text, the
exact mirror of the reader that walks one and produces bytes
([408](../408-values-back-into-text.md)).

Three smaller changes: the gather in-lines and the gather-depth comment
go with the pull path
([056](../../docs/implementation-notes/056-no-pull-path.md)); a port
with **no source at all** has to be writable, since a station can exist
before it is wired and the dump's whole value is that it says what is
actually there; and destination order now comes from an array rather
than a list's append order
([214](../214-destinations-without-a-lock.md)), which the round trip
depends on and which that change has to preserve deliberately.

**What this issue proved is the property everything else is checked
against.** A program using every feature loads, dumps, loads its dump,
and dumps again byte-identically. That is the cheapest possible proof
that a program built by any route is a real program — which is exactly
what one construction surface will need, since the test that a file and
a sequence of calls produce the same thing is this round trip wearing a
different hat.

And the finding it forced is still the one that stings: **the design
had proudly discarded station names at load**, and the dump could not
be written until they were kept. A thing that seems like pure overhead
until the moment something needs to describe itself.

The remainder describes it as built.

Built. The dump walks the live station table — never any remembered
file text — and writes the map format back out: statics with their
entries, station lines with kinds, in-lines for statics and gathers,
out-lines per port in wiring order. Derived facts ride as `#`
comments beside the lines that parse: each slot's resolved type and
element size, buffer capacities, station indices, the gather depth —
which required giving the format a comment syntax it never had
(issue 601 records the addition) and required loaded maps to retain
their station names, which the design had proudly discarded at load
(the phase 6 record and the report carry that one). The round trip
is proven at full strength: a map using every feature loads, dumps,
loads its dump, dumps again, and the two dumps compare
byte-identical. One honest gap for the second pass: statics render
their load-time text, and a runtime byte-write is noted in a comment
rather than re-serialized, because bytes cannot be turned back into
words without the text-versus-bytes decision the statics table still
owes. Callable at any moment; the demo asks a rewired map what it
now is.

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
