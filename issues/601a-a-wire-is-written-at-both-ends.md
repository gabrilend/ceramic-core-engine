# 601a — A wire is written at both ends

## Current behavior

**A wire is stated once, by the station producing it.** The map file
says where a value goes and never says where it came from:

```
station feed keep p entry
  out 0 - total.0

station total add p
  out 0 - seen.0
```

Reading `total` tells you nothing about what feeds it. To learn that, a
reader scans every other station in the file looking for an arrow that
names `total`, which in a large map means reading the whole file to
understand one station.

**`-` on an input line means no source at all.** `in 3 -` says port 3
has nowhere for a value to come from — a port on a station still being
built. It is an arrow pointing at nothing, which is why it shares the
character with the arrow on an output line.

**At run time the wire genuinely exists once**, as an entry in the
destination set on the producing station's output port. An input port
has no field naming its source and never needed one: delivery only ever
asks *where does this value go*, and that is the direction the data
structure is built for.

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
