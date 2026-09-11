# Conversation Summary: d34fab42-f004-45a7-b23d-484cd10af0c1

Generated on: 2026-09-08 10:59:09

--------------------------------------------------------------------------------

### User Request 1

hi can you add an issue file to make functionality that lets us manually define
which cores, threads, or even CPUs a particular box will run on. specify
available options, checklist style, and and or or similar optionality present.

--------------------------------------------------------------------------------

### Assistant Response 1

I'll look at the existing issue structure and docs to see how this fits.

Now I'll write the issue file.

Issue written. Two files touched:

- `issues/107-choosing-where-a-box-runs.md` — the blueprint
- `issues/phase-1-progress.md` — a row for it, and a paragraph saying phase 1
  is open again

**What the issue says, in short.** A station — not a box, since a box is a C
function with no location and a station is one placement of it — may carry a
line saying which processors it will run on. The vocabulary is the machine from
the bottom up: a logical processor (one hardware thread, one bit in a scheduling
mask), a physical core (all the hardware threads sharing execution units), a
package (the socket, the thing with its own memory controller), a NUMA node, an
L3 cache domain, and one software term, a worker (a thread this program made).
Two relative terms as well — *near this other station* and *apart from that
one* — because a map naming processor number six is a map about one machine.

Every one of those names a **set of processors**, so the operators are the three
set operations: `or` unions, `and` intersects, `not` complements. Intersection
earns its place immediately — the kernel numbers cores per package, so `core
2` is ambiguous on any two-socket machine and `package 0 and core 2` is the
author resolving it in the operator that already means what they mean.

**Three things underneath turned out to be the real work.** A worker the kernel
may migrate cannot belong to a placement class at all, so workers must get fixed
homes before any station can be obeyed — which means the pool grows placement
policies and a program naming a constraint under an unpinned pool is refused
rather than best-efforted. The single task queue becomes one ring per distinct
constraint, with a sequence number stamped at push so that first-in-first-out
survives intact instead of being weakened to per-queue order. And the last
sleeper's final look — the check that decides a program is finished — must
widen to every queue, or it declares completion with work outstanding, which is
exactly the silent-wrong-answer failure that rule was written to prevent.

**The price is one guarantee, sold knowingly.** Today no worker sits idle while
a task is ready. With placements, a worker whose queues are empty sleeps while
another queue holds work it is not permitted to run. That is the whole cost, and
it is the author saying *I would rather this ran there than soon*.

Eight open questions are written down at the end. Nothing is committed — the
issue is new, not complete, and `llm-transcripts/aug-22-26.md` is the only other
thing modified in the tree.

Here is the first question, which changes the shape of everything else:

**[Asked the user]**

*Q1 — Hard or soft:* Should a placement be a hard promise or a preference?
(Open question 7 of 8 in the issue.)
- Hard only (Recommended) — The kernel call sched_setaffinity writes a bitmask
  onto a thread; the scheduler will never run that thread on a processor whose
  bit is clear. A constrained station's tasks sit in a queue only matching
  workers read, so the placement is a promise. Cost: a worker with nothing
  eligible sleeps while other work waits. This is the literal reading of your
  request, and a placement that silently ran elsewhere would be a fallback used
  quietly, which this project calls an error.
- Both, said on the line — Two spellings — one that must hold and one that
  is a tendency. A soft placement means a worker drains its preferred queues
  first and takes from any queue when starved, so no worker ever idles and the
  placement is a hint. Costs: two mechanisms, two guarantees to state, and every
  report has to say which kind it is describing.
- Soft only — Nothing is ever forbidden; a placement only biases which queue a
  worker looks in first. Keeps the guarantee that no worker sits idle while
  ready work exists, entirely intact. Cost: you cannot say "this box runs on
  that core" and be believed — the answer is always "usually", which is not
  what you asked for.
→ **Answered:** Hard only (Recommended)" selected preview:
station churn grind p
  on package 0 and not core 0

-> a promise. If no worker lives there,
   the program is refused before it runs.. You can now continue with these
   answers in mind.

Question 7 is now answered in the file, with the reasoning written into it and
the idle worker booked as the accepted price. Next one:

**[Asked the user]**

*Q1 — Pool's home:* Where does the pool's own placement get decided — the
coarse one, where all of a program's worker threads live? (Open question 4 of
8.)
- At pool creation, not in the file (Recommended) — The pool exists before any
  map is read, and one pool can serve several programs at once (that is what
  lets a program start another beside itself). A map file that placed the pool
  would be one program deciding for its neighbours. So the placement is an
  argument to whoever makes the pool — C, a flag, an environment variable —
  and the map file only ever speaks about its own stations.
