# cera.c — the engine, entire

One translation unit holding the whole runtime: the thread pool, the
station table, the delivery path, constants, the reader, the reports,
the parts that change a running program, the parts that compile new code
into one, and the parts that end one.

## Why one file

**A function in the same translation unit as its callers can be
`static`, and a `static` function is not a linker symbol at all.**

Eleven files meant every joint between them had to be a global name,
because being in a header was the only way one file could reach another.
A host program linking this engine inherited about forty ordinary
English words it never asked for — `map_create`, `map_start`,
`pool_push` — and would fail to link if it had its own notion of a map.

One file makes private the default and public a deliberate act, the act
being a declaration in `cera.h`. That property cannot decay: a function
added next year is private unless somebody writes it into the header.

The cost is that any change recompiles the whole engine, which at this
size is a fraction of a second and is paid by the consumer's build
rather than by this one.

## How it is arranged

Eleven sections in the project's reading order, each formerly a numbered
file, each opening with a banner naming what it was:

| section | what it does |
|---|---|
| 012 | the pool — the task ring, workers, sleeping, termination |
| 019 | the station table and its growth |
| 020 | delivery, the readiness check, and routing |
| 027 | support for generated code |
| 033 | constants, and values to and from text |
| 042 | reading a description |
| 050 | reports and the observer thread |
| 051 | a live map written back out as a map file |
| 052 | changing a running program |
| 074 | boxes and maps compiled at run time |
| 092 | signals, capture, and the end |

A `#line` directive at every seam keeps compiler errors and debugger
backtraces pointing at the original numbered source.

## Two things the merge had to be careful about

**Preprocessor definitions leak forward.** In separate files a `#define`
died at the end of its file; here it would run to the bottom. Three
sections take their private macros with them by `#undef`ing at the end —
the pool's initial queue capacity, delivery's two timing macros, the
observer's growth threshold.

**The build-time facts are deliberately not undefined.** Which compiler
built the binary, where the generator is, and the two RAM tiers are used
by *two* sections — the one that compiles code at run time and the one
that writes a report on the way out. Undefining them after the first
would break the second. They arrive from the command line in an ordinary
build; the `#ifndef` fallbacks in section 074 never fire.

## What is not in here, and never will be

**The generated file.** It is derived at build time from box sources
this engine's author has never seen, so it is a separate translation
unit by necessity. That is what makes `cera.h` a real boundary: whatever
generated code calls is public whether anybody wants it to be or not.

**The demo boxes.** `src/boxes/` is example code the generator compiles
per-consumer. It exports `add`, which is the most collidable symbol in
C, and it is not part of the engine.

## Related

- [cera.h](cera.h.info.md) — the interface
- [901](../issues/completed/901-the-engine-becomes-one-file.md)
- [903](../issues/903-everything-else-goes-private.md) — the step this file exists to make possible
