# 704 — Rewiring while it runs

## Current behavior

A map is fixed once it loads. Changing a wire means editing the file
and restarting, which discards everything sitting in the buffers.

## Intended behavior

Connections may be added and removed while the engine is running.

**This is the feature the whole design has been quietly preparing
for.** Wires hold station indices rather than addresses. Stations never
move. The cycle check runs when a connection is made rather than when
it is traversed. Each of those was chosen partly for this, and each is
worth noting here as the debt being redeemed.

**The check and the insertion must happen under one lock.** Two threads
each adding an individually legal edge can produce an illegal pair —
neither closes a cycle on its own, and together they do. The cycle
walk from issue 404 and the append to the destination list are one
operation, not two.

**A wire being removed while a value is in flight is fine.** A value
already inside a task struct is a copy, and the task carries its
destination. The worst case is a value delivered down a wire that was
removed a moment ago, which is indistinguishable from having been
delivered a moment earlier — there is no correct alternative and no
way for an observer to tell.

**Adding a station is a different problem and is not in scope here.**
The station table is allocated once at load; growing it means
reallocating the array, and while every wire holds an index rather than
a pointer — so nothing dangles — every thread reading the table needs
to see the new base. That is a real design question and deserves its
own issue rather than being smuggled in alongside rewiring.

**Every load-time validation rule applies to a runtime edit.** Type
compatibility, gather cycles, mixed fan-out, the gatherer rule. A
rewiring path that skips them is a way to reach a state the loader
would have refused. The rules from issue 604 should be callable per
edge, not only per map.

**What happens on refusal** needs deciding rather than defaulting. The
project prefers a stop to a silent wrong turn, and an illegal rewire is
a programming error, so stopping is defensible. But an engine that dies
because a control socket sent a bad instruction is fragile in a way a
loader is not. Whichever is chosen, it should be chosen deliberately
and the reasoning left in a comment.

## Suggested implementation steps

1. A rewiring lock covering the port destination lists.
2. Connect: validate the edge against every rule from issue 604, walk
   for cycles per issue 404, append — all under that lock.
3. Disconnect: remove a destination from a port's list under the same
   lock.
4. Decide and document the refusal behaviour.
5. A test that a map rewired mid-run changes behaviour from that moment
   and that no value is lost across the change.
6. A test that two threads adding edges which are individually legal
   and jointly cyclic results in exactly one of them being refused.
7. Use issue 703's dump before and after, as the record of what changed.

## Related

- Issue 404 — the cycle check being reused
- Issue 604 — the rules being reused
- Issue 703 — how the resulting map is read back
