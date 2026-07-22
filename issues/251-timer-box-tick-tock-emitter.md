# 251 — Timer box: tick-tock emitter

## Status
open · design draft; the load-bearing open question is how to
schedule the inter-tick wait without spinning, without locking
a worker in a sleep, and without a dedicated timer thread

## Current behavior

Every existing box kind — call, read, write, map, and the routing
variants — fires only when data arrives: the dispatch queues a
task once a box's required input slots are filled, and nothing in
the runtime schedules work from the wall clock. A pool worker that
finds the queue empty blocks indefinitely on a condition variable
until new work is signalled; a run either drains to quiescence and
exits, or loops forever through a legal iterator cycle. There is
no timer kind in the schema, no deadline queue in the pool, and no
way to make a map tick at a fixed rate — the runtime documentation
already names this box as a design draft, not yet built.

## Intended behavior

A new timer box kind emits the string "true" on its single output
once per configured interval. The rate arrives either as a literal
number of milliseconds or on an optional input wire; each fire
pushes the tick downstream and re-arms itself at now plus the
rate, a rate of zero stops the loop, and a fresh rate restarts it.
The loader rejects a timer with neither a literal rate nor a wire
feeding one. The inter-tick wait must not spin, must not lock a
worker in a sleep, and must not add a dedicated timer thread — the
recommended shape gives the pool's existing idle wait a deadline
(a timed condition wait) fed by a small priority queue of pending
timer respawns.

## Concept

A new box kind, **`timer`**, that emits a single boolean `true`
on its output once per "tick-tock rate." The rate is set either
as a literal in the editor (a number of milliseconds) or as an
input value arriving on a wire. The timer's body is small: each
fire emits `true` and re-arms itself; setting the rate to zero
stops the chain; pushing a fresh rate restarts it.

The compelling shape is the **fire-then-respawn loop**. The
timer box's invoke does almost nothing — push one `true`
downstream, request a re-spawn at `now + rate_ms`. The runtime
then has to deliver that re-spawn at the right time without
burning a worker waiting for it.

## Concrete behavior

### Schema

```json
{
  "id":   "every_second",
  "kind": "timer",
  "rate_ms": 1000,
  "inputs": [
    { "name": "rate_ms", "type": "number", "optional": true }
  ],
  "connections": [
    { "to_box": "consumer", "to_input": "tick" }
  ]
}
```

- **`rate_ms`** literal field: the default tick interval in
  milliseconds. May be absent if a wire-fed `rate_ms` input is
  present.
- **`rate_ms`** input port: optional; when present and a value
  arrives, that value overrides the literal and re-arms the
  loop. Sending `0` stops the loop; sending a positive number
  restarts (or speeds up / slows down) it.

### Compile-time check

The graph loader rejects a timer box that has **neither** a
literal `rate_ms` **nor** a wire feeding its `rate_ms` input.
A pure unwired timer has no way to ever start; failing loud at
load is the project's preferred shape (CLAUDE.md "errors over
fallbacks").

### Output

Single output port, plain routing. Emits the string `"true"`
once per tick. Same shape as a write box's success boolean —
downstream consumers can treat it as a clock event without
caring about the bytes.

### Dispatch behavior

1. On graph load, if the timer has a literal `rate_ms`, the
   loader queues the first tick at `now + rate_ms` (or
   immediately at `now` — see open questions).
2. On task fire, the timer's body:
   a. Pushes `"true"` to every outgoing connection.
   b. Reads the current effective rate (literal, possibly
      overridden by a recently-arrived input value).
   c. If `rate == 0`: stop. Do not re-arm.
   d. If `rate > 0`: schedule a respawn at `now + rate_ms`.
3. When the `rate_ms` input slot receives a new value, the
   value is captured into the timer box's "current rate" cell
   AND the timer is re-added to the task queue immediately (so
   a stopped timer can be restarted; a running timer's next
   fire uses the new rate).

The "input arrival schedules a task" hook is the same one every
other box uses — the dispatch's `spawn_if_ready` on a push. The
timer box's first task on graph load is queued by the loader;
all subsequent tasks come from the fire-and-respawn loop.

## Open questions

### Q1 — How to wait for the next tick (the load-bearing one)

The user's constraints:
- No endless spinning with an otherwise-empty queue.
- No locking a pool worker in a sleep.
- No dedicated timer thread running in the background.

Four candidate shapes, with the trade-offs as I see them:

**Option A — `pthread_cond_timedwait` on the existing pool
idle-wait.** The pool's worker threads already block on a
condvar when no work is queued. Today that wait is
"infinite-until-signalled." We extend it to "wait until either
new work arrives OR the next-scheduled-timer's deadline."

