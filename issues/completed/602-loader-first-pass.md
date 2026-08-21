# 602 — The loader, first pass: stations

## Current behavior

**Built, and becoming a caller rather than a mechanism.**

Under [212](../212-one-way-to-build-a-program.md) there is one surface
for creating a station, configuring a port, and drawing a wire, legal
at any moment — and loading a file is its first caller rather than a
construction path of its own. So this pass survives as *turn each
station line into one call*, and the concept it was built around
disappears: there is no "still loading" state, because the table is no
longer sized before the stations exist.

Two of its details go with that. Ports no longer convert to
**gatherers** — that kind is gone
([056](../../docs/implementation-notes/056-no-pull-path.md)) — and the
name table no longer dies when loading ends, because there is no end;
names live on the program, which the dump already forced.

What this issue got right and keeps: **the registry supplies everything
about a station and the file supplies nothing about it but a name.**
Shim, parameter count, sizes, type names — all from the box record, so
the file cannot disagree with the C. That is why placement by name is
the only placement worth having, and why hand placement is being
retired rather than fixed.

The remainder describes it as built.

Built. Every station line becomes a station through placement by
name: the registry supplies shim, parameter count, sizes, and type
names; ports default to ring buffers; a comparator grows its typed
threshold port; the statics table fills first so bindings can parse
their entries. The misspelled-box message names the name and where
box sources live, and every input line naming a port beyond the box
stops the load saying how many ports exist and why. The name table
lives in file order and dies when loading ends.

One refinement of this issue's split, recorded in the first-pass
report: static input lines apply here, but gather input lines wait
for the second pass — their source is a *name*, and a name may
belong to a station declared further down, so the first pass cannot
resolve them by construction. The construction calls did become what
the loader calls rather than what a person calls, and issue 207's
record was updated when that moment arrived. Proven by the
forward-reference map and the port-range and misspelling refusals.

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
2. **Allocate the ports array**, one ring-buffer port per parameter —
   the default — each sized exactly `sizeof` its parameter. A
   comparator gets one extra port on the end, typed to match the box's
   return value.
3. **Record the name** in a lookup table used by the second pass and
   discarded when loading ends.
4. **Apply the input lines**, converting named ports from ring buffers
   to statics or gatherers. A line naming a port the function does not
   have stops the load.

**The statics table is filled here**, since a static port needs its
entry to exist. Each value is read from text into bytes using the type
of the port claiming it — issue 402's reader, walking issue 304's field
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

- [009 — Loading](../../docs/009-datapath-load.md)
- Issue 601 — the description this consumes
- Issue 603 — the second pass
- Issue 207 — the scaffolding this replaces
