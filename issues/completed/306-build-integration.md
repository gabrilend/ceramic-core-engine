# 306 — Build integration

## Current behavior

Built. Adding a box is writing a function: box sources are whatever
sits under `src/boxes/`, discovered by wildcard, so a box cannot
exist that the generator silently does not see. The make rule
regenerates whenever any box source or the generator itself is newer
than the emission; the emission lands in `src/generated/`, which is
ignored by history because a derived file in the repository is a file
that can be stale. A failing generator writes to a temporary name and
never moves it into place, so the build can never compile against
yesterday's registry — the no-partial-output guarantee is proven by a
shell test alongside the regeneration-follows-an-edit test. The
`describe` target prints what the parser saw.

## Intended behavior

**Adding a box is writing a function.** Nothing else. No registration,
no list to update, no build file to edit.

The generator runs before compilation, reads the box sources, and emits
into a build directory that is not tracked in history — the emissions
are derived, and a derived file in the repository is a file that can be
stale.

**Regeneration is triggered by the box sources changing**, so a build
after an edit cannot compile against a registry describing the previous
version of the code. Getting this wrong produces the worst class of
bug in the whole project: a shim that casts to a type the function no
longer returns, which the compiler accepts because the shim was
generated from something that was true once.

**The generator failing stops the build.** Its errors name the file and
line. It never emits a partial result — a registry with a hole in it
surfaces much later as a box that cannot be found by name, pointing the
reader at the map rather than at the real cause.

## Suggested implementation steps

1. A makefile rule that runs the generator whenever any box source is
   newer than the emitted files, and a clean target that removes them.
2. Make the box source set discoverable rather than listed, so a new
   file is picked up without an edit. Whatever the rule is, it must be
   impossible to write a box the generator silently does not see.
3. Ensure a failing generator fails the build rather than leaving
   yesterday's output in place.
4. Add the emitted directory to `.gitignore`.
5. A test that touches a box source, rebuilds, and asserts the registry
   reflects the change — the one guarding against a stale registry,
   which is the failure mode this issue exists to prevent.
6. A test that adding a new box source file makes it available with no
   other edit.

## Related

- [007 — The build path](../../docs/007-datapath-build.md)
- Issues 301 through 305 — what this runs
