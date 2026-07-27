# 405 — Altering the statics table while the program runs

## Current behavior

Built. One mutex over the table, taken on every claim and every
write, held for the length of one copy — reads constant, writes
rare, contention nil, exactly the analysis this issue made. The
write call is size-checked against the entry and refuses entries no
slot has bound, since their shape is unknown. Box-reachability
landed as the bare-name write against the process's active map,
because a box receives only values and has no map pointer — an
ambient-global cost the first-pass report weighs; the warning
comment sits at the call as demanded, naming the back channel for
what it is. Proven by four thousand claims racing a writer
alternating a struct between two self-consistent worlds: zero torn
reads, and the last write visible to the next claim.

## Intended behavior

The table lives in RAM and may be altered while the program runs.

**Reads need a lock the moment writes exist.** A static slot is read
during task construction. If a struct is half-overwritten while a slot
is copying it, the result is a value assembled from two different
worlds — fields that were never simultaneously true. For anything
larger than a machine word this is not theoretical.

One mutex over the table, held for the length of a copy, is enough.
Reads are constant and writes are rare, so contention is nil and a
reader-writer lock would be machinery bought for nothing.

**Two kinds of writer, and they deserve different amounts of
suspicion.**

*Something outside the graph* — a debugger, a control socket, a person
turning a knob — changing a threshold or a path while the engine runs.
This is what the capability is for and it needs nothing but the mutex.

*A box* writing to the table. This works, and it is a back channel
around "a box cannot remember." A box can stash a value in an entry and
read it back on its next run, which is exactly the shared mutable state
the design removed, relocated. None of it appears in the wiring — a map
showing no connection between two stations may still have them talking.

It is allowed. It should be treated with the suspicion a global
variable deserves, and for the same reason: it works fine until two
things use it and neither knows about the other.

**What a global has that this does not** is that you can grep for it.
Whether a box should declare which entries it writes, so the back
channel appears in the map, is left open — deciding it now would be
guessing about a use that has not appeared yet.

## Suggested implementation steps

1. Add the mutex to the statics table; take it on every read as well as
   every write.
2. The write call, taking an entry index and a value, checked against
   the entry's size.
3. Make it reachable from a box, since that is the case with
   consequences — and put a comment at that call site explaining what
   it costs, so the next reader meets the warning before the capability.
4. A test that a value altered mid-run is picked up by the next task
   built, and that no task ever sees a partially-written struct — best
   exercised by writing alternating values of a multi-field struct from
   one thread while many others read.

## Related

- Issue 401 — the table this guards
- [008 — Map file format](../docs/008-map-file-format.md)
