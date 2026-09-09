# 217a — One receipt for a box and a map

## Current behavior

**Adding a box and adding a map are one operation with two return
shapes.** Adding a part resolves its argument as either a box compiled
into the binary or a description on disk, refuses when it is both, and
then does the appropriate thing. That much is the intended design and
it works.

**But the two hand back different things.** A box becomes one station,
and its way in and way out are that station. A map is instantiated and
then reduced to a pair:

```c
typedef struct map_part {
    int entrance;
    int result;
} cera_map_part_t;
```

Two integers. **A map with two entrances has the second one looked up
and then discarded**, because there is no field to put it in. So a
two-argument map cannot be composed through this path at all; a caller
has to drop to instantiation and walk the station list itself.

**The receipt already exists and is already right.** Instantiating hands
back every station it created, in the order the description declared
them:

```c
typedef struct map_instance {
    int *station;
    int  count;
} cera_map_instance_t;
```

It is a note to the caller about where things landed, freed once wiring
is done, and nothing in the running program refers to it.

**So the asymmetry is the whole difference between a box and a map.**
Not how many values go in, not how many come out, not what runs when —
what the act of placing something hands back. Because the shapes differ,
everything downstream has to know which it is holding.

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

- [217 — A program inside another](completed/217-a-program-inside-another.md),
  whose receipt this makes universal
- [212 — One way to build a program](completed/212-one-way-to-build-a-program.md)
- [212a — One table, and a program is a receipt](212a-one-table-and-a-program-is-a-receipt.md)
- [213a](213a-a-door-is-a-port.md) and
  [209a](209a-results-go-where-the-caller-says.md), which give doors the
  numbers this addresses them by
- [135 — A box and a map are one thing](../docs/implementation-notes/135-a-box-and-a-map-are-one-thing.md)
