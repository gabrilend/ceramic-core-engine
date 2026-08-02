# 251a — Counter box: fixed timestep, count, and elapsed

## Status

open · sub of 251 (timer box: tick-tock emitter) · design
draft. Does not block on 251: a counter is a pure function of
the clock and needs no scheduling machinery at all. Five open
questions.

## Current behavior

Nothing in a map can ask what time it is. The runtime reads the
clock twice — `now_secs()` for transcript timestamps and
`mono_us()` for elapsed measurements, both file-static in
`src/012-dispatch.c` and duplicated in `src/008-pool-runner.c` —
and neither is reachable from a box. A box that needs to know
how long something took must call into its own language's
clock, which makes the answer language-specific, untestable,
and different on every re-run.

There is no notion of a map having its own time base. A run's
only sense of ordering is the transcript's timestamps, which
are wall-clock and therefore differ every run, which is why
the transcript normaliser exists to strip them before any
comparison.

## Intended behavior

A new box kind — working name **`counter`** — that holds an
origin and a step duration and answers questions about time.
It is a **pull-on-demand source**, in the same category as a
`read` box: it never queues, never fires as a task, never
appears as a `task_start` in the transcript. When a consumer
fires and needs a value from it, the value is computed at that
instant and handed over.

```json
{
  "id":      "beat",
  "kind":    "counter",
  "step_ms": 250,
  "emit":    "count",
  "connections": [
    { "from_box": "beat", "to_box": "sequencer", "to_input": "n" }
  ]
}
```

## The counter does not run

This is the design's whole shape and it is worth stating before
the details.

A counter stores two numbers — an origin and a step — and
everything it can answer is a closed form over those two and
the current time:

```
count      = floor((now - origin) / step)
elapsed_ms = now - origin
phase      = (now - origin) mod step          /* 0 .. step-1 */
remaining  = step - phase
```

No task queue entry, no worker, no re-arming, no drift, no
scheduling interaction of any kind. A map with forty counters
costs exactly as much as a map with none. This is the same
property that makes read boxes free, and it is why a counter is
a different thing from the timer box in 251 rather than a
variation on it:

| | 251 timer | 251a counter |
|---|---|---|
| what it is | an event source | a value source |
| fires as a task | yes, once per tick | never |
| costs scheduling | a deadline in the pool's wait | nothing |
| answers "what time is it" | no | yes |
| makes a map turn | yes | no |

They compose: a timer makes a loop turn, and a counter tells
the boxes in that loop where they are. Neither replaces the
other.

## Read modes

One counter box answers one question, chosen by its `emit`
field, because the single-output-per-box rule means a box
cannot hand back a record of four fields on four ports. Wanting
two answers means placing two counter boxes over the same
origin and step, which is cheap precisely because counters cost
nothing.

| `emit` | Value | Type |
|--------|-------|------|
| `count` | whole steps since the origin | integer |
| `elapsed_ms` | milliseconds since the origin | integer |
| `phase_ms` | milliseconds into the current step | integer |
| `phase_unit` | position within the step, 0 to 1 | fixed-point or double |
| `remaining_ms` | milliseconds until the next step boundary | integer |
| `step_ms` | the configured step, unchanged | integer |

`phase_unit` is the one that makes a counter composable with
the `nonlinearity` routing kind and with anything expecting a
normalised input, and it is the one whose numeric type is a
real question rather than an obvious one (see open questions).

## The logical clock

A counter's `count` is a **logical clock**: an integer that
advances one per step and is a pure function of elapsed time.
Two things follow, and the second is the more valuable.

First, a map can talk about time without talking about
milliseconds. "Every fourth beat" is `count mod 4`, which a
comparator routing kind already expresses, with no new
machinery.

Second — and this is the part worth protecting — **a run driven
by counts rather than by wall time is reproducible.** If a
map's behaviour depends only on `count`, then replaying the
same count sequence reproduces the run exactly, regardless of
how the scheduler happened to interleave that day. That makes a
timed map testable, which timed things usually are not, and it
is the property that lets the transcript diff harness (issue
311) compare two runs of a map that has a clock in it.

It also means the counter is trivially the most synthesizable
box in the project: in hardware a counter *is* a counter, and
the phase-5 equivalence work (issue 511) gets an easy first
customer.

## Where the origin comes from

Three candidates, and the choice changes what `count` means:

