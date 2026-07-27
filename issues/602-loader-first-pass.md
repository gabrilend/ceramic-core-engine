# 602 — The loader, first pass: stations

## Current behavior

A map file can be read into a description (issue 601), but nothing
turns that description into stations.

## Intended behavior

The first of two passes. Declaration order in a map file must not
matter — an arrow may point at a station declared further down — which
is why creation and connection are separated.

**For each station line:**

1. **Look the box function up in the registry.** That gives the shim
   pointer, the parameter count, and each parameter's type and size. A
   name not in the registry stops the load and says so; this is the
   most common error a map will have and its message should be the best
   one in the program.
2. **Allocate the slots array**, one ring-buffer slot per parameter —
   the default — each sized exactly `sizeof` its parameter. A
   comparator gets one extra slot on the end, typed to match the box's
   return value.
3. **Record the name** in a lookup table used by the second pass and
   discarded when loading ends.
4. **Apply the input lines**, converting named slots from ring buffers
   to statics or gatherers. A line naming a slot the function does not
   have stops the load.

**The statics table is filled here**, since a static slot needs its
entry to exist. Each value is read from text into bytes using the type
of the slot claiming it — issue 402's reader, walking issue 304's field
tables. The table itself never declares a type.

**This pass takes over from issue 207's construction calls.** They stop
being something a person calls and become what the loader calls. The
scaffolding was deliberately kept irritating so that this moment would
arrive rather than being indefinitely postponed; issue 207's
current-behavior section should be updated to say it has.

## Suggested implementation steps

1. Allocate the station table sized to the description's station count.
2. The per-station creation loop as described.
3. The name lookup table, freed at the end of loading.
4. The statics table fill.
5. Error messages naming the station and the box.
6. A test that a map with a forward reference loads — the reason there
   are two passes at all.
7. A test that each first-pass error stops the load with a message
   naming the right station.

## Related

- [009 — Loading](../docs/009-datapath-load.md)
- Issue 601 — the description this consumes
- Issue 603 — the second pass
- Issue 207 — the scaffolding this replaces
