# 008 — Map file format

A map is a text file. It names stations, says which box function each
one places, and draws the arrows between them. It carries no types and
no code — only names, numbers, and connections.

## A complete example

```
statics
  0 = 5
  1 = 100
  2 = "config.txt"
  3 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }

adder add p
  in 1 $0
  out 0 - printer.0
  out 0 - logger.0

depth measure c
  in 1 $1
  out 0 - shallow.0
  out 1 - exact.0
  out 2 - deep.0

split spread i
  out 0 - poet.0
  out 1 - mailer.0

reader read_config p
  in 0 $2

config load p
  in 0 reader
```

## The station line

Three words: the station's name, the box function it places, and its
kind.

```
adder add p
```

`adder` is this placement. `add` is the C function, looked up in the
registry. `p`, `c`, or `i` is plain, comparator, or iterator.

Names rather than numbers, because a map is read by people and because
it makes an error message legible: *"adder → printer.0: box returns
int, slot takes float."* The names cost one lookup table that is
discarded once loading finishes.

The kind is written rather than inferred. A comparator *is* inferable —
it is the station with one more input slot than its function has
parameters — but that means forgetting the threshold line silently
demotes a comparator to a plain box that routes everything one way. One
letter of redundancy buys an error instead of a wrong answer.

## Input lines

**A slot is a ring buffer unless a line says otherwise.** Only the
exceptions are written, and those carry the slot index:

```
in 1 $0        slot 1 is static, entry 0 in the statics table
in 0 reader    slot 0 is gathered from the station named reader
```

So `split` above has no input lines at all — every slot is an ordinary
ring buffer and there is nothing to say about them.

Slot indices match the box function's parameter order, so the loader
can check that no line names a slot the function does not have.

The `$` is technically unnecessary — a static reference is a number and
a gatherer source is a name, so they are already distinguishable. It is
there because `in 1 0` reading as "static entry zero" is not something
anyone will guess a year from now.

Ring buffers carry no capacity, because they grow on their own.

## Output lines

```
out 0 - printer.0
```

Port zero of this station delivers to slot zero of the station named
`printer`. Repeat the line to fan out; one port may carry any number of
destinations.

Port numbers stay explicit because they mean something: a comparator's
ports are less, equal, and greater in that order, and an iterator's are
the sequence it walks.

## The statics table

Entries are numbered and hold a value's shape. Nothing more.

```
statics
  0 = 5
  2 = "config.txt"
  3 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }
```

**The table carries no types.** A slot that references an entry knows
what type it is, because the box function's parameter at that position
says so, and the registry knows what that is. The text is read into
bytes at the moment a slot claims it, walking the field table the
generator emitted for that struct.

This is the same reason the wiring carries no types: if the table said
`int` where the box wanted `float`, there would be two sources of truth
and the file would be the one that was wrong.

A side effect worth knowing: two slots of different types may reference
the same entry and each read it their own way.

The table is in RAM once the program is running and may be altered
while it runs. Doing so requires holding its mutex, because a struct
half-overwritten while a slot is copying it yields fields from two
different worlds. Reads are constant and writes are rare, so the
contention is nothing.

A box may write to the table. This is a back channel around "a box
cannot remember" — a box can stash a value in an entry and read it back
on its next run, and none of it appears in the wiring. It works, and it
should be treated with exactly the suspicion a global variable
deserves.

## What the loader checks

Load-time failures, all of them fatal and all naming the offending
station:

- A box function not in the registry.
- An arrow to a station or slot that does not exist.
- A wire whose source return type and destination parameter type
  differ.
- More input lines than the box has parameters, or a slot index out of
  range.
- A comparator whose box returns a type with no compare function.
- A box whose output fans out to both a gatherer slot and a ring-buffer
  slot — neither pushed nor pulled coherently.
- A cycle among gather wires.

## Related

- [007 — The build path](007-datapath-build.md), which produces the registry this file is read against
- [009 — Loading](009-datapath-load.md), what happens to this file at startup
