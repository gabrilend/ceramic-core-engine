# 096-test-a-map-becomes-code.c — what it proves

A description read as text while a program runs, and the same
description compiled into C at build time, produce the **same
program**. Not equivalent — the same, by dumping both and comparing
the text byte for byte.

Comparing dumps is the cheapest proof available: the dump walks the
live station table and writes what is actually there, so two identical
dumps are two identical tables including every field a hand-written
comparison would forget to check. If the two paths ever diverge — a
default applied on one side, a port left in a different state, arrows
attached in a different order — the text differs and the test names
the first line where they part.

| Scene | What it proves |
|---|---|
| the build compiled what it was told about | A description is found by the path the build knew it as, or by the bare name somebody would type. One the program was not built with is absent. |
| identical dumps | Reading a map and compiling one are one path with two authors, not two paths that must agree. |
| built twice | The generated function records where each station landed rather than assuming they start at zero, so building it twice into one program gives two independent copies — the same claim instantiating from text makes. |

## Why the compiled form matters

**No box name survives.** Every one in the description was resolved on
the author's machine and became a direct call to that box's placement
function, so a misspelled box fails the build rather than somebody
else's startup, and nothing is looked up while the program runs.

**Station names do survive**, and that is not an inconsistency. A box
name was a question the engine had to answer at run time and no longer
is. A station name is data the program carries about itself, so it can
be written back out as a file that reads in again — which is exactly
what this test relies on.

## The description it uses

`maps/095-doubling.map`, which is deliberately not the simplest map
that could work: it has a constant, a port given a starting depth, a
fan-out, a port left with no source, and both doors. Each of those is
something one path could get right and the other wrong.

It lives in `maps/` rather than in a temporary directory because the
build has to see it. The maps a program declares are also a manifest,
and a map the build never saw cannot be compiled in.
