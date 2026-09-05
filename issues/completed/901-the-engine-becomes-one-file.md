# 901 — The engine becomes one file

First issue of phase 9. Everything before it built an engine that runs
inside this repository. This is the first step of the engine being
something somebody else can take away.

## Current behaviour

**Built.** The engine is `src/cera.c` and `src/cera.h` — one
translation unit of about 8,500 lines and one header of about 2,900,
each carrying the eleven and seven sections it was made from, with a
banner and a `#line` directive at every seam. The build compiles the one
file; the tests, the example and the generator's emitted output include
the one header. It compiled clean under `-Wall -Wextra -Werror` without
a single edit to the code that moved.

The numbered sources are still on disk and are compiled by nothing,
which is deliberate and temporary — the duplicate exists so the move
could be checked by comparison, and it goes in
[904](904-the-old-files-are-removed.md).

**Nothing yet distinguishes public from internal.** The header holds
everything, because everything in it was there so that one engine file
could reach another. A linked test binary publishes 102 symbols and
defines 289. Narrowing that is [902](902-the-header-says-what-is-public.md)
and [903](903-everything-else-goes-private.md), and those two numbers
are the measurement they will change.

### What the comparison found

Every test binary's output was captured from the numbered build and
from the one-file build and diffed. **Thirty-three of thirty-four are
byte-identical.** The thirty-fourth differs in one character and the
difference is the intended one: the test that watches a box being
compiled at run time quotes the compiler's own diagnostic, which names a
line in the emitted file, and the emitted file's include block lost
exactly four lines when five engine headers became one. 17 became 13.

Two things had to be normalised before any comparison was possible, and
finding out which is a result in itself:

- **Process ids and timings**, which vary between any two runs.
- **Four tests report counts produced by concurrent scheduling** — how
  many pages a port grew under load, how deep the scrapyard got, how
  many removals landed, what share of a delivery the copy took. These
  are the tests doing their job, and their numbers differ every run on
  an unchanged binary. They are compared by shape rather than by value,
  and the four are named in the capture script rather than being
  detected, so adding a fifth is a deliberate act.

Two consecutive runs of the unchanged build were diffed against each
other first, to establish that the instrument reads zero before it is
used to measure anything.

## Intended behaviour

**Two files: `src/cera.c` and `src/cera.h`.**

`cera.c` is every engine `.c` in reading order, one translation unit.
`cera.h` is every engine header in dependency order, one include guard.
A consumer takes two files and adds one include path — or drops them
into their own tree and adds none.

**This issue changes nothing else.** No renames, no narrowing of the
header, no function becoming `static`. Those are
[902](902-the-header-says-what-is-public.md) and
[903](903-everything-else-goes-private.md), and they are separable
precisely because this step is mechanical. A step that both moves code
and changes it cannot be checked by comparing output, and comparing
output is the only proof available here.

### Why one translation unit is the point rather than a side effect

The reason to concatenate is not tidiness. **A function that is not
declared in `cera.h` can be marked `static`, and a `static` function is
not a linker symbol at all.** The forty-collisions problem does not get
solved by renaming forty things; it stops existing for everything that
was never public in the first place. That is [903](903-everything-else-goes-private.md),
and this issue is what makes it possible.

The costs are known and accepted: any change recompiles the whole
engine, which at this size is a fraction of a second and is paid by the
consumer's build rather than ours.

### The reading order survives, and the numbers stop being filenames

The engine's files are a story meant to be read in sequence, and the
numbers in their names are their positions in it. Concatenating them
does not lose the order; it fixes it. Each seam carries a banner naming
the file it came from and its number, so the story reads the same and
the sections keep their identity.

Each seam also carries a `#line` directive naming the original path, so
compiler errors and debugger backtraces point at the numbered source
rather than at a line in a ten-thousand-line file — for as long as the
numbered sources exist. When [904](904-the-old-files-are-removed.md)
deletes them, the directives are re-pointed at `cera.c` itself.

### The two names carry no index, and that is deliberate

Every other source file in this project takes the next number from
`.file-index-counter`, because the numbers are a reading order across
the whole tree. These two do not, and the reason is the same reason the
project wants them: **they are the deliverable.** A consumer vendors
them into their own tree, where our reading order is not a fact about
anything. `018-station.h` naming the eighteenth thing to read is
precisely the awkwardness this issue removes; recreating it as
`110-cera.h` would remove nothing.

The reading order they used to carry moves inside `cera.c`, where the
section banners hold it.

### What the order is

