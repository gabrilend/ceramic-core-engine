# 251c — Schedule functions and number waves

## Status

open · sub of 251 (timer box: tick-tock emitter) · design
draft. The central question — what a "number wave" is — was
answered on 2026-08-03 and the definition now has its own
section below. Six open questions remain, and the load-bearing
one is now the cycle-length explosion rather than the
definition.

## Current behavior

Every timing shape the runtime can express is "every N
milliseconds." Issue 251's timer re-arms at `now + rate_ms`;
issue 251a's counter divides elapsed time by a fixed step;
issue 251b's expectations take a single duration. One period,
constant, per box.

Everything else a schedule might be — back off after each
failure, fire on the third beat of every fourth bar, coincide
with another schedule twice a minute, thin out as the day goes
on — has to be built by hand out of counters and comparators,
and each one is a small pile of boxes that says nothing about
its own intent.

## Intended behavior

A schedule stops being a number and becomes a **function**. A
schedule function takes an index and returns the time of that
firing:

```
t(n) -> the time of the n-th event
```

A constant period is the degenerate case, `t(n) = origin +
n·p`. Everything else in this issue is a different function
with the same signature, which means one mechanism carries all
of them: the deadline queue holds a schedule and an index
rather than a list of times, asks the function for the next
one when it needs it, and caches a window of upcoming values so
the common path is a lookup rather than an evaluation.

The request's phrasing is the goal: *as many recurring
schedules as we can.*

## The generator library

Each entry is a function from index to time, plus whatever
parameters it takes. This list is a starting inventory, not a
closed set — the point of a function-shaped schedule is that
the list grows without the mechanism changing.

| Family | Form | Expresses |
|--------|------|-----------|
| Periodic | `origin + n·p` | a plain heartbeat |
| Phased | `origin + φ + n·p` | the same beat, offset — two of these make a polyrhythm |
| Geometric | `origin + p·rⁿ` | exponential backoff, decay |
| Arithmetic ramp | `origin + p·n(n+1)/2` | something that slows steadily |
| Harmonic | `origin + p·Hₙ` | something that slows logarithmically |
| Integer sequence | gaps from Fibonacci, primes, triangular numbers | irregular but deterministic spacing |
| Euclidean | k pulses distributed as evenly as possible over m steps | the rhythm families most traditional music uses; two integers generate a startling number of distinct patterns |
| Number wave | one wave per prime `p`, crossing at every multiple of `p`; the state at a tick is a bitflag word over the family | coincidence, vacancy, and "which of my schedules fire now" as one integer — see the section below |
| Interference | the coincidences of two or more periodic schedules | beats, alignment, "every time these two line up" |
| Union | the merge of several schedules | one queue entry for a composite pattern |
| Threshold-crossing | sample a continuous `f(t)` and fire where it crosses a level | a schedule shaped like a waveform rather than a list |
| Calendar | anchored to wall-clock structure — the hour, the day | the cron case |

The last three are the ones that most need the open question
below resolved before they can be specified.

## Interference is where "measuring" enters

The request says these should be *useful for measuring
recurring schedules*, and that word points somewhere different
from generating.

Two periodic schedules with periods p and q coincide every
lcm(p, q), and between coincidences they drift through a
repeating pattern of offsets. That pattern is a real object —
it has a period, a shape, and a phase — and it is the thing you
would want to measure if you were asking "how do these two
recurrences relate?"

Which suggests the box family has two directions, not one:

- **Generate** — given a formula, produce the times.
- **Detect** — given observed times, find the formula. Given a
  stream of events, report the period that best explains them,
  the phase, the confidence, and the residual. This is
  autocorrelation, and on a stream of counts it is cheap.

The second is more interesting and more speculative, and it is
a separate box: something that watches a wire and reports what
rhythm it is seeing. A map could then feed its own observed
behaviour back into its own scheduling, which is the kind of
loop this project keeps being built to allow.

Whether both directions are in scope is one of the questions
below.

## Caching

A schedule function is evaluated lazily and cached forward:

- The deadline queue holds `(schedule, next_index)` rather than
  a list of times, so a schedule with a thousand upcoming
  firings occupies one entry.
- A small ring caches the next K evaluated times, refilled when
  it drains, so the idle worker's peek at the earliest deadline
  is a read rather than a call into a generator.
- Because a schedule is a pure function of its index and its
  parameters, the cache never needs invalidating except when
  the parameters change — and when they do, the whole cache is
  simply dropped and refilled. No incremental update, no stale
  entry, no partial state.

Purity is what makes this simple, and it is worth defending: a
schedule function that reads a clock, or that depends on
whether the previous firing actually happened, loses all of
the above. If a schedule needs to react to what happened, that
reaction belongs in a wire, not inside the function.

## Number waves

Defined 2026-08-03, in the requester's words: *periodic
functions that pass through prime numbers. There's one for 2
which crosses the Y axis at 2, 4, 6, 8, and one for 3 which
crosses at 3, 6, 9, 12. The idea is to use them to find vacant
spots, and to use those slots like bitflags.*

So: one wave per prime `p`, crossing at every multiple of `p`.
The wave family is indexed by the primes, and at any integer
`n` the interesting object is not one wave but the **set of
waves crossing there**.

### The vacancies are the primes

An integer crossed by no wave of smaller period is divisible by
nothing smaller than itself. The vacant spots are exactly the
primes — this is the sieve of Eratosthenes, read as
interference rather than as crossing-out.

The family therefore **generates itself**: every vacancy found
is the period of the next wave. There is no separate list of
primes to be supplied from outside; the structure produces its
own indices as it runs. That is a pleasant property for a
runtime to have, and it means a schedule library seeded with
nothing but the number 2 can extend itself indefinitely.

### The bitflags

At tick `n`, with `k` waves in play, the state is a `k`-bit
word — bit `i` set when `pᵢ` divides `n`. That word is the
thing the request reaches for, and it does three jobs at once:

- **It is the schedule signature.** Which of the k periodic
  schedules fire at this tick is one word, tested with one
  mask.
- **Coincidence is popcount.** Two schedules aligning is two
  bits set. "Every time these two line up" is a mask compare,
  not a search.
- **Vacancy is zero.** A tick no schedule claims is a word of
  all zeros — a free slot, available to be allocated to
  something new without colliding with anything already
  running.

### Why primes, and not any set of periods

This is the part worth writing down, because it is the
justification for the whole construction and it is not obvious.

Distinct primes are pairwise coprime, and three things follow:

1. **The combined pattern's period is the product.** With
   periods `p₁ … p_k`, the word sequence repeats after
   `∏pᵢ` — the longest cycle obtainable from k waves. Any
   shared factor would shorten it.
2. **Every combination occurs, exactly once per cycle.** By the
   Chinese Remainder Theorem the tuple of residues
   `(n mod p₁, …, n mod p_k)` determines `n` uniquely within one
   cycle. So **k prime waves make time into a k-dimensional
   coordinate space**, and the bitflag word is the degenerate
   read of that coordinate — which axes are at zero. Nothing in
   the schedule space is unreachable, and the count of ticks
   carrying a given word is a closed form: the product of 1 for
   each set bit and `pᵢ − 1` for each clear one.
3. **Composite periods carry no new information.** A wave of
   period 6 crosses exactly where the 2-wave and the 3-wave
   coincide. It is not a new wave; it is a mask over two
   existing ones. The prime waves are a **basis**, and every
   periodic schedule is a region in the space they span.

Point 3 is the answer to "how does this connect to
scheduling." A schedule expressed as a period and a phase is a
residue class, a residue class is a region in the coordinate
space, and the bitflag word is how you test membership in a
whole family of them at once, per tick, with one integer.

### Where the construction hurts

`∏pᵢ` explodes. The first six primes give 30030 ticks — a
table small enough to precompute and hold. The first ten give
6,469,693,230, which is not a table.