- The file declares, the creator decides — A header line in the map states
  what the program requires of its pool — all workers on one package, say —
  and validation refuses to run it in a pool that does not satisfy it. The file
  never places anything; it declares an interface and the check happens where
  every other check happens. Costs a second concept: a requirement is not a
  placement, and the two read alike.
- In the map file, as a header — The file describes the whole program
  including which processors it runs on, so everything about a program lives in
  one document. Costs the layering: the pool is made before the file is read,
  and a second program loaded into the same pool would find its placement
  already decided by somebody else's file — or would fight over it.
→ *(no answer recorded)*

You're right, and the correction matters more than the question did — I had a
creating thread in the picture, and there isn't one.

**What I changed in the issue.** The section that used to be "pool placement
policies, one of which is chosen at creation" is now *The workers need homes
first, and nobody hands them one*. A worker pins itself as it joins and writes
the mask it took into its own record in the shared structure; the pool's reach
is the union of what its members announced. Three things fall out of that: a
worker joining late needs no special case, a worker already placed from outside
(a cpuset, `taskset`, a container) just announces what it is permitted rather
than being told what it already is, and a refused station constraint can name
both sides — what it asked for and what the pool actually reaches — instead
of pointing at a policy nobody set.

That also puts a blocker at the top of the implementation steps, because the
correction exposes an unanswered question that is upstream of this whole issue
— and note 090 already asks it and says it should be answered before anything
is built toward either side:

**What does *shared memory* mean here?**

One reading is a single process whose threads share one address space. Every
core is already looking at the same bytes; a station table on the ordinary heap
is reachable from all of them and what differs between sockets is only how fast.
Nothing in the engine changes shape.

The other is several processes mapping one region — the project's own
`/dev/shm` tier. That reaches much further than it sounds. A station holds a
mutex, and a mutex is only usable across processes if it was created with that
attribute set. A station holds pointers to its port array and its slot pages,
and two processes need not map the region at the same address, so every one of
those would have to become an offset from the base of the mapping rather than an
address. And a box is a function pointer — it means nothing at all in another
process unless both loaded the same binary at the same address.

The reason I'm asking rather than picking: it decides whether a worker's
announcement is a write to a struct or a write into a mapping, which is step
three of this issue and everything after it.

**[Asked the user]**

*Q1 — Shared memory:* When you say the pool and the shared components live in
shared memory — is that one process whose threads share an address space, or
several processes mapping one region?
- Several processes, one mapping — The station table is built inside a
  /dev/shm mapping rather than on the heap. Every mutex is created
  process-shared, every pointer inside the region becomes an offset from the
  base (two processes need not map it at the same address), and a box's function
  pointer only means something if every participant loaded the same binary at
  the same address. A worker joining is a process attaching. This is the larger
  design, and the one that makes 'no main thread' literally true.
- One process, threads sharing an address space — What the engine does today.
  Workers are threads made with pthread_create; the pool, the station table and
  the queues are ordinary heap that every thread can already see. 'No main
  thread' then means no privileged owner among the threads — nobody creates
  and decides for the others — rather than no shared process. Nothing in the
  engine changes shape; only where memory is allocated from and which cores
  threads sit on.
- Both, eventually — Build for threads now and keep the second reading
  reachable — meaning no new pointer into the table that could not become an
  offset later, and nothing that assumes a mutex is private. Costs a standing
  discipline nothing enforces, which this project usually calls the weakest kind
  of guarantee.
→ **Answered:** One process, threads sharing an address space" selected
preview:
one process
├─ worker 0 ─┐
├─ worker 1 ─┼─> heap: pool, stations, queues
└─ worker 2 ─┘

mutex: ordinary
pointers: ordinary
box: a function in this binary. You can now continue with these answers in mind.

Settled. Recording it in both places — the issue, and note 090, which is where
that question actually lives.

Now note 090, where that question was originally asked:

Two down. Next:

**[Asked the user]**

*Q1 — Composition:* A program is brought inside another (composition produces
one station table). What happens to a placement a station was carrying? (Open
question 5 of 8.)
- Carried unchanged, re-checked (Recommended) — A placement names hardware,
  and hardware does not change when a program is composed — processor 6 is
  processor 6 in either program. So the line comes across as written and is
  checked against the workers the parent actually has, refusing by name if none
  of them can satisfy it. The relative terms (near, apart) resolve against the
  composed subgraph's own stations, which came across with it.
