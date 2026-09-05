# 908 — Two maps in one process

**Not on the critical path.** Phase 9 can finish without this and a
consumer can use the engine without it. It is here because a library
with a hidden singleton is a defect that only shows up in somebody
else's program, and because the fix is wanted for other reasons anyway.

## Current behaviour

**One process-wide variable holds the active map**, so that a box can
reach the statics table while it runs. There used to be two; the other
recorded where the last load's time went, broken into stages that
stopped existing, and it went with them.

The consequence is not a crash. **A host that wants two engines gets
one, and the second quietly writes into the first.** Nothing says so —
not the header, not the documents. It is the kind of restriction that is
discovered by a wrong answer.

## Intended behaviour

**A box reaches its map through its task**, which is the thing that
already knows which map it belongs to. The global goes away and two maps
in one process become ordinary.

**If it is not fixed, it is stated** — in the header, where somebody
reads it before they build on the assumption. An undocumented singleton
and a documented one are different defects, and only one of them is a
lie.

## Suggested implementation steps

1. **Find every read of the global** and check what each one actually
   wants — most will want the statics table of a specific map, which the
   task can name.
2. **Thread the map through the task struct.**
3. **Delete the global**, and let the compiler find what still wanted
   it.
4. **Test two maps at once in one process** doing different work, which
   is a test that cannot pass today and is the only proof that matters.

## Related

- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  decision 2 and step 7 of its rough order
- `notes/first-pass-report.md`, where this is already named as a debt
