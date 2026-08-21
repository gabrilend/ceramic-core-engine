# 407 — Gathering at pickup

## Current behavior

**SUPERSEDED. This will not be built, and the reason is worth more than
the design was.**

This issue moved *when* a pulled value gets pulled. What happened
instead is that pulling was removed altogether: a value that used to be
gathered is now written into a static port by an ordinary push, and
writing a static runs the readiness check on the station holding it. So
there is no pull to time.

[056](../../docs/implementation-notes/056-no-pull-path.md) carries the
full reasoning — what pulling was for, the three timings weighed for
it, and the accounting problem that ended the search. The short version
is that a pulled value arriving by delivery lands in a queue whose
depth nothing keeps in step with the station's other ports, and a
pulled value produced inside the invocation has to run user code
somewhere it does not belong. Neither was worth the one sentence the
pull path existed to make true.

Three things this issue argued for survive it, having turned out not to
be about gathering at all: the claim dispatch table's null rows
becoming explicit cases; the snapshot of a port's wiring taken under
the mutex that is already held; and the observation that the pool never
needed to change for any of it.

The rest of this file is kept as the design that was not built.

### What it was written against

A pulled value is gathered by the thread that scheduled the task, not
by the thread that runs it.

The sequence today: a delivery fills the last empty ring port of a
station, readiness fires, and that same delivering thread takes one
value from each ring port under the station's mutex, releases the
mutex, and then — still on the delivering thread — walks every gatherer
port and runs the upstream station to produce a value for it. Only once
every slot in the task is full does the task reach the queue.

Three things are wrong with that, and they are the same wrong thing
seen from three sides.

**The value is not fresh at the moment it is used.** It is fresh at the
moment it was scheduled, and between those two instants sits the entire
queue — which is the one quantity in the engine that nobody controls.
That is the promise the pull path exists to make, and it is the whole
reason a gatherer costs more than a ring buffer. Under a shallow queue
the difference is microseconds; under a backlog the difference is the
backlog.

**The deliverer pays for work it has no interest in.** A thread passing
through with one value ends up executing an arbitrary amount of
somebody else's map, because a gather chain can be several stations
deep and every one of them runs inline before that thread pushes a
single task. A station fanning out to a hundred destinations pays this
a hundred times in a row.

**Readiness stops propagating while it happens.** The delivering thread
is not in the queue and not looking at other stations; it is down a
gather chain reading a file. Everything else that had just become ready
waits on a disk it has nothing to do with. That is not a throughput cost
but a discovery cost, and discovering ready work is the only mechanism
this engine has for creating concurrency at all.

Two supporting pieces are also missing:

- The claim dispatch table has literal null rows for gathered and
  static ports, and the caller guards them with a null check. An absence
  is standing in for a decision, and the reader has to know which
  absence means what.
- The gather wiring is read without a lock. Today that window is a few
  instructions wide, because the read happens on the same thread
  immediately after the mutex is dropped. It is a real window all the
  same, now that runtime rewiring can repoint a gatherer mid-run.

## Intended behavior

**A pulled value is gathered by the worker that is about to run the
box, on that worker's own stack, immediately before the call.**

The claim is unchanged: inputs all present, one value taken from each
ring port under the station's mutex, static values copied in the same
window because they are bytes on the port and cost nothing to take. An
invocation becomes real and a task is built. What changes is that the
task reaches the queue with its gathered slots holding *a way to produce
a value* rather than a value, and the worker that pops it resolves them
and then calls the box without going back to the queue in between.

**Each slot in a task says which of two things it holds** — a value, or
a way to produce one — as an explicit tag with a case for each, never as
a null pointer meaning "nothing to do here". That tag arrives with the
input-port record ([210](210-input-port-record.md)) rather than being
invented here; this issue is what first reads it on the worker's side.
The task becomes self-describing: a worker walks its slots, resolves the
ones that say they need resolving, and calls the box, without consulting
the station to find out which is which.

**The pool must not learn that tasks have phases.** The obvious
implementation gives the task a second function pointer for a prepare
step and teaches the worker loop to call it, which would make the pool
aware of stations — and the pool's entire value is that it is not.

The way around it is a trick phase 1's tests already use and the pool's
header already documents as undetectable from inside the pool: allocate
a gathering station's task as a larger struct whose *first* member is
the task struct, with the map pointer and the snapshotted gather list
behind it, and aim the task's function pointer at one generic gathering
shim. That shim recovers the outer struct — the task pointer is the base
pointer — fills the slots, and calls the station's real shim. The pool
pops it, calls one function pointer, hands it to the delivery walk, and
frees the base pointer: its three unconditional steps, untouched. The
delivery walk sees a genuine box task with a real station index and a
real staging buffer: untouched.

**The wiring is snapshotted; only the values are late.** Deferring the
gather widens the unlocked read of a port's upstream index from a few
instructions to the full queue latency. The fix belongs in the claim:
copy the gather list into the task under the station's mutex, which is
already held. Then the semantics are one sentence — *wiring and buffered
values as of scheduling, pulled values as of use* — and a repoint can
never disturb an invocation already in flight.

**The rule that makes gathering safe survives untouched.** A station
that is gathered from has no ring-buffer ports, so every path upstream
through gather wires ends at a station with nothing to wait for. That
is not a restriction this issue relaxes: what forces it is that a gather
is a call which cannot wait and cannot decline, and moving *when* it
happens changes which thread pays, not whether it can park halfway.

**One exception in the engine dissolves.** Today a gathered box runs on
a thread that is in the middle of delivering someone else's value —
the single place where a box runs without a worker having picked up a
task, and the place surprises come from. Afterward, every box in the
engine runs on a worker that picked up a task. The concurrent-safety
rule for gathered boxes stays exactly as it is, because several workers
can still be pulling from one upstream station at the same moment.

