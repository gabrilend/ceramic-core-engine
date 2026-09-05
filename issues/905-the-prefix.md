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

## Intended behaviour

**One prefix on every public name**, applied once and grepped for
stragglers — including in the documents and the issue files, which name
these calls in prose and would otherwise describe an engine that no
longer answers to those names.

### The open question, which has to be answered before a line changes

**Which prefix.** Three candidates, and this is a one-shot decision
because the second application costs what the first did:

| candidate | for | against |
|---|---|---|
| `cera_` | matches the files, matches what the readme calls the thing — the ceramic core engine; short | `sora_` is already on ~15 symbols and in every include guard, so this renames those too |
| `sora_` | already used; renames fewer symbols; the project's own name for itself in code | the project's front door does not say "sora" anywhere; the name is inherited from the larger project this was distilled out of |
| both | leave `sora_` where it is, add `cera_` to the rest | two prefixes is no prefix — a consumer cannot tell which family a call is in, and neither can grep |

**Ask before starting.** Nothing below can begin until this is answered,
and answering it wrongly is a day of work twice.

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

1. **Answer the open question.**
2. **Rename in the header first**, since it is the list.
3. **Follow it into `cera.c`, the generator, the tests, the example.**
4. **Grep the whole tree** for the old names — including prose, which a
   compiler will never check.
5. **Collapse the export list** to the single pattern the prefix now
   allows, and decide whether it remains a file.

## Related

- [903](903-everything-else-goes-private.md), which shortened this list
- [902](902-the-header-says-what-is-public.md), whose open question
  this one settles
