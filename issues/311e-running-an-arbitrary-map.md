# 311e — Running a map you were not built for

Fifth child of [311](311-the-registry-dissolved.md), and a program
rather than an engine change. A tool that takes any map file and runs
it — by **compiling** it, never by interpreting it.

This is what makes the rest of the family survivable. Deleting the
runtime name table would otherwise cost the ability to open a map you
did not build, and this is where that ability comes back without a
table, a parser, or a switch anywhere in the engine.

## Current behavior

There is no such tool, and there did not need to be: every binary
carries a registry and a map parser, so any program can load any map
naming boxes it happens to contain. That capability is being removed by
[311d](311d-the-map-becomes-code.md), which is what creates the need
for this.

## Intended behavior

**Hand it a map; it compiles the map and runs the result.**

```
soramech-run foo.map
```

1. Run the generator over `foo.map` and the box sources it names —
   exactly what a build does.
2. Compile the result, including `build_program` with its direct
   pointers and no names.
3. Load that object and call the function.
4. Run it.

**No lookup happens anywhere**, because the generator resolved every
name while generating. This is the same generator-compiler-load path
that [310](310-boxes-compiled-at-runtime.md) uses for a single box,
with a whole program's worth of scope instead of one function.

**The runner is not a mode of the engine.** It is a program that
invokes the generator, the way `make` invokes GCC. Nothing in the
engine branches on whether it is being run this way, and no binary
declares anything to opt in or out. There is one shape of program; this
is a tool that produces one.

**It needs a toolchain, and that is what it is** — the way running a
Python script needs Python. Not a flag, not a configuration, just the
nature of a tool whose job is turning a file into a program.

### The workbench feeds it

[801](801-browser-workbench.md) draws a graph and emits a map file. It
has never run anything and was never going to; its own note says the
page *"doesn't run the map files, it simply helps generate them."* So
the workbench and this tool are two halves of one story: one produces a
map, the other turns it into a running program.

### Duplicated boxes, and why they are harmless

If the runner's own binary already contains `math.c` and the map it is
handed also names `math.c:add`, the newly generated object contains
**another copy** — its own shim, its own placement function, its own
compiled code. Reaching into the runner's existing symbols instead
would need every symbol exported, which
[311](311-the-registry-dissolved.md) rejected.

**Two copies of one box are indistinguishable, and the reason is the
rule the whole engine rests on.** A box may not remember anything
between calls, so two compilations of one pure function produce the
same output for the same input and have no state that could diverge.
Duplicated code is not duplicated state. The widths agree because it is
the same compiler on the same machine, and nothing anywhere compares
placement-function pointers for identity — the dump names a station by
the string its placement function wrote.

This is the third time the no-memory rule has paid for something it was
not written for, after making a program capturable and making a box
freely relocatable.

## Suggested implementation steps

1. The tool: a command taking a map path, invoking the generator,
   compiling, loading, and calling `build_program`. Failures name the
   compiler's own output rather than summarising it.
2. Compiled artifacts go to the RAM-backed build tier, treated like
   logs — written as they happen, gone at reboot.
3. A test that a map run this way and the same map compiled into a
   binary produce identical dumps, which is the proof the two paths
   really are one mechanism.
4. A test that a map naming a box no source provides fails during
   generation, naming the map line, rather than at any later point.

## Open questions

- **Does the runner cache compiled artifacts, keyed by the map and the
  sources it names?** Without caching, running the same map twice
  compiles it twice, which is wasteful but never wrong. With caching,
  something has to decide when a cached artifact is stale, and that is
  a second thing that can be wrong. Given that this is a development
  tool rather than a hot path, starting without a cache and adding one
  only if the wait becomes annoying is probably right — but it should
  be a decision rather than an omission.
- **May the runner be handed several maps at once?** The original
  question was whether a *program* could declare several maps and carry
  the union of their boxes, and that dissolved with the table — there
  is no union to carry. What remains is whether this tool runs several
  maps in one process, which the engine already permits, since nothing
  in it is process-wide and several maps cannot see each other. That is
  a question about the tool's command line rather than about the
  engine.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311d — The map becomes code](311d-the-map-becomes-code.md), whose
  emission this performs at run time
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  the same mechanism at the scale of one function
- [801 — The workbench in the browser](801-browser-workbench.md), which
  produces the maps this consumes
- [712 — Capturing a running program](712-capturing-a-running-program.md),
  whose artifact is re-run through this path
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where the difference between a shipped program and a development tool
  is what decides who needs a compiler