- Refused at composition — Bringing in a station that carries a placement
  stops and names it; the author restates it in the parent, where they can see
  the whole picture. Safest and loudest — nothing is silently reinterpreted
  — but it makes a placed subgraph un-composable, which is awkward given that
  composing is how you say 'keep these close together'.
- Relative carries, absolute refused — near and apart survive composition,
  because they name stations and the stations came too. Absolute terms (cpu 6,
  package 0) are refused, because a number written for one machine's pool is the
  thing most likely to mean something else in another. Splits the vocabulary
  into a portable half and a local half, and makes the portable half the one you
  compose with.
→ *(no answer recorded)*

What would you like to clarify about it?

For context on where the question comes from: bringing one program inside
another produces a single station table (that's issue 217 — *a program inside
another*), and note 090 reads that as a placement statement too, since one table
means one processor. So a station that arrived from the child carrying a line
like `on package 0` is now sitting in the parent's table, being run by the
parent's workers, whose announced homes may be entirely different processors.
The question is what that line should mean at that moment.

Tell me what part is off — whether it's the premise about composition, the
vocabulary, or something else about how you see it working.

--------------------------------------------------------------------------------

### User Request 2

I'm not answering questions right now

--------------------------------------------------------------------------------

### Assistant Response 2

Understood — stopping there.

**What's on disk:**

- `issues/107-choosing-where-a-box-runs.md` — the blueprint
- `issues/phase-1-progress.md` — a row for it, marked open, and a paragraph
  saying phase 1 is open again at the mechanism this time
- `docs/implementation-notes/090-one-table-per-processor.md` — its open
  question answered, and a pointer to 107 as the thing now being built toward
  its intent

**Two questions got settled and are written into the files as answered:**

A placement is a **hard promise**, not a preference. The kernel call writes a
bitmask onto a thread and the scheduler will never place that thread outside it;
a constrained station's tasks sit in a queue only matching workers read. The
accepted price is a worker that sleeps while ready work exists it is not
permitted to run.

*Shared memory* means **one process whose threads share an address space** —
ordinary heap, ordinary mutexes, pointers stay pointers, a box stays a function
in this binary. *No main thread* means no privileged owner among the workers,
which is why the issue now has workers **announcing** their own homes rather
than being handed one: each pins itself as it joins and writes down the mask it
took, and the pool's reach is the union of what its members announced. That
replaced a section built around a creating thread that doesn't exist.

**Six open questions remain in the file, unanswered** — box or station, what
to call an L3 domain, whether naming a worker directly is wise, composition,
whether `near` follows or resolves once, and non-Linux machines. The issue is a
blueprint in progress, not a finished one, and it says so.

Nothing committed. `llm-transcripts/aug-22-26.md` is still modified from before
this session started.

--------------------------------------------------------------------------------

### User Request 3

> The kernel call writes a bitmask onto a thread
>   and the scheduler will never place that thread outside it;

you can't just say a sentence like this and not say what the bitmask is, or what
you mean by "place the thread" because there's several different things it can
be. Be precise in your words.

--------------------------------------------------------------------------------

### Assistant Response 3

You're right — that sentence was doing three jobs and naming none of them.

**The bitmask.** `sched_setaffinity(tid, size, mask)` takes a `cpu_set_t`, which
is a flat array of bits — 1024 of them by default, `CPU_SETSIZE`, with
`CPU_ALLOC` for machines that have more processors than that. Bit *n* stands for
logical CPU *n*, using the kernel's own numbering: the same numbers that appear
as `processor:` in `/proc/cpuinfo`, as the directory
`/sys/devices/system/cpu/cpu6`, and as the `cpu6` line in `/proc/stat`. A
logical CPU is one hardware thread, so on a machine with simultaneous
multithreading two of those numbers are two hardware threads sharing one core's
execution units.

Those numbers are **enumeration order from firmware, not topology**. CPU 0 and
CPU 1 may be two threads of one core or two different sockets, and nothing about
the number says which. That is the whole reason the issue reads
`/sys/devices/system/cpu/cpuN/topology/` instead of doing arithmetic on CPU
numbers.

The first argument is a thread, despite being typed `pid_t` — it is a TID from
`gettid()`, and zero means the calling thread. `pthread_setaffinity_np` is
glibc's wrapper that takes a `pthread_t` instead.

