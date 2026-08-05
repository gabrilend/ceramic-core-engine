# 706 — Phase 7 demo: watching it think

## Current behavior

Built. The live view draws runs and buffer depths from the station
table while values flood in. The bottleneck scene finds the hot
station using only the engine's contention report — box time
attributed to the cruncher, the spare provably idle — and relieves
it by moving an iterator port mid-run, load visibly split with
nothing restarted; it also caught a finding the report keeps: a slow
box alone is not a bottleneck here, because the pool already runs
one station's invocations on every core, so the reports exist to
show who pays rather than to promise speedups. The dump then
disagrees with the file on disk and is right; the round trip closes
byte-identically; a refused gather cycle names both stations and the
next delivery still flows. The driving script builds the demo twice
and measures the measurement: the same workload with statistics
compiled out and in, the difference a number rather than a rumour.
Mirrored to the shared-memory tier.

Each scene opens with a story and reports in that story's units beside
the engine's own, under the standard issue 707 sets: a control room
whose gauges are wired to the machines rather than to the drawings, a
lane closed by cones, a set of as-built drawings against the
architect's plan, a translation checked by translating it back, and a
safety interlock that refuses without stopping the line. Scene six —
the thermometer that warms the room it measures — belongs to the
driving script, because neither build can measure the other. The
demo now says in advance that the loader will warn about the idle
spare station, so an expected warning does not teach a reader to
ignore warnings.

Measuring the instrumentation honestly took three attempts and the two
failures are recorded in the script. One run of each build reported the
instrumented one as faster; best-of-five run as two blocks reported the
same thing more confidently, because whichever build ran second
inherited a warm cache and a scaled-up processor. Repeating a
measurement carefully is not the same as taking it fairly. The runs now
alternate after a discarded warm-up, and the cost lands at a small
positive figure — with the script still prepared to say plainly that a
negative result means noise rather than rounding it to zero.

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
