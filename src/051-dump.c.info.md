# 051-dump.c — writing a running program back out as a file

One function. It walks the live station table and writes a map file
describing what the engine is **actually running**, which is not
necessarily what any file said — a program can be edited while it
runs, and this is how you find out what it became.

| Function | Takes | Gives | Does |
|---|---|---|---|
| `map_dump` | a program, a stream | nothing | Writes the whole program as a map file that reads back in. |

## What makes it worth having

**It reads back.** That is the whole property, and it is what makes
this the cheapest proof available that two ways of building a program
produce the same program: dump both and compare the text. Two
identical dumps are two identical station tables, including every
detail a hand-written comparison would forget to check.

It has already earned that twice. Comparing a file-built program
against a surface-built one found that a deepened buffer and a port
with no source were spelled the same way in the format, so the first
could be written down and not read back.

## What it needs from a program

**Every station must have a name**, because a station line begins with
one. A program built by hand and never named cannot be described on
disk, and this says so plainly rather than inventing a spelling — the
marker it writes instead is deliberately not a legal name, so such a
dump cannot be read back in silence.

The question is asked per station. It used to ask whether the program
had *any* names, which was the same question while reading a file was
the only way to build one — the loader named all of them or none.

## What it writes, and what it deliberately does not

**Derived facts appear as comments**, so they can be read without
being re-read as instructions.

**The format writes exceptions.** A port at the default depth says
nothing about depth; a port that is an ordinary buffer says nothing
about its source. A line for every property of every port would be a
file nobody could scan.

**Values are spoken from their bytes**, not echoed from remembered
text. A constant a runtime write changed dumps as what it *now is*,
which the older table-and-number form could not do — it printed what
the file had said. That hole was admitted in its own comment for a
long time before it closed.

**There is no statics section any more, and its absence is the dump
getting more accurate rather than less.** That section is notation —
a way to write a value once while describing a map and point ports at
it by number. With each value living on the port that reads it, every
constant is written beside its port, from its bytes. Two ports that
shared an entry dump as two ports each holding their own copy, because
that is what they now are.
