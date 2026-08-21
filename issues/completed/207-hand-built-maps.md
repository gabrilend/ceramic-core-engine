# 207 — Hand-built maps and hand-written shims

## Current behavior

**Built as scaffolding, and the scaffolding is finally coming down —
by being absorbed rather than deleted.**

The construction calls were kept deliberately irritating: placement
wants every element size spelled out by hand, which is precisely the
tedium phase 3 removed for the product path. What was never fixed is
that hand placement is told sizes and **never type names**, so a
hand-placed station cannot bind a static — turning `{ 5, 2.0, ... }`
into bytes needs the field layout. That was read for a long time as a
limitation of hand placement; it is really just an argument nobody ever
passed.

Under [212](212-one-way-to-build-a-program.md) there is one surface
for creating a station, configuring a port, and drawing a wire, used by
a file reader and a control socket alike. Hand placement either takes
the type names the registry already holds and *becomes* that surface,
or it stops existing. Either way there stops being a second class of
station that can bind fewer things than the first.

**The instinct this issue had was right and is worth naming**: build
the crude version first so the phases that replace it have something
working to plug into, and mark it in the header with the issues that
will replace it so nobody mistakes it for a design. Phase 3 replaced
the shims, phase 6 replaced the maps, and phase 2's own capstone is
replacing what is left. Scaffolding that names its own demolition is
the only kind that reliably comes down.

The hand-written shims here survive for a different reason and it is
not scaffolding: they wrap harness instrumentation that no generated
box can reach. Whether the test harness keeps them or finds another way
to reach its counters is a decision about tests, not about the engine.

The remainder describes it as built.

Built as intended scaffolding. The construction calls — create a
table, place a box with hand-supplied element sizes, connect a port
to a destination, start the pool — live in the station layer, marked
in the header as scaffolding with the replacing issues named, and
kept minimal enough to be irritating: placement wants every size
spelled out by hand, which is precisely the tedium phase 3 deletes.
Hand-written shims in the exact shape the generator will emit sit in
the phase 2 tests and demo, each carrying a note naming issue 302;
every cast inside them is an unchecked human promise. The worked
example map is the phase 2 demo's. Phase 3 has since landed: every
product-path shim is generated, and the hand shims here were
superseded rather than deleted — they wrap harness instrumentation
(test counters) that no generated box can reach, so they stay as
marked scaffolding inside tests only. When phase 6 lands the loader
becomes the only caller of the construction calls.

## Intended behavior

Deliberate scaffolding. Phase 3 replaces the hand-written shims with
generated ones, and phase 6 replaces hand-built maps with a text file.
Both replacements go much more smoothly if there is something already
working to plug into, so this issue builds the crude version on purpose
and marks it as temporary in the source.

**A small set of construction calls** that build a map in C: create a
table of N stations, place a box at one, set a slot's element size,
connect a port to a destination. Enough that a test or demo reads as a
description of a graph rather than a pile of assignments.

**Hand-written shims, following the shape the generator will emit.**
A shim reads its inputs out of the task struct, calls the real box
function with real types by value, and copies the return value into the
task's output field. Writing several by hand first is what makes the
generator's job obvious when phase 3 arrives — and it is worth writing
them in exactly the form the generator will produce, so the diff when
they are replaced is a deletion rather than a rewrite.

**Every hand-written shim is a place where nothing is checked.** The
cast inside it is a promise, and phase 3 exists to stop humans from
making that promise. Each one should carry a comment saying so, naming
the issue that will delete it.

## What must not happen

This issue must not grow into a permanent way to build maps. If it
becomes comfortable, phase 6 will feel optional and the project will
quietly become one where the graph is compiled in — which is a
different project. The construction calls should stay minimal enough
to be irritating.

## Suggested implementation steps

1. The construction calls in `src/`, all marked in comments as
   scaffolding with the replacing issue named.
2. Three or four hand-written shims for the boxes the phase 2 tests and
   demo need — one taking two of the same type, one taking two
   different types, one returning a struct, one returning nothing.
3. A worked example map built with the calls, used by the phase 2 demo.
4. When phase 3 lands, the shims here are deleted and this issue is
   updated to say so. When phase 6 lands, the construction calls become
   what the loader calls rather than what a person calls.

## Related

- [002 — Stations and ports](../../docs/002-stations-and-ports.md)
- [007 — The build path](../../docs/007-datapath-build.md), the shape the shims should follow
- Issue 302 — deletes the hand-written shims
- Issue 602 — takes over map construction
