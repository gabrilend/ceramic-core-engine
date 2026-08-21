# 311c — Source rides in the binary

Third child of [311](311-the-registry-dissolved.md). Every box source
the build includes is also emitted as text, so the compiled program
carries the C it was made from.

## Current behavior

**The text is carried, and reading names back out of it is blocked on
a decision.**

Every box source the build includes is emitted a second time as a C
array, with a small table from path to text. A program can be asked
for the C one of its sources was compiled from, by the path the build
knew it as or by the bare name somebody would type having seen the
file. **A test compares the carried text against the file on disk byte
for byte**, which is the assertion worth having: a source reported
from a program has to be the source that program was compiled from, or
it is worse than nothing.

The method is the unremarkable one — a C array in the generated file,
portable, no post-build step, nothing to learn. The alternatives and
what each costs are below.

**What is not built is reading *names* back out of that text**, and
the reason is a wrong assumption in this issue rather than a missing
afternoon. Step 2 says the generator's declaration parser is
"callable at runtime anyway", crediting
[308](completed/308-generator-in-c.md) for it. 308 made the generator
a **C program**, which is callable as a *process* — and that is how
[310](completed/310-boxes-compiled-at-runtime.md) reaches it, by
running the binary. The parser is not linked into the engine and
nothing has ever linked it.

So there is a fork here that this issue thought it had already
crossed, and it is written up under Open questions rather than
guessed at.

### What stood before

The generated file `#include`s each box source **whole**, so the
compiler can see the types and inline each box into its shim. The text
was therefore already read, already present at the moment of
generation, and then thrown away — it survived only as compiled code.

So a running program could not say what its boxes look like, and a
program handed to somebody else was a binary that needed a source tree
beside it before it could do anything with new code.

## Intended behavior

**Each included box source is emitted a second time, as data.**

```c
static const char src__math_c[] =
    "int add(int a, int b)\n"
    "{\n"
    "    return a + b;\n"
    "}\n";
```

A small table maps a box source's path to its text, the same way the
box table maps a name to a placement.

**The method is chosen for being unremarkable.** Three ways exist to
put bytes in a binary and this is the plainest:

| approach | how | why not |
|---|---|---|
| **a C array in the generated file** | it is just more generated C | **chosen** — portable, no post-build step, nothing new to learn |
| a named ELF section | `objcopy --add-section` after linking | smaller loaded image, but ELF-only and a build step that runs after the link |
| appended past the last segment | length trailer at end of file, found via `/proc/self/exe` | works anywhere, but finding your own executable is the fragile part — `/proc` is Linux-specific and `argv[0]` lies |

The generator already reads each source in order to `#include` it, so
emitting the same text as a string array is a few more lines in a pass
that is already running.

## What it buys

**One file is the whole system.** Compiled code, the numbers the
compiler folded into placement functions, and the text — all in the
binary. A program ships as a single file with no tree beside it.

**A box added at runtime has something to be compiled *against*.**
[310](completed/310-boxes-compiled-at-runtime.md) needs source at runtime, and
this is where the source it already had comes from.

**Type and argument names come back for free.**
[311b](311b-placement-instead-of-records.md) drops every name string
the engine was carrying, because the engine never used one. Error
messages and the dump do, and they read them here.

**And the names cannot be stale.** The embedded text is *by
definition* the text that was compiled. Somebody editing `math.c` on
disk after the build changes nothing the binary can see, which is
correct — the running code is the old code. This retires a check that
was designed and then found unnecessary: an earlier draft recorded each
source's modification time at placement so a debug report could warn
that names might not match. With the source embedded there is nothing
to warn about.

**The one case where source can still drift is a box added at
runtime**, whose source is live rather than embedded. Those already
save their own text to the RAM-backed directory the moment they are
created, treated exactly like a log. A report should say so **only when
it has drifted** — an unchanged source produces no line at all, because
a reassurance printed every time is noise that teaches a reader to skip
the section.

## The cost, stated

The text sits in the loaded image, so a program's binary grows by
roughly the size of its box sources. That is small next to what those
sources compile to, and it only counts the sources a map actually needs
once [311d](311d-the-map-becomes-code.md) lands. If it ever stops being
small, the move that keeps this decision is the ELF section above —
same data, out of the loaded image, at the price of portability.

## Suggested implementation steps

1. **Done.** A string array per included source, and a table from
   path to text. The generator's own file reader was exported rather
   than copied, so a source is read twice with one error message
   between the two readers.
2. **Half done.** Given a path, the text comes back — by the path the
   build knew it as, or by the bare name somebody would type. Given a
   box name and a parameter index, the parameter's *name* does not,
   because the parser that would read it is not linked into the
   engine. See the open question.
3. **Waits on 2.**
4. **Done by not doing.** The modification-time check an earlier draft
   designed is not built, and this issue is why: the embedded text is
   by definition the text that was compiled, so there is nothing to
   warn about. Somebody editing a source on disk after the build
   changes nothing the binary can see, which is correct — the running
   code is the old code.
5. **Done.** The carried text is compared against the file byte for
   byte. If somebody edits a box source and rebuilds, it passes; if
   they edit and do not rebuild, it fails, which is correct, because
   the binary is then carrying the truth and the disk is not.

## Open questions

**Open: how does a running program read a name out of the source it
carries?**

This issue assumed the answer and the assumption is wrong.
[308](completed/308-generator-in-c.md) made the generator a C
*program*, which is callable as a process — and
[310](completed/310-boxes-compiled-at-runtime.md) reaches it exactly
that way, by running the binary. The declaration parser is not linked
into the engine, and nothing has ever linked it.

Three ways, and none is obviously right:

- **Link the parser into the engine.** Two of the generator's four
  files — the text utilities and the parser, not the emitter — become
  part of every program. A program could then read its own carried
  source with no subprocess, which would also let runtime compilation
  check a box before shelling out to compile it. It costs every
  program an arena allocator and about a thousand lines of what is
  currently build tooling.
- **Ask the generator binary.** It already answers `--describe`, and
  the engine already runs it to compile a box while a program runs, so
  the dependency exists and nothing new is added. It costs a process
  per question, which is fine for a debug report and absurd for
  anything else — and it means a program that ships without the
  generator beside it loses the feature, which is most of what this
  issue was for.
- **Emit the names as data too.** The generator knows every parameter
  name at generation time and could write them out beside the source
  text. It is by far the cheapest and it runs against
  [311b](311b-placement-instead-of-records.md), which is deleting name
  strings from the engine on the grounds that the engine never reads
  them. The counter-argument is that these are read by *reports*
  rather than by the engine, and that a table of names beside a blob
  of text is a strange thing to carry when the text already contains
  them.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311b — Placement instead of records](311b-placement-instead-of-records.md),
  which drops the names this brings back
- [311d — The map becomes code](311d-the-map-becomes-code.md), which
  decides how much source there is to embed
- [310 — Boxes compiled while the program runs](completed/310-boxes-compiled-at-runtime.md),
  which needs source at runtime and now has it
- [308 — The generator, in C](completed/308-generator-in-c.md), whose parser this
  calls at runtime for argument names
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where shipping one file rather than a tree is the point
- [106 — Stopping on purpose](completed/106-stopping-on-purpose.md), whose report
  gains the argument names and the elision rule
