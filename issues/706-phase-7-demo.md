# 706 — Phase 7 demo: watching it think

## Current behavior

Phase 6's demo shows one binary running three different programs from
three text files. What happens inside a running map is still only
visible in numbers printed after the fact.

## Intended behavior

The demo about legibility. Every earlier phase demo reported what
happened; this one shows it happening, and then changes it without
stopping.

It should make one thing obvious: **the engine has no hidden state.**
Everything it is doing can be seen, and most of it can be altered.

**What it should show, in order of how convincing it is:**

**A live map.** The loaded graph drawn from the station table — not the
file — with buffer depths, per-station run counts, and which stations
are executing right now, redrawn continuously. Feed it enough work to
saturate the machine and watch where the values pool up.

**Rewiring under load.** With values flowing, change a wire. The
picture changes, the behaviour changes, nothing is lost, nothing
restarts. Then dump the map and show the dump reflects the new shape
rather than the file on disk.

**A bottleneck found rather than guessed.** Build a map with a
deliberate contention problem, then find it using only the reports:
mutex wait time pointing at the station, buffer growth pointing at the
slot behind it. Fix it by rewiring live and show both numbers fall.

**The round trip.** Load a map, dump it, load the dump, dump again,
show the two dumps identical. Quiet, and it is the proof that the
picture and the engine agree.

**A refused rewire.** Attempt an edit that would create a gather cycle
and show it refused with both stations named — the same check from
issue 404, now protecting a running program instead of a loading one.

**The instrumentation's own cost.** The same map run with statistics
compiled in and compiled out. Report both throughputs. A measurement
apparatus whose cost is unmeasured is a rumour.

**Everything, together, one last time.** Every phase's machinery in one
running map, with the occupancy figure reported alongside the six
earlier demos' numbers, so the whole sequence can be read as one line.

## Suggested implementation steps

1. Drive the live view from the station table, continuously redrawn.
2. Make the rewiring interactive if the terminal allows it; scripted
   with visible pauses if not.
3. Report every number by measuring it.
4. Write results to `tmp/shared-memory/` alongside the screen.
5. Confirm the root launcher finds it.

## Related

- Issues 701 through 705 — everything being demonstrated
- Issue 606 — the phase 6 demo this builds on