**"Place the thread" was three separate things.** The Linux scheduler keeps one
run queue per logical CPU. A runnable thread sits in exactly one of those queues
at any instant and executes on that CPU when it is selected. The mask constrains
three distinct moments:

- **Wakeup.** When a sleeping thread becomes runnable, the scheduler chooses
  which CPU's run queue to put it on. That choice is restricted to set bits.
- **Load balancing.** The scheduler periodically moves runnable threads between
  run queues to even out load. Migration targets are restricted to set bits.
- **The call itself.** If the thread is executing on a CPU whose bit you just
  cleared, it is migrated off immediately rather than at some later convenience.

So the precise claim is: **the thread is never enqueued on, nor executes on, a
logical CPU whose bit is clear.** What the mask does *not* do is choose among
the CPUs whose bits are set — the scheduler still picks freely and may move
the thread between them at any moment. Pinning to a set of eight is not pinning
to one.

**It is a restriction, not a preference.** There is no "prefer this CPU" call;
the interface offers only the hard form. And it is intersected with what the
thread is already permitted — a cpuset or container's `cpuset.cpus.effective`
— so a mask with no permitted CPU in it fails the call with `EINVAL` rather
than being widened.

**One case bends the promise, and it is worth writing down.** If every logical
CPU in a thread's mask goes offline — CPU hotplug, a machine being
reconfigured under a running program — the kernel does not block the thread
forever. It breaks the affinity and runs it somewhere else. That is a fallback
happening below the engine, silently, and it is exactly the kind of thing this
project treats as an error, so it needs detecting rather than assuming away.

Let me put that into the issue, since 107c is built on it:

The mechanism is now written out in the issue rather than gestured at — a new
subsection, *What the affinity call actually does*, sitting at the top of what
has to change underneath, since everything else in the issue rests on it. It
says what the bit array is and what a bit means, that the numbering is firmware
enumeration order and carries no topology, that the first argument is a thread
despite its type, and it breaks "place the thread" into the three moments the
mask actually constrains — the wakeup choosing a run queue, the load balancer
migrating between run queues, and the call itself evicting a thread from a
cleared CPU. Then the two limits: the mask does not choose *among* the set bits,
so pinning to eight is not pinning to one; and if every CPU in a mask goes
offline the kernel breaks the affinity rather than parking the thread, which is
a fallback happening below the engine.

That last one added a ninth open question, since a worker noticing its affinity
was broken needs a cadence and a response, and I don't know the right ones —
`sched_getcpu()` at a task boundary is the cheap candidate.

Seven questions remain open in the file. Nothing committed.

--------------------------------------------------------------------------------

### User Request 4

okay so we should design it such that each thread has it's own thread pool
queue, and it pops from that first when it needs a task. The "pointer" or
whatever that is the value that is present when the individual core's thread
pool is empty should return the next value from the global pool. Those are hard
requirements in the design - can you tell me how they are going to be
implemented, and how the system looks right now, in design?

--------------------------------------------------------------------------------

### Assistant Response 4

Let me read the pool as it actually stands before describing it.

Read. Here is the current design precisely, then how the two-level structure
lands on it.

## How it looks right now

**One ring, one lock, one condition variable, shared by everybody.**

The pool holds `task_t **slots` — an array of *pointers to* tasks, not tasks
— plus `capacity`, `head` (oldest), `tail` (next free), and one
`pthread_mutex_t` that guards all four. One slot is always left empty, so `head
== tail` means empty and never means full.

**Push** (`pool_push`, from any thread): lock the mutex; if advancing the tail
would land on the head, grow — allocate an array of twice the capacity, copy
the live entries oldest-first (two `memcpy`s if the ring has wrapped, one if
not), free the old array, set `head = 0`, `tail = count`; write the pointer at
`tail` and advance it modulo capacity; update the high-water mark;
**`pthread_cond_broadcast`** — wake every sleeping worker, not a chosen one;
unlock.

**The worker loop** holds that same mutex at the top of every iteration. If
`head != tail`, it takes `slots[head]`, advances head, **unlocks**, then bumps
its epoch to odd, records which station it is entering, calls `t->call(t)`,
calls the finish hook, frees the task, records station -1, bumps the epoch to
even, and **re-locks**. If instead the ring is empty, it does `sleeping++` under
that lock, and either declares the program finished (if its own registration
brought `sleeping` up to `n_workers` and no outside submitter holds a promise,
and a final look at `head`/`tail` still shows empty) or `pthread_cond_wait`s on
the shared condition variable.

