# 217a — One receipt for a box and a map

## Current behavior

**Built.** Placing a box and placing a map both hand back a **part
number**, and the caller cannot tell which it placed.

A number rather than the list itself, because a part travels on a wire
when a map builds a map and a wire carries values. The engine keeps the
receipts in a table that only grows and never moves a row.

**A box's doors are its ports; a map's are its marks.** A part naming
one station whose ports carry no marks is a box, so its argument N is
input port N and its result N is output port N. Those are not two rules
with a fallback between them — they are one rule, *the doors are
wherever the description put them*, and a description of one station
puts them on that station.

Wiring takes two part numbers and two door numbers, and is the same
call for every combination of box and map on either end.

**A placed map's doors are scoped to it**, which is what the receipt
turned out to be for. They say *this port is a useful place to put
values in or take them out of this description* and nothing about the
program that placed it, so placing one description twice does not give
the parent two argument zeros and ignoring a placed result costs
nothing.

The engine tells them apart without being told: loading a description
makes no receipt and placing one does. Bring-up's numbering therefore
counts only stations no part names, and the dump renumbers what it
writes — a file has no parts in it, so two placed copies would spell
two argument zeros and the reader would refuse. The dump already did
exactly this for station names, for exactly this reason.

## Intended behavior

**Placing a box and placing a map both hand back a receipt.** A box's
receipt holds one station; a map's holds however many its file named.
The caller cannot tell which it placed and never needs to.

**Doors are addressed against the receipt by number.** Argument zero,
result zero — resolved through the receipt's station list to a station
and a port. A box's argument zero is its first input port and its result
zero is its output port, by the same rule that resolves a map's, because
a box is a description of one station.

**Wiring takes two receipts and two numbers**, and is the same call for
every combination of box and map on either end. There is no seam: after
placing, both are stations with indices in one table.

**A map with several outputs may stand where a box stands.** A caller
wires the outputs it wants and leaves the rest, and an unwired output
discards, which is what unwired outputs have always done. Nothing counts
them and nothing refuses.

**A receipt survives placement**, because it is a program's identity —
[212a](212a-one-table-and-a-program-is-a-receipt.md) needs it to end a
program by pruning its stations. It stops being freed the moment wiring
finishes.

**What follows without being arranged.** A map holding one box has one
way in and one way out, because that is what its single station has. A
station consumes all its input ports in one claim, so a one-station map
joins its arguments and a four-station map does not — a difference that
falls out of counting stations rather than out of any rule about kinds.

## Suggested implementation steps

1. Make placing a box build a one-station receipt rather than return a
   station index.
2. Replace the entrance-and-result pair with door lookup against a
   receipt, by argument or result number, resolving to a station and a
   port.
3. Make wiring take two receipts and two numbers.
4. Stop freeing the receipt at the end of wiring; give the caller a way
   to release one when it is genuinely finished with the program.
5. Retire the pair struct and its two callers.
6. Tests: a two-argument map composed through the same path a box goes
   through; the same map placed twice, wired differently; a box and a map
   used interchangeably by a caller that does not know which it has.

## Related

- [217 — A program inside another](217-a-program-inside-another.md),
  whose receipt this makes universal
- [212 — One way to build a program](212-one-way-to-build-a-program.md)
- [212a — One table, and a program is a receipt](212a-one-table-and-a-program-is-a-receipt.md)
- [213a](213a-a-door-is-a-port.md) and
  [209a](209a-results-go-where-the-caller-says.md), which give doors the
  numbers this addresses them by
- [135 — A box and a map are one thing](../../docs/implementation-notes/135-a-box-and-a-map-are-one-thing.md)
