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
parser and still reads descriptions at run time; the generator emits a
shim for every box it was given rather than only for the ones a map
names; the linker is not yet told to discard what nothing reaches. Each
of those is a step below, and each of them changes what other parts of
the project are allowed to do — so they are separated from the step
that makes the capability exist.

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

**This spends build time to save binary size, deliberately.** You still
compile five hundred files; you ship three.

**Exporting is the thing that has to be narrow, and it was not.** A
symbol in the executable's dynamic table is a collection root by
definition: the point of exporting it is that code which does not exist
yet may look it up by name, so the linker can prove nothing about who
calls it and has to keep it. `-rdynamic` exports *every* global symbol,
so it declared the whole binary reachable and `--gc-sections` collected
nothing. The two flags were in direct opposition, and the sweeping one
won. Naming the families that are genuinely public —
[src/098-engine-surface.syms](../src/098-engine-surface.syms), handed
to the linker as `--dynamic-list` — exports the engine and lets
everything else be thrown away.

**But a table of every box is also a root, and that is the real
holdfast.** The linker discards what nothing points at; a table naming
every placement function points at all of them, and through them at
every shim. So no amount of linker configuration shrinks a program
while the engine still carries a list of everything it could place.

Measured on one test binary, which is worth keeping because it says
which half of the problem is which:

| built with | size | |
|---|---|---|
| `-rdynamic` | 160,352 | everything is a root |
| the surface list, table intact | 146,568 | −8.6%, engine internals collected |
| the surface list, table emptied | 126,392 | −21.2%, boxes collected too |

The table holds 20,176 bytes down on its own — more than narrowing the
export list recovers. **That is why trimming what the generator emits
was the wrong lever.** It would have worked, but only by shrinking the
table as a side effect; deleting the table is what this whole family is
for, and it gets the same bytes without the generator guessing which
boxes a program will want.

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
   ([src/098-engine-surface.syms](../src/098-engine-surface.syms)) and
   every function is in its own section, so the linker collects what
   nothing reaches — 8.6% of one test binary, all of it engine
   internals. **It cannot reach the boxes while the places table
   exists**, because that table points at every placement function and
   a pointed-at function is reachable. The remaining 12.6% arrives when
   step 7 removes the last reader of that table, and the table with it.
   The measurements are in *What the build includes* above.
7. The engine's runtime map parser deleted — **and the parser itself
   kept, as a box.** Nothing in the *engine* parses maps any more, but
   a program that runs other programs needs a box that reads one
   ([212](completed/212-one-way-to-build-a-program.md)), so the parsing functions
   move out of the engine and into a box source. The generator, which
   is becoming a C program in [308](completed/308-generator-in-c.md), links the
   same implementation. One parser, two callers, and neither of them
   the engine.
8. [009](../docs/009-datapath-load.md) rewritten around what replaced
   it.
9. **Done**, and by the stronger comparison: a program built from a
   compiled map and the same map *read as text* dump identically. The
   hand-written form is already proven identical to the read form
   ([212](completed/212-one-way-to-build-a-program.md)), so this
   closes the triangle.

## Open questions

**Outstanding:**

- *Should the export list name families or functions?* It names two
  families today — everything beginning `map_`, which is the
  construction surface, and everything beginning `sora_`, which is how
  a box ends the program it is inside. That is the whole set the box
  sources reach for, checked rather than assumed.

  The narrower alternative is to name the individual functions, which
  is a far shorter list: an emitted placement function calls exactly
  one engine function, since the one that hands back a station pointer
  is `static inline` and gets compiled into the caller. Naming that
  list would make "what code arriving after the build may call" an
  exact statement rather than an approximate one, and it would fail
  loudly the day the emitter reaches for something new.

  **Which is the feature and which is the cost depends on whether the
  surface has stopped moving**, and it has not — [212](completed/212-one-way-to-build-a-program.md)
  is still adding to it. A list that breaks on every addition is worth
  having once the additions stop. The failure mode is mild either way:
  a shared object needing a symbol the host did not export fails at
  `dlopen`, naming the symbol it wanted.

**Answered:**

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
- [311b — Placement instead of records](311b-placement-instead-of-records.md),
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
