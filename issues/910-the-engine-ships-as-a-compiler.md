# 910 — The engine ships as a compiler

**One executable, `cerac`, carrying the engine inside it.** Hand it a map
file and the C functions the map names, and it hands back a program.
Nothing else has to be on the machine — no copy of `cera.c`, no header,
no export list, no generator built from nine source files.

```
cerac accumulate.map arithmetic.c        ->  ./accumulate
```

## Current behavior

**Four things travel and a consumer assembles them.** The out-of-tree
test is the honest statement of what it takes today: copy `src/cera.c`,
`src/cera.h` and `src/098-engine-surface.syms`; copy nine files from
`scripts/`; compile the generator from those nine; run it over the box
sources and the map; then compile the emitted file together with the
engine and a hand-written `main`, remembering two linker settings that
are easy to omit and one of which fails silently when it is missing.

That is three compiler invocations and a `main` nobody wanted to write,
to run a description that already says everything about the program
except how many workers to start.

**A consumer must write a `main`.** The one in the out-of-tree test is
the whole shape: create an empty map, find the build function by the
map's filename, start the pool, bring the program up, deliver each
argument, register somewhere for results to land, wait, print. Every
line of it is mechanical from the description — the map says how many
arguments it takes and how wide each is, and the same for results — and
none of it is written down as something the engine will do for you.

**A shipped binary cannot compile a late box anywhere but the machine
that built it.** This is the defect, and it is not obvious from reading
the code. The build bakes in three absolute paths as `-D` definitions:
the compiler, the generator, and the directory holding `cera.h`. Two of
those name places in the *building* machine's filesystem — the generator
lands in a scratch directory under `/tmp` and the header include points
at this repository's `src/`. Copy the resulting binary to another
machine, hand it a description naming a box it does not carry, and the
runtime compile path invokes a generator that is not there and, if it
were, would look for a header at a path that does not exist. The failure
arrives as a compiler error about a missing file, naming a directory the
person reading it has never heard of.

## Intended behavior

### What `cerac` is

**The generator, plus the engine's own source carried as text.** It is
built from what `scripts/` already holds — the C parser, the emitter, the
map reader, the map writer — with `src/cera.c`, `src/cera.h` and
`src/098-engine-surface.syms` embedded in it as C string arrays.

Carrying source as a string array is not a new trick here: each box
source is already emitted a second time that way so the binary holds the
C it was made from. The same mechanism, pointed at the engine instead.

**So `cerac` can write the engine out** to a scratch directory whenever
it needs to compile against it, which is what removes the baked-in paths.
The header is wherever `cerac` just put it, which is somewhere `cerac`
chose, on the machine `cerac` is running on.

### What it does

```
cerac program.map boxes.c [more.c ...]      -> an executable
cerac --shared program.map boxes.c          -> a shared object
cerac --emit-c program.map boxes.c          -> the C file, and stop
cerac --unpack DIR                          -> cera.c, cera.h, the syms file
```

The first form is the one that matters and the steps behind it are:

1. Read the map. Resolve every box address against the sources named on
   the command line, refusing an unknown function or an ambiguous
   basename with the map line that asked for it.
2. Emit the construction C — a shim per box, a placement function per
   box, field tables, compare functions, and a build function turning
   each station line into a call and each arrow into a wire. This is what
   the generator already writes.
3. **Emit a `main`.** New, and described below.
4. Write `cera.c`, `cera.h` and the export list into a scratch directory
   under the executable RAM tier.
5. Invoke the C compiler on the emitted file and `cera.c`, with the
   include path pointing at step 4's directory and both linker settings
   supplied, because `cerac` is the thing that knows about them.

`--shared` is steps 1 through 5 with `-fPIC -shared` and no `main`, which
is what the runtime path needs when a description names a box the running
program does not carry.

`--emit-c` stops after step 3, for somebody who wants to compile it into
a larger program of their own.

`--unpack` is how a person who has only `cerac` gets the two files, for
writing a C program against the engine by hand.

### The generated `main`

**A program made of only a map takes its arguments from the command line
and prints its results.** Everything it needs is already in the
description and the emitted tables:

