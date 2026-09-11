# ceramic core engine

Build a program out of small C functions and a file saying what feeds
what. **Every one of them runs on every core, and there is no way to
opt out.**

```
   in ─┬─ twice ──┐
       │          ├─ total
       └─ plus ───┘
```

`twice` and `plus` have no path between them, so they run at the same
time. `total` has two inputs, so it runs when both have arrived. Nobody
wrote either of those facts down — they are what the picture *is*.

## Multithreaded whether you meant it or not

There is one rule, and it is the whole scheduler:

> A station runs when, and only when, every one of its input ports
> holds a value.

There is no sequential mode. Every invocation of every station is a
task on a pool of workers, so a program is concurrent from its first
line by construction rather than by anybody choosing it. A chain of
stations is a pipeline; a fan-out is parallelism; neither needed a
keyword, a thread, or a lock you can hold wrongly.

**Nothing you write is threaded.** A box is an ordinary C function that
takes arguments and returns a value and knows nothing about any of
this. The parallelism is not in it and never was — it is in the wiring,
which is a different file.

So making an existing pile of functions concurrent is not rewriting
them. It is placing them and drawing arrows. Code nobody designed for
threads becomes multithreaded by being put in a graph, and the argument
about which lock is held where does not get won — it stops existing,
because nothing is shared.

**And the concurrency is designed rather than coded.** It is expressed
in a markup file you can read, or drawn on a canvas. Somebody who could
not write correct threaded C can still build a program that saturates a
machine, because the part that is hard to get right is not the part
they are writing.

## One command

```
serac accumulate.map arithmetic.c        ->  ./accumulate
```

A description and the C functions it names are a program. It lands beside
the description, takes the description's arguments from its command line,
and prints its results one per line. `serac` carries the engine inside
it, so a machine needs a C compiler and nothing else — no copy of the
engine, no header, no export list, and no `main` anybody had to write.

`make serac` builds it, and `scripts/147-build-serac.sh` builds it from
a bare directory of sources with no Makefile — which is what a release
carries. `serac --unpack DIR` writes the engine back out for anybody who
would rather build against it by hand.

## The scheduling is free. The tasks are not.

**Finding ready work costs nothing, because nothing finds it.** There
is no ready-queue to scan, no work-stealing search, no thread waking to
look around, no polling of any kind. The write that fills a station's
last empty port is the write that starts it: the check is the tail of a
delivery, on the thread that was already standing there, and it looks
at one station's ports and nothing else.

That is as little as a scheduling decision can cost. It happens at the
only instant its answer can have changed, on the thread that changed
it, and it never examines a station that did not just receive a value.

**What it is not is free per task**, and that is the trade rather than
a footnote. Every invocation allocates, copies each input value into
the task, pushes it onto the pool's ring, sometimes wakes a sleeper,
and frees at the end. Calling `add(a, b)` by hand does none of that.
For work measured in nanoseconds this loses to a plain function call
and always will — the overhead buys the scheduling and is paid whether
or not the parallelism was worth having.

So: **you stop deciding what runs where, and you pay a fixed cost per
step to stop deciding.** Worth it when a step does real work, and not
when it does not.

## See it run

```sh
make example
```

About a minute from cloning. It builds the engine and runs the picture
above, then explains a thing it would otherwise have gotten away with
lying about.

```sh
make test      # everything, ~7 seconds
```

Needs **a C compiler and nothing else.** Regenerating the documentation
site also wants LuaJIT, which is project tooling rather than something
on the path you walk to build a program.

## Writing one

**A box is a plain C function.** No registration, no macro, no header
to edit. Drop it in `src/boxes/`.

```c
int add(int a, int b) { return a + b; }
```

It takes its arguments by value, returns one value, and **may not
remember anything between calls** — no statics, no globals, nothing
kept. Memory belongs to the **station**, which holds a value the box's
own output is wired back into, so an accumulator is an arrow you can
see rather than a variable you cannot. That rule is load-bearing; see
below.

**A map says where the boxes go and what feeds what.** This is
[`maps/107-example.map`](maps/107-example.map), the one the example
runs:

