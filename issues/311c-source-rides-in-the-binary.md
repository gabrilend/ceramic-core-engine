# 311c — Source rides in the binary

Third child of [311](311-the-registry-dissolved.md). Every box source
the build includes is also emitted as text, so the compiled program
carries the C it was made from.

## Current behavior

The generated file `#include`s each box source **whole**, so the
compiler can see the types and inline each box into its shim. The text
is therefore already read, already present at the moment of generation,
and then thrown away — it survives only as compiled code.

So a running program cannot say what its boxes look like, and a program
handed to somebody else is a binary that needs a source tree beside it
before it can do anything with new code.

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

1. The generator emits a string array per included source, and a table
   from path to text.
2. An accessor: given a box source path, hand back its text. Given a
   box name and a parameter index, hand back that parameter's name by
   parsing the text — which is the generator's own declaration parser,
   which [308](completed/308-generator-in-c.md) is making callable at runtime
   anyway.
3. The debug report uses it, printing argument names by index, and
   saying nothing at all about sources that have not moved.
4. The modification-time check designed for that report is not built;
   this issue is why.
5. A test that the embedded text for a source matches the file it was
   generated from, byte for byte, at build time.

## Open questions

None outstanding.

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
- [106 — Stopping on purpose](106-stopping-on-purpose.md), whose report
  gains the argument names and the elision rule
