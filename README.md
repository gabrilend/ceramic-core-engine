# minimal soramech

An engine for building programs out of small C functions wired
together, where **the wiring decides what runs and when**. There is no
main loop and no scheduler anybody writes. You describe a shape, and
the shape executes itself across every core on the machine.

```
station reader   read_line   p entry
  out 0 - parse.0

station parse    to_number   p
  out 0 - total.0

station total    add         p result
  in 1 = 0
```

That is a program. Three C functions, none of which knows the others
exist, and a file saying what feeds what.

---

## The three nouns

The whole engine is these, and it is worth knowing them before
anything else.

- **A box** is a plain C function. It takes its arguments by value,
  returns one value, and **is not permitted to remember anything
  between calls.** No statics, no globals, no handles kept between
  invocations.
- **A station** is one placement of a box in a map. It owns the buffers
  holding values waiting to be fed to that box, the lock guarding them,
  and the list of places its output goes. The same box can appear at
  several stations, each with its own buffers and wiring.
- **A task** is one invocation — a copy of each input value plus a
  pointer to the code that will run.

Their lifetimes decrease in that order: a box is compiled into the
binary, a station lives as long as the program, a task lives for one
call.

## The one rule

> **A station runs when, and only when, every one of its input ports
> holds a value.**

Nothing polls. Nothing scans for work that is ready. The check is the
tail end of a write: whoever delivered a value into a port looks at
that station's other ports, and if all of them are occupied, takes one
value from each and builds a task.

That is the entire scheduler. Everything else in this repository is
consequences of it.

## What the rule buys

**Concurrency you do not write.** Two stations with no path between
them are independent by construction, so they run at once without
anybody deciding they may. A fan-out is parallelism. A chain is a
pipeline. Neither needed a keyword.

**A box you can reason about alone.** Because it may not remember
anything, a box is a function of its arguments and nothing else. It
cannot be in a bad state, because it has no state. Two copies of one
box are indistinguishable, which is not a curiosity — it is what makes
[compiling one while the program runs](#code-arriving-late) safe.

**A program that can be written down and picked up again.** With no
hidden state anywhere, everything a program *is* lives in the graph,
and the graph can be walked. That is why a running program can be
[captured whole](#putting-a-program-down) and revived with its work
still in flight.

---

## Building it

```sh
make          # build the test binaries and the documentation site
make test     # build, then run everything
./run-demo    # run a phase demo
```

It needs **a C compiler and nothing else**. Regenerating the
documentation site additionally needs LuaJIT, which is project tooling
rather than something a consumer walks past.

For current counts — lines, tests, boxes — run `wc -l libs/* src/*` and
`make test` rather than trusting a number written here, which is a
number that goes stale.

## Writing a program

**Write a C function.** No registration, no macro, no header to edit.

```c
int add(int a, int b) { return a + b; }
```

**Write a map naming it.** The station line says what the station is
called, which box it places, and its kind — `p` plain, `c` comparator,
`i` iterator.

```
station sum add p result
  in 1 = 10
  out 0 - print.0
```

**Build.** The generator reads the C, works out every size the engine
needs by asking the compiler for it, and turns the map into the
construction calls it describes.

There is no step where you tell the engine about a type. The map file
mentions none, deliberately: a map that carried a type would be a
second source of truth able to disagree with the compiler, and one of
them would be wrong.

---

## The parts worth knowing about

### Sizes come from the compiler, always

`sizeof` is a compile-time operator. A running C program has no types
at all — the compiler erases them — so you cannot hand one the text
`vec3` and get 12 back. Every size and offset the engine runs on is
therefore emitted as a `sizeof` or `offsetof` expression that the C
compiler evaluates. Nothing guesses about padding, and nothing can.

This is the constraint the build path is shaped around, and it is why
there is a generator at all.

### A map is compiled, not interpreted

A description becomes the calls it describes, and those calls are made.
There is one way a description becomes a program, at build time and
while running alike: **generator → compiler → load**.

No program built with this engine carries a map parser. Reading text is
the compiler's job.

### Code arriving late

Hand a running program some C and it gains a box: the source is written
out, the generator runs, the compiler that built the binary compiles
it, and the result is loaded. The *same* compiler, on purpose — that is
what gives a program exactly one answer to `sizeof` by construction
rather than by checking.

A description handed to a running program goes through the same pipe.
Somebody adding a box does not need to know whether it is a map of
boxes.

### Putting a program down

A running program can be written to disk with its work still in its
hands — every value waiting in a buffer, every iterator's place in its
exits — and picked up again in a fresh process, going on from where it
was.

Two doors, because there are two situations. The polite one shuts the
entrance, lets everything in flight finish, and writes something
complete by construction. The other writes immediately, whatever is
happening, and **says what it lost** — because a program that cannot
drain is exactly when a capture is worth most. Reading a lossy artifact
is refused unless you ask for salvage by name.

### Seeing inside

Buffer growth is reported rather than silently absorbed. Per-station
counters locate a bottleneck instead of leaving it to be guessed at. A
running map can be dumped back out as a map that reads in again, and
rewired while it runs.

---

## How this repository is organized

| | |
|---|---|
| `vision` | why the project is shaped this way. Read first. |
| `docs/` | the documentation, and a generated HTML mirror of it under `docs/HTML/` |
| `src/` | the engine. Every file has a companion `.info.md` — read that before the source unless you are debugging that specific file. |
| `src/boxes/` | example box functions |
| `libs/` | the thread pool |
| `scripts/` | the generator, the map parser, and project tooling |
| `tests/` | one binary per concern |
| `issues/` | open blueprints; `issues/completed/` is the buildable history |
| `maps/` | descriptions the build compiles in |
| `workbench/` | a canvas for drawing a map in a browser |

**The issue files are the real documentation.** They are blueprints
rather than work logs: each says what stood before, what should stand
after, and why the alternatives were refused. The project is meant to
be reconstructible by working through `issues/completed/` in order.

Filenames carry a number that runs across the whole project rather than
per directory, so the tree reads in one order.

---

## What this is not

**It is not a general-purpose task framework.** The one rule is the
whole scheduler, and if your work does not fit "run when every input is
present" then fighting it will be worse than not using it.

**It is not a language bridge.** The larger SoraMech project — on the
`original` branch of this repository — bridges between languages. This
one deliberately cannot, and dropping the bridge is what buys focus on
the runtime underneath it.

**It is not finished.** Phases 1 through 7 stand. Phase 8, the tools
that live outside the engine, has started.

## License

Source-available, not open source. Use it freely if you are a person, a
hobbyist, a student, a researcher, a non-profit, or a small company. If
your group has a hundred or more people or a million dollars a year of
revenue, [get in touch](LICENSE.md#getting-a-license) first.

Full terms in [LICENSE.md](LICENSE.md).
