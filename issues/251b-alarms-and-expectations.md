# 251b — Alarms and expectations

## Status

open · sub of 251 (timer box: tick-tock emitter) · design
draft. Depends on 251 for the pool's deadline wait — an alarm
is a scheduled wake-up and needs the same machinery a timer
needs. Six open questions.

## Current behavior

A map cannot wait for something and notice that it did not
arrive. Every box fires when its inputs are ready; a box whose
input never arrives simply never fires, silently, forever. The
run either drains to quiescence with that box having done
nothing, or loops on without it. Nothing distinguishes "this
value has not arrived yet" from "this value is never going to
arrive," and nothing in the transcript records the difference,
because a non-event writes no line.

That is the single most common shape of a bug in a real
pipeline — a stage that quietly never ran — and the runtime
currently has no vocabulary for it.

## Intended behavior

Two related box kinds, sharing one deadline mechanism.

**An alarm** is a deadline that fires. Armed with a duration or
an absolute count, it emits when the deadline is reached and
then either stops or re-arms.

**An expectation** is a deadline with an event racing it. It
watches an input port and one of two things happens:

- the value arrives before the deadline — the expectation
  emits on its `met` branch, carrying how long it took;
- the deadline passes first — it emits on its `missed` branch,
  carrying the deadline it blew.

Either way something is emitted, something is recorded in the
transcript, and a downstream box can respond. The silent
non-event becomes a routed event, which is what makes it
debuggable.

```json
{
  "id":       "await_response",
  "kind":     "expectation",
  "within_ms": 5000,
  "inputs": [
    { "name": "value", "type": "string" }
  ],
  "routing": { "kind": "expectation" },
  "connections": [
    { "from_box": "await_response", "from_branch": "met",
      "to_box": "handle", "to_input": "value" },
    { "from_box": "await_response", "from_branch": "missed",
      "to_box": "retry", "to_input": "trigger" }
  ]
}
```

## Why the expectation is the interesting half

An alarm alone is a timer that fires once, and 251 already
builds most of it. The expectation is the thing that does not
exist anywhere in the runtime today, and it is worth being
explicit about what it changes.

The map model's whole discipline is that a box fires when its
inputs are ready. That rule has no timeout in it, and adding
one anywhere general would be wrong — a box that gave up on its
inputs after a while would make every map's behaviour depend on
load. The expectation box confines the timeout to one place
where the user asked for it, expressed as a wire they drew,
which is the same move phase 4 makes for graph mutation and the
same move issue 259 makes for LLM retries: the mechanism is a
box, and the policy is the wiring.

The `missed` branch wired back upstream is a retry. The
`missed` branch wired to a writer is a log. The `missed` branch
wired to a different pipeline is a fallback that the user chose
explicitly and visibly — which is the only kind of fallback the
project's rules permit.

## Time since a marked event

Issue 251a leaves this here, because it needs a box with an
input and this is that box.

An expectation already records when it was armed and when the
value arrived. Exposing that difference — the elapsed time —
as the value on the `met` branch gives "time since" its most
useful form: not since the run started, but since the thing I
was waiting on started.

A degenerate expectation with no deadline is therefore a
**stopwatch**: armed by one event, stopped by another, emitting
the interval. Whether that is a third kind or the same kind
with `within_ms` absent is an open question, and the second is
tempting.

## Arming, in count or in milliseconds

A deadline can be stated two ways, and both are wanted:

- **In milliseconds** — `within_ms: 5000`. What a person means
  by a timeout.
- **In counts** — `within_counts: 20` against a named counter
  (issue 251a). Reproducible: the same count deadline produces
  the same behaviour on a loaded machine and an idle one, which
  makes a timed map testable.

The count form is the one that makes an expectation appear in a
deterministic test, and the millisecond form is the one people
will actually write. Support both, and make the transcript say
which was used, because a run that behaved differently under
load should be diagnosable from its transcript alone.

## Scheduling and caching

An alarm's deadline goes into the same priority queue 251
builds for timer re-arms. Nothing new is needed for one alarm.

