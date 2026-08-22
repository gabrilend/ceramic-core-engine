# 311d — The map becomes code

Fourth child of [311](311-the-registry-dissolved.md), and the one that
removes the last name from a running program. The generator reads a map
and emits the construction calls it describes, so a map file is a
**blueprint for the compilation** rather than something a program parses
while it runs.

**This waits for [212](completed/212-one-way-to-build-a-program.md), and the
dependency is deliberately kept hard.** What the generator emits *is*
construction calls, so writing it against the placement and wiring
functions that exist today would mean writing generated code that
calls things scheduled for deletion, and then retargeting the emitter
once they go. That is cheaper than it sounds — the output is generated,
so retargeting is a change to the strings one emitter prints — and it
was still refused, for two reasons. There would be a period in which
the project's canonical example of how a program is built is code
calling an interface the project has decided against, which is a bad
thing for the most-read generated file to be. And a seam that forwards
old names to new ones would be indirection existing solely because two
things landed in an awkward order, which is the kind of layer that
never gets removed. **The other three children do not wait**; this one
sits behind the construction surface, and that is accepted as the cost
of emitting against the final interface only.

## Current behavior

**A map is compiled into the calls it describes, and nothing has been
deleted yet.**

The generator is told which descriptions the build should compile —
discovered from a directory, never listed, the same rule box sources
have always had. For each one it emits a function that builds it:
create a station, name it, call that box's **placement function
directly**, mark a door, set depths and sources, and then draw every
wire once every station exists.

**No box name survives into the running program.** The text `seven`
was resolved on the author's machine and became a call to that box's
placement function. A name that matches nothing, or matches boxes in
two sources with no path given, **fails the build naming the map
line** — none of which was checkable before, because the build had
never seen a map.

**Station names do survive, and that is not an inconsistency.** A box
name was a question the engine had to answer at run time and no longer
is. A station name is data the program carries about itself so it can
be written back out as a file that reads in again — the same status
the box's own name literal already has.

**The generated function works at an offset.** It records where each
station landed rather than assuming they are numbered from zero,
because adding a station hands back a freed place before it grows the
table. Building one compiled description twice into one program gives
two independent copies, which is the same claim instantiating from
text makes ([217](completed/217-a-program-inside-another.md)).

**And the two paths are proven to be one.** A description read as text
and the same description compiled into C are dumped and the text
compared byte for byte. Not equivalent — the same. The generated
function calls the same construction surface a person calling C would,
so reading a map and compiling one stop being two paths that must
agree and become one path with two authors.

### What has not happened

Everything that *removes* something. The engine still carries the map
parser and still reads descriptions at run time, so there are two
implementations of one job; the table of box names is still there
holding every station-builder reachable; and a box arriving late is
opened privately, so nothing arriving after it can bind to it. Each of
those is a step below, and each changes what other parts of the project
are allowed to do — so they are separated from the step that made the
capability exist.

### What stood before

The generator globbed the box source directory and emitted a shim for
every function it found. The build had never seen a map file, so it
could not know which of those a program uses, and it compiled in all
of them.

At run time a loader read the map as text, resolved each box name
against the registry, and called the construction surface. So a
program carried a parser, a table, and every box anyone ever wrote —
and nothing checked that a name in a map corresponded to anything at
all until that loader ran, on somebody else's machine.

## Intended behavior

**A map is compiled into a function that builds it.**

```c
void build_program(map_t *m) {
    map_place(m, 0, place__math_dot_c__add,   PLAIN);
    map_place(m, 1, place__io_dot_c__print,   PLAIN);
    map_wire (m, 0, /*out*/0, /*to*/1, /*port*/0);
    map_set_static(m, 0, /*port*/1, 5);
}
```

**No name survives.** The text `math.c:add` was consumed by the
generator and became a pointer. The station names, the box names, the
port numbers, the constants — all of it resolved while somebody could
still read an error message about it.

**This does not weaken *one way to build a program*.** The generated
function calls the same construction surface a person calling C would,
so loading a map and building one by hand stop being two paths that
must agree and become one path with two authors. The loader stops being
a parser and becomes generated code.

**And the map file loses nothing it had.** It is still text somebody
writes by hand, still the thing a person reads to understand a program,
still what the dump produces. What changes is only what consumes it.

