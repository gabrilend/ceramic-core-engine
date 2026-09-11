# 107 — Several queues, and a station may name one

> **Confer before building any of this.** Nothing here exists in code.
> The design is being worked out in conversation and has already been
> reversed once in a way that changed its shape completely: an earlier
> draft made a worker deliver into its own queue by default, on a
> cache-locality argument, and that was wrong — it assumes a
> destination is a thread in this process, which is the assumption the
> whole structure exists to avoid. The naming is also unsettled;
> *destination* collides with the wire's far end, which this engine
> already calls a destination. **A blueprint that has been wrong once
> is a blueprint to talk through, not to pick up and execute.** Ask
> explicitly before starting any step below.

The pool stops being one queue and becomes **several**. A station may
say which one its tasks are delivered into. **By default it says
nothing, and nothing goes anywhere it does not go today.**

That last sentence is the requirement and the most important line in
this file. A queue is a **destination**, and the engine must never
assume it knows what a destination is.

---

## Why the default is "say nothing"

The obvious design — a worker delivers into its own queue, because the
value it just produced is hot in its own cache — was considered and is
**wrong**, and the reason is worth stating before anything else.

**It bakes in the assumption that a destination is a thread in this
process.** That assumption is the one thing this structure exists to
avoid. A destination should be free to become, later and without
rebuilding any of this:

- another physical processor in the same machine
- a graphics processor, whose queue is a submission ring rather than a
  list of function pointers
- a machine across a network, reached by a transport that has nothing
  to do with threads
- something that is not a computer at all — a queue of work for a
  department, a machine shop, a person

A default of *deliver to my own thread* would make every one of those
a special case grafted onto a rule that says otherwise. A default of
*deliver to the ordinary place* leaves the concept open, and a station
that wants somewhere specific says so.

**So locality is not the reason for this feature and must not be its
default.** It is one thing a destination can happen to express, opted
into by a station that knows it wants it.

---

## Current behavior

**One ring of task pointers, one mutex, one condition variable, shared
by every thread in the process.**

The pool holds `slots`, an array of *pointers to* tasks; `capacity`;
`head`, the oldest entry; `tail`, the next free slot; and one mutex
that guards all four. One slot is always left empty, so `head == tail`
means empty and never means full.

**Pushing**, from any thread at all: take the mutex; if advancing the
tail would land on the head, grow — allocate an array of twice the
capacity, copy the live entries oldest-first (one `memcpy` if the ring
has not wrapped, two if it has), free the old array, set `head` to
zero and `tail` to the count; write the pointer at `tail` and advance
it modulo capacity; update the high-water mark; **broadcast on the
condition variable, waking every sleeping worker rather than a chosen
one**; release the mutex.

**The worker loop** holds that same mutex at the top of every
iteration. Finding `head != tail` it takes `slots[head]`, advances
head, **releases the mutex**, bumps its epoch to odd, records which
station it is entering, calls the task's function, calls the finish
hook, frees the task, records station -1, bumps its epoch to even, and
**takes the mutex again**. Finding the ring empty it increments the
sleeper count under that same lock, and then either declares the
program finished — if its own registration brought the count up to the
worker total, no outside submitter holds a standing promise, and a
final look still shows empty — or waits on the shared condition
variable.

**So the costs, stated plainly.** Every worker acquires one
process-wide mutex **twice per task**, once to take it and once on the
way back. Every push acquires that same mutex and then wakes every
sleeping thread.

**And the lock is load-bearing beyond the queue.** The sleeper count
is exact *because* the check and the registration happen inside one
hold of it, and that exactness is what makes the termination decision
sound. Anything that moves work off this lock has to leave that
property standing.

`cera_pool_pop()` exists in the header but no worker uses it. Workers
inline the take because they need to keep the lock across the decision
to sleep; the function is for tests and for whoever owns the pool.

There is exactly one queue and no way to name it, because there is
nothing to distinguish it from.

---

## Intended behavior

### A destination is a queue plus the set of servers that draw from it

That is the whole of the concept and it is deliberately thin.

- **A queue** holds tasks in arrival order. What is in it is opaque to
  whoever serves it beyond the one function pointer.
