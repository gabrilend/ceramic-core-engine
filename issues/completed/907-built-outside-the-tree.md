# 907 — Built outside the tree

The capstone of phase 9. Everything above it makes the engine *look*
portable; this is the issue that finds out.

## Current behaviour

**Done, and it passes.** `make test` copies `cera.c`, `cera.h`, the
export list and the generator into a scratch directory with no
relationship to this repository, writes a box source and a map and a
program from nothing, runs the three steps a consumer's build runs, and
checks what the program printed. Twelve fed into a box that adds thirty
comes back as forty-two.

It also greps the generated file for this repository's path, so an
emission that would not relocate fails here rather than in somebody
else's tree.

### Two things it caught immediately, which is the argument for having it

**The scratch tree was in the wrong RAM tier.** It was written into the
artifact tier under `/dev/shm`, which is mounted without execute
permission, so the generator compiled and then could not be run. The
project keeps two tiers for exactly this reason and the test now uses
the executable one.

**The map reached for one of this repository's own demo boxes.** The
first version placed `keep`, which exists here and would never exist in
a consumer's tree. It passed nothing and proved nothing; the imaginary
consumer now owns both the boxes its map names. A test for portability
that quietly borrows from home is the failure it was written to catch.

## What is still owed

**The train game in the sibling directory.** It builds the engine by
copying its source in and still names the Lua generator that was
replaced by the C one, so it does not build. Fixing it is separate work
in a separate project, and it is the natural second consumer now that
the first one passes. This test proves a *new* program can be built; it
says nothing about the existing one being repaired.

## Intended behaviour

**A test that builds a program with this engine, outside this tree, and
runs it.**

Not a script that a person remembers to run. Part of `make test`, so a
change that breaks the packaged shape fails the ordinary test run rather
than being discovered by whoever tries to use it a month later.

### What it does

1. Copies `cera.c`, `cera.h` and the generator into a scratch directory
   in the RAM tier — somewhere with no relationship to this repository
   and no include path pointing back at it.
2. Writes a box source: one small C function.
3. Writes a map file placing it.
4. Runs the three steps a consumer's build runs — generate, compile,
   link — with the two linker settings the engine requires.
5. Runs the result and checks what it printed.

### What it proves, one property per failure mode

- **The header is sufficient.** A missing declaration fails at step 4,
  naming it.
- **The engine does not reach back into its own tree.** Any leftover
  include of a numbered filename fails at step 4 with no include path to
  find it on.
- **The three-step build is really three steps**, and the generator
  really depends on nothing the engine provides.
- **The linker settings are stated somewhere a consumer can find them.**
  Omitting the export list produces a program that builds and then fails
  at run time when a box arrives, which is the failure mode most worth
  catching automatically, because it looks like success.

### And the worked example is the real documentation

The scratch directory's contents — one box, one map, one program, and
the build rule tying them together — are exactly what a consumer needs
to see. They should be readable as an example, not merely correct as a
test, because the file that shows how the build-time half and the
run-time half fit together is the thing nobody can infer from a header.

### What it does not cover

**The train game.** Fixing it is a separate piece of work in a separate
project, and it is the natural second consumer once this one passes.
Worth saying plainly: this test proves a *new* program can be built, not
that the existing one has been repaired.

## Suggested implementation steps

1. **Write the scratch build as a shell test** beside the others, taking
   the project root as its argument like the rest of them do.
2. **Copy rather than symlink**, so a forgotten include path fails
   rather than silently resolving.
3. **Check the output**, not just the exit status.
4. **Make it run in the RAM tier**, so it leaves nothing behind.
5. **Then fix the train game against the current engine**, and record
   what that took — it is the first honest measurement of what an
   upgrade costs a consumer.

## Related

- [901](901-the-engine-becomes-one-file.md) through
  [906](906-an-error-reaches-the-host.md), all of which this checks
- [057 — Packaging](../docs/implementation-notes/057-packaging.md), step
  6 of its rough order of work
