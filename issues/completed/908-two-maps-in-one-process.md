# 908 — Two maps in one process

**Not on the critical path.** Phase 9 can finish without this and a
consumer can use the engine without it. It is here because a library
with a hidden singleton is a defect that only shows up in somebody
else's program, and because the fix is wanted for other reasons anyway.

## Current behaviour

**Already true, and now proven.** There is no process-wide "active map"
pointer. It went when statics moved onto the ports that read them: the
only thing that needed it was a box reaching the statics table, and a
box cannot write a static any more. The measurement hook that was its
last remaining user now rides its timing out on the task instead.

So this issue needed no code. What it needed was a test, because **a
singleton is invisible until two of something exist**, and nothing built
two.

Two programs are now built with different constants, brought up on two
separate pools, fed the same twenty values alternately, and checked
against what each one's own constant says its answers should be. Then
one is destroyed and the other is confirmed still standing.

It is deliberately not the same as starting a program beside another,
which shares one pool and reaches the second through its doors. These
two share nothing and do not know about each other.

### What is still process-wide, and is meant to be

The table of boxes that arrived after the program started, the signal
state, and the report file. None is map state: a box compiled at run
time belongs to the process, and so does a signal.

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
