# 311a — Boxes addressed by file

First child of [311](311-the-registry-dissolved.md), and first because
it decides what the box table's key is. Everything else in the family
is written against that key.

## Current behavior

A map's station line names a box by a **bare function name**:

```
adder add p
```

`add` is looked up in the registry, which is one global table holding
every function the generator found anywhere under the box source
directory. Two files each defining a function called `read` are a
collision nobody declared and nothing detects — the second one to be
emitted simply wins, or the build breaks with a duplicate-symbol error
that names a symbol rather than a design mistake.

Nothing in a map says where a function lives, so nothing in a map tells
the build which sources a program actually needs.

## Intended behavior

**A station line names the file and the function.**

```
adder math.c:add p
```

**Provenance belongs in the file a person reads.** A bare name is not
an address; it is a name in a namespace nobody wrote down. Naming the
file makes it an address, and it is the same information a reader of
the map wants anyway when they go looking for what `add` actually does.

**A bare file name is tried first; a path settles ties.** `math.c`
resolves when exactly one box source anywhere in the tree is called
that. If two are, the reader refuses and names **both paths**, and the
author writes one out in full:

```
adder src/boxes/math.c:add p
```

Both forms are legal at any time. **The path is not a fallback**, it is
a more specific way of saying the same thing, so nobody has to guess
which form is "the real one" — and an author who prefers paths
everywhere is not fighting the format.

The basename-first rule is the one that carries the cost of being
convenient, and it is worth naming: it means box source basenames are a
**flat global namespace**, and two files called `math.c` in different
directories cannot both be addressed briefly. That matches what the
project already does — the file index counter runs across the whole
tree rather than per directory, so a single global ordering of
filenames is already the model here.

**A collision is fatal at build time**, naming both paths and saying
which map line asked. It is not a warning and it does not pick one.

**The dump writes whichever form is unambiguous** — the bare name when
it resolves uniquely, the full path when it does not. So a dumped map
always reloads into the program it came from, while staying as readable
as it can be.

### A collision that appears while the program runs

A box source arriving through
[310](310-boxes-compiled-at-runtime.md) may share a basename with one
already loaded. That is **allowed**, because refusing it would mean a
legitimate second `math.c` could never be brought in.

**Stations already placed are unaffected.** They resolved their names
when they were placed; a name is resolved once and the result is a
pointer, not a string consulted again later.

**Bare references to that basename become ambiguous from then on**, and
the next one refuses with both paths named. So a map that loaded
yesterday can refuse today — which is a real cost, stated here so it is
not a surprise, and the fix is one word in the map.

## Suggested implementation steps

1. The reader parses `file:function` on a station line, and refuses a
   bare function name with a message saying what to write instead —
   because the old form loading into *something* would be worse than it
   refusing.
2. Resolution: exact path match first if the name contains a separator,
   otherwise basename search across the known box sources. One match
   resolves; several refuse and name every candidate.
3. The build-time collision check, run over the whole box source tree
   rather than only over what a map mentions, so two `math.c` files are
   caught whether or not anyone has referenced them yet.
4. The same check at runtime when a box source arrives late, marking
   the basename ambiguous rather than refusing the source.
5. The dump chooses bare-or-path by asking whether the bare form
   resolves uniquely right now.
6. A test that a map naming an ambiguous basename refuses and names
   both paths; and a round-trip test on a program containing two boxes
   whose files share a basename, proving the dump wrote paths where it
   had to.

## Open questions

None outstanding.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  the new station line and the resolution rule
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  where a late-arriving source can create an ambiguity
- [703 — The map dump](completed/703-map-dump.md), which has to choose
  a form
- [801 — The workbench in the browser](801-browser-workbench.md), whose
  note already anticipated this: *where a function's home matters, the
  file name is typed beside it*