- **Graph load.** Every counter in a map shares one origin, set
  once when the run starts. Counts across different counters
  stay in a fixed relationship, which is what makes
  polyrhythms and beat patterns work (issue 251c).
- **First pull.** The counter starts when someone first asks.
  Each counter has its own origin and they drift apart.
- **An explicit reset.** Someone sends a value and the origin
  moves to now.

Default to graph load, because a shared origin is what makes
several counters comparable, and comparability is the whole
point of having more than one. The reset case is real and is
the reason this box may need an input port after all — see the
open questions.

## Time since what

The request names "time since" as a wanted value, and a
counter's `elapsed_ms` answers it for exactly one origin: the
run's start. The more useful question — time since *that thing
happened* — needs a marker, and a marker needs the counter to
observe an event, and observing an event means having an input
port, and a box with an input port is not a pull-on-demand
source any more.

That tension is real and this issue does not resolve it. The
proposal is that plain `since-the-origin` lives here, in the
pure form, and that `since-a-marked-event` lives in 251b with
the alarms, where a box with inputs and outputs is already
required. Splitting it that way keeps the free thing free.

## Open questions

1. **Does a counter have any input ports at all?** A `reset`
   port, or a `step_ms` port so the step can change mid-run,
   would both be useful and would both break the "never
   queued, never a task" property that makes counters free.
   The read box has the same shape and solved it by not having
   inputs; the honest options are to follow that, or to accept
   that a counter is a hybrid and say precisely what a pull
   costs when the box also has state.
2. **What is `phase_unit`'s type?** A `double` matches the
   `nonlinearity` kind's current arithmetic; a fixed-point
   value matches where phase 5 is pushing that arithmetic
   (issue 507). Choosing `double` now means changing it later.
3. **Which clock?** `CLOCK_MONOTONIC` cannot go backwards and
   is immune to the system clock being set, which is correct
   for measuring durations. `CLOCK_REALTIME` is what a person
   means by "what time is it" and is what a schedule anchored
   to a wall-clock hour needs. A counter probably wants
   monotonic; a cron-like schedule (issue 251c) probably wants
   realtime, and they must not be silently mixed.
4. **Integer width and overflow.** `elapsed_ms` in a 32-bit
   integer wraps after 49 days. A long-running map is an
   intended use. State the width, state the wrap behaviour,
   and do not discover it.
5. **Does a counter appear in the transcript?** It fires no
   tasks, so it produces no events, and a map whose behaviour
   depends on time would then have no record of what time it
   thought it was. A `pull` event with the value would make
   timed runs debuggable and would add an event per pull to
   every transcript.

## Suggested implementation steps

1. **A shared monotonic time source first.** `mono_us()` and
   `now_secs()` are currently duplicated as file-statics in
   the dispatch and the pool runner. One time module, one
   implementation, used by the transcript, the pool, and the
   counter. This is a small cleanup that this box makes
   necessary rather than merely tidy.
2. **Schema**: `src/001-schema.lua` accepts `kind: "counter"`
   with `step_ms` and `emit`, rejecting an unknown `emit` and a
   non-positive `step_ms`.
3. **Loader**: `BOX_COUNTER` in the box-kind enum, the fields
   parsed, and the same
   "must-be-pulled-by-someone" validation a read box gets.
4. **The pull path**: wherever the dispatch resolves a read
   box's bytes for a consumer's empty port, a counter resolves
   the same way, computing its value at that instant.
5. **Editor**: kind dropdown, a `KIND_COLOR` entry, a step
   input, and an `emit` selector in the inspector.
6. **Fixture**: a map where a counter feeds a comparator and
   the branch taken changes with elapsed time; asserted with a
   tolerance on wall time but exactly on count.

## Relevant files

- `src/012-dispatch.c` — `now_secs()` / `mono_us()`, and the
  read-box pull path a counter joins
- `src/008-pool-runner.c` — the second copy of the clock
  helpers
- `src/010-graph-loader.{c,h}` — the box-kind enum and field
  parsing
- `src/001-schema.lua` — kind acceptance
- `assets/js/002-boxes.js`, `assets/js/004-inspector.js` —
  editor surface
- issue 244 (data box pull on demand) — the precedent for a box
  that answers rather than produces
- issue 251 (timer box: tick-tock emitter) — the sibling that
  makes maps turn
- issue 251b (alarms and expectations) — where markers and
  deadlines live
- issue 251c (schedule functions and number waves) — the
  consumer that turns a count into a schedule