- **Arguments.** The map's marked input ports that nothing feeds, in the
  order their numbers say. Each command-line word becomes bytes through
  the same reader that turns `= 5` in a map file into a value, so a
  struct argument in brace syntax works with nothing added. A wrong count
  is refused saying how many the program wanted.
- **Results.** Each marked output port gets an array registered for it.
  Every value from every result is printed **one per line, in port
  order**, through the generated writer for that type — the reverse
  direction of the same field walk that reads a constant out of a map.
  No headings and no columns: two results are two stations at two
  unrelated moments, so anything that laid them out side by side would
  draw a relationship the engine refuses to promise.
- **Workers.** One per core unless told otherwise.

A map with no result marks prints nothing, which is a complete program: a
description whose boxes write files or draw pictures has said what it
does.

**It ends the way every program made with this engine ends**, and needs
no rule of its own: when no task remains in the queue and no worker is
mid-task. That is the pool's own termination, decided by the last worker
to fall asleep re-scanning before anyone sleeps, and the generated `main`
simply waits for it.

A map that loops does not terminate, and that is not a case to handle —
it is what a looping map *is*. The three ways a program ends from outside
already cover stopping one, and the generated `main` inherits all three
because they belong to the engine rather than to any caller.

**It is a default, not a ceiling.** Anybody who wants a different `main`
writes one and uses `--emit-c`, or `--unpack` and builds by hand. The
generated one exists so that the smallest useful thing is a map file and
a C file.

### What this does to the engine

**Three baked-in paths become one.** The compiler, the generator and the
include directory collapse into the path to `cerac`, because `cerac`
knows its own compiler and writes its own headers. The engine's runtime
compile path invokes `cerac --shared` instead of a generator and a
compiler in sequence.

**And a shipped binary becomes relocatable.** Finding `cerac` is then the
only question, and it is answered in two places, in order:

1. **Beside the map file**, which is where a programmer who expects to
   add boxes puts it. A description and the tool that can extend it
   travel together, and a directory holding both is a self-contained
   thing somebody can hand to somebody else.
2. **On the path**, for a machine where `cerac` is installed once and
   every program finds the same one.

Failing both, the refusal says that extending this program needs `cerac`
and names the two places it looked — because the alternative is a
compiler error about a file nobody asked for.

**A program that is never handed a new description needs none of this.**
It carries no engine source, invokes no compiler, and runs on a machine
with no toolchain. The search only happens when genuinely new code is
arriving.

### What stays true

**The engine is still two files a person may take and use directly.**
`--unpack` is not a rescue for people who could not manage the four-file
route; it is the route, with the assembly done for them. A consumer who
wants `cera.c` in their own build system gets exactly what they get
today.

**Nothing about the runtime changes.** No station, port, task, or
delivery behaviour is touched by any of this. It is packaging and one
generated file.

### The alternatives, and why not

**Leave the four files and document the assembly better.** The steps are
written down and the out-of-tree test runs them. Rejected because the
documentation cannot fix the baked-in paths, which are a defect rather
than a difficulty, and because "write your own `main`" is a real barrier
to a description that already contains the whole program.

**Ship a prebuilt `cerac` binary.** Rejected on the same grounds the
generator was rewritten from Lua into C: a consumer should need a C
compiler and nothing else. A binary is a platform, a libc version and a
trust decision. `cerac` is built from source like everything else.

**Have the program carry the engine's source itself**, so a late box
needs no `cerac` on the machine. Every binary then carries roughly
another megabyte of text it will almost certainly never use, to serve the
case where a description arrives naming code that does not exist yet.
Rejected for the cost falling on every program rather than on the one
that needs it. What answers the same worry more cheaply is the search
order above: a programmer who expects to add boxes puts `cerac` beside
the map file, and the directory holding both is the self-contained thing
they hand to somebody else.

**Make `cerac` a shell script around the existing pieces.** It would work
today and need nothing embedded. Rejected because it reintroduces the
dependency the C rewrite removed, in a different language, and because a
script cannot carry the engine's source — so the paths stay baked in and
the defect stays.

## Suggested implementation steps

