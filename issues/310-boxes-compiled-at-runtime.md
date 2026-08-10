# 310 — Boxes compiled while the program runs

## Current behavior

Every box a program can ever place is decided before it starts.

The generator reads whatever sits under the box source directory,
parses the function declarations, and emits one file: a shim per box,
a record per box holding its name and its parameter and return types
and sizes, a field table per struct, and a comparison per orderable
type. That file is compiled in. The registry is a fixed array, and
looking a box up by name is a walk over it.

So placing a station by the text `"add"` reaches a compiled C function
— which is the whole trick that lets a text file describe a program.
And the set of names that trick works for is frozen at build time.

Runtime construction ([212](212-one-way-to-build-a-program.md)) makes
that limit visible. A program can now grow stations, ports, and wires
while it runs, and every one of those stations must place a box the
program was compiled with. You can rearrange the furniture but not
bring in new furniture.

## Intended behavior

**A box's C source can be handed to a running program, and a station
can place it.**

The path is not exotic and each step already exists somewhere:

1. Take the C source for one or more boxes.
2. Run the generator over it, producing the shim, the box record, any
   field tables, and any comparisons — exactly what the build does.
3. Compile that to a shared library, by invoking a compiler.
4. Load it and look up the shim.
5. Add the row to the registry.

**A function signature is not enough on its own, and it is worth being
precise about why.** A signature gives you names: the box's name, its
parameter type names, its return type name. What it cannot give you is
**sizes and offsets** — how many bytes a cell holds, how far into a
struct a field sits, how large a task must be. Those are what the
engine actually runs on, and the standing rule is that the compiler
computes every one of them and the generator never guesses. So the
signature supplies the paperwork and a compiler supplies the numbers.
There is no version of this that skips the compiler.

**The registry becomes a growable table.** It is indexed by name when a
station is placed and by row afterward, so it takes the same paging
shape as everything else that grows: add a block, never move what is
already there, and readers resolving a row are never disturbed. That is
the fourth or fifth use of the same pattern in this engine, and by now
it should be a shared piece rather than a fifth hand-rolled one.

**A box arriving late needs to report the width of each input and of
its output, and that is the whole of what the type system asks of it.**
Under [309](309-types-by-width.md) a wire is legal when the two sides
count the same number of bytes, and those numbers come from the same
place they always came from: a `sizeof` the compiler computed while
compiling this box. So a runtime box supplies exactly what a build-time
box supplies, by exactly the same means.

**It introduces no new kind of error**, which is worth stating because
an earlier draft of this issue said the opposite. Two structs of the
same width and different layouts already wire without complaint at
build time — that is the accepted cost of comparing widths. A box
compiled later increases the *opportunity* for that to happen, since
two separately-written sources are likelier to disagree than one, but
the failure is the same failure and it is already on the books. Shape
comparison would have closed it, here and everywhere else equally; it
was declined for reasons that have nothing to do with this issue, and
declining it does not make this one more dangerous than the ground it
stands on.

What a runtime box must still supply beyond widths is mechanical: a
name to be found by, a shim to call, and — if its return type is
orderable — a comparison, for a comparator to use.

**The generator has to be reachable at runtime**, which is what
[308](308-generator-in-c.md) delivers by making it a standalone C
program with no dependency on the engine. As a Lua script it is a build
tool; as a C program it is something the engine can call, and it is the
same parser either way — which is the point, because a second parser
would eventually disagree with the first about what a box is.

## Suggested implementation steps

1. The registry as a growable table, paged, with lookup and placement
   reading it safely while it grows.
2. Compile-and-load as a deliberate call: take source, produce a shared
   library in the RAM-backed build tier, load it, resolve the shim.
   Failures name the compiler's own output rather than summarising it.
3. Adding a registry row: name, shim, per-parameter widths, return
   width, task size, and a comparison if there is one. Nothing about
   the new box's types is checked against the program's, because
   [309](309-types-by-width.md) checks widths when a wire is drawn and
   there is nothing earlier worth asking.
4. A test that a box written after the program started is placed,
   wired, and delivers values byte-identically.