**What this costs, said out loud.** A task sitting in the queue is no
longer a complete description of an invocation, which is the same
property as the freshness seen from the other end. A task's run time
becomes its box's run time plus its gathers, so a slow gather shows up
as a slow task rather than as slow delivery somewhere else — the honest
place for it, since the cost belongs to the invocation that demanded the
value, but it means throughput is no longer runs multiplied by per-box
cost. And a worker inside a gather chain is neither asleep nor taking
work, for an unbounded time, which the termination protocol cannot
currently distinguish from a worker inside a box.

## Suggested implementation steps

1. Snapshot the gather list into the task during the claim, under the
   station's mutex. Worth doing first and on its own, because it is
   correct under the behaviour that exists today.
2. Replace the null rows in the claim dispatch table with explicit
   cases, so every port kind has an answer and no caller checks for an
   absence. Land this with no behaviour change.
3. The outer task struct and the generic gathering shim, proven first
   on a hand-built map so the mechanism stands before the loader can
   reach it.
4. Move static resolution into the claim, under the mutex, alongside
   the ring pop — which follows the statics-onto-ports work rather than
   this issue, and is noted here because it is what leaves gathering as
   the only thing deferred.
5. Measure, on one map, before and after: deliverer time per delivery,
   queue depth across a run, per-task time split into gather and box,
   and the age of a pulled value at the moment the box reads it. The
   last of those is the whole point and is the one nobody has measured.
6. Retire the inline gather on the delivering thread, along with the
   named exception to "boxes only run from the pool" that it created.
7. Correct [004 — Gathering](../../docs/004-datapath-statics.md) and
   [058 — Guarantees](../../docs/058-guarantees.md), both of which
   currently describe the value as fresh as of scheduling.

## Open questions

**All three are moot — nothing gathers — but two got answers worth
keeping, because they were about the engine rather than about
gathering.**

- *What happens to a task whose gather fails halfway?* It could only
  ever have been fatal. By the time a worker holds the task, the ring
  values have already been taken out of their buffers and cannot be put
  back — other values were claimed after them, so the order would break
  — which means abandoning the task loses real data silently. Retrying
  is worse, because a box that reaches out to the world is not
  necessarily idempotent and may already have consumed what it read.
  Can't retry, can't abandon: stop and name what failed.
- *Does the sleeper count need to tell a worker inside a gather chain
  from one inside a box?* No, and the framing was wrong. A gathered box
  is a box; there is no engine-versus-your-code distinction to draw. A
  worker running one is busy either way, so termination was never at
  risk — the only thing at stake was whether a report could say which
  box a stalled worker is inside, which it should, without needing to
  classify how the box was reached.
- *What is in the queue when the starting gate opens?* Whatever
  construction's writes produced. There is no seed sweep any more.

## The note that started this

Kept verbatim, because the reasoning in it is the specification:

> we should add an option to, when checking the input values to see if
> there's a value present for every slot on a box station so that the
> box function can be enqueued as a task struct, we should be able to
> instead of gathering it there, to instead when the box function is
> called, to instead call the gather from within that call, meaning the
> worker thread does the gathering, rather than the enqueue'ing thread.
> This allows us to initiate a bunch of tasks quickly, rather than
> processing on the "enqueue 100 tasks" hot path.
>
> So essentially, when a task struct is picked up from the thread pool
> by a worker thread to run a box function, it will first pull data
> from each of the gather slots it has. This can't be known at compile
> time, because we will need to be able to wire up new slots. Meaning,
> we can't just macro it into the box function's definition. Two box
> stations could have different wiring - the same box function might
> have one input, and on one box station it could be a static value and
> on the other it could have an input-only box meaning it should be
> gathered. On a third box station it might be a ring buffer, all with
> the same box function signature.
>
> So, to address this problem, we should add a small layer of
> indirection - the task struct should contain a function pointer, this
> is known, and the function should point to a spot in memory with a
> void pointer and each function should know how to deconstruct the
> values in the void pointer, and assemble them into the variables that
> the function needs. Or maybe we're doing that automatically? I
> forget. Anyway, during that stage, if we do that OUTSIDE of the box
> function in the task_run() or whatever function, then we have a
> perfect opportunity to gather the gather input slot's values. If not,
> then we need to have a second pointer in each of the task structs
> that is set to a value of 0 or NULL if the box only has ring buffers
> or is without input slots at all. This pointer will point to a
> generator function that gathers all the input values with gather
> style inputs.

The note's own uncertainty — *"Or maybe we're doing that automatically?
I forget"* — has an answer: the generated shim already does exactly
that deconstruction, reading the task's slot bytes into typed
arguments. That is what makes the outer-struct approach available
instead of the second task-struct pointer the note falls back to. The
indirection the note asks for is already there; this issue moves one
step inside it.

## Related

- [056 — Gather timing](../../docs/implementation-notes/056-no-pull-path.md),
  which states this timing as the design and gives the reasoning, the
  guarantees, and the costs in full
- [210 — What an input port is](210-input-port-record.md), which
  supplies the per-slot tag this reads and the port record it reads it
  from
- [403 — Gatherer slots](403-gatherer-ports.md) and
  [404 — Gather chains and cycles](404-gather-chains-and-cycles.md),
  the mechanism whose timing this moves
- [704 — Rewiring while it runs](704-runtime-rewiring.md),
  which makes the unlocked read of a port's upstream index a real
  window rather than a theoretical one
- [004 — Gathering](../../docs/004-datapath-statics.md) and
  [058 — Guarantees](../../docs/058-guarantees.md), both of which state the
  freshness promise this makes literally true
