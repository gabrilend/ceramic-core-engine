# 905 — The prefix

## Current behaviour

**Done.** The engine exports 100 symbols and every one of them begins
`cera_`. There is no second family and no exception.

3,008 renames across 52 files, plus 706 respellings of the older half
of the name, and **not one byte of test output changed**. The rename
reached the header, the body, the generator that writes engine calls
into every emitted file, the tests, the example, the linker's export
list, and the prose in the documents — which a compiler would never
have checked.

**The export list collapsed to one line.** It named two families
because the engine was carrying two spellings of its own name; one
spelling means one pattern. The file survives anyway, and deliberately:
what cannot go in a link command is the thirty lines around the pattern
explaining why the narrow form is needed and what the sweeping form
costs. That answers the open question 902 left about whether it should
still exist.

**One rename changes what a person types**, not only what a linker
reads: the environment variable that says how many workers to start.

### The trap in a prefix rename, which cost one build

A word boundary sits between `-` and `D`, not between `D` and `S`. So a
whole-identifier rename finds `CERA_ROOT` written on its own and does
not find it in `-DCERA_ROOT` on a compile line, and the build breaks
somewhere that looks unrelated — a test failing to see a macro the
Makefile is definitely defining.

Worth writing down because the tool was right and the mental model was
wrong: **the definition of a name and the use of a name are not written
the same way**, and a rename that only understands uses will silently
skip every definition on a command line.

### The question dissolved rather than being answered

It was asked as a choice between two names and it is not one.

```
soramech
ceramic
```

**Same word.** The consonants are identical and in order — S, R, M, K —
the vowels are all reduced to almost nothing, and the trailing `h` is
silent. Read down the columns: positions three, four, five and seven are
the same letters outright, and the first pair only differ in which
letter English uses to spell that sound.

So the engine was never renamed when it was distilled onto this branch.
It was **respelled**, and nobody noticed, including the people doing it.
`cera_` and `cera_` have been two spellings of one prefix the whole time,
which is why the project could carry both without anything feeling wrong
— there was no collision to feel, because there were not two names.

That reframes what is being decided. Not *which name*, since there is
one, but **which spelling of it a consumer meets**, which is a much
smaller question with a much clearer answer:

**`cera_`, because it is what the file says.** A consumer holds two
files called `cera.c` and `cera.h`, writes `#include "cera.h"`, and then
calls something. If that something is `cera_map_create`, they have been
handed a spelling mismatch on their first line and no way to know it is
not a second library. If it is `cera_map_create`, the include and the
call agree and there is nothing to explain.

The cost — renaming the fifteen-odd symbols already spelled `cera_`, and
every include guard — is not a cost of choosing between names. It is the
cost of the respelling having been half-done for months.

### What the rename touches

- The header's declarations and the definitions in `cera.c`.
- The generator, which writes calls to engine functions into every
  emitted file, and writes station-builder symbols under a prefix.
- The linker's export list, which names families by prefix — after this
  it is one pattern, which is when [902](902-the-header-says-what-is-public.md)'s
  open question about whether that file still needs to exist can be
  answered.
- Every test and the example.
- The documents and the completed issues, which name these calls in
  prose.

### What it does not touch

**Box functions.** A box is the consumer's own C function and carries
their names, not ours. The only engine-derived symbol attached to a box
is its generated station-builder, which lives under the engine prefix
already.

## Suggested implementation steps

1. **Rename in the header first**, since it is the list.
2. **Follow it into `cera.c`, the generator, the tests, the example.**
3. **Grep the whole tree** for the old names — including prose, which a
   compiler will never check. The include guards go too: `CERA_STATION_H`
   and its siblings are already gone into `CERA_H`, but the generated
   code and the documents still say `cera_` in places a compiler will
   never read.
4. **Collapse the export list** to the single pattern the prefix now
   allows, and decide whether it remains a file.

## Related

- [903](903-everything-else-goes-private.md), which shortened this list
- [902](902-the-header-says-what-is-public.md), whose open question
  this one settles
