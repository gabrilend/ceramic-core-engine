# 902 — The header says what is public

Depends on [901](901-the-engine-becomes-one-file.md), which produced a
header holding everything. This one decides what belongs in it.

## Current behaviour

**Done.** `cera.h` declares 100 symbols and the engine exports exactly
those 100 — checked by a test rather than by reading, so the property
cannot decay quietly.

Twenty-three joints left the header for the body, each carrying its own
documentation, under one banner that says what they are: how the engine
reaches itself. They are the slot state machine, the pages a ring grows
by, the constant a port holds, the text helpers the dump prints
through, the output port lookup and its destination sets, task
construction, the pool's own callback, the scrapyard entire, and the
box-table matcher.

Two of the twenty-three were found by the compiler rather than by
reading — a late-box placement lookup and a box recovery call, neither
of which had ever been declared in any header and both of which were
exported anyway. That is the argument for deriving the list instead of
writing one.

**Three declarations needed a judgement rather than a move**, because
they shared a documentation block with a public call, and splitting a
block is a decision about prose. Two turned out to be internal after
all — a slot setter that had drifted away from the slot calls it
belongs with, and the whole scrapyard, whose only outside callers were
two tests that now compile inside the engine. The third, putting values
back into a port, is genuinely public and stayed.

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
| building a station from a compiled map | `cera_map_add_station`, `cera_map_place`, `cera_map_name_station`, `cera_map_configure_port`, `cera_map_wire`, `cera_map_in_port_start_depth`, `cera_map_station` |
| marking doors | `cera_map_designate_input`, `cera_map_designate_output` |
| a map placed inside another map | `cera_map_add_part`, `cera_map_connect_parts` |
| a box ending its own program | `cera_stop_now` |
| handing a built map back | `cera_built_take` |
| charging a box's time to its station | `cera_stats_box_time` |
| a value to and from text | the eleven `cera_text_*` calls |

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

### The completed issues learn to state a signature — moved out

This was to be part of this issue: every completed blueprint gaining a
short section naming the calls it produced and their signatures, so that
working through `issues/completed/` in order is enough to rebuild the
project.

**It is [909](909-the-blueprints-name-their-calls.md) instead**, and
the reason is worth keeping. Crediting an issue with the calls it names
returns six issues out of seventy-four, because the house style
describes a function in English rather than by its name — which is the
style working, not failing. The mapping cannot be derived from the text;
it has to be decided one blueprint at a time.

What this issue did produce is the list itself: all 101 calls with their
signatures, generated from the header into `src/cera.info.md`, which
is the thing 909 has to reconcile blueprints against.

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
5. **Generate the full list of calls and signatures** from the header
   into the interface document, so it is derived rather than typed.
   Writing them back into the individual blueprints is
   [909](909-the-blueprints-name-their-calls.md).

## Open questions

- **Does the export list stay a separate file?** It names families by
  prefix and the header names functions. After [905](905-the-prefix.md)
  gives every public name one prefix, the list could become a single
  pattern — at which point it is arguably a line in the Makefile rather
  than a file. Deciding needs [905](905-the-prefix.md) done first.

## Related

- [901](901-the-engine-becomes-one-file.md), which this narrows
- [903](903-everything-else-goes-private.md), which enforces it
- [057 — Packaging](../../docs/implementation-notes/057-packaging.md)