- **A server** is anything that takes from a queue and does the work.
  Today every server is a worker thread. The design must not say so
  anywhere it can avoid saying so.
- **The default destination** exists always, is served by every
  worker, and is what a station that names nothing delivers into. It
  is today's queue, unchanged, and a program that names no destination
  anywhere behaves exactly as it does now — same order, same mutex,
  same wakeups.

**Nothing is inferred.** No station acquires a destination by being
near something, by being produced by a particular worker, or by any
other rule the engine could apply on its own. A destination is written
down or it is absent.

### Servers subscribe; queues overlap

Each server holds **an ordered array of the queues it draws from**, and
taking a task means walking that array and taking from the first that
yields one.

```
worker i:  sources = [ ...the destinations this worker serves..., default ]
```

**The array is ordered and the order is the policy.** Earlier entries
are served first. The default is last, so a server that has a
specialization attends to it before it attends to general work.

**Queues may overlap in who serves them, and this is the point.** Two
destinations may share servers, share none, or share some. A
destination is not a partition of the machine and imposes no structure
on any other destination — so specializations and priorities can be
layered without any of them having to know about the others.

Three arrangements that fall out of this without new mechanism:

| written as | is |
|---|---|
| a queue served by exactly one worker | *this work happens on that thread* |
| a queue served by every worker, placed first in each array | *this work happens before ordinary work* — a priority lane |
| a queue served by the four workers on one socket | *this work happens over there* — which is what 108 computes from a hardware expression |

Nothing distinguishes those three in the engine. They are the same
structure with different membership, which is why none of them needs
its own code.

### What a station says

A station's record gains **one destination, or none**. None is the
default and is what every station has today.

The map file gains one optional attribute line, alongside `in` and
`out`. Whether it is spelled `to` and whether 108's hardware
expression is the same line or a second one is open question 1 — 108
computes a server set from a description of hardware, and that is a
*way of naming a destination* rather than a different concept.

```
station churn (math.c:grind)
  to slow-lane
  in 1 $0
  out 0 - printer.0
```

**Delivery does the lookup, not the pool.** A station's destination is
a map fact; which queue a task goes into is a scheduling fact. The
delivery path already translates every other map fact into something
the pool ferries without understanding, and this is one more: a queue
index, resolved from the station, handed to the pool. The pool reads
it to choose a queue and learns nothing about stations by doing so.

### What this costs when nobody uses it

**Nothing.** One destination exists, every worker serves it, every
station delivers into it, the order is the order it is today, and the
mutex and the broadcast are where they are today. That is the test of
whether this was built correctly: a program with no destinations named
anywhere must be indistinguishable from the current engine, in
behavior and in measurement.

---

## What it costs when somebody does use it

### First in, first out becomes per-queue

**P3** promises that a waiting task cannot be starved by newer
arrivals. With several queues and an ordered source array, a task in a
later queue waits while earlier queues have work. That is what an
ordered array *means* — a priority lane that is not served first is
not a priority lane.

So the guarantee becomes: **first-in-first-out within each queue, and
the source order between them.** The global sentence is gone and the
guarantees page has to say the weaker thing rather than leave the
stronger one standing.

**Starvation is reachable and needs an answer.** A destination served
first, kept permanently non-empty by a push cycle, means the default
queue behind it is never looked at while the machine is fully busy.
Three answers, and this needs deciding before anything is built:

- **Take from a later queue every Nth task.** One counter per server,
  no coordination, bounds the wait. Costs a knob nobody can pick from
  first principles.
- **Compare arrival order across the heads**, using a sequence number
  stamped at push, and take the oldest unless an earlier queue is
  marked as a genuine priority. Keeps something close to global order
  and makes *priority* an explicit property of a destination rather
  than a side effect of array position.
- **Say it plainly in the guarantees**: an earlier destination starves
  a later one, on purpose, and that is what ordering a source array
  means.

### No worker idle while a task is ready — the hole is real but narrow