So the shape of the thing, stated as costs: **every worker acquires one
process-wide mutex twice per task** — once to take it, once on the way back
— and **every push acquires that same mutex and then wakes every sleeping
thread**. A station fanning out to a hundred destinations produces a hundred
pushes, which is a hundred acquisitions of the one lock and a hundred
broadcasts. The lock is also load-bearing for correctness, not just for the
queue: the sleeper count is exact *because* the check and the registration
happen inside a single hold, and that is what makes the termination decision
sound.

(`pool_pop()` exists in the header but workers do not use it. They inline the
take because they need to keep the lock across the decision to sleep. It is
there for tests and for whoever owns the pool.)

Now let me write the design down before describing it further — it's a fixed
requirement now, so it needs a blueprint, and it turns out to be more
foundational than the placement work, so it takes the lower number.

Now the new 107 — the queue structure itself:

## How the two-level structure gets implemented

**The take becomes a walk over an array, not a branch.** Each worker holds an
ordered array of queues it may draw from, and taking a task means walking that
array and taking from the first that yields one:

```
worker i:  sources = [ own queue, shared queue ]
```

That is what "the value that is present when the local one is empty" becomes —
not `if (local empty) global`, but position 1 in a list whose position 0 came up
empty. Two reasons for the array. The project's own rule prefers a table indexed
by position over a chain of conditionals. And more usefully, **the array is the
extension point**: the placement work needs a worker to draw from the queues of
the placements it is eligible for, and under this shape those are more entries
in the same array — `[own, ...eligible placement queues..., shared]` —
rather than a second mechanism sitting beside the first. That's why I put the
queue split at 107 and moved placement to 108; the split is the foundation and
placement is a specialization of it.

**Where a task goes when it is created** is one thread-local read.
`pool_worker_index()` already exists and already returns -1 off-pool, so push
asks who is calling: a worker delivering after a box ran writes into
`sources[0]`, its own queue; anything that is not a worker — a test, the seed
sweep before the gate opens, an outside submitter holding a standing promise —
writes into the shared queue. No new identity mechanism.

**Why the producing worker's own queue is the right destination**, not merely
the convenient one: the task about to be created holds a copy of the value the
box just returned, which this core wrote microseconds ago and which is therefore
sitting in this core's own cache. A task run by this same core reads it from
there. A task handed to another core reads it across the interconnect, and on a
second socket across a slower one. The split is a cache-locality mechanism first
and a lock-contention mechanism second, and both arguments point the same
direction.

**Two things leave the common path.** A worker that produced its own next task
never touches the shared mutex — it pushed to its own queue and it takes from
its own queue; it acquires the shared lock only when it runs dry, which is when
it was about to sleep anyway. And pushing to your own queue wakes nobody,
because nobody else can take from it: today's fan-out of a hundred costs a
hundred broadcasts, each waking every sleeping thread so they can discover that
one task exists among them, and under this shape it costs none.

**One invariant saves the termination rule.** A worker never sleeps holding a
non-empty queue of its own — it drains its own, then looks at the shared one,
then considers sleeping. So if every worker is asleep, every worker's own queue
is empty *by construction*, and the last sleeper's final look still only has to
examine the shared queue: exactly the check that is there today. That holds
**only while local queues are private**. The moment anything else can push into
or steal from a worker's queue, the invariant dies and the last sleeper must
walk every queue in the pool before declaring the program over. Not fatal — a
loop instead of a comparison, run once per program — but it is the price of
that option and the failure mode if it's missed is the one thing 104 exists to
prevent: a program that exits cleanly having silently not done part of its work.

## Two things break, and both need your decision before anything is built

**Fan-out hides work.** A station wired to a hundred destinations produces a
hundred tasks on one worker, all of which land in that worker's private queue
where nobody else can see them, while fifteen other workers sleep. Today those
hundred go to the shared queue and every sleeper is woken to take one. Taken
literally with nothing added, the requirement makes the engine *slower*, and
slower at precisely the pattern that produces parallelism. Three answers:
**spill** past a cap into the shared queue (recommended — it keeps every local
queue genuinely private, which is what keeps it lock-free and keeps the sleeping
invariant); **keep exactly one** locally, the one you'll run next, and push the
rest to shared; or **steal**, which is the general answer and the one that costs
a concurrent structure and the invariant above.

**A push cycle never runs dry.** Cycles are legal and necessary — a box cannot
remember, so a loop through a ring buffer is the only way to carry state. A
worker inside one produces into its own queue, takes from its own queue, and
never looks at the shared queue at all. A task sitting there waits forever while
the machine is fully busy. Answers: take from shared every Nth task, or stamp a
sequence number at push and take the older of the two heads, or accept it and
say so in the guarantees.

