# 904 — The old files are removed

## Current behaviour

**The engine exists twice.** `cera.c` and `cera.h` are built and tested
— the whole test suite runs from them and the example runs from them —
while the eleven numbered bodies and seven numbered headers they were
made from are still on disk, compiled by nothing. Two copies of the engine
means an edit can land in the one nothing builds, and be believed.

This state is deliberate and temporary. It exists so that the move can
be checked by comparison rather than by reading, and it must not
outlive the comparison.

## Intended behaviour

**One engine on disk.** The numbered sources and their `.info.md`
companions are deleted. `cera.c` and `cera.h` gain their own `.info.md`
files. The `#line` directives at each seam, which pointed at the
numbered sources, point at `cera.c` itself — a directive naming a file
that no longer exists is worse than none, because a debugger will
believe it.

### The condition, which is not "the tests pass"

**Byte-identical output.** Every test binary's standard output and
standard error, captured from the numbered build and from the `cera`
build, compared byte for byte and found equal. The tests passing proves
they still pass; identical bytes prove nothing moved that was not meant
to move.

Two things legitimately differ and are excluded by construction rather
than by exception: paths printed by a test that names its own source
file, and anything containing a timing. Both are known in advance, and
anything else that differs is a finding.

### What goes with them

Deleting the numbered engine sources leaves references behind. Grep is
the tool and the whole tree is the target:

- **The build's include paths.** `-I libs -I src` becomes one path, or
  none if `cera.h` sits beside what includes it.
- **The generator's emitted includes.** It writes between one and five
  `#include` lines naming numbered headers into every emitted file.
  Those become one.
- **The documentation.** Docs, implementation notes, `.info.md` files
  and issue text all name these files in prose. A reference to a deleted
  file is a broken promise to a reader.
- **The HTML mirror**, which is generated and swept, so it corrects
  itself once the sources do.
- **`scripts/110-amalgamate.lua`**, the one-shot tool that produced the
  two files. Once its inputs are gone it cannot run, and it has been in
  the record for a commit, which is all a migration tool is owed.
- **`.file-index-counter`.** Eighteen indices are freed. They are not
  reused: the numbers are positions in a reading order, and a reused
  number makes two things claim one position. The gap is the record that
  something was there.

## Suggested implementation steps

1. **Capture the output of every test twice** — once from the numbered
   build, once from the `cera` build — and diff, with
   `scripts/111-capture-test-output.sh`, which
   [901](completed/901-the-engine-becomes-one-file.md) built for exactly
   this and which knows which four tests report a race and must be
   compared by shape. Do not proceed on a difference; understand it.
   901 did this once already and found one, explained by the emitted
   file's include block losing four lines; a second difference appearing
   now would mean something moved that should not have.
2. **Delete the eleven bodies, the seven headers, and their `.info.md`
   companions**, in one commit, so the removal is one thing in the
   record rather than scattered.
3. **Delete the eighteen interface files.** Their replacements —
   `cera.c.info.md` and `cera.h.info.md` — were written alongside the
   sources in [901](completed/901-the-engine-becomes-one-file.md),
   because a source file without one is a file a reader has to open.
   The header's is the important one: it is the document a consumer
   reads instead of the source, and it grows a real function list when
   [902](902-the-header-says-what-is-public.md) settles what that list
   is.
4. **Re-point the `#line` directives** at `cera.c`.
5. **Grep the whole tree** for every deleted filename and fix what it
   finds — build, generator, docs, notes, interface files, issue text.
6. **Regenerate the documentation site** and let the sweep remove the
   pages whose sources are gone.

## Related

- [901](901-the-engine-becomes-one-file.md), which created the duplicate
  this removes
- [711 — An index is a position in the reading order](711-the-index-means-reading-order.md),
  whose rule decides that the freed numbers stay free