```
station in (029-demo-boxes.c:keep)
  in 0 - 0$
  out 0 - twice.0
  out 0 - plus.0

station twice (029-demo-boxes.c:double_it)
  in 0 - in.0
  out 0 - total.0

station plus (029-demo-boxes.c:add)
  in 0 - in.0
  in 1 = 10
  out 0 - total.1

station total (029-demo-boxes.c:add)
  out 0 - 0$
  in 0 - twice.0
  in 1 - plus.0
```

A station line is its kind, a name, and the box it places in brackets —
`station` plain, `comparator`, `iterator`. `out 0 - twice.0` runs a wire
from this station's exit 0 into `twice`'s input 0, and `twice` says the
same thing from its own side, so reading one station tells you everything
it takes and gives. `in 1 = 10` parks a constant on input 1, and `0$`
marks the port as the map's argument zero.

**Then build.** The generator reads the C, asks the compiler for every
size, and turns the map into the construction calls it describes.

**You never tell it a type.** Every size the engine runs on is a
`sizeof` the C compiler evaluated, and the map mentions no types at
all — a file that carried one could disagree with the compiler, and
then one of them would be wrong.

## What follows from the rule

Things that sound unrelated to it and are not:

- **A box you can reason about alone.** Forbidden to remember anything,
  it is a function of its arguments and cannot be in a bad state.
- **Two copies of a box are the same box**, which is what makes
  compiling one *while the program runs* safe.
- **A running program can be written to disk and picked up again**,
  because with no hidden state anywhere, everything a program is lives
  in a graph that can be walked.

And the one that surprises everybody: **a station pairs whatever its
ports hand it.** There is no batch and no round. Two branches that
rejoin can pair one value's result with another's — which is not a
defect but the same independence that let them run at once. Things that
must stay together have to *be* one value. `make example` says this at
more length, and [`docs/058-guarantees.md`](docs/058-guarantees.md)
states it exactly.

## Whether it fits

**Good fit:** work that decomposes into steps that each do enough to be
worth a task. Pipelines, transforms, fan-out-and-rejoin — anything
where you would otherwise be writing thread plumbing by hand, and
anything already written as plain functions that you now want running
on more than one core.

**Bad fit:** steps too small to pay for themselves. A box that adds two
integers costs more in task overhead than it saves in parallelism, and
no amount of wiring fixes that — make the boxes bigger or do it by
hand.

**Also bad fit:** work that does not want to be shaped as "run when
every input is present." If you are fighting that sentence, fighting it
will be worse than not using this.

**Also not:** a language bridge. The larger SoraMech project, on this
repository's `original` branch, bridges between languages. This one
deliberately cannot, and dropping the bridge is what buys the focus on
the runtime underneath.

**And not finished.** Phases 1 through 7 stand. Phase 8, the tools that
live outside the engine, has started — there is a canvas you can draw a
map on, and it cannot yet hand you the file. Phase 9 is the engine
leaving home, and it has: `src/cera.c` and `src/cera.h`, everything else
private, one prefix on everything public, and `make test` builds a
program with this engine in a directory that cannot see this repository
and checks the answer that comes back.

## Where to go from here

| | |
|---|---|
| [`vision`](vision) | why it is shaped this way. Start here if the ideas interest you more than the code. |
| [`docs/`](docs/) | the documentation, in reading order. `make html` builds `docs/HTML/`, a cross-linked site of the same thing — generated, so it is not in the repository. |
| [`docs/058-guarantees.md`](docs/058-guarantees.md) | every promise the runtime makes, numbered, with what each one costs. |
| [`issues/completed/`](issues/completed/) | **the real documentation.** Blueprints, not work logs: what stood before, what should stand after, why the alternatives were refused. The project is meant to be rebuildable by working through them in order. |
| [`src/`](src/) | the engine — two files, `cera.c` and `cera.h`, each with a `.info.md` beside it. Read those first unless you are debugging the source itself. |
| [`scripts/144-serac.c.info.md`](scripts/144-serac.c.info.md) | `serac`, the one command that turns a description and some C functions into a program. `make serac` builds it; it carries the engine inside it, so nothing else has to be on the machine. |
| [`example/`](example/) | the program `make example` runs, commented at length. |
| `workbench/` | a canvas for drawing a map in a browser. Early. |

Filenames carry a number that runs across the whole project rather than
per directory, so the tree reads in one order.

## License

Copyright © 2026 gabrilend. **GNU Affero General Public License v3** —
see [LICENSE](LICENSE). Ask if you need different terms.