### Editing, which is where the distinction earns itself

Two acts that look similar and are not:

- **Editing the map file** → an incremental rebuild of the parts that
  changed. Splitting the generated material per box source is what
  makes it incremental rather than whole.
- **Editing the running program's structures** — adding a station that
  places a box already present, moving a wire, writing a constant —
  → **no compilation at all.** These are pointers and indices in RAM,
  and no name was ever involved.

So the compiler enters only when new *code* appears, which is the rule
everywhere else in this engine.

### One pipe, and the binary is iteration zero

**A program is compiled iteratively, and the build is only the first
iteration.** The loader for a box arriving mid-run already writes the
source out, runs the generator, runs the same compiler that built the
binary, and opens the result. That is a compiler with a C compiler as
its back end, and nothing about it is specific to a single function.

So a map arriving later goes through the same pipe, and the shape is
the same at every scale:

| what arrives | what happens |
|---|---|
| a station placing a box already present | pointers and indices, no compiler |
| a wire, a constant, a door | the same |
| a box function nobody has compiled | generator, compiler, load |
| a whole map | generator, compiler, load |

**Somebody adding a box does not need to know whether it is a map of
boxes.** They hand over source; a station appears running it. Whether
that source was one function or a description of fifty stations wired
together changes nothing they can observe, which is the same claim
[217](completed/217-a-program-inside-another.md) makes from the other
direction.

**Nothing is compiled twice.** An arrival whose path and bytes match
something already in the process refers to the existing copy. That
needs the existing copy to be *bindable*, which is why the
station-builders stop being private: a shared object can only bind to a
name the thing it was loaded into published.

**Which is where the binary stops being special.** Treating iteration
zero differently — self-contained copies for the boxes the build
compiled in, shared copies for everything after — would be the
complication, dressed as an optimization.

### What the build includes

**The linker decides what actually ships.** Built with
`-ffunction-sections -fdata-sections -Wl,--gc-sections`, every function
lands in its own section and the linker discards every section nothing
reaches — computing exact reachability through includes, through
hand-written `extern` declarations, and through function pointers taken
by name. That is every case a source parser would get wrong, and it
costs nothing but build time.

**Following `#include` directives instead would be a heuristic with a
hole in it**, worth naming so nobody re-proposes it: linking resolves
*symbols*, not includes, so a file may call a function it never
included a header for by declaring it by hand.

**What it does not discard is the boxes, and that is a choice rather
than a limit.** Publishing a station-builder so that code compiled
later can bind to it also makes it a root the collector may not touch.
So a program keeps the boxes it was built with, and shedding the unused
ones is given up — measured at 21,658 bytes of code on one test
binary.

It is given up because **extendable at run time is a requirement of
this system and small was never one.** A program that could only be
extended by boxes it already happened to contain would be extendable in
name only, and the engine is supposed to be incurious about what runs
inside it.

The engine's own internals still shed, which is the 8.6% in the table
above; what stays is the boxes.

**Exporting is the thing that has to be narrow, and it was not.** A
symbol in the executable's dynamic table is a collection root by
definition: the point of exporting it is that code which does not exist
yet may look it up by name, so the linker can prove nothing about who
calls it and has to keep it. `-rdynamic` exports *every* global symbol,
so it declared the whole binary reachable and `--gc-sections` collected
nothing. The two flags were in direct opposition, and the sweeping one
won. Naming the families that are genuinely public —
[src/098-engine-surface.syms](../src/098-engine-surface.syms.info.md), handed
to the linker as `--dynamic-list` — exports the engine and lets
everything else be thrown away.

**But a table of every box is also a root, and that is the real
holdfast.** The linker discards what nothing points at; a table naming
every placement function points at all of them, and through them at
every shim. So no amount of linker configuration shrinks a program
while the engine still carries a list of everything it could place.

Measured on one test binary, which is worth keeping because it says
which half of the problem is which:

| published | code | whole file | |
|---|---|---|---|
| everything (`-rdynamic`) | 126,225 | 160,264 | every symbol is a root |
| the surface, and the station-builders | 117,400 | 146,520 | engine internals collected |
| the surface only | 95,742 | 129,832 | the boxes collected too |