Implementation:
- The dispatch maintains a small priority queue of
  `(deadline, box_idx)` pairs — the pending timer respawns.
- When the work queue is empty, the worker about to sleep peeks
  the timer queue's head (the earliest deadline).
- `pthread_cond_timedwait(condvar, mutex, earliest_deadline)`
  blocks until the deadline or until another worker signals
  new work.
- On wake, the worker checks the timer queue for any deadlines
  that have passed and pushes those box_idx values onto the
  task queue, then proceeds with its normal pull.

This satisfies every constraint:
- No spin: the worker is in a blocking system call.
- No locked-in-sleep worker: the worker that's waiting is
  ALREADY the idle worker the pool always has. We're not adding
  a wait, we're tightening the existing one's timeout.
- No dedicated timer thread: any idle worker handles it,
  whichever one happens to be in the idle slot.

The "one worker is in cond_timedwait" claim is the subtle one
— is that "locking a thread in a sleep"? My read: no, because
that worker would be in `pthread_cond_wait` anyway (idle pool),
and a worker waiting on the work-queue condvar IS the pool's
correct idle behavior. We're not adding a thread; we're adding
a deadline to the wait it already does. If a new task arrives
before the deadline, the condvar signal wakes it just as today.

**Option B — `timerfd` + `epoll` integration.** Linux's
`timerfd_create` gives each timer a file descriptor that
becomes readable after a specified timeout. The pool grows an
epoll set; the idle worker calls `epoll_wait` with the same
deadline as Option A, but the kernel handles the timer
expiration via the timerfd's readability.

More machinery, but two real advantages:
- Multiple timers coalesce in one epoll_wait — the kernel does
  the "earliest deadline" pick for free.
- Generalises naturally to any future async-I/O box (a network
  socket box, a file-watcher box) — the epoll set is the right
  pattern for those too.

Cost: Linux-specific (kqueue equivalent on BSD/macOS); slightly
more code than Option A.