**U1** can be violated: a server sleeps while a queue it does not
subscribe to holds work. That cannot happen by default, because the
default destination is served by everybody. It happens exactly when
somebody narrows a destination's server set — which is the author
saying *I would rather this ran there than soon*, knowingly.

The important part is that **the default costs nothing**, so this
guarantee is not sold by building the mechanism. It is sold, per
program, by whoever uses it.

---

## The invariant that keeps termination sound

Today the last worker to fall asleep re-scans **the queue** and, on
finding it empty, declares the program finished. With several queues
the rule is the same sentence with one word pluralized: **the last
sleeper re-scans every queue in the pool**, not only the ones it
subscribes to.

A task in a queue this server does not serve is still work that
exists. A re-scan that missed it would declare completion with tasks
outstanding, which is the silent-wrong-answer failure that rule exists
to prevent, reintroduced through the side door.

**And a queue with no server at all is a hang.** If a destination's
server set is empty, the last sleeper correctly sees a non-empty queue
and correctly declines to declare completion, and nothing can ever run
that task. So a destination with no servers is **refused when it is
named**, not discovered later — and re-checked when a station is
placed or repointed on a running program, since construction is legal
at any moment.

The walk is a loop over a handful of queues, run once at the end of a
program, in place of a comparison. It is not on any hot path.

---

## What has to be counted across several queues

Every existing thing that says something about "the queue" now has
several to say it about. Small, and forgetting one produces a report
that quietly lies.

| what | today | after |
|---|---|---|
| `cera_pool_queued()` | how many tasks are in the ring | the sum over every queue |
| `cera_pool_destroy()`'s complaint about unrun tasks | counts the ring | counts all of them, or it under-reports exactly the work a stopped program abandoned |
| `cera_pool_queue_stats()` — capacity, high water, growths | one ring's three numbers | one set per queue. Aggregate, or report per queue and let the caller add up. The phase 1 demo reads these, so whatever shape is chosen has a reader waiting. |
| `cera_pool_stop()` | queued tasks stay queued and are never run | unchanged in meaning, plural in fact |

---

## One ring type, used several times

The ring — slots, capacity, head, tail, count, doubling growth — is
currently written once, inside the pool, with the mutex and the
condition variable and the sleeper count wrapped around it. **It
should become a thing of its own**, and then the pool holds one per
destination.

Two reasons beyond tidiness. The growth logic with its wrapped-and-
unwrapped `memcpy` pair is the fiddliest code in the file and there is
no version of this issue where it is worth having twice. And each
instance wants **its control fields on their own cache line**, so that
two servers pushing to two queues are not writing the same line and
invalidating each other's — which is exactly the padding the
per-worker epoch records already use, and which is straightforward to
apply to a struct and awkward to apply to fields scattered through the
pool.

**Whether each queue keeps its own mutex and condition variable, or
they continue to share the pool's, is open question 4.** Sharing keeps
the sleeper accounting exactly as it is, which is the property most
worth not disturbing. Splitting is what would take the contention off
the common path, and it is the harder change.

---

## Suggested implementation steps

1. **Lift the ring out** into its own type with push, take, count and
   growth, leaving the pool holding one of them and behaving exactly
   as it does today. Every existing test passes unchanged, or the lift
   was wrong. No behavior changes at all in this step.
2. **Make the queue array exist with one entry in it.** Each server's
   source array is `[default]`, the take walks the array, and
   everything else is untouched. Again every test passes unchanged —
   this proves the walk before the walk carries anything.
3. **A destination that can be created and subscribed to**, still with
   nothing naming one: create a queue, give it a server set, and have
   those servers' arrays grow an entry. Includes the refusal of a
   destination with no servers.
4. **The station's destination field and the map file line**, so a
   station can name one, plus the dump writing it back.
5. **The last sleeper's widened re-scan**, with a test that ends a
   program with work outstanding in a queue the last sleeper does not
   serve.
6. **The starvation answer** from P3 above, whichever is chosen, plus a
   test that reproduces the starving case first and then does not.
7. **The counting and reporting**, so `cera_pool_queued`, the destroy
   complaint and the queue statistics tell the truth about several
   queues.
8. **Then 108's placements** become a way of computing a server set
   from a hardware expression, which is a notation over this rather
   than a mechanism beside it.

