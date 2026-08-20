# 606 — Phase 6 demo: the capstone

> **The program this built was deleted ([713](../713-demos-you-can-steer.md)).**
> Every word below stands as the record of what the first generation of
> demos was and what it proved; what replaced it is a live control panel
> per phase rather than a paged transcript. Nothing here needs rebuilding.

## Current behavior

**Built, and three of its pieces demonstrate things that are going.**

The pushed-plus-gathered pair, the gathered addend in the
everything-map, and the refused gather cycle among the failure modes
all belong to the pull path
([056](../../docs/implementation-notes/056-no-pull-path.md)). So does
the seed being explained station by station "with the vacuously-ready
gatherer correctly skipped" — there is no seed sweep now, and what
starts a program is that construction's static writes run the ordinary
readiness check.

**The demo's spine is untouched and it is the important part.** One
binary running three programs; a chain losing a hop by one text edit
and leaving 14 instead of 28; a threshold changed from 5 to 100 and the
value landing on the other branch — with the compiler never invoked
after the first line. That is the phase's whole claim and none of it
depended on gathering.

What should replace the removed scenes is close at hand: a **static
written mid-run**, recalculating everything downstream of it without
the program restarting. It demonstrates the mechanism that replaced the
pull path, it is a text edit like the others, and it makes the same
point more strongly — the shape of a program can change while it runs,
not only between runs.

The remainder describes it as built.

Built, shell-driven, one compile at the top and never again. The
same binary runs three map files into three behaviours — a doubling
chain leaving 28, a pushed-plus-gathered pair leaving 14, a gate
routing seven high — then the chain loses a hop by one sed edit
(28 becomes 14) and the gate's threshold turns from 5 to 100 (the
seven lands low), the compiler untouched throughout. Six failure
modes are triggered deliberately and shown verbatim, including the
one that arrives as a warning before its fatal partner. The
everything-map runs all six phases' machinery at once — two seeded
sources, a comparator against a static, a gathered addend, an
iterator dealing to two writers — drawn as built from the station
table with names the loader now retains, the seed explained station
by station with the vacuously-ready gatherer correctly skipped, and
the load cost broken into parse, passes, validation, and seed.
Mirrored to the shared-memory tier.

Each scene opens with a story and reports in that story's units beside
the engine's own, under the standard issue 707 sets: a player piano
whose rolls are the music, a railway signal box where one lever
redirects every later train, a customs desk that names which line of
the form is wrong, and an orchestra's first full rehearsal. The six
deliberate failures are quoted verbatim rather than paraphrased, one
blank line apart, because a demo that retypes an error message can be
wrong about it.

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