Headers first, in dependency order — each one needs the ones above it:

| # | header | needs |
|---|---|---|
| 011 | pool | nothing but the C library |
| 018 | station | pool |
| 026 | emitted | pool, station |
| 040 | mapfile | station |
| 049 | observe | station |
| 073 | latebox | station, emitted |
| 091 | stopping | station |

Then the bodies, in their existing numbered order: pool, station,
delivery, emitted-support, statics, loader, observe, dump, rewire,
latebox, stopping.

### What the merge has to be careful about

Found by reading the sources rather than assumed:

- **Preprocessor definitions leak forward.** In separate files a
  `#define` dies at the end of its file; in one file it runs to the
  bottom. Four sections define private macros — the pool's initial
  queue capacity, delivery's two timing macros, the observer's
  growth-shout threshold. Each is `#undef`'d at the end of its own
  section, so a section still cannot see the one above it.
- **The build-time facts latebox falls back on** — which compiler, where
  the generator is, which include directories, the two RAM tiers — are
  `#ifndef`-guarded fallbacks for values the build normally supplies on
  the command line. They stay guarded and stay in latebox's section.
- **Nothing else collides.** Every engine file's file-scope statics were
  checked against every other's: the four names that appear twice
  (`tb_addf`, `say`, `port_text_to_bytes_ending`, `load_file`) are each a
  forward declaration and its definition inside one file, not two files
  claiming a name. There is nothing to rename.
- **Inter-file `#include` lines are dropped**, since every header is now
  above every body in the same file. The C library includes are hoisted
  to the top of `cera.h`, deduplicated.

### What generated code and tests need, which decides nothing here but constrains 902

Two callers reach the engine from outside `cera.c`, and they are the
reason the header cannot simply vanish:

- **The generated file is a separate translation unit by necessity** —
  it is derived at build time from box sources the engine's author has
  never seen. It calls twenty-three engine functions. Those are public
  whether anybody wants them to be or not.
- **The tests are white-box.** Eight of the functions they call are
  internal joints — the ones that reach a slot, move a value inside a
  ring, count a port's pages, read an output port's destination set.
  [903](903-everything-else-goes-private.md) decides what happens to
  them; this issue leaves them exactly as they are.

## Suggested implementation steps

1. **Write `cera.h`.** Concatenate the seven headers in the order in the
   table above, under one `CERA_H` guard, dropping each file's own guard
   and each inter-header `#include`. Hoist and deduplicate the C library
   includes. Banner each section with the file and number it came from.
2. **Write `cera.c`.** `#include "cera.h"`, then the eleven bodies in
   numbered order, dropping their `#include`s of engine headers, keeping
   their C library includes, with a banner and a `#line` at each seam and
   an `#undef` at the end of every section that defined a macro.
3. **Build it beside the old files, not instead of them.** The engine
   source list becomes `cera.c` plus the generated file; the numbered
   sources stay on disk, compiled by nothing. Nothing is deleted here —
   deletion is [904](904-the-old-files-are-removed.md), and it happens
   after the comparison below, not before it.
4. **Point the tests, the example, and the generator's emitted includes
   at `cera.h`.** One include line each, replacing between one and four.
5. **Compare the output.** Every test binary's standard output and
   standard error, captured before the change and after it, compared
   byte for byte. Not "the tests pass" — the same bytes. A mechanical
   move that changes a byte of output has not been mechanical.

## What it means for the issue files below it

Every completed issue that built an engine function described where the
function went, when it said anything about placement at all. Those
statements are now wrong in a specific and harmless way: there is one
place. The correction is not to delete the information but to change
what is stated — an issue says **which functions it builds and what
their signatures are in `cera.h`**, which is a more useful thing to have
recorded than a filename ever was, and which is what a person rebuilding
this project from `issues/completed/` actually needs.

That rewriting is deliberately **not** done in this issue. It is done in
[902](902-the-header-says-what-is-public.md), after the header has been
narrowed, because a signature written against the intermediate header
would have to be written twice.

## Related

- [057 — Packaging the engine as a library](../docs/implementation-notes/057-packaging.md),
  the survey this phase executes. Its recommendation was a script that
  derives an amalgamation from the numbered files; this phase instead
  makes the amalgamation the source and deletes the numbered files,
  which removes the derivation step and the drift it could carry.
- [902](902-the-header-says-what-is-public.md) — narrowing the header
- [903](903-everything-else-goes-private.md) — the internals go private
- [904](904-the-old-files-are-removed.md) — the numbered sources go