---

## How it is tested

**That naming nothing changes nothing.** The whole existing test suite,
unchanged, from step 1 onward. This is the most important test in the
issue and it is one that already exists.

**That a named destination is honored.** A station pointed at a
destination served by one worker, a box that records
`cera_pool_worker_index()`, several hundred invocations, and an assertion
that every one ran on that worker. Directly observable, no timing
involved.

**That nothing is lost across the split.** The existing tests that push
several hundred tasks and count them arriving exactly once matter most
here: a task that falls between two queues is the failure this design
can produce and the current one cannot.

**That termination still cannot be fooled.** A program that ends with
work in a queue the last sleeper does not serve.

**That an unservable destination is refused** at the moment it is
named, and again when a station is repointed at one on a running
program.

**That the starving case starves before it is fixed.** A destination
served first and kept permanently fed, one task in the default queue,
and a measurement of how long it waits. Written as a failing test
first, so whichever answer is chosen is proven to answer it.

**That the counts add up.** `cera_pool_queued()` against a known number of
tasks spread over several queues, and the destroy complaint against a
deliberately stopped program with work left in more than one.

---

## Open questions

1. **Is a destination named directly, or described, or both?**
   *Directly* is `to slow-lane` — a name, resolved to a queue.
   *Described* is 108's `on package 0 and not core 0` — a hardware
   expression that computes a server set. They are the same concept
   reached two ways, and it is not settled whether that is one line
   with two notations or two lines with two meanings.
2. **What is the answer to the starvation edge** — take from a later
   queue every Nth task, compare arrival order across heads, or state
   it as intended behavior?
3. **Who creates a destination, and when?** A station naming a
   destination that does not exist is either a refusal or an implicit
   creation, and implicit creation means a typo becomes a queue nobody
   serves. Refusal seems right and needs somewhere for the declaration
   to live.
4. **One mutex and condition variable for all queues, or one each?**
   Sharing leaves the sleeper accounting exactly as it is, which is
   the property most worth not disturbing. Splitting is what actually
   takes contention off the common path, and it is where the
   termination argument would have to be re-made from scratch.
5. **What does `cera_pool_pop()` mean now?** It is public, used by tests and
   pool owners, and called from threads that are not workers. Popping
   the default queue is the obvious reading; whether it can reach a
   named one is a separate question.
6. **What is a server, once one is not a thread?** The design says
   *anything that takes from a queue*, and today every one is a worker
   thread in this process. A graphics processor's queue is a
   submission ring, not a list of function pointers, and a machine
   across a network needs the task's values serialized rather than
   pointed at. Naming the concept now is cheap; working out what a
   non-thread server actually implements is its own issue and should
   be, before anything here hard-codes the easy case.

---

## Related

- [006 — Scheduling](../docs/006-datapath-scheduling.md), the document
  this rewrites: "one queue of tasks" stops being true, and the
  termination section gains a plural
- [058 — Guarantees](../docs/058-guarantees.md), where P3 weakens from
  global order to per-queue order plus a source order, and U1 gains a
  hole that only a program which uses this can fall into
- [101 — The task queue ring](completed/101-task-queue-ring.md), whose
  ring becomes a type used several times
- [103 — Sleeping and waking](completed/103-sleeping-and-waking.md),
  whose broadcast-on-every-push is the thing splitting the mutex would
  change, and question 4 is whether to
- [104 — Termination by the last sleeper](completed/104-termination-by-last-sleeper.md),
  whose rule survives with one word pluralized
- [105 — Phase 1 demo](completed/105-phase-1-demo.md), which reads the
  queue statistics that now describe several queues
- [108 — Choosing where a box runs](108-choosing-where-a-box-runs.md),
  which computes a server set from a description of hardware, and is
  therefore a notation over this rather than a mechanism beside it
- [214 — Destinations without a lock](completed/214-destinations-without-a-lock.md),
  whose per-worker cache-line padding is the pattern the per-queue
  records follow — and whose *destination* means a wire's far end, not
  a queue, which is a collision of words this issue should probably
  resolve before it spreads