**Option C — Pure YARQ requeue at scheduler frequency.** The
timer box's fire always re-queues itself immediately, and on
re-pull the worker checks if `now >= deadline`. If not, YARQ
(per issue 320's primitive) puts it back at the tail. Workers
keep round-robining through the empty queue until enough
real-time has passed.

Satisfies the no-thread / no-sleep constraints but VIOLATES the
no-spinning one: an otherwise-empty queue with one pending
timer becomes a CPU-burning loop of YARQ requeue cycles.
Rejected unless there's a way to gate the requeue rate.

**Option D — Hybrid: timer wheel inside the dispatch.** A small
data structure (hashed wheel, sorted list, whatever) holds
pending timer deadlines. Same idle-wait extension as Option A,
but the data structure is built-in to the dispatch rather than
piggy-backing on the pool's condvar via priority queue.

Effectively the same as Option A; the question is just whether
the structure lives in the pool layer or the dispatch layer.
The pool layer feels right — the pool already owns the idle
condvar.

**Recommendation:** Option A is the simplest shape that
satisfies every constraint, and the work is bounded:
- One small priority queue inside the pool (or dispatch).
- One `pthread_cond_timedwait` extension to the worker's idle
  loop.
- A `pool_schedule_at(deadline, box_idx)` API.

Option B is the right shape if we ever build network / file
boxes that need async I/O; defer until that drives the
decision.

### Q2 — Drift accumulation

A timer with rate 1000ms takes 5ms inside the dispatch to fire
and re-arm. Does the next deadline measure from the START of
the previous fire (drift-free, periodic) or from the END
(drift accumulates, but each interval is honest)?

- **Periodic** (`next_deadline = previous_deadline + rate_ms`):
  predictable cadence; if a fire takes longer than the rate
  (system load, slow downstream consumers blocking the push),
  the timer "catches up" by firing immediately on next idle —
  potentially producing back-to-back fires.
- **Interval** (`next_deadline = now + rate_ms`): no catch-up;
  the timer's actual rate slows under load.

The standard `setitimer` / Go ticker pattern is periodic. I'd
default to periodic; document the catch-up behaviour.

### Q3 — Rate changes mid-run

When a new `rate_ms` input value arrives, three behaviours are
possible:
- **Re-arm from now.** The currently-scheduled tick (if any)
  gets cancelled; the next tick is `now + new_rate_ms`.
- **Apply to NEXT cycle.** The currently-scheduled tick still
  fires on its original deadline; subsequent ticks use the new
  rate.
- **Re-arm preserving phase.** The next tick's deadline gets
  adjusted to match the new period offset.

Option 1 (re-arm from now) is the simplest and matches "input
arrival schedules a task" — the input arrival immediately
fires the timer with the new rate, then the fire body schedules
the next at `now + new_rate_ms`. Default to this.

### Q4 — Stopping cleanly

When `rate_ms = 0` arrives, the next fire's body sees
`rate == 0` and skips the re-arm. But:
- Should any in-flight scheduled deadline for this box be
  cancelled, or does the next pull just become a no-op?
- Does the timer emit one final `true` when stopping, or skip
  the push too?

Lean toward: the value=0 push triggers one final fire (so the
downstream sees the "I'm stopping" event), and the re-arm step
is the explicit skip. The scheduled-deadline-cancellation
question is moot if Option 1 above is chosen (the input arrival
cancels the scheduled deadline as part of re-arm).

### Q5 — Editor surface

- Number input for `rate_ms` literal.
- The `rate_ms` input port renders as a normal input dot
  (wires connectable; if a literal is set the dot becomes the
  nodule per issue 239).
- Header colour: needs a fresh entry in `KIND_COLOR` — yellow
  or amber reads as "active / pulsing" without colliding with
  call (blue), read (green), write (cheddar), or map (violet).
- A small "•" badge that pulses in the editor canvas at the
  configured rate is a fun stretch goal — not blocking.

### Q6 — Interaction with quiescence

A timer that keeps re-arming prevents the pool from ever
reaching quiescence (no tasks queued, no in-flight work). Two
options:
- **Timer prevents quiescence.** A long-running map with a
  steady tick simply runs forever; the user terminates with
  Ctrl-C. This is the natural shape for a long-running event
  pipeline.
- **Timer counts as "background" work.** Quiescence ignores
  the pending-timer queue, so a map with only timers and
  consumers waiting on them quiesces. Wrong for a real-time
  pipeline; right for tests.

Default to the first; introduce a `background: true` flag if
the second becomes useful.

## Suggested implementation steps

1. **Schema**: `src/001-schema.lua` accepts `kind: "timer"`
   with optional numeric `rate_ms` field and the optional
   `rate_ms` input port. Validates the
   neither-literal-nor-input compile-time check.
2. **Box kind enum**: `src/010-graph-loader.h` adds
   `BOX_TIMER`; loader maps `"timer"` to it.
3. **Pool extension**: add a small priority queue of
   `(deadline, box_idx)` to the pool layer, plus a
   `pool_schedule_at(deadline, box_idx)` API.
4. **Idle-wait change**: when the pool's worker is about to
   block on the work-queue condvar, peek the timer queue and
   pass the earliest deadline as the condvar timeout. On wake,
   drain any expired timers into the work queue.
5. **Dispatch**: `do_timer_box` is tiny — push `"true"`, read
   current rate (from the literal-or-input-cached cell), call
   `pool_schedule_at` if rate > 0.
6. **Rate-input plumbing**: an arriving `rate_ms` input value
   updates a per-box atomic cell, then triggers
   `spawn_if_ready` like any other input (which queues the
   timer for immediate fire under the new rate).
7. **Loader hook**: at graph load, every timer with a literal
   `rate_ms` gets one initial `pool_schedule_at(now + rate_ms,
   timer_box_idx)` so the loop starts.
8. **Editor**: kind dropdown grows `timer`; inspector renders
   `rate_ms` number input; canvas KIND_COLOR entry for timer.
9. **Fixture**: `tests/maps/timer-tick/` — a timer at 100ms
   feeds a counter box; assert the counter sees N fires across
   a fixed test run duration (loose tolerance for scheduler
   jitter).

## Relevant files

- `src/001-schema.lua` — kind acceptance
- `src/010-graph-loader.{c,h}` — BOX_TIMER, compile-time check
- `src/012-dispatch.c` — `do_timer_box`, the fire-and-schedule body
- `libs/task-pool/pool.{c,h}` — priority queue, scheduled-deadline
  API, condvar-timeout idle wait
- `assets/js/002-boxes.js` — KIND_COLOR + KIND_HEADER for timer
- `assets/js/004-inspector.js` — `rate_ms` editor control
- `tests/maps/timer-tick/` — fixture

## Notes

- The timer box is the first one whose firing schedule isn't
  driven entirely by data arrival on its inputs. Every other
  box fires when its inputs are ready; the timer fires when
  the wall clock crosses a deadline. That's a new
  scheduling-source category for the dispatch — worth being
  explicit about so future categories (signal-driven, fd-
  readable, etc.) inherit the same machinery.
- Multiple timers in one map all coalesce naturally into the
  same priority queue. The idle wait blocks until the earliest
  deadline across all of them.
- This issue is the prerequisite for any future "every N
  minutes, run this pipeline" cron-like pattern at the SoraMech
  layer. The cron primitive is just a timer with a much larger
  rate — same mechanism.
