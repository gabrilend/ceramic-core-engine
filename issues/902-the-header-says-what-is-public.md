# 902 — The header says what is public

Depends on [901](901-the-engine-becomes-one-file.md), which produced a
header holding everything. This one decides what belongs in it.

## Current behaviour

**`cera.h` is the seven old headers stacked up, and it draws no line.**
A declaration is in it because one engine file needed to call another,
not because anybody outside should. The construction surface a consumer
uses and the joint by which delivery reaches a slot sit in the same
file with the same standing.

Nothing anywhere states which is which. The closest thing the project
has is the linker's export list, which names two families by prefix —
the construction surface, and stopping plus the generated
station-builders. That file answers *what may a shared object bind to*,
which overlaps the question here without being it.

## Intended behaviour

**`cera.h` holds the public surface and nothing else.** Everything a
consumer or generated code may call is declared there; everything else
moves into `cera.c`, above its first use.

### What forces its way in, measured rather than argued

**The generated file is a separate translation unit and always will
be** — it is derived at build time from box sources the engine's author
has never seen, so it cannot be inside `cera.c`. It calls twenty-three
engine functions today, and every one of them is public by necessity:

| what it needs them for | the calls |
|---|---|
| building a station from a compiled map | `map_add_station`, `map_place`, `map_name_station`, `map_configure_port`, `map_wire`, `map_in_port_start_depth`, `map_station` |
| marking doors | `map_designate_input`, `map_designate_output` |
| a map placed inside another map | `map_add_part`, `map_connect_parts` |
| a box ending its own program | `sora_stop_now` |
| handing a built map back | `sora_built_take` |
| charging a box's time to its station | `sora_stats_box_time` |
| a value to and from text | the eleven `sora_text_*` calls |

**And a published symbol is also a run-time requirement.** A box or a
map compiled while the program runs arrives as a shared object and binds
to these by name. That is what the export list already says; this issue
makes the header agree with it, so the two stop being separate claims
about the same boundary.

**What a consumer needs** is the rest: create a map, load one, start it,
wait, destroy it; deliver a value from outside; read a result; edit it
while it runs; watch it; put it down and pick it up again.

### What leaves

The joints. Task construction, the output-port lookup, the claim
helpers for constants, statics teardown, page allocation on a ring, the
slot-state reads and writes, the destination-set builder. These exist so
that one part of the engine can reach another, and after
[901](901-the-engine-becomes-one-file.md) all of them are in the same
file as their callers.

Eight of them are called by tests, which is what
[903](903-everything-else-goes-private.md) has to answer for.

### The completed issues learn to state a signature

This is where the rewriting promised by [901](901-the-engine-becomes-one-file.md)
happens, because the surface is settled here and not before.

Every completed issue that built an engine function gains one short
section — **what it adds to the header** — naming each function it built
and the signature that function has in `cera.h`. Where an issue
currently says which file something went into, that line goes: there is
one file, and saying so seventy times is saying nothing.

The point is not tidiness. This project's standard is that it can be
rebuilt by working through `issues/completed/` in order, and a blueprint
that names a behaviour without naming the call that provides it leaves
the reader to invent a signature. Recording it makes the completed
issues, read in order, into the derivation of `cera.h` — which is the
thing note 057 wanted a script to derive and is better held by the
documents that decided each piece.

An issue that built no callable function — a demo, a removal, a
decision — gains nothing and is left alone.

## Suggested implementation steps

1. **Sort the declarations.** Take every declaration in `cera.h` and put
   it in one of three groups: called by generated code, called by a
   consumer, called only by the engine. The first group is measured, not
   judged — grep the generated file.
2. **Move the third group into `cera.c`**, each above its first use, so
   the file still compiles with no forward declarations added by hand
   where the order already works.
3. **Section the header** so a reader meets the surface in the order
   they would use it: build a map, load one, run it, watch it, edit it,
   put it down; then the pool on its own; then what generated code binds
   to, marked as such.
4. **Check the header against the export list.** Anything the linker
   publishes that the header does not declare is one of the two files
   being wrong, and which one has to be decided rather than patched.
5. **Write the signatures back into the completed issues**, one section
   each, for every issue that built a call that survived into the
   header.

## Open questions

- **Does the export list stay a separate file?** It names families by
  prefix and the header names functions. After [905](905-the-prefix.md)
  gives every public name one prefix, the list could become a single
  pattern — at which point it is arguably a line in the Makefile rather
  than a file. Deciding needs [905](905-the-prefix.md) done first.

## Related

- [901](901-the-engine-becomes-one-file.md), which this narrows
- [903](903-everything-else-goes-private.md), which enforces it
- [057 — Packaging](../docs/implementation-notes/057-packaging.md)