5. A test that a box whose parameter is a different **width** from what
   feeds it is refused, naming both types and both widths — the same
   refusal a build-time box gets, arriving by the same path.
6. A test that a box whose struct disagrees in *layout* with one
   already loaded, at the same width, **is accepted and delivers
   scrambled fields.** Written deliberately, because this is the
   accepted cost of comparing widths and a test that pins it is how
   somebody later discovers it was a decision rather than an oversight.
7. The source saved to the RAM-backed directory at the moment the box
   is compiled, named so a reloader can find it from the box's name
   alone. A test that a dump taken after a runtime box is added reloads
   in a fresh process.
8. Unloading, on the retire-sweep-free mechanism from
   [214](214-destinations-without-a-lock.md), with the per-worker
   counter widened to span the whole task rather than the delivery
   walk. A test that a box compiled, never placed, and unloaded frees
   its library while a saturated pool runs.

## Open questions

**Answered:**

- *Where does the compiler come from?* **Assume it is there.** The
  engine calls a C compiler at runtime and does not negotiate about it.
  What answers the packaging concern is an install script that builds
  the dependencies from source into a local place, written once the
  system is finished rather than designed around now — so the
  dependency is real, acknowledged, and handled by installation rather
  than by making the engine tiptoe. Compiling boxes late is not a
  capability worth crippling to spare a deployment one package.

- *Can a loaded box ever be unloaded?* **Yes, by retire, sweep, free —
  the mechanism [214](214-destinations-without-a-lock.md) builds for
  destination arrays**, including the scrapyard's own lock and the rule
  that nothing is acquired while holding it. Unloading a shared library
  out from under a worker running its code is the same lifetime problem
  wearing different clothes.

  **Two things about it are not the same, and both matter.**

  The liveness test is a different window. The destination sweep asks
  whether a worker is inside a *delivery walk*; this has to ask whether
  a worker is inside a *box*, which happens earlier in the same task.
  Rather than carry two counters, one per worker bumped at the start
  and end of the whole task — call, finish hook, and free — is odd
  across both windows and answers both questions. Less precise for
  destinations, since an array becomes freeable slightly later than it
  strictly must; one mechanism instead of two, on a path nothing
  measures.

  And **you can only reach the retire step for a box no station uses.**
  A station's box cannot be changed and a station cannot be removed
  ([211](211-growing-the-station-table.md) keeps growth only, because
  an index is a position and reclaiming one means either a hole every
  walk must skip or a renumbering that invalidates every wire at once).
  So unloading serves the box that was compiled and then not placed, or
  placed nowhere that survived. That is narrower than it sounds useful,
  and it should be built anyway, because the alternative is code that
  accumulates forever in a program whose whole point is being edited
  while it runs.

- *Does a runtime-added box survive a dump and reload?* **Yes, because
  its source is written down when the box is created, not when a dump
  asks for it.** Every box compiled at runtime has its source saved to
  the RAM-backed directory the moment it comes into existence, treated
  exactly like a log: written as it happens, ephemeral, gone at reboot.

  **Writing it at creation rather than at dump time is the whole
  decision**, and it is the same one [106](106-stopping-on-purpose.md)
  makes about opening the report's destination during startup. A dump
  may be taken while the program is dying, and a failure path is the
  worst possible moment to discover that something needs saving. The
  artifact exists before anybody needs it.

  So a dumped program reloads into any process that can see that
  directory, which includes a fresh process on the same machine and
  excludes one after a reboot — the same lifetime a log has, stated
  rather than implied.

## Related

- [308 — The generator, in C](308-generator-in-c.md), which is what
  makes the parser callable at runtime rather than only at build time
- [309 — Types compared by width](309-types-by-width.md), a hard
  prerequisite — by-name comparison makes this corrupt silently
- [303 — The registry](completed/303-registry-emission.md), the table
  this makes growable
- [306 — Build integration](completed/306-build-integration.md), whose
  no-partial-output guarantee applies here too: a failed generation
  must leave nothing loadable
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which made the frozen registry a visible limit
- [801 — The workbench in the browser](801-browser-workbench.md), which
  reads C files the running program never compiled
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where a runtime dependency on a compiler has to be weighed