## What's on disk

- `issues/107-every-worker-keeps-its-own-queue.md` — new, the queue split
- `issues/108-choosing-where-a-box-runs.md` — the placement issue, renumbered
  from 107 and rewritten where it assumed a queue arrangement it now inherits
- `issues/phase-1-progress.md` — both rows, and the closing paragraph
  rewritten for both
- `docs/implementation-notes/090-one-table-per-processor.md` — its pointer
  repointed

Six open questions in 107, seven in 108. Nothing committed.

--------------------------------------------------------------------------------

### User Request 5

ummmmm we should have each box station specify a thread it wants to deliver to
if it wants. The default should be that there is no thread specified, and it
goes to the default queue as expected. That is the default. It's only if it
wants a SPECIFIC core to accomplish something that we specify. The reason for
this is later, when we add multi-processor support, or even CPU and GPU support,
we'll want some tasks to go to specific locations. Or if we build an abstraction
layer later that allows us to send it over the network. Or if we abstract away
from computers entirely and just use the system for more generic organization.
We can't have the "by default, deliver to our own thread" system.

I love the idea of having multiple queues to sift through though. That means we
can have overlapping priorities or specializations that do not depend on one
another. Excellent!

By the way a box absolutely can remember, just update it's static variables.
That invariant is deprecated and should be removed when we see it.

--------------------------------------------------------------------------------

### Assistant Response 5

Three separate things. Let me take the correction first, then find every place
the box-memory rule is written down.

Three things, and the third has a consequence I want to put in front of you
before I sweep it out of the docs.

Rewriting 107 around the corrected default first:

Now the box-memory rule. Before I sweep it out, three consequences you should
see, because two of them are live:

Now the entry in *not guaranteed*, which is where the retired rule has to land,
and the consequences it drags with it:

Now the other places the rule is stated:

**The default is fixed.** 107 is rewritten around it, and the correction leads
the file rather than hiding in it: a station names nothing by default, and a
program that names nothing anywhere is indistinguishable from the current engine
— one queue, served by everybody, today's order, today's mutex. The old
draft's cache-locality argument is written down as *considered and wrong*, with
your reason: it bakes in the assumption that a destination is a thread in this
process, which is the one assumption the structure exists to avoid. Locality
survives as something a destination can happen to express, opted into, never
assumed.

**A queue is a destination — a queue plus the set of servers that draw from
it.** A server is *anything that takes from a queue and does the work*; today
every one is a worker thread, and the design says so nowhere it can avoid saying
so. That leaves room for a submission ring on a graphics processor, a transport
across a network, or a queue of work for a department.

**The overlapping part is now load-bearing.** Servers subscribe to destinations,
destinations may share servers or share none, and a server's array is ordered
— earlier first, default last. Three things that need no separate code because
they are one structure with different membership:

| written as | is |
|---|---|
| a queue served by one worker | *this work happens on that thread* |
| a queue served by everyone, placed first in each array | *this work happens before ordinary work* — a priority lane |
| a queue served by the four workers on one socket | *this work happens over there* — what 108 computes from a hardware expression |

That also demotes 108 correctly: a placement is not a queue arrangement of its
own, it is a way of *computing* a server set from a description of hardware.
108's costs table is rewritten to match — 107 no longer spends U1 at all,
since its default is served by everybody, so 108 is where that guarantee is
actually sold, and it can't be avoided there because widening a placement is
what a placement forbids.

## The box-memory rule is out

Swept from the guarantees page, the overview, the README, the project notes, the
routing doc, the map-format doc, and two completed issues. Retired rather than
deleted, following the page's own procedure: the line stays, struck through, and
the consequences are stated in *not guaranteed*.

Two of those consequences are live, and I want them in front of you rather than
buried:

**Two invocations of one station run at once, and now they share whatever the
box keeps.** That was B3, and it used to be safe *because* of the rule you just
retired — there was nothing to share. The engine still builds a task the
moment a station's inputs are present and hands it to whichever worker is free,
so two calls to the same C function can be inside it on two cores. A `static`
counter incremented by both is a plain data race, and there is no lock in the
engine that covers it: the station's mutex guards its ports and is released
before the box is ever called. A box that keeps state owns the synchronization
of that state.

