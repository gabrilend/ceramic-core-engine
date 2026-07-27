# 033-statics.c — the statics table, from outside

Numbered constants a slot binds instead of being fed: thresholds,
paths, configuration. Always full, never consumed, never part of
readiness.

## Functions

**map_statics_alloc(map, entry count)** — create the table and its
mutex.

**map_static_set_text(map, id, text)** — give an entry the text the
map wrote. Must precede any binding; once per entry.

**map_slot_static(map, station, slot, id)** — convert a ring slot to
a static bound to an entry. Parses the entry's text into bytes shaped
by the slot's registry type: a number for int/unsigned/float slots, a
brace walk over the generated field table for a struct slot, the
characters (claimed as a pointer to table-owned storage) for a
`const char *` slot. Fatal, naming entry and field, on any mismatch:
too many values, too few, a string where a number belongs, unknown
entry, untyped slot.

**map_static_write(map, id, bytes, size)** — alter an entry mid-run;
size-checked, mutex-held for the length of the copy so no claim ever
sees fields from two worlds.

**sora_static_write(id, bytes, size)** — same, against the active
map; the variant a box can call. A back channel around "a box cannot
remember" — treat with a global variable's suspicion.

**static_claim(map, slot, out)** — internal: the locked memcpy a
task build performs.

## The first-pass departure

Entries hold parsed bytes (shaped by the first binder), not
re-parsed text per claim; two slots may share an entry only at equal
sizes. The docs' "each slot reads the text its own way" conflicts
with byte-level runtime mutation — the report explains the choice.
