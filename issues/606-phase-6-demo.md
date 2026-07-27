# 606 — Phase 6 demo: the capstone

## Current behavior

Phase 5's demo shows a map that decides. Every map so far has been
built in C, which means the shape of a program and the code of a
program have been the same artifact.

## Intended behavior

The demo the whole project has been building toward. It should make one
thing obvious: **a program is a directory of C functions and a text
file, and changing the shape of the program does not touch the C.**

**What it should show, in order of how convincing it is:**

**The same binary, three different programs.** One compiled executable,
three map files, three genuinely different behaviours. Nothing
recompiled between them. This is the claim of the entire project and it
should be the first thing on screen.

**A map edited between runs.** Change a wire in a text file, rerun, and
show the behaviour follow. Then change a threshold in the statics table
and rerun. The compiler is never invoked.

**Every error message, deliberately triggered.** A misspelled box name,
a type mismatch on a wire, an arrow to a station that does not exist, a
gather cycle, a comparator missing its threshold, a map with nothing to
seed. Show each message. This is the part worth being proud of — the
failure modes are the surface a person actually touches, and a demo
that only shows success is hiding most of the product.

**The seed, explained by counting.** Report which stations were seeded
and why each qualified, alongside the ones that did not and why not.
The gatherer that passes its readiness check and is still correctly
skipped is the subtle one.

**A load-time cost breakdown.** Parse, first pass, second pass,
validation, seed — time each. This is the number someone will ask about
when a map gets large, and having measured it once is worth more than
speculating later.

**Everything from every previous phase, running together.** Pool,
stations, generator, gatherers, statics, comparators, iterators — one
map using all of it, loaded from a file. Report the occupancy figure
one last time, so all six demos read against each other.

**A visual.** The loaded map drawn from the in-memory station table
rather than from the file — stations, arrows, live buffer depths — so
what is displayed is what the engine actually built, not what the file
said. If those two ever disagree, this is what shows it.

## Suggested implementation steps

1. Write the three map files as demo assets, kept alongside the demo.
2. Drive the error walkthrough from a shell script so each failure is
   visibly a separate run.
3. Report every number by measuring it.
4. Draw the map from the loaded table, never from the file text.
5. Write results to `tmp/shared-memory/` alongside the screen.
6. Confirm the root launcher finds it.

## Related

- [008 — Map file format](../docs/008-map-file-format.md)
- [009 — Loading](../docs/009-datapath-load.md)
- Issues 601 through 605 — everything being demonstrated
- Issue 505 — the phase 5 demo this builds on
