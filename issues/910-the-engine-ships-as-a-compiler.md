# 910 — The engine ships as a compiler

**One executable, `serac`, carrying the engine inside it.** Hand it a map
file and the C functions the map names, and it hands back a program.
Nothing else has to be on the machine — no copy of `cera.c`, no header,
no export list, no generator built from nine source files.

```
serac accumulate.map arithmetic.c        ->  ./accumulate
```

## Current behavior

**`serac` exists and does the first form.** `make serac` builds it in
two named stages, and one command over a description and a box source
produces a program that lands beside the description, takes its
arguments from the command line and prints its results. `--unpack`,
`--emit-c`, `--shared` and `--map-boxes` all work, and the three compose:
`--emit-c` plus `--unpack` plus a hand compile reproduces what one
command does, which is checked rather than asserted.

**The engine's runtime path goes through it**, and the three baked-in
paths are gone. What the build defines now is one name. A name with a
slash in it is a path and is used as it stands, which is how this
repository's own build points at the copy in its build tree; a bare name
is looked for beside the program and then on the path, which is what a
program `serac` built gets, and what makes it relocatable.

**What is not done:** the `--results` bound is a number chosen at build
time rather than anything derived, and the open questions below have not
been worked through.

## Intended behavior

### What `serac` is

**The generator, plus the engine's own source carried as text.** It is
built from what `scripts/` already holds — the C parser, the emitter, the
map reader, the map writer — with `src/cera.c`, `src/cera.h` and
`src/098-engine-surface.syms` embedded in it as C string arrays.

Carrying source as a string array is not a new trick here: each box
source is already emitted a second time that way so the binary holds the
C it was made from. The same mechanism, pointed at the engine instead.

**So `serac` never needs the engine to exist as a file.** It builds the
whole program as one piece of text in its own memory — the header, then
the engine body, then the construction code it just emitted, then the
`main` — and hands that text to the compiler down a pipe. This is what
removes the baked-in paths: there is no include directory to name,
because nothing is included.

**An include is a filesystem lookup, and concatenation dissolves it.**
`cera.c` carries one `#include "cera.h"`, and the emitted file carries
another. That line is what would have forced a directory to exist
somewhere with a file of that name in it. Placing the header's text
ahead of the body's and dropping the line means the declarations are
already in scope when the definitions arrive, which is the same thing
the include was for and the same move the engine already made when
eleven files became `cera.c`. The compiler is handed a program on
standard input and never opens a file that `serac` did not put on the
command line.

**The export list becomes a flag.** The other file the build needed was
`098-engine-surface.syms`, handed to the linker as `--dynamic-list`.
Asking the linker directly to export the one family named in it does the
same job with nothing on disk, and produces the identical set of exported
symbols. The file stays in the repository, because what it actually holds
is the reasoning — what the sweeping form costs, what was measured — and
a flag with thirty paragraphs of comment around it is that file with
extra steps.

What this leaves is a compiler that reads box sources and a map the
caller named, and writes one executable. Nothing else is touched.

### What it does

