# 601a — A wire is written at both ends

## Current behavior

**Built.** Every wire is written twice in a map file, and the loader
refuses when the two declarations disagree.

Four mistakes are told apart rather than lumped together, because they
are four different things to have done: an arrow with no receiving end,
a receiving end with no arrow, two ends naming different ports, and a
source naming a station the map does not declare. The whole file is
checked before anything is refused, so somebody fixing a map sees every
mismatch at once.

**`-` on an input line still means no source when nothing follows it.**
The dash was always the arrow and the keyword always said which way it
pointed, so the two readings are one form with and without its far end.

**Nothing changed at run time.** The second declaration is checked while
loading and then dropped; a wire still exists once, as a destination
record on the producing station's output port. An input port still has
no field naming its source, because delivery only ever asks *where does
this value go*.

**The dump writes both ends**, derived on the spot by asking every
station what its output ports point at — the same sweep removal does,
for the same reason. Derived rather than remembered means those lines
cannot disagree with the wires they describe. Without this a dump would
produce a file its own reader refuses.

**A migration tool came with it**, because five maps and eight test
files were suddenly missing half their wires, and hand-editing
forty-three of them is how a mistake gets in. It reads the arrows a map
already has and writes the receiving ends back into the file, leaving
every existing line untouched — including a mode for map text embedded
in C string literals, which is how every test in this project writes a
map.

## Intended behavior

**Every wire appears twice in the file — once on each end — and the
loader refuses when the two disagree.**

```
station feed keep p entry
  out 0 - total.0

station total add p
  in 0 - feed.0
  out 0 - seen.0

station seen keep p result
  in 0 - total.0
```

Reading one station now tells you the whole truth about that station,
with no scanning.

**The direction is carried by the keyword, not by the arrow.** `-` is
still a wire; `out 0 - total.0` points away and `in 0 - feed.0` points
toward. `in 3 -` keeps its current meaning — an arrow from nothing —
because the form is the same and only the destination is absent.

**Fan-in repeats the input line**, the way fan-out already repeats the
output line. Several wires into one port are several `in` lines with the
same port number.

**A wire declared on one end only is refused**, naming both stations and
both line numbers, because either the author added a wire and forgot its
other half or removed one and forgot the same. Guessing which they meant
would be picking a winner between two statements that were supposed to
agree.

**Constants gain no counterpart.** `in 1 = 5` has no producing station,
so there is nothing to write on the other side. A port is fed by exactly
one of: a constant, one or more wires, or nothing.

**Nothing changes at run time.** The second declaration is checked at
load and then discarded; the destination set on the output port remains
the only place a wire exists once the program is running. A person
reading a file wants both ends written down; a worker delivering a value
only ever asks one direction, and giving it a second structure to
maintain would be two things that can disagree in the one place that
cannot afford it.

**The dump writes both ends**, so a dumped map reloads and a round trip
is unchanged.

## Suggested implementation steps

1. Parse `in N - station.port` as a source declaration alongside the
   existing constant and no-source forms, keeping the station name and
   port for the checking pass rather than acting on it.
2. Build both sets during loading: the wires the output lines declare and
   the wires the input lines declare. Compare them.
3. Refuse a wire present in one set and not the other, naming both
   stations, both ports, and both line numbers. Collect these with the
   other load-time faults so an author fixing a file sees all of them at
   once, as the loader already does.
4. Teach the dump to write the input side, derived from the destination
   sets it already walks.
5. Update every map in `maps/` and every map written as text inside a
   test, since all of them are now missing half their wires.
6. Tests: a map with both ends loads; a map missing an input line is
   refused naming both places; a map whose two declarations name
   different ports is refused; a dumped map reloads.

## Related

- [601 — Map file parser](completed/601-map-file-parser.md)
- [603 — Loader second pass](completed/603-loader-second-pass.md), where
  wires are drawn and where the comparison belongs
- [604 — Load-time validation](completed/604-load-time-validation.md),
  whose fault-collecting this joins
- [703 — Map dump](completed/703-map-dump.md), which must write both ends
- [008 — Map file format](../docs/008-map-file-format.md)