Read the last two rows as the choice rather than as a loss. **21,658
bytes of code is what a program pays to stay extendable** — for
publishing the station-builders, so that a map compiled later can bind
to a box compiled now. A program that published only the construction
surface would be smaller and could be extended only by boxes carrying
their own copy of everything they touch.

The middle row is also what the program measures *today*, because the
table of box names is still holding those same functions reachable. So
publishing them costs nothing yet; what it does is keep them held after
the table goes.

**And this is why trimming what the generator emits was the wrong
lever.** It would have shrunk the table as a side effect, which is a
roundabout way of doing what deleting the table does directly — and
without the generator having to guess which boxes a program will want.

### The build checks every box reference

Naming a function that does not exist, naming a file that does not
exist, or naming a bare basename that matches two files with no path
given — all of it fails at build time, on the author's machine, naming
the map line. None of that was checkable before, because the build had
never seen a map.

### When the toolchain is needed, which now follows rather than being stipulated

- **A shipped program** compiled from its own map → **no compiler at
  run time, ever.** One file, source text included, runs on a machine
  with no toolchain on it.
- **A map the binary was not built for** → compiled when it arrives, by
  a program that runs other programs. That is not a tool and not a mode:
  it is a map whose boxes read a description, compile what it names,
  start a fresh map, and feed it through its input station. See
  [212](completed/212-one-way-to-build-a-program.md).
- **A box arriving mid-run** → compiled when the source arrives, which
  is [310](completed/310-boxes-compiled-at-runtime.md)'s path.

The last two are the same mechanism at different scale, and both are
the compiler being needed exactly when new code genuinely arrives.

## Suggested implementation steps

1. **Half done.** The generator reads map files, by linking the same
   parser the engine has. The parser has *not* moved yet, because the
   engine still reads descriptions at run time — so there is one
   implementation with two callers rather than two implementations,
   which is the property that matters. Moving it is step 7's work.
2. **Done.** Create, name, place, mark a door, set depths and
   sources, then draw every wire — the reader's own order, so that
   the two produce the same program rather than two similar ones. It
   records where each station landed rather than assuming they start
   at zero, so the same function works on an empty program and a
   crowded one.
3. **Done for the dependency**, not for the splitting. Descriptions
   are discovered from a directory and join the generated file's
   prerequisites, so editing one regenerates. Splitting the generated
   material per box source, which is what would make that
   *incremental* rather than whole, is not built.
4. **Done for the parts a station line carries.** Every box name in
   every named map is resolved at build time, and a name matching
   nothing or matching two sources fails the build naming the map
   line. The parameter count is not checked against the line, because
   a station line does not state one — it says which box, and the box
   says how many ports it has.
5. **Dropped, and the reason is the useful part.** This step had the
   generator emit shims only for the functions the declared maps name.
   It would have worked, but it was a guess standing in for a
   measurement: the generator knows what a map *names*, while the
   linker knows what the code *reaches* — through hand-written externs
   and through function pointers taken by name, which is where a
   name-based guess goes wrong. Step 6 does the same job exactly, and
   runs anyway.

   Reordering was considered and does not work: the linker's input
   *is* the shims, so there is nothing to trace through until they
   exist. Running it twice — emit everything, link, ask
   `--print-gc-sections` what went, re-emit the survivors, link again —
   produces a byte-identical binary for a second full compile, because
   the discarding already happened during the first link.
6. **Half done.** The export list is narrow now
   ([src/098-engine-surface.syms](../src/098-engine-surface.syms.info.md)) and
   every function is in its own section, so the linker collects what
   nothing reaches — 8.6% of one test binary, all of it engine
   internals. **It cannot reach the boxes while the places table
   exists**, because that table points at every placement function and
   a pointed-at function is reachable. The remaining 12.6% arrives when
   step 7 removes the last reader of that table, and the table with it.
   The measurements are in *What the build includes* above.
7. **Done. Station-builders stop being private, and late arrivals open
   globally.** A generated station-builder is `static` today, which is
   what makes it invisible from outside and therefore un-bindable by
   anything compiled later. It joins the published list, one symbol per
   box — and that one symbol holds its shim and the box body behind it,
   so the published list stays short while the code stays reachable.
   A box arriving late is opened privately today; opened globally
   instead, the *next* arrival binds to it with no lookup at all.

   Measured before writing this: a second shared object naming a
   function it does not define binds straight to the first one's copy,
   with the host uninvolved. A shared object naming a function that
   lives in the executable binds only if the executable published it,
   and fails at load naming the symbol otherwise.