```
serac program.map boxes.c [more.c ...]      -> an executable
serac --shared program.map boxes.c          -> a description, compiled
serac --shared boxes.c                      -> boxes alone, compiled
serac --emit-c program.map boxes.c          -> the C file, and stop
serac --unpack DIR                          -> cera.c, cera.h, the syms file
serac --map-boxes program.map               -> which boxes it names
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
4. Assemble one piece of text: the header, the engine body, the emitted
   construction code, the `main`. The one include of the header is
   dropped from each half that carries it, and `serac` refuses rather
   than proceeding if that line is not where it expects — a silently
   missed one is a duplicate declaration hundreds of lines into a file
   nobody can open.
5. Invoke the C compiler on that text through a pipe, with the export
   flag and the section collector supplied, because `serac` is the thing
   that knows about them.

`--shared` is the same steps with `-fPIC -shared` and no `main`, and with
the engine body left out — what is loaded binds to the engine already in
the host process rather than carrying a second copy of it. The header
still goes in front, because the emitted code calls what it declares.

**It takes two forms, and the difference is whether there is a
description.** With one, the boxes are already in the process that will
load the result, so what is emitted declares the functions that build
their stations and defines nothing. Without one, it is boxes arriving
before anything names them, so those functions are defined here because
nothing else holds them yet. Both are what a running program asks for,
in that order.

`--emit-c` stops before the engine goes on the front and writes
everything else — the construction code and the `main` — as an ordinary
C file that includes `cera.h`. It is exactly what would have been
compiled minus the engine, which is the property worth having: `--emit-c`
plus `--unpack` plus a compiler reproduces what one command does, and
that is the only way a person can check the claim that it does anything
ordinary. Somebody who wants their own `main` in it passes `--main=`,
which replaces the generated one in the same place.

`--unpack` is how a person who has only `serac` gets the two files, for
writing a C program against the engine by hand.

`--map-boxes` prints the box names a description references, one per
line, unresolved. A running program handed a description has to know what
it asks for before it can compile it.

### Where the program lands

**Beside the map file, named after it.** `serac accumulate.map
arithmetic.c` writes `accumulate` into the directory holding
`accumulate.map`, not into whatever directory the caller happened to be
standing in. A description and the program built from it belong together
for the same reason `serac` itself is looked for beside the description:
that directory is the self-contained thing somebody hands to somebody
else, and a build that scatters its output according to where the shell
was does not produce one.

`-o PATH` says otherwise, and is the only thing that does.

**And never over something it was asked to read.** The default lands
`--emit-c` over `program.map` on `program.c`, and a box source of that
name beside it is an entirely ordinary thing to have. Overwriting it
destroys the source and then fails to compile it, and the message is
about a brace on a line nobody wrote. Refused, naming the file.

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
include directory collapse into the path to `serac`, because `serac`
knows its own compiler and carries its own header. The engine's runtime
compile path invokes `serac --shared` instead of a generator and a
compiler in sequence.

**And a shipped binary becomes relocatable.** Finding `serac` is then the
only question, and it is answered in two places, in order:

1. **Beside the map file**, which is where a programmer who expects to
   add boxes puts it. A description and the tool that can extend it
   travel together, and a directory holding both is a self-contained
   thing somebody can hand to somebody else.
2. **On the path**, for a machine where `serac` is installed once and
   every program finds the same one.

Failing both, the refusal says that extending this program needs `serac`
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

**Ship a prebuilt `serac` binary.** Rejected on the same grounds the
generator was rewritten from Lua into C: a consumer should need a C
compiler and nothing else. A binary is a platform, a libc version and a
trust decision. `serac` is built from source like everything else.

**Have the program carry the engine's source itself**, so a late box
needs no `serac` on the machine. Every binary then carries roughly
another megabyte of text it will almost certainly never use, to serve the
case where a description arrives naming code that does not exist yet.
Rejected for the cost falling on every program rather than on the one
that needs it. What answers the same worry more cheaply is the search
order above: a programmer who expects to add boxes puts `serac` beside
the map file, and the directory holding both is the self-contained thing
they hand to somebody else.

**Make `serac` a shell script around the existing pieces.** It would work
today and need nothing embedded. Rejected because it reintroduces the
dependency the C rewrite removed, in a different language, and because a
script cannot carry the engine's source — so the paths stay baked in and
the defect stays.

## Suggested implementation steps

1. **Embed the engine in the generator's build.** The tool that turns a
   file into a C string array is already part of the generator, so the
   build becomes two stages: compile the generator, use it to write the
   engine and the export list out as arrays, compile `serac` from the
   generator's sources plus those arrays. Name the staging plainly in the
   Makefile, because a two-stage build that is not obvious is a build
   somebody breaks.
2. **Add `--unpack`**, which is the smallest useful thing the embedding
   buys and proves it round-trips. A test compares what comes out against
   the files in `src/` byte for byte, the way the box-source embedding is
   already checked.
3. **Add the concatenation and the compile invocation**, so
   `serac map.map boxes.c` produces an executable with a hand-written
   `main` supplied on the command line. This is the whole pipeline
   working before the generated `main` exists. Proven in advance on the
   files as they stand: the header, the engine, an emitted file and the
   worked example concatenated and piped to the compiler produce a
   program that runs, exporting the same 142 symbols and, stripped,
   coming to the same size as the one the Makefile builds.
4. **Emit the `main`.** Arguments from the command line through the
   existing text-to-value reader, results into registered arrays and out
   through the existing value-to-text writers, worker count from the core
   count.
5. **`--shared` and `--emit-c`**, which are the same pipeline stopping at
   different points.
6. **Point the engine's runtime compile path at `serac`** and delete the
   two baked-in paths that named the build machine. The one that remains
   is how to find `serac`.
7. **Tests.** A map and a box source produce a program that runs and
   prints the right answer, with nothing else in the directory. The
   unpacked engine matches `src/` byte for byte. A binary built on one
   path and moved to another still compiles a late box. A wrong argument
   count is refused saying how many were wanted. A map with no results
   prints nothing and exits zero. The out-of-tree test gains a second
   half that does the same work through `serac` in one command.
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

**Answered: `serac` is found beside the map file first, then on the
path.** A programmer who expects to add boxes puts it next to the
description, so the two travel together. A machine that has it installed
once answers for every program. A binary that will never be handed a new
description carries no engine source and needs neither.

**Answered: the name is `serac`** — the ceramic compiler.

**Dissolved: there is no scratch directory.** The question was where to
put the engine when it had to be written out, and how long to keep it.
It never has to be written out. The compiler will read a program from a
pipe, and the only reason a file seemed necessary was the one include of
the header, which concatenation removes. The export list was the other
file and a flag replaces it. So there is nothing to place in either RAM
tier, nothing to keep between runs, and no stale copy to guard against —
which is a better answer than either of the two that were on offer,
because it removes the failure rather than choosing which way to survive
it.

The engine's own late-box path still writes a shared object and opens
it, and that still lands in the executable RAM tier, for the reason it
always did: `/dev/shm` is commonly mounted so that nothing on it may be
executed. That directory belongs to the engine at run time and is
untouched by this.

**Still open: what does `serac` do when the compiler it invokes is not
there?** It is baked in at `serac`'s own build, so it names a real
compiler on the machine that built `serac` and possibly nothing on the
machine running it. The failure today is an exec that fails and a
sentence naming the compiler, which is honest but arrives after the work
of parsing and emitting has been done for nothing. The engine answers
the same question about `serac` before it starts; these should agree.

**Still open: is `--results` the right shape?** A result's values land in
an array the program holds, and the engine never grows it, because that
is what lets a worker write into it without a lock. So a bound has to
exist, and today it is a number chosen when the program is built —
default a thousand and twenty-four, `--results=N` to change it. Going
past it is a refusal naming how many there were, never a truncation, so
nothing is lost silently. But a person who does not know how many values
their program will produce has to guess, rebuild, and guess again. The
alternatives worth weighing: a count read from the environment when the
program starts, which moves the guess to the person running it rather
than the person building it; or collecting into memory the generated
`main` grows itself, which means the `main` stops being a thing anybody
could have written by hand.

**Still open: is `--main=` worth keeping?** It was a scaffold — a way to
build the whole pipeline and test it before the generated `main` existed
— and it survived because it composes with `--emit-c` to mean "my main,
joined to the construction code". That is genuinely useful. But it is
also a third answer to a question `--emit-c` and `--unpack` already
answer between them, and three routes to one place is how a tool starts
being hard to describe.

**Still open: what root should a program built by `serac` use?** A box's
generated symbol carries its path shortened against a root, and `serac`
uses the description's own directory unless told. A box source outside
that directory keeps its absolute path, which puts the building machine's
filesystem back into the binary — the exact thing this issue removes
elsewhere. Refusing such a source would be consistent and might be too
strict; nothing yet decides.

**Still open: the format has two writers and this added a third reader.**
Phase 6 recorded that the generator's map writer and the engine's own
dump each carry their own copy of the station-line format, tied together
by nothing. `serac` now reads descriptions in a third place — its own
`--map-boxes` — and the `.map` extension is a fourth thing that has to
agree about what a description is. None of it is derived from anything
else.