**Capturing a running program no longer captures it.** The capture feature shuts
the entrance, drains the pool, and writes out every port value and every
iterator's cursor — and it was feasible *precisely* because there was no
hidden state to miss. A box that remembers has state the capture doesn't know
exists. The artifact looks complete and the revived program has silently
forgotten. That's recorded in the guarantees page, in the completed issue, and
in the phase 7 status line, with the two ways out named and neither taken:
capture learns to ask a box for its state, which means a box gains a second
entry point and stops being a plain C function; or capture says in the artifact
that it doesn't carry box memory.

Nothing committed. Twelve files modified, two new issues, and the transcript.

--------------------------------------------------------------------------------

### User Request 6

> Two invocations of one station run at once, and now they share whatever the
> box keeps. That was
> B3, and it used to be safe because of the rule you just retired — there was
> nothing to share.
> The engine still builds a task the moment a station's inputs are present and
> hands it to
> whichever worker is free, so two calls to the same C function can be inside it
> on two cores. A
> static counter incremented by both is a plain data race, and there is no lock
> in the engine that
> covers it: the station's mutex guards its ports and is released before the box
> is ever called.
>   A box that keeps state owns the synchronization of that state.


the static values can only be modified by a box through the return value of the
function that the box runs.

> Capturing a running program no longer captures it. The capture feature shuts
> the entrance,
> drains the pool, and writes out every port value and every iterator's cursor
> — and it was
> feasible precisely because there was no hidden state to miss. A box that
> remembers has state the
> capture doesn't know exists. The artifact looks complete and the revived
> program has silently
> forgotten. That's recorded in the guarantees page, in the completed issue, and
> in the phase 7
> status line, with the two ways out named and neither taken: capture learns to
> ask a box for its
> state, which means a box gains a second entry point and stops being a plain C
> function; or
>   capture says in the artifact that it doesn't carry box memory.


the box does not remember, the box STATION remembers. Capturing a running
program should still work.

so, the phrasing you have for the "server" and "destination" stuff needs more
explanation. Can you go through it with me?

--------------------------------------------------------------------------------

### Assistant Response 6

Both corrections land in the same place, and they undo the two consequences I
flagged. Let me check the mechanism before rewriting.

Confirmed in the engine: a wire delivering into a static port overwrites it,
built under issue 405, and the write takes the station's own mutex — the same
lock a claim takes. So station memory already exists, it is drawn as an arrow,
and the dump already writes it. Both consequences I flagged were consequences of
*my* wrong reading, not of yours. Undoing them:

Now the other files, including the capture claim I wrongly holed:

Two stale passages surfaced while checking — both say the wire-into-a-static
path isn't built, and it is:

Corrections are in. B1 says what it always said about a box and now says the
other half out loud — **the station remembers, by holding a value the box's
output is wired back into**. B3's row says why that is safe: the write happens
on the delivery path under the station's mutex, never inside the box on two
cores. The capture claim in 712 is restored and strengthened, since station
memory sits on a port and the dump already walks ports. And two stale passages
turned up while I was checking: the statics doc and the map-format doc both
still said the wire-into-a-static path was unbuilt and pending issue 405. It was
built. Both now say so, and both now name the self-arrow as how a station
remembers.

The only thing left in the *not guaranteed* section is a C `static` inside a box
source — not the mechanism, unchecked, raced over by two invocations,
invisible to capture, and absent from the picture.

---

Now the walkthrough. I'll build it from what's actually in the code, because the
vocabulary only makes sense against that.

## What's there now

`pool_t` holds `task_t **slots` with `head` and `tail` — that array *is* the
queue. And `worker_t *workers`, which is N pthreads. Every worker reads that one
array.

Notice what is **not** written down anywhere: *which workers read which queue.*
It doesn't need to be. There is one queue and everybody reads it, so the
relation is a constant and constants don't need storage.

**That relation is the entire new thing.** Everything else in this change is
bookkeeping around it.

## The relation, written two ways

Once there are several queues, somebody has to say who reads what. The same fact
can be stored from either end:

- **per queue** — "these workers read me"
- **per worker** — "I read these queues, in this order"

The code wants the second, because that's the direction the take walks: a worker
asks *what do I read next*, not *who reads me*. But they are one relation, and
if you find one form clearer to think in, that's fine — they're
interchangeable.

## Why I reached for two words

A **queue** is only a container: a ring of task pointers, arrival order,
doubling growth when full. Nothing about a queue says who empties it.

But when a station says *put my tasks here*, it is not naming a container — it
is naming **a place that somebody empties**. A container nobody empties is a
hang, and I put a refusal in for exactly that. So the useful unit is the pair:
the container, plus who empties it. A pair that gets named in a map file needs a
name.