1. **Embed the engine in the generator's build.** The tool that turns a
   file into a C string array is already part of the generator, so the
   build becomes two stages: compile the generator, use it to write the
   engine and the export list out as arrays, compile `cerac` from the
   generator's sources plus those arrays. Name the staging plainly in the
   Makefile, because a two-stage build that is not obvious is a build
   somebody breaks.
2. **Add `--unpack`**, which is the smallest useful thing the embedding
   buys and proves it round-trips. A test compares what comes out against
   the files in `src/` byte for byte, the way the box-source embedding is
   already checked.
3. **Add the scratch-directory write and the compile invocation**, so
   `cerac map.c boxes.c` produces an executable with a hand-written
   `main` supplied on the command line. This is the whole pipeline
   working before the generated `main` exists.
4. **Emit the `main`.** Arguments from the command line through the
   existing text-to-value reader, results into registered arrays and out
   through the existing value-to-text writers, worker count from the core
   count.
5. **`--shared` and `--emit-c`**, which are the same pipeline stopping at
   different points.
6. **Point the engine's runtime compile path at `cerac`** and delete the
   two baked-in paths that named the build machine. The one that remains
   is how to find `cerac`.
7. **Tests.** A map and a box source produce a program that runs and
   prints the right answer, with nothing else in the directory. The
   unpacked engine matches `src/` byte for byte. A binary built on one
   path and moved to another still compiles a late box. A wrong argument
   count is refused saying how many were wanted. A map with no results
   prints nothing and exits zero. The out-of-tree test gains a second
   half that does the same work through `cerac` in one command.
8. **The documents.** [009](../docs/009-datapath-load.md) describes the
   four movements from a description to a running program and would
   describe one command. [057](../docs/implementation-notes/057-packaging.md)
   says the deliverable is a library, a generator and a build rule that
   ties them together; it becomes one program. The overview's opening —
   "two files to add to a project" — gains the other reading.

## Related documents and tools

- [007 — The build path](../docs/007-datapath-build.md), the generator
  and everything it emits
- [009 — Loading](../docs/009-datapath-load.md), the four movements this
  collapses into one command
- [057 — Packaging](../docs/implementation-notes/057-packaging.md), whose
  survey said the deliverable is three things
- [907 — Built outside the tree](completed/907-built-outside-the-tree.md),
  whose test is the current statement of what a consumer assembles, and
  which this rewrites
- [311c — Source rides in the binary](completed/311c-source-rides-in-the-binary.md),
  the embedding mechanism, pointed at the engine instead of at boxes
- [310 — Boxes compiled at runtime](completed/310-boxes-compiled-at-runtime.md),
  the path whose baked-in directories this removes
- [408 — Values back into text](completed/408-values-back-into-text.md),
  the generated writers the emitted `main` prints results through
- The generator's entry point and the map reader beside it under
  `scripts/`; the embedding tool in the same directory; the runtime
  compile path in `src/cera.c`, which is where the three baked-in
  definitions are read

## Open questions

**Answered: every value, one per line, no labels.** Results print in port
order and nothing marks where one ends and the next begins. The reading
that was refused is the one that lays them out as columns: entry three of
one result and entry three of another did not come from the same input,
so a table would draw a relationship the engine promises against. One
value per line is also the form something downstream can read.

**Answered: it ends the way every program made with this engine ends** —
no task in the queue, no worker mid-task. The generated `main` needs no
rule of its own and gets no flag. A map that loops does not terminate,
which is not a case to handle but a description of what a looping map is,
and the three endings that arrive from outside already stop one.

**Answered: `cerac` is found beside the map file first, then on the
path.** A programmer who expects to add boxes puts it next to the
description, so the two travel together. A machine that has it installed
once answers for every program. A binary that will never be handed a new
description carries no engine source and needs neither.

**Answered: the name is `cerac`** — the ceramic compiler.

**Still open: where does the scratch directory go, and is what it holds
kept between runs?** The project has two RAM tiers and the executable one
is the right home for something that gets compiled and then run. Keeping
the written-out engine between invocations saves rewriting close to a
megabyte every time; throwing it away means no stale copy can ever be
compiled against. The second is the project's habit, and the first is the
one that will be noticed on a machine where `cerac` runs often.