8. **Done. One question, asked over everything the program is made
   of.** The sources the build compiled in ([311c](completed/311c-source-rides-in-the-binary.md))
   and every source that has arrived since, together, keyed by the path
   the box was addressed as. It is what lets a running program be
   written out as a complete map file, and it is what the identity
   check in step 9 reads.

   **This is not the deleted table coming back.** It holds *text*. It
   answers nothing about what a box is, roots no code, and nothing
   consults it to build a station.

   It needed no new storage either, which is the part worth keeping. A
   loaded object already carries its own source as a C array, for the
   same reason the program's own generated file does, so the arrival
   brings the text with it and the lookup points into the object rather
   than copying anything. What was added is the second half of one
   question, not a second table.

9. **Done. A map compiles at run time, through the pipe that already
   exists, and the engine's own walk is deleted.**

   Built and proven: a description handed to a running program is
   written out, turned into the calls it describes by the generator,
   compiled by the compiler that built the binary, and loaded. The
   generator gained one flag — *the boxes are already in the process
   that will load this* — which changes only the emission, never the
   resolution, so a misspelled box is refused with the same message and
   the same map line as at build time.

   What comes out carries **no box code at all**: measured on the first
   one, zero call wrappers and zero station-builders defined, three
   station-builders bound from the program it was loaded into. The
   description and nothing else.

   The three routes are compared rather than described: a map read as
   text, the same map compiled at build time, and the same map compiled
   while the program ran, all dumped and compared byte for byte.

   **And the walk is gone.** Reading a description into a program is
   now compiling it and calling what comes back, so there is one way a
   description becomes a program and the engine is a caller of it
   rather than a second implementation. Three functions went with it —
   the two passes and the search that translated a description's
   station names into places.

   The linker noticed immediately: `mapfile_parse` is absent from every
   test binary, because nothing in the engine reaches it any more. **No
   program built with this engine carries a map parser**, and that
   became true by deleting callers rather than by moving files.

   Three things changed shape and each is worth knowing:

   - **Two refusals moved into the compiler and gained a line number.**
     A box name that answers to nothing, and an arrow pointing at a
     station nobody declared, are caught while the description is still
     text — so the complaint can say *which line*, which the engine
     never could, because by the time it looked there were no lines
     left.
   - **A refusal from a built description is a malformed input, not a
     bad call.** It said bad-call while the hand-written reader said
     bad-file for the same fault, so one description gave two different
     exit codes depending on which route had read it. There is one
     route now.
   - **Fetching a box the program does not hold is ordinary, not a
     rescue.** A program that grew and wrote itself down names boxes
     that arrived after it started; whoever reads that back has to
     compile them first. The compiler is asked what a description
     references — reading text being its job — and anything missing is
     recovered and compiled before the description is.

   **What it costs, measured rather than estimated.** Each description
   read at run time now spends two compiler invocations, one to ask
   what it references and one to build it. The test that reads ten of
   them takes 1.7 seconds; the whole suite takes 7.4. A program built
   from its own descriptions pays none of this, because it never reads
   one.
   The loader for a box arriving mid-run already writes the source out,
   runs the generator binary, runs the same compiler that built the
   binary, and opens the result. A map goes through the same pipe: the
   generator turns it into a station-building function exactly as it
   does at build time, and the result is opened and called.

   **The engine's own walk from description to program is deleted, not
   moved.** Two implementations of one job existed — resolve each name
   against a table and call the construction functions, or emit those
   same calls and compile them — and only the second survives. That is
   the mechanism this whole family is removing, and relocating it into
   a box would have kept it.

   **An arrival already compiled is not compiled again.** Same path,
   same bytes, according to the table in step 8, and the emitted code
   refers to the existing copy instead of carrying its own. The binary
   is iteration zero and is treated like every iteration after it.

10. **Done. The map text parser moved into the compiler**, beside the
    box source parser.

    It had already stopped being part of any program before the file
    moved: the linker discards what nothing reaches, and nothing in
    the engine reached it once descriptions were compiled rather than
    walked. What the move fixed was the tree claiming otherwise. The
    engine is ten source files now and none of them reads text.

    The build got simpler as a side effect — the parser used to be
    named separately because it lived in the engine's directory and had
    two callers, and beside the generator it is picked up like any
    other generator source.