So the cache described below is a **window**, not a full
period, and the closed-form question "does wave i cross tick
n" (`n mod pᵢ == 0`) stays available for any tick regardless of
how far out it is. The table is an optimisation over a
near-term horizon; it is never the definition. A design that
required materialising the full cycle would work beautifully in
a demo with three waves and fail on the seventh.

### What this does not settle

The sieve reading gives primes as the vacancies, and a real map
will not necessarily have prime-numbered schedules — it will
have whatever periods the user's work actually has. When those
periods are not coprime the clean theory degrades into a
general covering-system question: the cycle is the least common
multiple rather than the product, some residue combinations
never occur, and the vacancy structure has to be computed
rather than reasoned about. Both cases should work; only the
prime case is elegant, and the implementation should not
quietly assume it.

## Open questions

1. **How wide is the bitflag word, and what happens past it?**
   A 64-bit word holds 64 waves, which is plenty of schedules
   and nowhere near enough primes to reach interesting
   vacancies — the 64th prime is 311, and sieving to find
   primes this way needs every wave below the square root. If
   the word is a schedule signature it is small and fixed; if
   it is a sieve it is a growable bitset. These are two
   different data structures and possibly two different
   features wearing one name.
2. **Is the vacancy hunt a scheduling feature or a number-
   theory toy?** Finding free slots among the user's actual
   schedules is directly useful. Finding primes is the same
   mechanism pointed at a different question, and it is not
   obvious the runtime should ship it — though it is very
   nearly free once the waves exist, and it is the thing that
   makes the family self-generating.
3. **Is detection in scope**, or only generation?
4. **Is a schedule a box, or a field on the timer box?** A
   `schedule` field on 251's timer keeps the box count down. A
   separate schedule box lets one schedule drive several
   consumers and be inspected on its own.
5. **Are schedules composable as wires?** Union and
   interference take schedules as inputs, which means a
   schedule would have to be a value that travels on a wire —
   a much bigger idea than a field, and the one that would make
   this family genuinely expressive.
6. **What is the cache window K?** Big enough that evaluation
   is rare, small enough that a schedule change does not
   discard much work. The number-wave section's cycle-length
   explosion is the hard constraint on this one.
7. **Do schedules run on counts or on milliseconds?** Counts
   are reproducible and make timed maps testable (issue 251a);
   milliseconds are what a calendar schedule needs. Probably
   both, explicitly, never mixed silently. Number waves are
   defined on integers and therefore want counts.
8. **What happens when a schedule falls behind?** A firing
   whose time has already passed when the queue gets to it:
   fire immediately, fire once and skip the rest, or fire every
   missed one in a burst. Issue 251's drift question (Q2) is
   the same question and the two should get one answer.

## Suggested implementation steps

1. **The schedule-function interface** — index in, time out,
   parameters in a small struct, purity enforced by the
   interface taking no clock.
2. **Periodic and phased**, which reproduce existing behaviour
   and prove the mechanism against something already known to
   work.
3. **The cache ring and the deadline-queue integration**, so
   one entry covers a whole schedule.
4. **The number-wave family**, in the order the theory
   suggests: one wave as `n mod p == 0`; then the bitflag word
   over k waves; then the vacancy test as a zero word; then the
   self-generating extension where a vacancy becomes the next
   wave. Each step is a few lines and each is separately
   testable against a table of known primes, which makes this
   the best-tested thing in the timing family almost by
   accident.
5. **Geometric and Euclidean**, the two families that carry the
   most expressiveness per line of code.
6. **Interference and union**, which need question 5 resolved.
7. **Detection**, if it is in scope, as a separate box with its
   own issue.

## Relevant files

- `libs/task-pool/pool.{c,h}` — the deadline queue that would
  hold schedules rather than times
- issue 251 (timer box: tick-tock emitter) — the drift and
  catch-up question this shares
- issue 251a (counter box) — the count-space these schedules
  can be expressed in, and the reproducibility that buys
- issue 251b (alarms and expectations) — the high-volume arming
  case that cached schedules exist for