And "who empties it" wanted its own word because today the answer is always *a
pthread in this pool*, and the entire reason you gave for this feature is that
later it might not be — a submission ring on a graphics processor, a sender
that puts the task on a network, a queue of work for people. If the word for
that is "worker," the vocabulary starts lying the first time one isn't a thread.

That's where "server" came from. It may be the wrong word; it was doing that
job.

## Where I went wrong on the other one

**"Destination" is already taken in this engine, and I shouldn't have reused
it.** A destination here is *a wire's far end*: in `out 0 - printer.0`, the
destination is station `printer`, port 0. There is a `destination` struct and a
`dest_set` struct in the station header, and issue 214 is literally titled
*destinations without a lock*. Putting a second meaning on that word inside the
delivery path — which is where both meanings would live — is the kind of
collision that costs somebody an afternoon two years from now.

So: the concept is right, the word is wrong, and I flagged it at the bottom of
107 rather than in front of you, which was the wrong place for it.

## What a task's journey looks like, before and after

Today, when a box returns:

1. Delivery walks the output port's wire-ends.
2. For each, it writes the value into that station's port and runs the readiness
   check.
3. If that station is now ready, it claims one value per port and builds a task.
4. It pushes the task — to the one queue.
5. Some worker takes it.

After, steps 1–3 are untouched. Step 4 asks **the station being started** one
question — *where do your tasks go?* — whose default answer is *the ordinary
place*. Step 5 becomes *a worker takes it from the first of its queues that has
anything*.

One question and one walk. That's the whole mechanical change.

## The overlap, concretely

Eight workers, numbered 0–7:

| queue | worked by | is |
|---|---|---|
| ordinary | all eight | the default; every station uses it unless it says otherwise |
| urgent | all eight, **placed first** in everyone's list | a priority lane |
| gpu-feed | worker 3 alone | a specialization |
| socket-1 | workers 4, 5, 6, 7 | a placement, which is what 108 computes from `on package 1` |

And the per-worker lists that fall out:

```
worker 0:  [ urgent, ordinary ]
worker 3:  [ urgent, gpu-feed, ordinary ]
worker 5:  [ urgent, socket-1, ordinary ]
```

Worker 3 belongs to three, worker 0 to two, and **none of the four queues knows
the others exist**. A priority, a specialization and a hardware placement are
the same structure with different membership and different array positions —
which is the thing you said you liked, and the reason none of them needs its own
code.

## What I'd like your read on

**The word.** `lane` fits the project's vocabulary — you work a lane, a lane
can be a fast one, "the GPU lane" reads right — and collides with nothing.
`desk` also fits the light-industrial family the engine already uses (boxes,
stations, ports) and carries the staffing sense: a desk is staffed by some
number of people. `channel` is available but arrives loaded with meaning from
other systems.

**And whether "server" needs to exist yet at all.** The alternative is to keep
saying *worker*, accept that today every one is a thread, and introduce a second
word only when the first non-thread one actually appears. That's cheaper now and
costs a rename later.

Which part do you want to push on — the naming, or the relation itself?

--------------------------------------------------------------------------------

### User Request 7

okay can we make a note at the top of these issue files (nothing's been
implemented right? just issue files written?) that we should confer with the
user explicitely before doing any of them?

--------------------------------------------------------------------------------

### Assistant Response 7

Confirmed — nothing implemented. Every changed or new file is markdown: docs,
issue files, the readme, the project notes, and transcripts. No `.c`, no `.h`,
no Makefile, no script.

Adding the note to both:

Done. Nothing has been implemented — I checked, and every file touched across
this whole conversation is markdown. No `.c`, no `.h`, no Makefile, no script.

The note sits at the top of both issue files, right under the title, and says
what it's warning about rather than just saying *ask first*:

**107** — nothing exists in code; the design has already had its central
default reversed once (a worker delivering into its own queue, which assumed a
destination is a thread in this process); and the vocabulary is unsettled, since
*destination* collides with the wire's far end that the engine already calls a
destination.

**108** — nothing exists in code; it's downstream of 107, which is itself
unsettled; two of its own decisions have already been reversed (the pool's
placement being handed out by a creator that turned out not to exist, and the
question underneath it about what *shared memory* means); and seven questions
are still open at the bottom.

I also put a matching paragraph at the end of the phase 1 progress page, so
somebody arriving at the status list rather than at the issues gets told the
same thing.

--------------------------------------------------------------------------------
