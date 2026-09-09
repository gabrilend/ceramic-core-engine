# 601b — The dollar sign means the boundary

The map file's spelling of [213a](213a-a-door-is-a-port.md) and
[209a](209a-results-go-where-the-caller-says.md), and the deletion of
the notation it displaces.

## Current behavior

**`$N` points at the statics section**, which is a block of numbered
values at the top of a file that ports can refer to:

```
statics
  0 = 5

station adder add p
  in 1 $0        port 1 holds 5
```

`in 1 $0` and `in 1 = 5` produce the identical result — the number five,
parsed into that port's own storage. The section is notation only: the
engine keeps no table behind it, each value ends up on the port that
reads it, and two ports sharing an entry become two ports each holding
their own copy. **The dump never writes a statics section**, so a
hand-written file and a dumped one differ by a spelling that means
nothing.

**And it reads as a shell positional and is not one.** `$0` sitting in a
file whose other keywords are English words looks exactly like an
argument reference, which is what it is not.

**A door is a word on the station line.** `entry` and `result` after the
kind letter, marking the whole station.

## Intended behavior

**`$N` means: this crosses the map's boundary, at position N.** One
meaning, and it is the meaning a reader already guesses:

```
in 0 $0            this port is argument 0
in 0 - feed.0      from another station's port
in 0 = 5           a constant
in 0 -             nothing yet

out 0 - total.1    to another station's port
out 0 $0           this port is result 0
```

The `in` or `out` keyword carries the direction, so one notation covers
both ends and there is no second form to learn.

**Four sources for an input port, and the list is closed**: a wire, a
constant, the boundary, or nothing.

**The statics section is deleted** — the keyword, the numbered-entry
lines, and the `$N` reference to them. It was a second spelling of a
constant, and the constant form is the one the dump writes.

**`entry` and `result` are deleted from the station line.** A station is
a name, a box, a kind, and optionally where an iterator had got to.

**Braces stay.** A struct constant written in braces is the only
spelling of a different thing rather than a second spelling of the same
thing, and removing it would mean a program with a struct on a port
could not be written down at all — which is one of the numbered
guarantees. Large data belongs behind a box that reads it, with the map
naming a path as an ordinary constant.

## Suggested implementation steps

1. Delete the statics section from the parser: the keyword, the section
   state, the numbered-entry handler, the entry list on the description,
   and the `$N` lookup.
2. Parse `$N` on an input line as an argument mark and on an output line
   as a result mark, both carrying the number.
3. Delete `entry` and `result` from the station line, and the
   description field that carried them.
4. Emit port-level designation calls from the map compiler instead of
   station-level ones.
5. Teach the dump to write `$N` on the marked ports, and to stop writing
   the door words on station lines.
6. Update the five tests that write `$N` as a statics reference, and
   every map in `maps/`.
7. Tests: a map with two argument marks and two result marks round-trips
   through a dump; a duplicate argument number is refused; the old
   statics keyword is refused with a message saying what replaced it.

## Related

- [601 — Map file parser](completed/601-map-file-parser.md)
- [607 — No reserved words](completed/607-no-reserved-words.md), which
  loses two of the words a station line could carry
- [401 — Static ports](completed/401-static-ports.md) and
  [402 — Struct constants](completed/402-struct-constants.md) — the
  *port* kind stays exactly as it is; only the file notation goes
- [213a](213a-a-door-is-a-port.md) and
  [209a](209a-results-go-where-the-caller-says.md), which this spells
- [008 — Map file format](../docs/008-map-file-format.md)