What is new is **volume**. A map that arms an expectation per
item across a stream of thousands has thousands of pending
deadlines, and the naive shape — one priority-queue entry per
armed expectation, removed on arrival — is a lot of churn on a
structure that a worker consults every time it goes idle.

Two mitigations, and the second is where 251c enters:

- **Lazy cancellation.** An expectation that is met does not
  remove its deadline; the deadline fires, finds the
  expectation already resolved, and is discarded. Cheaper
  arming, more useless wake-ups. The standard answer, and
  usually the right one.
- **Cached schedules.** When deadlines are regular — every
  expectation in a batch armed for the same duration — the
  queue holds one entry for the batch rather than N. That
  generalises into "a schedule is a function, and the queue
  holds the function rather than its outputs," which is
  precisely what issue 251c is about.

## Open questions

1. **Is `expectation` a box kind or a routing kind?** The
   `met` / `missed` split is branch selection, which is what
   routing kinds do, and there is already machinery for named
   branches. But the branch here is chosen by the clock rather
   than by the value, which no routing kind does. Getting this
   wrong means either a routing kind that can fire without a
   value, or a box kind that duplicates branch machinery.
2. **What does `missed` carry?** The deadline that was blown,
   the elapsed time, or the partial inputs that did arrive.
   The third is the most useful for diagnosis and the most
   complicated, since a box with three ports may have two of
   them filled.
3. **Can an expectation be re-armed after it misses?** A
   watchdog wants to keep watching. A one-shot wants to stop.
   The `rate_ms = 0` convention from 251 gives a precedent for
   expressing "stop" as a value rather than as a flag.
4. **What happens to a value that arrives after `missed`
   fired?** It is late, the expectation already routed, and the
   value is sitting in a slot. Dropping it silently is a
   fallback. Emitting it on a third `late` branch is honest and
   is a third branch. Refusing to accept it is a push failure
   with a reason, which the transcript already has a field for.
5. **Does an armed expectation prevent quiescence?** Issue 251
   asks the same question for timers and defaults to "yes, a
   timer keeps the map alive." An expectation is different: it
   is waiting for something, and if nothing else in the map is
   running then nothing is ever going to arrive, and the
   correct behaviour is arguably to fire every pending
   `missed` immediately and *then* quiesce. That is a genuinely
   nice property — a run that cannot progress reports every
   expectation it failed instead of hanging — and it needs the
   quiescence check to know about the deadline queue.
6. **Is the stopwatch a separate kind?** An expectation with no
   deadline, armed by one event and stopped by another. Same
   machinery, different vocabulary.

## Suggested implementation steps

1. **Wait for 251's deadline queue.** Alarms and expectations
   both sit on it; building a second scheduling path would be
   the mistake.
2. **The alarm kind first** — it is a timer that does not
   re-arm, and it validates the deadline queue's API with the
   simplest possible customer.
3. **Resolve the box-kind-versus-routing-kind question** before
   writing the expectation, because it decides where the code
   lives.
4. **The expectation's arm / resolve / fire state machine**,
   with lazy cancellation.
5. **Count-based deadlines** against a named counter, and the
   transcript field recording which form was used.
6. **The quiescence interaction** — pending expectations fire
   `missed` when the map can no longer progress.
7. **Fixtures**: a met case, a missed case, a late-arrival
   case, and a map that deadlocks and is expected to report
   rather than hang.

## Relevant files

- `libs/task-pool/pool.{c,h}` — the deadline queue from 251
- `src/012-dispatch.c` — branch selection, push results, and
  the quiescence check
- `src/013-jsonl-events.c` — the events an alarm and an
  expectation write
- issue 251 (timer box: tick-tock emitter) — the deadline
  machinery this shares
- issue 251a (counter box) — count-based deadlines
- issue 251c (schedule functions and number waves) — cached
  schedules for high-volume arming
- issue 259 (code-extraction box) — the same "route the failure
  back upstream as a wire" pattern
