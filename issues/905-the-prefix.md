# 905 — The prefix

## Current behaviour

**The public names are ordinary English words.** After
[903](903-everything-else-goes-private.md) there are far fewer of them,
but the ones that remain are the collidable ones — a consumer's own
notion of a map or a pool meets `map_create` and `pool_push` and the
link fails, naming a function they never wrote.

Two prefixes are already in use and neither covers the surface.
`sora_` is on stopping, the text conversions, and every generated
station-builder, and appears in every include guard. `map_` and `pool_`
are layer names that read as prefixes without being one.

And the two spellings of the engine's own name have been sitting beside
each other unremarked — see below, where that turns out to be the whole
of what this issue was actually about.

## Intended behaviour

**One prefix on every public name**, applied once and grepped for
stragglers — including in the documents and the issue files, which name
these calls in prose and would otherwise describe an engine that no
longer answers to those names.

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
`sora_` and `cera_` have been two spellings of one prefix the whole time,
which is why the project could carry both without anything feeling wrong
— there was no collision to feel, because there were not two names.

That reframes what is being decided. Not *which name*, since there is
one, but **which spelling of it a consumer meets**, which is a much
smaller question with a much clearer answer:

**`cera_`, because it is what the file says.** A consumer holds two
files called `cera.c` and `cera.h`, writes `#include "cera.h"`, and then
calls something. If that something is `sora_map_create`, they have been
handed a spelling mismatch on their first line and no way to know it is
not a second library. If it is `cera_map_create`, the include and the
call agree and there is nothing to explain.

The cost — renaming the fifteen-odd symbols already spelled `sora_`, and
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
   compiler will never check. The include guards go too: `SORA_STATION_H`
   and its siblings are already gone into `CERA_H`, but the generated
   code and the documents still say `sora_` in places a compiler will
   never read.
4. **Collapse the export list** to the single pattern the prefix now
   allows, and decide whether it remains a file.

## Related

- [903](903-everything-else-goes-private.md), which shortened this list
- [902](902-the-header-says-what-is-public.md), whose open question
  this one settles
