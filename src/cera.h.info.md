# cera.h — the engine's interface

The whole of what a program built with this engine can call, in one
file. With `cera.c` beside it this is the engine; there is nothing else
to install and no include path to configure beyond the directory the two
files sit in.

## What replaced what

Seven numbered headers, in dependency order, each now a section:

| section | what it covers |
|---|---|
| 011 — the pool | worker threads, the task ring, sleeping and waking, termination |
| 018 — stations | the station table, ports, wires, and the construction surface |
| 026 — emitted | what the generator produces, and how a value becomes text and back |
| 040 — mapfile | reading a description, and placing a whole map inside another |
| 049 — observe | the reports, the observer thread, the dump, and rewiring |
| 073 — latebox | boxes and maps compiled while the program runs |
| 091 — stopping | signals, capture, and ending a program |

The numbers are kept because they are the reading order: the pool knows
nothing of stations, stations know nothing of the generator, and each
section stands on the ones above it.

## What is in here, and what is not

**100 symbols, every one of them beginning `cera_`, and the engine
exports exactly those 100.** Not "roughly
these" — checked on every test run by
`tests/112-test-public-surface.sh`, which compiles the engine alone,
asks the object file what it publishes, and fails naming anything that
is published without being declared here.

That is the property this file exists for: **private by default, public
only by a deliberate act**, the act being a line in this file. It
cannot decay, because breaking it means writing a function and
forgetting one word, and something checks.

**What is not here** is how the engine reaches itself — the slot state
machine, the pages a ring grows by, the constant a port holds, the text
the dump prints through, the output port lookup and its destination
sets, task construction, the pool's own callback, the scrapyard, the
box-table matcher. Twenty-three of them, in `cera.c` under a banner
saying so, with the documentation they always had.

## The one thing that is already known about the boundary

**Generated code is a separate translation unit and always will be.**
It is derived at build time from box sources this engine's author has
never seen, so it cannot be inside `cera.c`. Twenty-three functions are
therefore public by necessity rather than by choice — building a station
from a compiled map, marking doors, placing a map inside a map, a box
ending its own program, charging a box's time to its station, and the
eleven calls that turn a value into text and back.

A box or map compiled *while the program runs* binds to those same names
through the executable's dynamic symbol table, which is what
[098-engine-surface.syms](098-engine-surface.syms.info.md) publishes.
That file and this one describe the same boundary from two directions —
what a shared object may bind to, and what a program may call. They
agree today by both naming the same families; [905](../issues/completed/905-the-prefix.md) gave every public name one
prefix, so the linker's list is now a single pattern — and it stayed a
file, because what will not fit in a link command is the explanation
around the pattern rather than the pattern.

## Using it

```c
#include "cera.h"
```

One include path, or none if the two files sit beside your own source.

Two linker settings are required and are not optional:

```
-Wl,--dynamic-list=098-engine-surface.syms -Wl,--gc-sections
```

The first publishes the engine so that a box or a map compiled while
the program runs can bind back into it; the second throws away what
nothing reaches. They only make sense together, and the obvious way to
write the first — `-rdynamic` — cancels the second, because an exported
symbol is a root the collector may never touch and exporting everything
declares the whole binary reachable. See
[098-engine-surface.syms](098-engine-surface.syms.info.md), which
carries the measurements.

## Related

- [cera.c](cera.c.info.md) — the implementation
- [901](../issues/completed/901-the-engine-becomes-one-file.md) — why there is one file
- [902](../issues/completed/902-the-header-says-what-is-public.md) — what is in this file, and why
- [903](../issues/completed/903-everything-else-goes-private.md) — what is not, and what checks
- [057 — Packaging](../docs/implementation-notes/057-packaging.md)
