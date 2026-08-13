# 008 — Map file format

A map is a text file. It names stations, says which box function each
one places, and draws the arrows between them. It carries no types and
no code — only names, numbers, and connections.

**It is a schematic, not a save file.** The file describes how to build
a structure in memory when the program starts; it is a map to a map. It
does not hold data that persists between runs, and nothing in the engine
writes back to it. A running map can be dumped to a new file on demand —
that is a deliberate act with a name, the program's equivalent of a
save-as — and the result is another schematic, describing whatever shape
the map had grown into by then.

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
  out 0 - config.0

config load p
  in 0 $3
```

## Comments

A `#` starts a comment that runs to the end of the line. Added in
the first build pass: the dump (issue 703) writes derived facts —
resolved types, element sizes, station indices — as comments beside
the lines that parse, and the format as originally written had no
way to carry them.

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

**A port is a ring buffer unless a line says otherwise.** There are two
ways for a line to say otherwise, and both carry the port index:

```
in 1 $0        port 1 holds the value written at statics entry 0
in 1 = 5       port 1 holds 5
in 2 = { 1.5, 2.5, 3.5 }       and a struct is written the same way
```

So `split` above has no input lines at all — every port is an ordinary
ring buffer and there is nothing to say about them.

Port indices match the box function's parameter order, so the loader
can check that no line names a port the function does not have.

**The two forms differ only in where the text comes from.** `$0` fetches
it from the statics section, which is notation for writing a value once
and pointing several ports at it; `=` carries it on the line. Either
way the text is parsed into that port's own storage, at that port's own
type, and nothing is retained afterwards.

The `=` matches the statics section's own `N = value`, so a value is
spelled the same way wherever it is written. It is the form **the dump
writes**, because a dump has values sitting on ports and no entry
numbers to point back at — inventing a numbered section to refer to
would be notation the engine made up rather than something a person
wrote.

The `$` is there because `in 1 0` reading as "static entry zero" is not
something anyone will guess a year from now.

**A second exception is coming: a port with no source at all.** A
station can be created before it is wired, so a port may be
unconfigured — a state, not a value, in which the station simply never
becomes ready. The dump writes those out rather than omitting them,
because the dump's value is that it says what is actually there, and a
half-built program should dump to a faithful record of a half-built
program. So the format needs a form for it. Issue 210b.

**There used to be a second form** — `in 0 reader`, meaning this port
pulls its value from the station named `reader` when it is needed. The
pull path is gone, and with it that line. A station whose value another
station reads now simply has an arrow drawn to it, and if the
destination port is a static, the arrow overwrites it rather than
queueing.
[056](implementation-notes/056-no-pull-path.md) is why.

**A file still using that form is refused, not reinterpreted.** The
reader demands the dollar and says what the bare name used to mean, so
an old map stops with an explanation rather than loading into
something its author did not write. This matters more than it looks:
the two forms differ by one character, and the wrong one would have
loaded and run.

Ring buffers carry no capacity, because they grow on their own.

That is becoming *no capacity is required*. Issue 210b gives every port
a buffer of ten values at instantiation and lets a port be told a
different starting depth — in this file or as an argument to the call
that creates the station — with growth still covering any figure that
turns out wrong. What the rule above was keeping out of the file was a
*required* tuning number that an author had to get right; an optional
hint that costs nothing to omit is a different thing, and this section
will say so once it lands.

## Output lines

```
out 0 - printer.0
```

Port zero of this station delivers to slot zero of the station named
`printer`. Repeat the line to fan out; one port may carry any number of
destinations.

An arrow whose destination port holds a static will, once issue 405
lands, **overwrite that static** rather than queue into a ring buffer —
so a value can become a constant the destination reads on every later
invocation. It does not make the destination run, because a static
never gates readiness. This is how a constant gets computed at startup
instead of written here by hand, and it is deliberately a property of
the arrow rather than of the box, so that it shows up in this file
instead of happening invisibly inside C.

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

**The section is notation and nothing else**: a way to write a value
once while describing the map, and point ports at it by number. It is
resolved as the file is read, and the running program holds no table.

That is worth stating plainly because it used to be otherwise, and the
difference is visible. Each port that names an entry gets **its own
copy** of that value, so two ports written from one entry are
independent from the moment they are written — changing one cannot
change the other, and neither can be shaped by the other's type. The
engine used to keep the table alive for the whole run, shared, behind a
mutex of its own, and two consequences followed that were never
features: two ports of different types could reference one entry and
read the same bytes each their own way, and a box could write to the
table, which was a back channel around "a box cannot remember" and, by
requiring a process-wide pointer to the running map, the reason a
process could hold only one program.

**Sharing, when it is wanted, is drawn.** One station holds the value
and everyone who needs it has an arrow from it. That costs a station
and gains visibility: a constant five stations read appears in the
wiring as five wires rather than as five references to a number that
appears nowhere in the shape of the map.

A dump therefore has no `statics` section. Every constant is written
out beside the port that holds it, from its bytes — including anything
a runtime write changed, which the old form could not say.

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

Two rules used to sit here and no longer can be stated: a box fanning
out to both a gatherer and a ring buffer, and a cycle among gather
wires. With nothing pulled, neither situation is describable. A cycle
in the push direction stays perfectly legal — it is how anything
repeats — and needs a finite companion input to ever stop.

## Related

- [007 — The build path](007-datapath-build.md), which produces the registry this file is read against
- [009 — Loading](009-datapath-load.md), what happens to this file at startup
