# ceramic core engine

Build a program out of small C functions and a file saying what feeds
what. The wiring is the scheduler.

```
   in ─┬─ twice ──┐
       │          ├─ total
       └─ plus ───┘
```

`twice` and `plus` have no path between them, so they run at the same
time. `total` has two inputs, so it runs when both have arrived. Nobody
wrote either of those facts down — they are what the picture *is*.

## What's unusual about it

- **No main loop and no scheduler.** Not a small one, not a hidden one.
  A station runs when its inputs are full, and the check that notices
  is the tail end of somebody else's write.
- **Nothing polls or scans.** There is no queue being watched, no list
  of ready work, no thread looking for something to do.
- **You never tell it a type.** Every size it runs on is a `sizeof` the
  C compiler evaluated. The map file mentions no types at all,
  deliberately — a file that carried one could disagree with the
  compiler, and then one of them would be wrong.

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
kept. That rule is load-bearing; see below.

**A map says where the boxes go and what feeds what.** This is
[`maps/107-example.map`](maps/107-example.map), the one the example
runs:

```
station in keep p entry
  out 0 - twice.0
  out 0 - plus.0

station twice double_it p
  out 0 - total.0

station plus add p
  in 1 = 10
  out 0 - total.1

station total add p result
```

A station line is a name, the box it places, and a kind — `p` plain,
`c` comparator, `i` iterator. `out 0 - twice.0` runs a wire from this
station's exit 0 into `twice`'s input 0. `in 1 = 10` parks a constant
on input 1.

**Then build.** The generator reads the C, asks the compiler for every
size, and turns the map into the construction calls it describes. No
step tells the engine anything it could have worked out.

## The one rule

> A station runs when, and only when, every one of its input ports
> holds a value.

That is the whole scheduler. The check happens at the end of a
delivery: whoever wrote a value into a port looks at that station's
other ports, and if all of them are full, takes one from each and
builds a task.

Everything else follows from it, including the things that sound
unrelated:

- **Concurrency nobody writes.** Two stations with no path between them
  are independent by construction.
- **A box you can reason about alone.** Forbidden to remember anything,
  it is a function of its arguments and cannot be in a bad state.
- **Two copies of a box are the same box**, which is what makes
  compiling one *while the program runs* safe.
- **A running program can be written to disk and picked up again**,
  because with no hidden state anywhere, everything a program is lives
  in a graph that can be walked.

And one thing that surprises everybody: **a station pairs whatever its
ports hand it.** There is no batch and no round. Two branches that
rejoin can pair one value's result with another's — which is not a
defect but the same independence that let them run at once. Things that
must stay together have to *be* one value. `make example` says this at
more length, and [`docs/058-guarantees.md`](docs/058-guarantees.md)
states it exactly.

## Whether it fits

**Good fit:** work that decomposes into small pure steps with data
flowing between them. Pipelines, transforms, fan-out-and-rejoin,
anything where you would otherwise be writing thread plumbing by hand.

**Bad fit:** work that does not want to be shaped as "run when every
input is present." If you are fighting that sentence, fighting it will
be worse than not using this.

**Also not:** a language bridge. The larger SoraMech project, on this
repository's `original` branch, bridges between languages. This one
deliberately cannot, and dropping the bridge is what buys the focus on
the runtime underneath.

**And not finished.** Phases 1 through 7 stand; phase 8, the tools that
live outside the engine, has started.

## Where to go from here

| | |
|---|---|
| [`vision`](vision) | why it is shaped this way. Start here if the ideas interest you more than the code. |
| [`docs/`](docs/) | the documentation, in reading order. `docs/HTML/` is a generated site of the same thing. |
| [`docs/058-guarantees.md`](docs/058-guarantees.md) | every promise the runtime makes, numbered, with what each one costs. |
| [`issues/completed/`](issues/completed/) | **the real documentation.** Blueprints, not work logs: what stood before, what should stand after, why the alternatives were refused. The project is meant to be rebuildable by working through them in order. |
| [`src/`](src/) | the engine. Every file has a `.info.md` beside it — read that first unless you are debugging that exact file. |
| `workbench/` | a canvas for drawing a map in a browser. Early. |

Filenames carry a number that runs across the whole project rather than
per directory, so the tree reads in one order.

## License

Copyright © 2026 gabrilend. **GNU Affero General Public License v3** —
see [LICENSE](LICENSE). Ask if you need different terms.
