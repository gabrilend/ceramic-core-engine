# 401 — Static slots and the statics table

## Current behavior

Built, in its own statics module. The table is numbered entries on
the map, guarded by one mutex; a static slot is always full, never
affects readiness, and claiming copies without consuming — proven by
a station running fifty times off one entry, driven only by its
buffer side, and by an all-static station that delivery can never
wake. Binding requires the slot to know its type, which only
registry placement provides — hand-placed stations cannot bind
statics, a sharper rule than the issue stated.

One deliberate departure, reasoned in the first-pass report: entries
hold parsed bytes shaped by the first binder rather than re-parsed
text per claim. Runtime mutation (issue 405) writes bytes, and a
table that is sometimes text and sometimes bytes is two tables
wearing one name; the docs' "two slots read one entry each their own
way" narrows to same-size types. Statics remain slightly
discouraged, as the design wants.

## Intended behavior

The second of the three slot kinds. A static slot holds an index into a
table of constants rather than a buffer. Its value never arrives; it is
simply always there.

**A static slot is always full**, which means it never affects whether
a station is ready. The readiness check's dispatch table gains a row
that answers "yes" without looking at anything.

**Claiming from a static does not consume it.** A ring buffer pop
advances the head; a static read leaves the entry exactly as it was.
This is the whole reason the kind exists.

**The statics table** is a numbered list of values, loaded from the map
file's `statics` section and held in RAM for the life of the program.

**The table carries no types.** A slot that references an entry knows
what type it is, because the box function's parameter at that position
says so and the registry knows what that is. The text is read into
bytes when a slot claims it. This is the same rule that keeps types out
of the wiring: one source of truth, and it is the C. If the table
declared `int` where the box wanted `float`, the file would be the one
that was wrong, and nothing would be enforcing it.

A consequence: two slots of different types may reference the same
entry and each read it their own way.

**Statics are slightly discouraged.** They are how a comparator gets a
fixed threshold and how a read box gets a path, and those are the
cases they are for. A map that carries a great deal of its behavior in
constants is a map whose behavior is not visible in its wiring.

## Suggested implementation steps

1. Add the statics table to the loaded map — a numbered array of
   values, each with its size.
2. Populate the static row of the readiness dispatch table: always
   filled, and read without consuming.
3. Reading a primitive or a string from text into bytes. Structs are
   issue 402.
4. Extend the map construction calls from issue 207 to place a static
   slot, so phase 4 is testable before the file format exists.
5. A test that a station with one buffer input and one static runs
   repeatedly, driven only by the buffer, with the static value
   arriving identically every time.
6. A test that a station whose slots are all static never becomes ready
   by delivery — nothing can write to it — which is the property issue
   403 depends on.

## Related

- [002 — Stations and slots](../docs/002-stations-and-slots.md)
- [008 — Map file format](../docs/008-map-file-format.md)
- Issue 402 — struct constants
- Issue 405 — altering the table while the program runs