11. **The table of box names deleted**, its last readers gone: the
    engine's parser (step 9), and the dump's question about whether a
    short name is unambiguous, which is a question about *this
    program's stations* rather than about every box ever compiled and
    should be asked of them.

12. **Done.** [009](../docs/009-datapath-load.md) rewritten around what
    replaced it: four movements, only the last of them the engine's,
    with what it costs and who pays stated rather than implied. Its two
    passes are gone as a described structure — declaration order still
    does not matter, but that is now one requirement expressed at build
    time in the order the calls come out, rather than two passes at
    startup.
13. **Done already, and by the stronger comparison**: a program built
    from a compiled map and the same map *read as text* dump
    identically. The hand-written form is already proven identical to
    the read form
    ([212](completed/212-one-way-to-build-a-program.md)), so this
    closes the triangle. It keeps its proof once reading text means
    compiling it, because what is compared is the program, not the
    route it arrived by.

## Open questions

**Answered:**

- *Should the export list name families or functions?* **Families, and
  there turned out to be a third one nobody had counted.**

  It named two: the construction surface, and how a box ends the
  program it is inside. Those are what the box sources reach for,
  checked rather than assumed. The narrower alternative was to name
  individual functions, since an emitted station-builder calls exactly
  one engine function — the one that hands back a station pointer is
  compiled into its caller rather than called.

  What settles it against the narrow list is that the published set
  stopped being *what generated code calls* and became *what code
  compiled later may bind to*, which is a larger and less predictable
  thing: every station-builder, so that a map arriving next year can
  place a box this build compiled. A list of individual names would
  have to be regenerated per build, which makes it a derived artifact
  rather than a statement somebody wrote.

  The failure mode stays mild either way: a shared object needing an
  unpublished symbol fails at `dlopen`, naming the symbol it wanted.

- *Can the build now check wires, and should it?* **It could, and it
  does not. Wires are a run-time concern, checked when a wire is drawn
  — at startup for the ones a map wrote, and at the moment of the edit
  for the ones a running program draws.**

  The generator sees both ends of every wire in a file and could emit a
  `_Static_assert` comparing the two `sizeof` expressions, failing the
  build with the map line named. Nothing about that is unsound; both
  checks compare the same compiler-computed numbers by different
  routes, so they could not disagree.

  **What decides against it is what a map is for at build time.** The
  build reads a map to learn **which functions to compile in**. That is
  its whole business with the file. Whether the shape those functions
  are wired into is complete, or correct, or finished at all is not a
  build-time question — and it must not become one, because **a map
  with incomplete wiring is a legitimate map.** A port with no source
  yet is written `in 2 -` and is an ordinary state; a station that can
  never become ready is not an error; a program assembled from nothing
  and wired one arrow at a time is the thing
  [212](completed/212-one-way-to-build-a-program.md) exists to allow.

  A build that refused a half-wired map would be refusing exactly the
  program somebody is in the middle of writing. So the generator stays
  incurious about shape: it resolves names, it emits calls, and it
  leaves every judgement about whether the graph makes sense to the
  moment the graph is actually built.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311a — Boxes addressed by file](completed/311a-boxes-addressed-by-file.md),
  whose resolution rules this applies
- [311b — Placement instead of records](completed/311b-placement-instead-of-records.md),
  whose functions this calls
- [311c — Source rides in the binary](completed/311c-source-rides-in-the-binary.md),
  whose embedded text shrinks to what a map actually needs
- [212 — One way to build a program](completed/212-one-way-to-build-a-program.md),
  where composing and starting are separated, and where a program that
  runs other programs turns out to be a map rather than a tool
- [310 — Boxes compiled while the program runs](completed/310-boxes-compiled-at-runtime.md),
  the same path at the scale of one function
- [602 — The loader, first pass](completed/602-loader-first-pass.md)
  and [603 — The loader, second pass](completed/603-loader-second-pass.md),
  which move into the generator
- [007 — The build path](../docs/007-datapath-build.md) and
  [009 — Loading](../docs/009-datapath-load.md), both of which this
  rewrites
