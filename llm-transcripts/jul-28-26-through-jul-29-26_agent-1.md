# Conversation Summary: 2eb7c935-cb59-4810-bc12-c0640c1c23d1

Generated on: 2026-08-19 14:34:49

--------------------------------------------------------------------------------

### User Request 1

hi, can we work on this issue?


● Found them — five new untracked files in the project root, actually. Three
are about presentation (issue-2: story-form demos; new-issue-3: a browser canvas
that builds maps and generates source; new-issue-4: slideshow plus transcripts
in the HTML docs). The two that reach into the

  engine's guts — and I'll assume these are the two you mean, since they're
  the ones where integration is the real question — are the worker-side
  gathering proposal (new-issue-please-sort) and the station-table growth design
  (update-to-issue-704). Here's my read on each. I haven't

  sorted or built anything yet; this is the discussion.


  ---

  1. Worker-side gathering


  What it changes, in one sentence: today the gathered values are pulled when
  the task is assembled, by whichever thread delivered the completing value; you
  want them pulled when the task begins running, by the worker that picked it
  up.


  How it works now. When a delivery completes a station's input set, the
  delivering thread claims the buffered values under the station's lock,
  releases the lock, and then builds the task — and during that build it runs
  every gathered source inline, on its own stack, before the task

  ever reaches the pool. So a burst that enqueues a hundred tasks pays a hundred
  gathers serially on the delivering thread. The first-pass report caught
  exactly this: our phase 4 demo's timing was wrong at first because seeded
  values paid their gather cost on the seeding thread,

  before the measured window even opened. Your proposal parallelizes that cost
  across the pool and shortens the hot path. It also makes values fresher —
  read at the moment of use rather than at the moment of scheduling — which is
  arguably more faithful to the pull path's own

  philosophy than what I built.


  The good news: your "generator function" already exists, generically. The
  issue worries that each box might need its own gather-filling code since the
  same function can be wired three different ways at three stations. But wiring
  lives on the station, not the box: each slot record

  carries a one-byte kind tag and, for gathered slots, the source station index
  (an int). The task already carries which station it belongs to (an int32). So
  one engine function can do the fill for every box that will ever exist: walk
  the station's slots, and for each slot tagged

  "gathered," run its source inline into the task's already-reserved value
  bytes. No per-box generation, no macro — the same trick as the readiness
  dispatch.


  What the task struct needs (current fields, plus the two candidates):


  ┌─────────────────┬──────────────────────────────────────┬────────────────────────────────────────────────────────────────────────────┐

  │ field │ type │ status │

  ├─────────────────┼──────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ call │ function pointer, void (*)(task_t *) │ exists — the shim │

  ├─────────────────┼──────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ station │ int32_t │ exists │

  ├─────────────────┼──────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ port │ int32_t │ exists │

  ├─────────────────┼──────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ n_in / in / out │ int32_t, void **, void * │ exist — value pointers
  into the task's own tail block │

  ├─────────────────┼──────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ prepare │ function pointer, or a one-byte flag │ new — null/zero
  when nothing to gather; the worker runs it before the shim │

  ├─────────────────┼──────────────────────────────────────┼────────────────────────────────────────────────────────────────────────────┤

  │ map │ pointer to the owning map │ new — and this is the catch │

  └─────────────────┴──────────────────────────────────────┴────────────────────────────────────────────────────────────────────────────┘


  The catch is that last row. The fill function must reach the station table,
  and a task carries no route back to its map. Statics writes and statistics
  already hit this same wall and got the process-global "active map" workaround,
  with its silent one-map-per-process restriction.

  This issue is the third customer, and I'd say it tips the decision: thread the
  map pointer through the task (second-pass priority #2 in the report) as part
  of this work, rather than leaning on the global a third time.


  What gets replaced or updated: the task struct and one line of the worker loop
  (run prepare, then call); the task-construction step in the delivery file
  (stop pulling gathers there, set the prepare marker instead); the gather
  module's timing attribution (still charged to the

  pulling station, but now measured on the worker — which actually simplifies
  the story issue 702 tells); the seed sweep gets faster for free (seeded
  gather-heavy stations currently pay all pulls serially on the load thread);
  docs 004's sentence "at the moment a task is being

  assembled" becomes "at the moment the invocation begins"; and the freshness
  test's phrasing, though its assertions still hold.


  One hazard I found while thinking this through — an existing latent race,
  not one you'd introduce: the runtime gather-repoint added in phase 7 mutates a
  slot's source under the station's lock, but the gather walk reads that source
  without any lock. Today the window is small;

  deferring the read to the worker widens it to the full queue latency. Whatever
  we choose has to make "read the gather wiring" and "repoint the gather wiring"
  agree on a lock or an atomic. Deferred gathering also changes what a repoint
  means: a task enqueued before the rewire would

  gather from the new source when it runs. I think that's correct — "true at
  the moment used" — but it's a semantics decision, not an accident, so it
  should be written down.


  Questions back at you:

  - Should statics defer the same way? They're claimed at assembly too, under
    the statics-table lock — same hot-path cost, and deferring both keeps the
    rule simple ("ring values are claimed at readiness; everything else resolves
    at run"). But then a threshold written mid-run reaches

  tasks already queued, not just tasks assembled afterward. Which behavior do
  you want a turned knob to have?

  - Is deferral the only mode, or per-slot? A slot could conceivably want
    assembly-time gathering when it needs the value as-of-scheduling. If
    per-slot, the map format needs a word for it, which touches parser, loader,
    and dump.


  ---



❯ so what you're saying is that instead of running gather functions when we
get the task from the task pool, we should instead create tasks for any upstream
gather functions and put them in the thread pool, pointing their outputs at the
slot that we're creating for the enqueue'd

  task? Luckily statics we can just fill in directly. And, that we shouldn't
  enqueue a given task while it has input-only gather tasks still incomplete,
  and should instead build out the task almost entirely, and give ownership of
  it to the gatherer handler utility on the task struct

  which is generated for each task with gatherer tasks, and upon completion of
  all the gatherer handler utility's array of gather boxes to run in order, it
  will not only fill in the remaining unknowns that the original enqueue'ing
  thread has long since moved past, but it will also

  enqueue the task that needed gathering. Essentially, when we need to enqueue a
  task with non ring-buffer inputs, we first fill in everything we can (popping
  from ring buffers if any) and then we hand off the completion of the
  enqueue'ing task to another thread by putting a

  specialized "gather" task which runs the gatherer handler utility and such.
  Are we on the same page? We should come back to 704 after working through
  this.



● Almost the same page — but you've actually gone a step past what I
proposed, and your version is better. Let me draw the line between the two
precisely, because the difference matters for what gets touched.


  What I had sketched: one trip through the pool. The incomplete task goes into
  the queue immediately; the worker that picks it up runs a "prepare" step
  (gather everything) and then the box, in one pickup. That needs a new field on
  the task struct and a new line in the worker loop —

  the pool learns something.


  What you're describing: two trips. The enqueueing thread pops the ring
  buffers, copies the statics, and then — instead of pushing the box task —
  pushes a different, specialized task whose job is to finish the first one. A
  worker picks up that gather task, runs the gather boxes in

  order, writes their outputs into the waiting task's input slots, and then
  pushes the now-complete task itself. The delivering thread moved on long ago,
  exactly as you say.


  Yours wins on three counts, and one of them dissolves a problem I flagged last
  time:


  1. The pool stays completely ignorant. A gather task is just a task — the
     pool runs its function pointer like any other. No prepare hook, no new
     worker-loop branch. The specialization lives entirely inside what the
     gather task's call points at.

  2. The box task stays completely ordinary. By the time it's enqueued it is a
     plain, fully-materialized task, indistinguishable from one that never had
     gather inputs. Tasks with only ring and static slots take today's exact
     path with zero added cost.

  3. The context problem evaporates — locally. My version needed the task to
     carry a map pointer (or lean on the process-global active map a third
     time). In yours, the wrapper is built by the engine at claim time, so the
     engine can stash the map pointer inside the wrapper itself. No

  global, no task-struct change, no pool change. The larger "engine has no
  context" thread still exists for statistics and statics writes, but this
  feature no longer forces it — which reorders my earlier sequencing advice:
  gathering no longer waits on that work.


  Concretely, the gather task would be a small struct with a task embedded at
  its head — the same trick the phase 1 tests use, so the pool cannot tell:


  ┌───────────┬───────────────────────────────────┬────────────────────────────────────────────────────────────────────────────────────────────────────────┐

  │ field │ type │ meaning │

  ├───────────┼───────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────┤

  │ base │ task struct (embedded first) │ its call is the gatherer
  handler; its station field is a sentinel, minus one — "belongs to no
  station" │

  ├───────────┼───────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────┤

  │ map │ pointer to the owning map │ how the handler reaches the station
  table; carried here, not globally │

  ├───────────┼───────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────┤

  │ pending │ pointer to the waiting task │ the task being completed;
  ownership is here, as you said │

  ├───────────┼───────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────┤

  │ n_gathers │ int │ how many slots below │

  ├───────────┼───────────────────────────────────┼────────────────────────────────────────────────────────────────────────────────────────────────────────┤

  │ gathers[] │ array of { slot int, source int } │ which input slots to
  fill, from which upstream stations, in order │

  └───────────┴───────────────────────────────────┴────────────────────────────────────────────────────────────────────────────────────────────────────────┘


  One clarification on your phrase "generated for each task": this is
  instantiated per task, not code-generated per box. No generation is needed —
  the wiring lives on the station's slot records, and the waiting task already
  reserves the right number of bytes for every input, so one

  handler function serves every box that will ever exist: walk the array, run
  each source inline (chains recurse exactly as they do today, just on the
  worker's stack now), copy each result into the pending task's slot, then push
  the pending task.


  Now the details that need deciding — the "what gets replaced" of it:


  The free-after-run contract is why the wrapper should be a separate
  allocation. The worker loop today is unconditional: run the task, hand it to
  the delivery hook, free it. Your phrase "the gatherer handler utility on the
  task struct" could be read as the pending task carrying its

  own handler and re-pushing itself — one allocation, the task's call swapped
  from handler to shim between trips. That's elegant, but a self-re-enqueueing
  task gets freed by the worker that just ran its gather phase, while it sits in
  the queue — the pool would need a "don't free me

  this time" signal, which breaks the one contract it has. The separate wrapper
  keeps it whole: the wrapper is run, finished, and freed normally; the pending
  task is pushed by the handler and freed after its run. Cost: one small malloc
  per gathered task. The delivery hook needs one

  line — a task whose station is the sentinel is machinery, not a box run:
  skip delivery, skip the run counter.


  Take the gather snapshot under the station's mutex, during the claim. This
  closes a latent race I found last time: the runtime gather-repoint mutates a
  slot's source under the station lock, but today's gather walk reads it with no
  lock. If the claim step — which already holds the

  mutex — copies the {slot, source} pairs into the wrapper right there, the
  race is gone entirely, and the semantics become crisp: wiring as of
  scheduling, values as of gathering. A repoint after enqueue doesn't touch
  tasks already in flight.


  Serial or parallel gathers per task? You settled on "array of gather boxes to
  run in order" — one wrapper, gathers run serially on one worker. I agree for
  the first cut: the win we're buying is "the delivering thread pays nothing,"
  and that's fully bought. The alternative — one

  pool task per gather slot, outputs aimed at the pending task's slots, last one
  to finish enqueues it — is exactly your opening sentence, and it
  parallelizes within one task's assembly, but it needs an atomic countdown on
  the pending task and a rule for who pushes. Worth having as a

  later option for tasks with several slow gatherers; not worth the machinery
  now, since gathers across different tasks already parallelize (each task's
  wrapper lands on its own worker).


  Statics: agreed, and it settles my earlier question. Filled directly at build
  time, under the statics lock as today — so a threshold written mid-run is
  seen by the next task built, not by tasks already queued. That keeps the
  existing mutation semantics and test exactly as they are.


  Smaller consequences, so nothing surprises us: queue traffic doubles for
  gathered tasks (two entries instead of one — the ring doubles itself,
  demonstrably cheap); a gathered task waits in the FIFO twice, so its latency
  rises while throughput doesn't; assembly order across tasks is

  no longer gather order, which nothing ever promised; the seed sweep gets
  genuinely faster, since seeded gather-heavy stations currently pay every pull
  serially on the load thread before release; the gather-time statistic moves
  into the handler and stays charged to the pulling

  station — the attribution story actually gets simpler; and a gatherer that
  aborts (missing file) now dies on a worker instead of the deliverer, which is
  the same program-stopping behavior from a different thread. Docs 004's "at the
  moment a task is being assembled" becomes "when

  its gather task runs," and the completed gathering issues get their
  current-behavior sections updated.


  Two questions before this becomes a numbered issue file:


  1. Wrapper as separate allocation with the sentinel-station rule — agreed?
     It's the version where the pool's run-finish-free contract stays untouched;
     the price is one small allocation per gathered task.

  2. Do you want the parallel-per-slot variant designed into the issue as a
     stated future option (the atomic-countdown shape), or left out entirely
     until a real map has several slow gatherers on one station?


  And noted on 704 — the station-table discussion stays parked until we've
  landed this one.



  One clarification on your phrase "generated for each task": this is
  instantiated per task, not code-generated per box. No generation is needed —
  the wiring lives on the station's slot records, and the waiting task already
  reserves the right number of bytes for every input, so one

  handler function serves every box that will ever exist: walk the array, run
  each source inline (chains recurse exactly as they do today, just on the
  worker's stack now), copy each result into the pending task's slot, then push
  the pending task.


can you explain this part a bit more? I was thinking that, if there's multiple
gather tasks (which might be nested) then we should collect them and iteratively
go through them one-by-one in the worker who's picking up the "gather" tasks. It
sounds like what you're suggesting is that we make one task pool task for each
gather box that is upstream of a box that has partial ring buffer and gather
inputs? Is that correct? Can you compare both approaches and tell me the
pros/cons?


  The free-after-run contract is why the wrapper should be a separate
  allocation. The worker loop today is unconditional: run the task, hand it to
  the delivery hook, free it. Your phrase "the gatherer handler utility on the
  task struct" could be read as the pending task carrying its

  own handler and re-pushing itself — one allocation, the task's call swapped
  from handler to shim between trips. That's elegant, but a self-re-enqueueing
  task gets freed by the worker that just ran its gather phase, while it sits in
  the queue — the pool would need a "don't free me

  this time" signal, which breaks the one contract it has. The separate wrapper
  keeps it whole: the wrapper is run, finished, and freed normally; the pending
  task is pushed by the handler and freed after its run. Cost: one small malloc
  per gathered task. The delivery hook needs one

  line — a task whose station is the sentinel is machinery, not a box run:
  skip delivery, skip the run counter.



can you explain this a bit more too?


I think we should try and laser focus our terminology, it's easy to lose track
of what we're talking about when we use synonyms. For this conversation can you
try and be more rigid and systematic? Just to try it out.


--------------------------------------------------------------------------------

### Assistant Response 1

Grounded in the current source, and using one name per thing throughout. Here's
the vocabulary I'll hold to:


| term | what it is | type |

|---|---|---|

| **box** | a plain C function someone registered | — |

| **station** | one placement of a box in a map; owns slots, mutex, ports | station struct in a flat array, addressed by `int32_t` index |

| **slot** | one input of a station; kind is ring / gather / static | slot struct, `unsigned char kind` + `int source` |

| **task** | the pool's unit of work: a function pointer plus freight | task struct — `call`, `station`, `port`, `n_in`, `in`, `out` |

| **box task** | a task whose `call` runs a station's box (every task that exists today) | task struct |

| **gather task** | the proposed new kind: a task whose `call` finishes a box task | its own struct, task struct embedded first |

| **gather list** | which slots to fill and from where | array of `{ int slot; int source_station; }` |

| **gather handler** | the one C function a gather task's `call` points at | `void (*)(task_t *)` |

| **the inline pull** | today's recursive routine that runs an upstream station on the caller's stack | — |


---


## 1. What I meant, and the two words that misled you


**No — I am not proposing one pool entry per gather slot.** Your reading of my
paragraph is the opposite of what I meant, and the fault is two ambiguous words.


**"generated."** That sentence wasn't about scheduling at all. It was answering
a worry in your issue file: that each box might need its own bespoke
gather-filling code, the way each box needs its own generated shim. It doesn't.
The gather list is *data* read off the station's slot records at claim time, so
one hand-written gather handler serves every box that will ever exist.
"Instantiated per task, not code-generated per box" — that was the whole point
of the paragraph. It says nothing about how many pool entries are involved.


**"run each source inline."** By *inline* I meant the existing inline pull — a
plain recursive C call on whatever stack is running, no pool involved. In my
sentence, that stack is the worker running the gather handler. So: one gather
task per box task, its worker walks the gather list one entry at a time, and
each entry is a direct function call that recurses for nested chains.


That is exactly what you described. We agree; I described it badly.


---


## Comparison anyway, since you asked


**Design A** — one gather task per box task. Gather list holds the station's
top-level gather slots; the worker loops over them; nesting recurses on that
worker's stack.


**Design B** — one gather task per gather slot. Each computes one slot's value
directly into the box task's reserved bytes; an atomic countdown on the box
task; the last one to finish pushes it.


| | **A: one gather task, serial list** | **B: one gather task per slot** |

|---|---|---|

| pool entries per box task | 2, always | 1 + N, where N = gather slots |

| allocations per box task | 2 | 2 + N (each gather task is its own malloc) |

| synchronization added | none — a `for` loop | atomic countdown on the box task, plus a "who pushes it" rule |

| nested chains | recursion, unchanged from today | either still recursive (so only the top level parallelizes — a half-measure, since depth is where serial cost accumulates) or flattened, which means a second dependency scheduler |

| side-effect order across a station's gatherers | slot order, deterministic, identical to today | nondeterministic — two gatherers reading one clock or one file no longer land in a known order |

| latency of one station's gathering | sum of its gatherers | slowest of its gatherers |

| the common case (one gather slot, cheap gatherer) | 2 entries, 2 allocations | 2 entries, 2 allocations — identical, but you paid for the countdown machinery to get there |

| a gatherer that aborts | dies on one worker, siblings not yet started | dies on one worker while siblings are mid-flight and a countdown will never reach zero |


The parallelism B buys is *already there across tasks* — every box task gets
its own gather task, so a hundred queued tasks gather on every core
simultaneously. B only helps **within one station's assembly**, which requires
one station to have several genuinely slow gather slots. Nothing in any current
map looks like that. And B's cost is not just the countdown: it's queue traffic
and malloc traffic scaling with slot count while the work per entry is often a
clock read.


**A now.** B written into the issue as a stated future option, with a trigger
condition expressed in a number the engine already measures: the per-station
gather-time counter, read against the deepest-gather-chain figure the map
records at wiring time. When one station's gather time is large and its chain is
shallow-but-wide, B is the answer. Not before.


## The nesting nuance, which is the real reason A is simple


You said "collect them and iteratively go through them one-by-one." For the *top
level* of one station, that's exactly right. For *nested* chains, there's a
hidden cost to flattening that recursion pays for free.


When the inline pull runs an upstream station, it builds that station's input
values in a stack frame — a variable-length array sized to that station's
inputs — and a borrowed task struct pointing at it. A gathered station three
levels deep needs somewhere to put its own gathered arguments before it can run.
Recursion gives you that storage automatically, one frame per level, released on
the way back out. A flattened list would have to allocate those intermediate
frames explicitly and track which flattened entry feeds which other entry's
argument slot — a dependency graph, in the middle of a machine whose entire
job is already dependency graphs.


So: **flat list for the station's own gather slots, recursion for everything
below them.** The gather list is one level deep on purpose.


---


## 2. The free-after-run contract


The worker loop is three unconditional steps per pool entry, with no branches
(libs/012-pool.c:264):


```

t->call(t);              /* run it */

p->finish(ctx, t);       /* hand it to the delivery walk */

free(t);                 /* give the memory back */

```


That is the pool's *entire* relationship with the station layer. Its header
states the premise plainly: the pool reads exactly one field — `call` — and
treats everything else as freight. Whatever goes in must be a malloc base
pointer, runs once, and is dead when the worker moves on.


**The trap.** Your phrasing "the gatherer handler utility on the task struct"
can be read as: the box task carries its own handler, goes through the pool
twice, and swaps its `call` from handler to shim between trips. One allocation,
very tidy. But trace it against those three lines. First pickup: the handler
runs, fills the slots, calls `pool_push` on *itself*, returns. Control comes
back to the worker — which then calls the delivery hook on it and **frees
it**, while its pointer is sitting in the queue waiting for a second pickup.
Use-after-free, and the corrupted read happens on a different thread later,
which is the worst possible place to debug it.


To make one-allocation work, the worker needs to conditionally skip
finish-and-free — a return value from `call`, or a flag on the task. Either
way the pool now knows that tasks have phases. That is a station-layer concept
installed inside the one module whose value is that it doesn't have any.


**The separate allocation.** Two allocations, two ordinary lifetimes, zero pool
changes:


- The **gather task** is its own malloc, with the task struct embedded as its
  first member — the same trick the phase 1 tests already use, and the pool
  provably cannot tell. It is popped, run, finished, freed. Normal life.

- The **box task** is built by the delivering thread exactly as today, minus the
  gather resolution. It is pushed by the gather handler, then popped, run,
  finished, freed. Normal life.


I checked whether the two could share one malloc to save a call: they can't. The
worker frees precisely the pointer it popped, and `free` demands a malloc base.
Two pool entries means two base pointers means two allocations. The cost is one
small malloc per box task that has at least one gather slot — the gather task
is the task struct, a map pointer, a box task pointer, a count, and eight bytes
per gather slot. Roughly sixty-odd bytes plus the list.


**Why the map pointer lives in the gather task.** The delivery hook receives the
map for free — it's the pool's finish context, set once at creation. But
`call` receives *only* the task. The gather handler needs the station table to
run the inline pull, so the map pointer rides in the gather task's own struct.
That is why this design doesn't need the process-global active map, and doesn't
need the task struct to change shape.


**The one line in the delivery walk.** The finish hook runs on every task,
including gather tasks. It currently increments the station's run counter and
then indexes the station table — so a gather task must be recognized *before*
that index happens. One branch at the top: a task whose station index is below
zero is machinery, not a box invocation — nothing to count, nothing to route,
return. The gather task's other freight fields sit unused: no inputs, no output.


**One thing that could have been a bug and isn't.** The pool decides the program
is over when the last worker falls asleep and finds an empty queue; anything
pushing from outside must register, or the pool can declare completion between
two pushes. The gather handler pushes from *inside* a worker that is awake and
mid-run, so it can't be counted asleep at that moment. No registration needed.
Worth writing into the issue explicitly, because it's exactly the sort of thing
someone later assumes is broken and "fixes."


---


Where that leaves us: we agree on the shape. Design A, separate allocation,
sentinel station index, gather snapshot taken under the station's mutex during
the claim. Two things still open before I'd write the issue file — do you want
the per-slot parallel variant written in as a named future option with its
trigger condition, or left out entirely? And should the gather list capture only
gather slots, or every non-ring slot, given you've already settled that statics
stay filled directly at build time?


--------------------------------------------------------------------------------

### User Request 2

  ┌─────────────────┬───────────────────────────────────────────────────────────────────────────────┬────────────────────────────────────────────────────────────┐

  │ term │ what it is │ type │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ box │ a plain C function someone registered │ — │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ station │ one placement of a box in a map; owns slots, mutex, ports │
  station struct in a flat array, addressed by int32_t index │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ slot │ one input of a station; kind is ring / gather / static │ slot
  struct, unsigned char kind + int source │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ task │ the pool's unit of work: a function pointer plus freight │ task
  struct — call, station, port, n_in, in, out │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ box task │ a task whose call runs a station's box (every task that
  exists today) │ task struct │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ gather task │ the proposed new kind: a task whose call finishes a box
  task │ its own struct, task struct embedded first │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ gather list │ which slots to fill and from where │ array of { int
  slot; int source_station; } │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ gather handler │ the one C function a gather task's call points at │
  void (*)(task_t *) │

  ├─────────────────┼───────────────────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────┤

  │ the inline pull │ today's recursive routine that runs an upstream
  station on the caller's stack │ — │

  └─────────────────┴───────────────────────────────────────────────────────────────────────────────┴────────────────────────────────────────────────────────────┘



almost, not quite. The input slot is the physical memory where the input value
is stored, the input port is a datastructure that holds the slot but also has
the interface to the task giving the input. It holds other data too like the
ring buffer if present, and details about the gather tasks if any. The slot is
just the memory where the actual value is placed, and it's only really present
in the task struct - not the box, not the box station, just the task struct
which is in the thread pool, and whose ownership is given to the worker task
(must be in shared memory of course).


Something to note is that we have output ports, but not output slots. We don't
store the return value in the task struct at all, we simply write it to a
destination that is held in the output port.


... okay can we make a document that describes a potential design where there's
three options per station, and option 1 is that we gather the gather values when
we're enqueue'ing that particular station, option 2 is where we create another
thread pool task for the gathering (as we're discussing now) and the third
option is that we gather at the start of the function, when it's picked up from
the thread pool? I can see situations where all three would be beneficial. Idk,
maybe we could invent some sort of heuristic that determines which is better to
use at runtime? Put ideas for that in the speculative design option note, and
since this is a library that is intended to be built upon when implemented (this
is the minimal implementation) let's just pick one and go with it (whichever one
we have already built is fine I guess). Since this is intended to be built upon
for the implementation, maybe... Aw nuts maybe we need all three. I don't know!
I want it to be minimal! What do you think? Which option has the fewest
contradictions or considerations? Which do you think would be performant in
which types of tasks, can you make me a table that compares them?


--------------------------------------------------------------------------------

### Assistant Response 2

Two corrections absorbed, and one of them has teeth. Let me check the docs
layout before writing.


Both corrections land, and the second one has a consequence I didn't see last
turn. Writing the document now.


Written to `docs/056-gather-timing.md`, linked from the table of contents, index
counter bumped to 056.


## On your correction


Ports and slots are now used strictly in the document: a **port** is the
standing interface on the station (ring buffer, or upstream index, or statics
entry), a **slot** is the transient bytes one value occupies inside one task in
shared memory. The source currently uses one word for both — that's a naming
debt across the station header, delivery, gathering, statics, loader, and dump,
worth doing deliberately rather than as a side effect of this.


One place your model and the code disagree, and I think the code is right: **the
task does keep the return value.** It has to, because the port isn't chosen
until *after* the box returns — a comparator picks its exit by examining the
returned value — and a port with several destinations writes the same value
into each. So there's a gap between "the box returned" and "a destination has
it" that needs somewhere to put the value. I've named those bytes a **staging
buffer** rather than an output slot, precisely because nothing rests there: by
the time the worker frees the task, the value has already been copied into every
destination's input ring.


## The comparison


| | **1 — at enqueue** | **2 — own task** | **3 — at pickup** |

|---|---|---|---|

| exists today | **yes** | no | no |

| queue entries / allocations | 1 / 1 | 2 / 2 | 1 / 1 |

| changes to the pool | none | none | none |

| changes to the delivery walk | none | one branch | none |

| gather cost falls on | the deliverer | a worker | a worker |

| **delays readiness propagation** | **yes** | no | no |

| value age when the box runs | one queue traversal | one queue traversal | ~zero |

| queued task is self-contained | **yes** | yes | no |

| a queue entry is always an invocation | **yes** | no | yes |

| extends to per-slot parallel gathering | no | **yes** | no |

| lines to build | 0 | ~90 | ~50 |


**Fewest contradictions: option 1.** Each option breaks exactly one thing the
project has already written down — and option 1's is a *sentence* while the
others' are *structures*. Option 1 breaks gathering's stated reason for existing
(values are fresh as of scheduling, not as of use). Option 2 breaks the pool's
opening line, "a task is one invocation" — some queue entries become
machinery. Option 3 breaks the task's guarantee that it depends on nothing
another thread can change.


**Performance, by map shape:** cheap gatherers (clock, environment variable) →
option 1, since the gather is shorter than the machinery to offload it. Slow
gatherers, wide fan-out, deep backlogs, freshness-critical values, deep chains
→ option 3. Several independently slow gatherers on *one* station → option
2, the only shape that extends to gathering them simultaneously.


The row I'd weight heaviest is *delays readiness propagation*. Delivery's own
comment says a worker walking a hundred destinations is acceptable because each
delivery may unblock a station — it's manufacturing parallelism for everyone
else. Option 1 puts a file read in the middle of that walk. That's not a
throughput cost, it's a **discovery** cost, and discovering ready work is the
only mechanism this engine has for creating concurrency at all.


## What I'd do


Ship option 1, name all three, implement one — a field on the station and a
three-row dispatch table with two rows null. That's the house style, and it's
paid off twice already: slot kinds carried inert gather and static rows from
phase 2 to phase 4, station kinds carried inert comparator and iterator rows
from phase 2 to phase 5, and both arrived as a row rather than a restructure.
Cost is one enum, one field, one table.


**And when a second strategy does get built, it should be option 3, not option
2** — which reverses what I recommended before all three were side by side.
Option 3 gives everything option 2 was wanted for at half the queue entries and
half the allocations, with no reserved station index and no branch in the
delivery walk. It avoids touching the pool by allocating the task as a larger
struct with the task struct first and pointing its function pointer at a generic
gathering shim — the same trick the pool's header already documents phase 1's
tests using. And it's *fresher*, because option 2 makes a gathered value sit
through a full queue traversal between being read and being used.


One thing worth fixing regardless: snapshot the gather list into the task during
the claim, under the mutex that's already held. Runtime repointing currently
races with an unlocked read of the wiring; options 2 and 3 would widen that
window from a few instructions to the full queue latency. Doing it now makes the
rule sayable in one line — wiring as of scheduling, values as of gathering.


The speculative section has five heuristics, ordered: declared in the map file,
fan-out width computed at load, measured gather cost with hysteresis, queue
depth at claim time, and last — the strategy field as a statics entry, so a
map can contain a controller box that turns the knob itself, and the engine
ships with no policy at all.


I haven't committed. Your working tree has five untracked files that aren't mine
(the three presentation issues, the gathering proposal, and the 704 update) —
say the word and I'll stage only the two documentation files.


--------------------------------------------------------------------------------

### User Request 3

This table is very helpful to me. Can you ensure that it's in the document you
just made?


Also, we should have a directory in the docs for "implementation notes" where we
put ideas like this, for considerations that people can have for their specific
use-cases and our ideas for how to implement them.


let's stick with 1 for now. The reason is, this way the values are all gathered
at enqueue time. It also ensures that all tasks of a given box will run in the
same amount of time (as long as their internals aren't variable in execution
speed). I think this "guarantee" if we can guarantee such things will be helpful
- the more guarantees we can enumerate, the more helpful and useful our code
will become.


--------------------------------------------------------------------------------

### Assistant Response 3

Three things: fold the table in, make the directory, and record the reasoning.
Let me check the HTML generator first, since a new subdirectory under `docs/`
may not be picked up.


Done. Both tables are in the document now — the one you found helpful,
verbatim, plus a second smaller one below it for the build-shape rows that
didn't make the cut (new struct, reserved station index, worker yielded, timing
attribution).


**`docs/implementation-notes/`** exists, with the document moved into it and a
`README.md` stating what belongs there: decisions where more than one answer was
defensible, the reasoning behind the one taken *including the arguments against
it*, and extension points the minimal implementation deliberately left as a
named row rather than a built feature. Three rules written down — say what is
built, enumerate guarantees, argue the other side. The document was never
committed, so there's no earlier path in git to preserve.


The HTML generator's docs listing was **not** recursive, so the subdirectory
would have been silently invisible. Added a separate pass for it with its own
sidebar heading, with a comment saying why it's separate: the numbered documents
are a reading order, and folding a subdirectory into that sequence would break
it. Regenerated — 80 pages, zero unresolved references, so all the `../` links
from the new depth resolve.


## On your reasoning


Recorded as the deciding argument, with one nuance worth knowing: the guarantee
you named is real but it's shared with option 2, since option 2's box tasks also
run pure box time. What's **unique to option 1** is a stronger version that
follows from the same instinct —


**All of an invocation's values are read at one instant.** Ring values popped
under the mutex, statics copied, gathers pulled — one window, one thread,
before the task exists. So a box that gathers a clock and a file gets them from
the *same moment*, not from two moments separated by however long the queue
happened to be. Options 2 and 3 both smear that read across the queue latency.
That one is only purchasable with option 1, and I think it's the deeper
guarantee.


Four are enumerated in the document: uniform run time, single-instant reads,
every queue entry a fully-formed invocation, deterministic gather order — each
marked with which other options keep it.


And the honest cost, written down rather than left to be discovered: option 1
doesn't remove the timing variance, it **relocates** it. A forty-millisecond
gatherer still takes forty milliseconds; it takes them inside some other task's
delivery walk. So the guarantee's scope is the *run*, not the worker's whole
occupancy — option 1 makes box time uniform by making delivery time variable.
Good trade, since box time is what a map author controls and what the statistics
report, but it shouldn't be a surprise later.


Given you want guarantees enumerated as a general practice: there are probably a
dozen scattered across the existing documents and source comments — a box
can't remember, a task is self-contained, a station never moves once placed, the
map is acyclic by induction, the queue never blocks on user code. Want me to
collect them into a single page of things that are always true, with what each
one costs? That would be a document, not an issue.


--------------------------------------------------------------------------------

### User Request 4

  The HTML generator's docs listing was not recursive, so the subdirectory would
  have been silently invisible. Added a separate pass for it with its own
  sidebar heading, with a comment saying why it's separate: the numbered
  documents are a reading order, and folding a subdirectory

  into that sequence would break it. Regenerated — 80 pages, zero unresolved
  references, so all the ../ links from the new depth resolve.


can you re-open the HTML docs issue file with this concern added? No need to
work on it now though.


Okay. Can you give me a report on what it would take to package this system up
as a library? Do we need to write an API or something? Encapsulate it somehow?
We should probably have a script that's kinda like the compile script, except it
creates a version of the system that can be imported into a project easily and
used. What do you think? Tell me in the report.


--------------------------------------------------------------------------------

### Assistant Response 4

Two things. Let me look at 705 and the build system.


The RAM tier was wiped by a reboot, so the `tmp/` symlink dangles and the build
can't start. Fixing that first — it's a bug, not a feature.


All tests pass now. Let me check one more thing before writing the report —
what the generated registry needs from the engine.


The generated registry `#include`s box sources by absolute path and calls one
engine function for timing — that pins down the packaging boundary exactly.
First, issue 705.


Issue 705 was already open — marked in progress with two interactive pieces
outstanding — so I added the concern rather than moving it back. It's stated
as a design requirement in the intended behavior ("no document can be silently
absent"), with the reasoning that discovery should walk downward while keeping
nesting meaningful, since the numbered top-level documents are a reading order
and a subdirectory is a group with its own character. A directory names itself
in the sidebar by carrying a README.


One bug fixed on the way: the build was dead. The `tmp/` symlink survives in the
repository but its target in `/tmp` doesn't survive a reboot, so `mkdir -p
tmp/build` failed on a dangling symlink and reported `File exists` — an error
that says nothing about the code. The Makefile now recreates both RAM tiers and
the shared-memory link before anything writes. Full suite passes.


---


## The packaging report


Written to `docs/implementation-notes/057-packaging.md`. The findings came from
reading the built code and the symbol table of a linked binary, so the specifics
are real rather than estimated.


**The framing that decides everything else: this isn't one library, it's two
halves in different times.** A consumer writes their own boxes in C; the
generator reads *their* sources and emits the registry. That output can never be
shipped, because it's derived from code you've never seen. So the deliverable is
a library, a code generator, and a build rule tying them together — closer in
shape to a parser generator than to a math library. Anything that ships only the
compiled half ships something nobody can use. It also means the consumer's build
depends on LuaJIT, which should be declared loudly rather than discovered.


**What's actually in the way,** in descending order of how badly it bites:


- About forty engine symbols are exported as common English words —
  `map_create`, `map_connect`, `pool_push`. Any host with its own notion of a
  map fails to link.

- The demo boxes export `add`, `mix`, `keep`, `seven`, `swallow`. The build
  wildcards `src/boxes/` in; `add` is roughly the most collidable symbol in C.

- Internal joints leak too — task construction, port lookup, the claim
  helpers.

- Two process-wide globals mean **one map per process, silently**. A host that
  wants two engines gets one, and the second writes into the first.

- Every error calls `abort()`, taking the host process down.

- The generator writes absolute paths into the emitted registry, so builds
  aren't relocatable or reproducible across machines.


**Shape: I'd take the amalgamation** — one `.c` and one `.h`. The decisive
reason is that a single translation unit makes every function not named in the
public header `static` for free. The internal symbols stop existing as linker
symbols at all, with no discipline required and no way to regress, which shrinks
the renaming job to only the genuinely public names. It also drops include paths
entirely, and it preserves the numbered reading order as banners in one file
with `#line` directives so backtraces still point at the real sources. Compared
against headers-plus-archive and source drop-in in a table in the report.


The packaging script derives rather than duplicates: it reads the public header
to decide what stays visible, so a function added to the engine is **private by
default** and becomes public only by being declared. That's the default that
can't rot. It also emits a worked example and — the part that matters —
builds the package outside the repository and runs it, as a test beside the
others. Packaging that hasn't compiled somewhere else is a guess.


**Four decisions I can't make for you.** The largest is error policy. Today's
`abort()` is right for a program that owns its process and hostile inside
someone else's. My proposal keeps your stated preference for loud failure
intact: an installable handler called with the message immediately before the
engine dies. The host logs and flushes; the engine still stops. What it must not
become is an error code returned up a call chain a consumer can ignore —
that's a fallback wearing a return type.


The others: whether to fix the one-map-per-process singleton now (I'd fix it —
a hidden singleton is a defect that only surfaces in someone else's program),
whether the engine owns the process's threads, and whether the pool ships as its
own artifact. That last one is nearly free and would prove the layering is real
rather than intended, since the pool has no dependency on anything above it.


Docs regenerated, 81 pages, zero unresolved references. Still uncommitted, and
your five untracked files from before are still untouched.


--------------------------------------------------------------------------------

### User Request 5

  Given you want guarantees enumerated as a general practice: there are probably
  a dozen scattered across the existing documents and source comments — a box
  can't remember, a task is self-contained, a station never moves once placed,
  the map is acyclic by induction, the queue never

  blocks on user code. Want me to collect them into a single page of things that
  are always true, with what each one costs? That would be a document, not an
  issue.


yes please. Then, I want to go through and invalidate some of them, and possibly
create new ones as well.


  The framing that decides everything else: this isn't one library, it's two
  halves in different times. A consumer writes their own boxes in C; the
  generator reads their sources and emits the registry. That output can never be
  shipped, because it's derived from code you've never

  seen. So the deliverable is a library, a code generator, and a build rule
  tying them together — closer in shape to a parser generator than to a math
  library. Anything that ships only the compiled half ships something nobody can
  use. It also means the consumer's build depends on

  LuaJIT, which should be declared loudly rather than discovered.




why does the consumer's build depend on LuaJIT? I thought we were just using C
for this project?


  - Two process-wide globals mean one map per process, silently. A host that
    wants two engines gets one, and the second writes into the first.


why is this behavior present?


  - Every error calls abort(), taking the host process down.


what is a better way to collect errors?


  - The generator writes absolute paths into the emitted registry, so builds
    aren't relocatable or reproducible across machines.


This is fine. When the user compiles their programs, they will need to provide
the directories they're going to run from. They should recompile if they're
moving, or if they'd prefer they can make some config files or something when
doing their own implementation.


  Four decisions I can't make for you. The largest is error policy. Today's
  abort() is right for a program that owns its process and hostile inside
  someone else's. My proposal keeps your stated preference for loud failure
  intact: an installable handler called with the message

  immediately before the engine dies. The host logs and flushes; the engine
  still stops. What it must not become is an error code returned up a call chain
  a consumer can ignore — that's a fallback wearing a return type.


sounds good. Make them restart the engine if they want to handle their own
errors. Break the machinery if it's use is unethical!


  The others: whether to fix the one-map-per-process singleton now (I'd fix it
  — a hidden singleton is a defect that only surfaces in someone else's
  program), whether the engine owns the process's threads, and whether the pool
  ships as its own artifact. That last one is nearly free

  and would prove the layering is real rather than intended, since the pool has
  no dependency on anything above it.


I mean yeah ideally we would be able to call multiple map files from within the
current map and they'd be able to incorporate their tasks into the main thread
pool. Are there issue files we could re-open for this design? The idea being,
that when creating new map files inside the same program and running them, we
should be able to point at the thread pool (which unfortunately is global) and
they should be able to run it. Or do you think it'd be better if we had
multiple, internalizable thread pools that ran whenever we gave worker threads
to them? When would we do so? What would those semantics look like, if you think
they're valid?


--------------------------------------------------------------------------------

### Assistant Response 5

Several of these need facts before I answer. Let me look.


Everything I need. Writing the guarantees document.


Done — `docs/058-guarantees.md`, 40 guarantees across boxes, stations, tasks,
the pool, gathering, the build path, and errors. Each row carries what it costs
and, more usefully for what you want to do next, **how it is held**: structure,
build, load, runtime, or discipline. That last column is the invalidation
surface.


The finding that fell out of building the list: **the two guarantees the most is
built on are the two nothing enforces.** "A box cannot remember" and "a box
never blocks" are pure discipline. Everything parallel rests on the first;
deadlock-impossibility is bought entirely by the second. One box with a `static`
counter or a blocking socket read withdraws both, and no part of the system says
so. A linter over box sources would move both to build-time — and the
generator already parses those files.


There's a "not guaranteed" section for things people would reasonably expect and
don't get, and a five-step protocol for retiring one, since you're about to. The
short version: find what rests on it, find what it paid for, find the tests that
hold it, say what replaces it, and move the line into the non-guarantees section
rather than deleting it — an absence is worse than an explicit "no".


Also updated: 057 now records the error policy as settled (restart is the
recovery story) and reframes absolute paths as declared behavior rather than a
blocker.


---


## Why LuaJIT


The engine is entirely C — nothing Lua runs at runtime. The dependency is at
**build time only**: the generator is a Lua script that reads your box sources,
parses their declarations, and emits `registry.c` — the shims, the box
records, the struct field tables, the comparison functions. That emitted file is
what lets a map file say `add` in text and reach a C function.


So a consumer needs LuaJIT to *build*, not to *run*. It can't be dodged by
shipping a pre-generated registry, because the registry is derived from the
consumer's own boxes. It could be dodged by rewriting the generator in C, which
means bootstrapping a compile before you can compile, in a language much worse
at text work. It's a build-tool dependency in the same category as bison —
worth declaring loudly, not worth removing.


## Why one map per process


One variable, set when a map starts and cleared when it's destroyed. It exists
because **a box receives only its input values** — it holds no pointer to
anything. Two things need to reach the map from inside box-side code:


- a box writing to the statics table, which is the deliberate back channel for
  turning a knob from inside the graph;

- every generated shim charging its elapsed time to its station, when statistics
  are compiled in.


Neither has a map to hand, so the global is the back channel. The station header
is candid about it — "shared mutable state, relocated, wearing a table for a
disguise."


The second global is dumber and less interesting: loading a map returns a map
pointer, so the staged load timings — parse, first pass, second pass,
validation, seed — had nowhere to go. A return-value limitation, not a
position.


The fix for both is the one that keeps reappearing: **the task carries its
map.** With shared pools it needs one extra step, because the box body still
isn't handed the task — the worker publishes the task's map in a thread-local
for the duration of the call. That stays correct even when two maps' tasks
alternate on the same thread, which a global cannot.


## Collecting errors


Beyond the handler you approved, three things worth having, in order:


**Die with a dump.** The engine can already write its live state back out as a
map file, and the round-trip is tested. On a fatal error, dump first. You get
the exact wiring, buffer depths, and statics at the moment it broke — a
post-mortem that reads back into the engine.


**Give the handler fields, not a sentence.** Subsystem, station, port, what was
expected, what arrived. A formatted string is for a human at a terminal; a host
with a structured log wants to index on the station.


**Write it to the shared-memory tier before dying.** A supervisor that restarts
the engine can read what killed the last one without scraping standard error.


## Sub-maps and pools


One correction that inverts the premise: **the pool is not global — it's
per-map.** Starting a map creates its own pool and stores it on the map. The
*map* is the global. Two maps in one process today means two pools and twice the
threads.


What actually blocks sharing a pool is narrower and sharper: the pool's finish
hook is **one function pointer plus one context, fixed when the pool is
created**, and that context is a map. Every finished task is delivered against
that one map. A task from map B handed to map A would index the wrong station
table — silent corruption, not an error. So sharing a pool requires the task
to carry its map. That's the same change again, now with four customers.


**On donated threads: I'd say no, and firmly.** The termination rule is "every
worker asleep and nobody outside can push." If workers can be given and
withdrawn, the worker count is mutable, and "everyone is asleep" stops being
provable — you'd be trading the one genuinely clever proof in the codebase for
a knob. Worse, deciding which pool gets threads *is* a scheduler, and this
engine's whole character is that it doesn't have one. Readiness is the
scheduler.


The one case where separate pools genuinely win is isolating blocking work — a
sub-map full of slow disk gatherers occupies workers for long stretches and its
neighbours queue behind it. But notice that situation only arises by violating
"no box blocks," so the honest fix is upstream.


**The question you actually have to answer first** is whether a sub-map is a
*composition* or an *invocation*. Composition means loading one just adds
stations and wires across the boundary by name — no lifetime, no completion,
the graph simply gets bigger. Invocation means it runs, finishes, and something
downstream waits on it — which needs a completion concept that a dataflow
graph doesn't naturally have, because push cycles are legal and "done" isn't
well-defined for a subgraph. I'd build composition first; it's nearly free once
tasks carry their map.


**Issue files worth reopening**, and what each would have to say:


- **Termination by last sleeper** — loading a sub-map mid-run is an outside
  submitter, and the registration has to be held across the load and seed or the
  pool can declare completion in the gap.

- **The task struct** — where the map pointer goes. This is the enabling
  change and everything else waits on it.

- **Hand-built maps** — starting a map currently fuses pool creation with
  binding; joining an existing pool means splitting them.

- **Statics mutation** — the global exists because of that issue's back
  channel.

- **The loader's two passes** — loading into a pool that already exists.

- **Station statistics** — the box-timing hook reads the global.


The composition feature itself has no existing issue and would be a new one.
Given it needs the task-carries-map change, a shared-pool attach path, and a
sub-map wiring syntax in the map file, it reads like a phase 8 rather than an
addition to phase 7.


82 pages regenerated, zero unresolved references. Still uncommitted; your five
untracked files remain untouched.


--------------------------------------------------------------------------------

### User Request 6

  So a consumer needs LuaJIT to build, not to run. It can't be dodged by
  shipping a pre-generated registry, because the registry is derived

  from the consumer's own boxes. It could be dodged by rewriting the generator
  in C, which means bootstrapping a compile before you can

  compile, in a language much worse at text work. It's a build-tool dependency
  in the same category as bison — worth declaring loudly, not

  worth removing.



Can you make an issue file to translate the luaJIT dependency into C? It's
better to have fewer dependencies for a project like this. I like using Lua but
I want this to be usable anywhere there's a C compiler and nothing else.


  Why one map per process


  One variable, set when a map starts and cleared when it's destroyed. It exists
  because a box receives only its input values — it holds no

  pointer to anything. Two things need to reach the map from inside box-side
  code:


  - a box writing to the statics table, which is the deliberate back channel for
    turning a knob from inside the graph;

  - every generated shim charging its elapsed time to its station, when
    statistics are compiled in.


  Neither has a map to hand, so the global is the back channel. The station
  header is candid about it — "shared mutable state, relocated,

  wearing a table for a disguise."


  The second global is dumber and less interesting: loading a map returns a map
  pointer, so the staged load timings — parse, first pass,

  second pass, validation, seed — had nowhere to go. A return-value
  limitation, not a position.



Um, why can't we just... put the static value into the box station...? then we
don't need to store any state with the map, which could be useful. The box
station's whole job is to store state (ring buffers, remember?) and create
tasks. So, a static value is just, a replacement for a ring buffer that's just a
single value. We would of course need to be able to swap from static to ring
buffer to gather at runtime, which shouldn't be too hard - just build the
machinery and slot it in. If we need to move the box station in order to fit
increasing sizes of ring buffers for example, that's totally doable as well.
Some of these guarantees are just, wrong?


  - every generated shim charging its elapsed time to its station, when
    statistics are compiled in.


um when the heck are we compiling statistics into the binary? what does that
mean?


  Collecting errors


  Beyond the handler you approved, three things worth having, in order:


  Die with a dump. The engine can already write its live state back out as a map
  file, and the round-trip is tested. On a fatal error, dump

  first. You get the exact wiring, buffer depths, and statics at the moment it
  broke — a post-mortem that reads back into the engine.


  Give the handler fields, not a sentence. Subsystem, station, port, what was
  expected, what arrived. A formatted string is for a human at a

  terminal; a host with a structured log wants to index on the station.


  Write it to the shared-memory tier before dying. A supervisor that restarts
  the engine can read what killed the last one without scraping

  standard error.



this sounds like a performance nightmare. Better for the minimal implementation
to be fast, close to the metal, and coherent.


  One correction that inverts the premise: the pool is not global — it's
  per-map. Starting a map creates its own pool and stores it on the

  map. The map is the global. Two maps in one process today means two pools and
  twice the threads.


So, would the OS handle scheduling threads between the two processes? That might
be okay.


  What actually blocks sharing a pool is narrower and sharper: the pool's finish
  hook is one function pointer plus one context, fixed when

  the pool is created, and that context is a map. Every finished task is
  delivered against that one map. A task from map B handed to map A

  would index the wrong station table — silent corruption, not an error. So
  sharing a pool requires the task to carry its map. That's the

  same change again, now with four customers.



Sorry, what's a "finish hook"? What do you mean by "one context"? Why do we have
two maps in one?


  On donated threads: I'd say no, and firmly. The termination rule is "every
  worker asleep and nobody outside can push." If workers can be

  given and withdrawn, the worker count is mutable, and "everyone is asleep"
  stops being provable — you'd be trading the one genuinely clever

  proof in the codebase for a knob. Worse, deciding which pool gets threads is a
  scheduler, and this engine's whole character is that it

  doesn't have one. Readiness is the scheduler.


  The one case where separate pools genuinely win is isolating blocking work —
  a sub-map full of slow disk gatherers occupies workers for

  long stretches and its neighbours queue behind it. But notice that situation
  only arises by violating "no box blocks," so the honest fix is

  upstream.



valid.


  The question you actually have to answer first is whether a sub-map is a
  composition or an invocation. Composition means loading one just

  adds stations and wires across the boundary by name — no lifetime, no
  completion, the graph simply gets bigger. Invocation means it runs,

  finishes, and something downstream waits on it — which needs a completion
  concept that a dataflow graph doesn't naturally have, because

  push cycles are legal and "done" isn't well-defined for a subgraph. I'd build
  composition first; it's nearly free once tasks carry their

  map.



yeah okay let's do composition.


--------------------------------------------------------------------------------

### Assistant Response 6

Several of these need checking before I answer — including one thing I got
wrong in the guarantees document.


Checked. Three answers change based on what I found, and one thing in the
guarantees document I need to correct. Issue file first.


Issue 308 written — the generator as a standalone C program, no dependency on
the engine it feeds, so there's no bootstrap circularity: compile the generator,
run it, compile everything else. The honest bulk of that work isn't parsing,
it's the few hundred lines Lua was giving away free — a growable buffer, an
arena, a string scanner — and the issue says to build those as their own
tested pieces first, because a bug there surfaces as garbled C hundreds of lines
away. Parity against the Lua generator is held by a deliberately temporary test
that can't outlive what it compares to. The documentation generator stays Lua,
and the issue says so explicitly: building needs a C compiler, regenerating our
docs needs LuaJIT, and only the first is a consumer's problem.


## One of them was wrong — mine


"All of an invocation's values are read at a single instant" was overstated, and
you were right to push. Ring values *are* claimed atomically with respect to
each other, under one hold of the station's mutex. Statics and gathered values
are resolved **after** that mutex is released, one at a time — and a gatherer
runs user code that can take milliseconds. So a box pulling a clock and a file
gets them from two nearby moments, and its ring values from a third, slightly
earlier one.


Corrected in both documents, with what's actually true: the whole read happens
in one uninterrupted stretch on one thread. That's short and bounded, which is a
real property — but it isn't atomicity, and I sold it as atomicity.


The other one you gestured at is a choice rather than an error. A station never
moving is currently free: growing a ring buffer reallocates only the storage the
port points at, never the port and never the station, so wires and in-flight
values are untouched. You'd only need stations to move in order to *add*
stations — and the cost then is that anything holding a station pointer across
a lock release has to re-derive it from the index. Doable, as you say. Just not
free.


## Statics on the station


You're right, and the reason is sharper than the one you gave.


**Why the table exists:** sharing. The map file format deliberately supports
several ports binding one entry — including two ports of *different types*
reading the same entry each their own way. One threshold used at five stations,
turned by one write.


**But the table is not why the global exists.** The global exists because a box
calls the statics write from inside its own body, and a box receives only its
input values — no handle to anything, not even its own station. Moving statics
onto stations doesn't fix that; a box still couldn't name where to write.


So the actual cause of one-map-per-process is this: **the global supports a back
channel the documentation already flags as deserving "exactly the suspicion a
global variable deserves."** Delete the box-writes-a-static feature and the
global's only remaining user is the box-timing hook, which is opt-in at compile
time. The restriction is downstream of a feature that's already described as a
smell.


And your instinct goes further than you stated it. The shared knob is something
the graph can already express: one station holding the value, gathered by the
five that need it. The statics table is a *second* mechanism for something the
wiring already does — which is exactly the kind of duplication this design
otherwise refuses.


That suggests the shape: **the statics table becomes a load-time construct.**
Named initial values live in the map file, each binding port gets its own copy
at load, and the running map holds no table at all. You gain no map-level
mutable state, no map-wide mutex, and a static claimed under the station's own
mutex in the same window as the ring values — which would strengthen the
guarantee I just had to weaken. You lose cross-station sharing, and you get it
back by drawing it.


Half the machinery exists: the port kinds are already a dispatch table, and
repointing a gatherer at runtime is already built. Want an issue file for this?


## When are statistics compiled in


Never, in the default build. There's no such define in the compiler flags. Only
the phase 7 demo builds a second binary with it deliberately, to show what
measuring costs against the same run without it.


The split: run counts and production counts are **always on** — plain atomic
increments where the work already is, costing essentially nothing. The *times*
are what the define controls, and without it every clock read compiles out
entirely. The fields stay in the struct so its shape never changes; the observer
prints "times compiled out" so a reader isn't left wondering where the numbers
went.


## Error collection — you're right, drop it


Conceded. All three only run on the dying path, so there's no steady-state cost
— but that's not the real objection, and the real objection stands: it's
machinery added to a minimal engine for a case that ends the program anyway.


The minimal version is what you already approved and nothing more: print, call
the handler, die. If someone wants a dump written before death, **they write
that in their handler** — they have the map pointer and the dump call is
public. The engine doesn't need to own any of it. One hook, and the policy
belongs to whoever installed it.


## Two pools in one process


Yes, the OS handles it — they're threads, and it time-slices them like any
others. It works correctly: each pool's termination logic is entirely per-pool
and stays sound.


What it costs is that neither pool knows it's sharing. Each sizes itself to one
worker per core by default, so two maps means twice the threads oversubscribing
the same cores — context switches and cache pressure that buy nothing. Fine
for two small maps, wasteful as a general answer. Not corruption, just paying
twice.


## The finish hook, plainly


When you create the pool, you hand it two things: **one function, and one
pointer.** After a worker runs a task and before it frees it, the worker calls
that function, passing it that pointer and the finished task. That's the entire
extent of the pool's knowledge of the engine — one function pointer's worth.
It's how a finished value gets carried onward without the pool ever learning
what a station is.


"Context" is that one pointer, fixed when the pool is created. Today it's the
map. So every finished task is delivered against that same map, whichever map
the task actually came from.


And to your last question — we *don't* have two maps in one process today.
That's the situation composition would create, which is why it's the thing
standing in the way of it.


Composition noted and settled. 83 pages regenerated, zero unresolved references.
Still uncommitted.


--------------------------------------------------------------------------------

### User Request 7

  So the actual cause of one-map-per-process is this: **the global supports a
  back channel the documentation already flags as deserving

  "exactly the suspicion a global variable deserves."** Delete the
  box-writes-a-static feature and the global's only remaining user is the

  box-timing hook, which is opt-in at compile time. The restriction is
  downstream of a feature that's already described as a smell.



we need to have API functions (or similar) to modify every single one of a box
station's properties at runtime. We need to be able to completely bootstrap and
assemble a map from scrath each time we create the program. If you have any
questions let's walk through them one-by-one.


  **But the table is not why the global exists.** The global exists because a
  box calls the statics write from inside its own body, and a box

  receives only its input values — no handle to anything, not even its own
  station. Moving statics onto stations doesn't fix that; a box

  still couldn't name where to write.



One reason not to have a global table like this is that each box station will
need an entry for each of it's statics. They aren't shared across box stations,
so why would they be in a global table? Remember, a box station is an
instantiation of a box type - it has all it's own input values because it's
wired differently than all the other box stations that share the same box
function. A box function always takes in exactly a certain type of inputs - they
are simply the arguments to the box function. So, a box station, which manages
and handles running the box function, must be able to translate arbitrary inputs
(whose shape we define on the box station) into the specific argument types that
the box function requires. Because box stations may be wired up differently,
some of them may have static input values while others might have ring buffers
or gatherer functions in a certain input port. The slot is always the same -
that's what gets passed to the task struct, and eventually to the box function.
But the input port, it's structure and design, depends entirely on how the
upstream boxes that feed into that port are wired. There is a restriction here -
you can't have BOTH a ring buffer and a gather/static input port. But, now that
I think of it, maybe we should just design it such that every box station has
ring buffers for each and every single one of it's input ports, and a spot to
store a static value or a gatherer function pointer. That way we can be
completely versatile - preferring first any value in the ring buffer (if the
ring buffer value count flag increment counter variable is greater than 0), then
the static value (if it exists), then running a gather function pointer
(potentially with recursive nesting) to gather the value and storing that in the
input slot when checking to see if all the input values for a box that just had
a value written into it are filled, and if we should create a task struct for
that box with the values popped and gathered from the box station and placed in
the thread pool with the other task structs that are awaiting a worker thread to
come and pick them up for completion.


The reason to have all three types is simply so the user doesn't have to think
about wiring them correctly. We don't have to worry about whether or not there's
a ring buffer style input (the return value of another box function on a
different box station that has it's output port wired up to one of our input
ports) or a gatherer function or a static value. The user can just wire them up
as they please and if we inform them in the documentation that we prefer first
the ring buffer inputs, then gatherer functions, then static values... What do
you think?


  ## Two pools in one process


  Yes, the OS handles it — they're threads, and it time-slices them like any
  others. It works correctly: each pool's termination logic is

  entirely per-pool and stays sound.


  What it costs is that neither pool knows it's sharing. Each sizes itself to
  one worker per core by default, so two maps means twice the

  threads oversubscribing the same cores — context switches and cache pressure
  that buy nothing. Fine for two small maps, wasteful as a

  general answer. Not corruption, just paying twice.



would this change if we implement the composition approach? Would it be
meaningfully different than running two programs with full CPU utilization on
all threads each?


  ## The finish hook, plainly


  When you create the pool, you hand it two things: **one function, and one
  pointer.** After a worker runs a task and before it frees it, the

  worker calls that function, passing it that pointer and the finished task.
  That's the entire extent of the pool's knowledge of the engine

  — one function pointer's worth. It's how a finished value gets carried
  onward without the pool ever learning what a station is.


  "Context" is that one pointer, fixed when the pool is created. Today it's the
  map. So every finished task is delivered against that same

  map, whichever map the task actually came from.



Okay let's go over this more slowly. We have too many tasks up in the air at
once. After a worker finishes a task, it has the task's return value in hand and
it puts it in the spot that it's been told to. The location where it should
store it's return value is in the task struct. I'm confused why the worker is
calling a function with a function pointer and a map pointer?


--------------------------------------------------------------------------------

### Assistant Response 7

## The finish hook, slowly


You're right up to a point, and the gap is what happens *after* that point.


The worker pops a task. It calls the one function the task carries — the shim
— which unpacks the input values, calls your box, and writes the return value
into the task's own memory. That's the part you described, and it's exactly
right.


But that spot is **inside the task's allocation, which the worker is about to
free.** The value is in a staging buffer with seconds to live. Something has to
copy it out into the input ports of whatever stations are wired downstream, and
that copying is what may complete some other station's input set and produce the
next task.


The worker cannot do that itself, because **the worker doesn't know what a
station is.** The pool is a generic queue: it moves opaque structs and calls one
function pointer per struct. If the worker knew how to deliver a value, the pool
would have to know about maps, ports, and wiring — and the entire reason the
pool is a separate reusable thing would evaporate.


So the seam is one function pointer, handed over once when the pool is created:


1. Worker pops a task.

2. Worker calls the task's function → box runs → return value lands in the
   task's staging buffer.

3. **Worker calls the finish function it was given, passing it the pointer it
   was given, plus the task.** That function is the delivery walk. It reads
   which station the task belongs to, picks an output port, and copies the
   staged value into every destination station's input port.

4. Worker frees the task.


Step 3 is the only place the engine gets to run per task, and the pointer handed
along is the map — because the task records its station as a *number*, and
turning station 7 into an address requires the station table. The task has the
index; the map has the table.


That's all "context" means: one pointer, chosen when the pool was made, handed
back to the finish function every time. Today it is one map, which is why two
maps sharing one pool would deliver both maps' values against whichever map was
named first.


## Your three-source port


The storage half of this I think is right, and I'd take it today. Give every
input port room for all three — ring buffer, a static value, a gatherer
pointer — and switching a port's source becomes a field write instead of
freeing storage and re-tagging. That directly serves what you want from the
runtime API: mutating a port stops being surgery.


The preference chain is the part I'd push back on, and the argument isn't mine.


**It's a fallback.** "Use the ring buffer, or fall back to the static, or fall
back to gathering" is precisely the shape your own rule forbids — prefer error
messages and breaking functionality over fallbacks, treat fallbacks as warnings
and warnings as errors. Right now, wiring a push into a gather-configured port
is refused at load, by name. Under the preference chain that same typo becomes
legal behavior: the value flows somewhere quietly and the station runs on the
static instead. The engine stops being able to tell a deliberate arrangement
from a mistake.


Three consequences that follow whether or not that argument lands:


**Readiness goes nearly vacuous.** A port with a static or gatherer configured
is always full, so it never gates. If a port has both a ring buffer and a
static, the station becomes ready *even when nothing has been pushed there* —
a synchronizing join silently becomes a non-synchronizing one. That's
occasionally what you want and rarely what you meant.


**The same station runs on different sources depending on timing.** Two
invocations firing microseconds apart could take a pushed value and a static
respectively, purely on whether a delivery landed first. Same map, same inputs,
different behavior between runs.


**Gathering gets genuinely dangerous.** Today "a gatherable station has no ring
buffers" is what makes pulling safe — nothing could ever fill them mid-pull.
If every port has a ring buffer, that definition dissolves, and a pull would run
a station that might **consume buffered values** on some unrelated thread,
outside any task. You'd need a rule that a pulled station ignores its rings —
which means the preference order is different when pulled than when pushed, and
now there are two orders to remember.


What I'd do instead: keep your storage layout, keep the source **explicit per
port**, and make changing it a cheap runtime call. You get the versatility
you're after — wire it however you like, change your mind while it runs —
without a value silently arriving from a source nobody selected.


## Statics per station


Agreed, and your framing is better than mine. A station is an instantiation of a
box type, wired differently from every other instantiation of the same box; its
input configuration belongs to it. A shared numbered table is a second mechanism
for something the wiring already expresses.


One factual correction so the decision is made against what's actually there:
the map file format *does* currently document sharing, including the case of two
ports of different types reading the same entry each their own way. So this
isn't removing an accident, it's retiring a documented feature. Cheap to retire
— the shared knob comes back as one station holding the value, gathered by
everyone who needs it — but the format document and the loader both have to
change.


## Two pools under composition


The problem disappears, because composition doesn't create a second pool.
Stations join the existing table, tasks join the existing queue, and there's one
pool sized once for the machine.


Against two separate programs each taking all cores: composition is meaningfully
better, and the reason is work conservation. Two programs put twice the threads
on the same cores and each one's idle workers sit there while the other is
saturated. One pool has a single queue — any free worker takes whatever is
ready, from either map. No oversubscription, no idle-while-busy.


What you give up is isolation. One address space, and given that errors end the
process, a bad map file in the sub-map takes the whole thing down.


## The runtime mutation API


Noted as a requirement: every property of a station changeable while running,
and a map assemblable from nothing at startup. Four questions, and I'd start
with the third.


1. **Which preference order?** You wrote ring → static → gather in the long
   description, and ring → gather → static in the summary sentence. If the
   chain survives at all, these differ in an observable way.

2. **Does the chain count as a fallback by your own rule?** I think it does, and
   that this is the decision that settles the whole design.

3. **Can stations be added at runtime, or only modified?** This is the one I'd
   take first, because it decides everything structural. The station table is
   one flat allocation fixed at creation, and wires are indices into it.
   Modifying stations in place needs nothing new. *Adding* stations means
   growing that array, which means any code holding a station's address across a
   lock release has to re-derive it from its index — the delivery walk does
   exactly that today. Assembling a map from scratch at startup is already
   possible within a fixed count; growing past it is a different piece of work.

4. **What happens to tasks already in the queue when a property changes?** A
   task built before a change carries the old values and the old function
   pointer, and will run that way. That's defensible — it matches how a
   repointed gather already behaves — but it should be a stated rule rather
   than a discovered one.


--------------------------------------------------------------------------------

### User Request 8

● The finish hook, slowly


  You're right up to a point, and the gap is what happens after that point.


  The worker pops a task. It calls the one function the task carries — the
  shim — which unpacks the input values, calls your box, and writes

  the return value into the task's own memory. That's the part you described,
  and it's exactly right.


  But that spot is inside the task's allocation, which the worker is about to
  free. The value is in a staging buffer with seconds to live.

  Something has to copy it out into the input ports of whatever stations are
  wired downstream, and that copying is what may complete some

  other station's input set and produce the next task.


  The worker cannot do that itself, because the worker doesn't know what a
  station is. The pool is a generic queue: it moves opaque structs

  and calls one function pointer per struct. If the worker knew how to deliver a
  value, the pool would have to know about maps, ports, and

  wiring — and the entire reason the pool is a separate reusable thing would
  evaporate.


  So the seam is one function pointer, handed over once when the pool is
  created:


  1. Worker pops a task.

  2. Worker calls the task's function → box runs → return value lands in the
     task's staging buffer.

  3. Worker calls the finish function it was given, passing it the pointer it
     was given, plus the task. That function is the delivery walk.

  It reads which station the task belongs to, picks an output port, and copies
  the staged value into every destination station's input port.

  4. Worker frees the task.


  Step 3 is the only place the engine gets to run per task, and the pointer
  handed along is the map — because the task records its station as

  a number, and turning station 7 into an address requires the station table.
  The task has the index; the map has the table.


  That's all "context" means: one pointer, chosen when the pool was made, handed
  back to the finish function every time. Today it is one map,

  which is why two maps sharing one pool would deliver both maps' values against
  whichever map was named first.



Okay... the thread pool is part of the map. It doesn't make sense to try and
keep maps, stations, boxes, wires, etc as separate, because they are the same
thing. They are integrated into the cohesive whole. The "worker" is really a
"worker thread" which is a box that is running it's box function and delivering
it's values. The box knows how to record it's return values. Of course it does,
what else would it do with them? I'm concerned when you say it's passing so much
data back and forth when really the task struct is in shared memory, meaning it
doesn't need to be passed around at all. We just point at it. The place where
the return value is placed must ALSO be in shared memory. This means box
stations must be in shared memory, where more than one thread can see them,
because we need to be able to write the return value somewhere, and we can't do
that if we can only place it somewhere in the worker thread's memory context.


The thread pool is part of the map. We can run other map files while a process -
to do so, we simply create a worker task for them with all the locations where
we will store the new map file's return values (speaking of which, we need a way
to "return" a value out of a map, when the map file has finished. Maybe we
should have a ring buffer with arbitrarily sized slots for arbitrary outputs
that we can collect over the course of the run, and return that to any upper
maps and/or stdout or something? If you like that idea, make an issue file,
don't wait for me.) we simply create a worker task for the new map files using
the information in the task struct which already has memory locations allocated
(during the task struct's creation) which are stored in shared memory. Then, we
point the new map file at our currently active thread pool and say "send your
tasks here" and it'll just, work from that point on because it's essentially
another part of the map now, composed with ours.


  Your three-source port


  The storage half of this I think is right, and I'd take it today. Give every
  input port room for all three — ring buffer, a static value, a

  gatherer pointer — and switching a port's source becomes a field write
  instead of freeing storage and re-tagging. That directly serves

  what you want from the runtime API: mutating a port stops being surgery.


  The preference chain is the part I'd push back on, and the argument isn't
  mine.


  It's a fallback. "Use the ring buffer, or fall back to the static, or fall
  back to gathering" is precisely the shape your own rule forbids

  — prefer error messages and breaking functionality over fallbacks, treat
  fallbacks as warnings and warnings as errors. Right now, wiring a

  push into a gather-configured port is refused at load, by name. Under the
  preference chain that same typo becomes legal behavior: the value

  flows somewhere quietly and the station runs on the static instead. The engine
  stops being able to tell a deliberate arrangement from a

  mistake.



so true bestie, fallbacks are bad, unreliable, and obscurely arcane. Better to
have a 2 bit flag with four values, representing "ring buffer", "gatherer",
"static", and "none" for inputs that are optional. The shim should be able to
create a valid null value, and the box function should check any of it's values
that can be null to see if they are null, and if so, then to continue the
function as if they were null.


  One factual correction so the decision is made against what's actually there:
  the map file format does currently document sharing,

  including the case of two ports of different types reading the same entry each
  their own way. So this isn't removing an accident, it's

  retiring a documented feature. Cheap to retire — the shared knob comes back
  as one station holding the value, gathered by everyone who

  needs it — but the format document and the loader both have to change.



uh, yeah, the idea that they could share static input values implies a
fundamental misunderstanding of how we should be doing statics, which is as a
value stored on the box station. So make sure the documentation is correct,
please.


  What you give up is isolation. One address space, and given that errors end
  the process, a bad map file in the sub-map takes the whole

  thing down.


good


--------------------------------------------------------------------------------

### Assistant Response 8

Nothing is copied in step 3. Passing a pointer is two machine words in registers
— the task is on the heap, the station table is on the heap, every thread sees
both. I described a *call*, not a transfer. The only copy anywhere in that path
is the return value into the destination port's buffer, and that one is
unavoidable precisely because the task's memory is freed on the next line.


Writing the output-collection issue now.


## Your model, taken to its conclusion


If the pool is part of the map and the whole thing is one integrated object,
then the honest version of that isn't "pass the map along" — it's **stop
needing the map on the delivery path at all**.


Here's what forces the map to be there today: a task records its station as a
*number*, and a destination records its target as a *number*. Numbers are
meaningless without the table they index. That's the only reason the delivery
walk takes a map.


Make wires point at stations directly and the map vanishes from the hot path
entirely:


- A task points at its station. Delivery reads the station's output port
  straight off it.

- A destination points at a station and one of its input ports. Copy the value
  in, check readiness, done.

- No lookup, no table, no context pointer, no finish hook — the worker loop
  calls delivery because delivery is part of what running a box *is*, exactly as
  you put it.


And every problem we've been circling dissolves at once. Two maps sharing a pool
stops being ambiguous, because nobody ever asks "which map" — a station is a
station. Composition becomes literally a wire: the sub-map's station pointer
goes in a parent port's destination list and there is nothing else to arrange.
The one-map-per-process restriction has no mechanism left to live in.


The cost is that the station table can no longer be one flat array, because
growing an array moves everything in it and every pointer dies. Stations get
allocated individually and never move — which is *stronger* than what exists
now, since today's guarantee only holds because the array is never resized,
which is also why you can't add stations at runtime. Individually allocated
stations give you permanent pointers **and** unlimited growth, which is what
assembling a map from scratch actually needs.


What the map keeps: a name, a list of its stations for iteration and teardown
and reporting, and the pool. Nothing on the hot path. The station number stops
being an identity and becomes a label for the dump — and the map file already
speaks names rather than numbers, so the file format doesn't notice.


## The output collection — issue written


Built as a **destination kind**, which keeps it on the delivery path where it
belongs: an output port's destination currently names a station and an input
port, and it may instead name the map's output. A row, not a special case.


Records carry their own length and their type's name, because unlike an input
port — whose cells are exactly one parameter wide, which is what makes a write
a copy with no allocation — the collection holds every type the map produces.
Length-prefixed byte ring, one mutex, since writes are rare next to internal
deliveries and a lock-free append buys nothing.


Two things I decided in the issue that you should overrule if you disagree.
**Arrival order is not stable** — two runs of the same map may collect the
same records in a different sequence, because which of two ready stations
finished first is whatever the threads did. The test asserts the set and
explicitly does *not* assert the order, so the non-guarantee is written where it
can't rot. And **composition doesn't use this**: once a sub-map's station can be
wired directly into a parent's station, a wire is a wire and no collection sits
between them. This is the seam between the engine and whatever embeds it,
including a map run straight from the command line with its records streamed to
standard output.


## Statics documentation — corrected


The format document now says a static value belongs to the input port that reads
it, and that the table in the file is *notation* — a way to write a value once
while describing the map, not a shared thing at runtime. Sharing is described as
falling out of the table's shape rather than as a feature, with the note that
nothing should be built on it.


I kept it honest about the gap: the running engine still holds the shared table,
and the doc says so plainly rather than describing a design that isn't there
yet. It also records why per-port is better in mechanical terms — the value
gets claimed under the station's own mutex alongside the buffered values, one
lock instead of two, and the map holds no state at all.


## The four-value flag — one question


Ring, gatherer, static, none. The first three are clean. **"None" has two
readings and they're very different**, so I'd rather ask than guess.


**Reading one: unconfigured.** The port has no source yet, so the station cannot
become ready. A station built at runtime starts with every port "none" and
becomes runnable as you configure them. That serves the bootstrap-from-scratch
requirement directly, costs nothing, and adds no concept — it's just the
honest state of a half-built station.


**Reading two: optional.** The port is deliberately empty, the shim synthesizes
a null, and the box checks. Two problems with this one. There is no null for a
value type — zero is a perfectly good `int`, and all-zero bytes are a
perfectly good struct — so "is it null" isn't answerable without a presence
bit riding alongside the value, which changes what a box's signature looks like.
And it's a nil check, which your own standing rule calls asking for errors:
never assume null is an option, find out why it would be null, and fix that real
error.


I'd take reading one. Which did you mean?


84 pages regenerated, zero unresolved references. Two new issue files, one
documentation correction, nothing committed.


--------------------------------------------------------------------------------

### User Request 9

  If the pool is part of the map and the whole thing is one integrated object,
  then the honest version of that isn't "pass the map along" —

  it's stop needing the map on the delivery path at all.


Great sounds good.


  The output collection — issue written


  Built as a destination kind, which keeps it on the delivery path where it
  belongs: an output port's destination currently names a station

  and an input port, and it may instead name the map's output. A row, not a
  special case.



why don't we just make the output of a map be a station, that collects a bunch
of data over the course of the map's run, and as the final act that a map does
(when all it's threads are silent and the thread pool task list is empty) it'll
output the the collected output values in a format defined in the map output
station's box function?


  Two things I decided in the issue that you should overrule if you disagree.
  Arrival order is not stable — two runs of the same map may

  collect the same records in a different sequence, because which of two ready
  stations finished first is whatever the threads did. The test

  asserts the set and explicitly does not assert the order, so the non-guarantee
  is written where it can't rot.


that's totally fine. :)


And composition doesn't use

  this: once a sub-map's station can be wired directly into a parent's station,
  a wire is a wire and no collection sits between them. This is

  the seam between the engine and whatever embeds it, including a map run
  straight from the command line with its records streamed to

  standard output.



great. the "map output station" I mentioned above should be what the return
value of the box station that imports a map by calling the sub-map and importing
it into a larger map runs to gather.


  The four-value flag — one question


  Ring, gatherer, static, none. The first three are clean. "None" has two
  readings and they're very different, so I'd rather ask than guess.



  Reading two: optional. The port is deliberately empty, the shim synthesizes a
  null, and the box checks. Two problems with this one. There

  is no null for a value type — zero is a perfectly good int, and all-zero
  bytes are a perfectly good struct — so "is it null" isn't

  answerable without a presence bit riding alongside the value, which changes
  what a box's signature looks like. And it's a nil check, which

  your own standing rule calls asking for errors: never assume null is an
  option, find out why it would be null, and fix that real error.



this one, with some caveats. We should use an enum or something, not a literal
NULL value because we need the shim to know that it's not actually intended to
be a null value, but rather a flag that says "there is intentionally no value
provided for this slot in the task struct." which tells the shim that it needs
to feed the function a value that implies that the optional value was
purposefully ommitted. And the shim should know (by examining the signature of
the box function during the shim's dynamic creation) which values are optional,
so if a null enum is passed for a value that is not optional, it'll crash the
program and state exactly why.


  The format document now says a static value belongs to the input port that
  reads it, and that the table in the file is notation — a way to

  write a value once while describing the map, not a shared thing at runtime.
  Sharing is described as falling out of the table's shape rather

  than as a feature, with the note that nothing should be built on it.



yes exactly! The map file is only to describe how we want the system to create
the actual map in memory. It is a map to a map, if you will. (silly). It is not
meant to store data values that persist between runs, and it is not meant to be
updated unless the user specifically does some sort of dump, save-as, or compile
function in their program that creates a new map file and overwrites this one.
Essentially, it is a schematic for building the program's actual structure at
runtime.


  I kept it honest about the gap: the running engine still holds the shared
  table, and the doc says so plainly rather than describing a

  design that isn't there yet. It also records why per-port is better in
  mechanical terms — the value gets claimed under the station's own

  mutex alongside the buffered values, one lock instead of two, and the map
  holds no state at all.



um, it kinda sounds like you put plans for future designs in the docs? Why put
it there when we could (re)open issue files for it?


--------------------------------------------------------------------------------

### Assistant Response 9

Fair hit on the documentation — I put intended design where only built design
belongs. Fixing that properly means reopening the statics issues. Let me read
them.


Right — that was documentation describing a design that doesn't exist. Moved
to where it belongs.


**Issues 401 and 405 reopened** (moved out of completed). 401 is now "static
input values": current behavior records what's built and what's wrong with it
— the table is map-level state, and map-level state is what forces one map per
process. Intended behavior is the per-port design, with the claim moving under
the station's own mutex beside the ring-buffer pop.


405 traces the singleton to its actual source. The locking analysis was right
and survives; what changes is which mutex does the work. The removal is the
box-reachable write: **a feature the design already distrusted was quietly
charging the whole engine its ability to compose.** There's a test in the steps
that can't be written today — two maps running in one process without seeing
each other's values.


**008 reverted** to describing only what's there: the table exists, here are its
two consequences, neither is a feature, issues 401 and 405 are retiring both. I
did keep your framing as documentation, because it describes what the format
*is* rather than what it will be — it's a schematic, not a save file, a map to
a map. Nothing writes back to it; a dump is a deliberate act with a name, and
what it produces is another schematic of whatever shape the map had grown into.


## The output station is better than what I wrote


Three things it wins on. No new destination kind — a value delivered there
goes into an input port like every other value, and the delivery walk is
untouched. **The accumulator already exists**: an input port's ring buffer grows
without bound and is already guarded against however many threads are writing.
The output station just never becomes ready during the run, so nothing consumes
what collects. And the format is a box function, so the engine has no opinion
about output at all.


It also kills the machinery I'd designed. I had records carrying their own
length and type name because a collection holds every type the map produces.
With a station, **the output type is the box's parameter type** — everything
wired to it must produce that type, checked at load like any other wire. The
type discipline that already exists does the work.


Two things it pins down. Its box runs **once per collected value, in buffer
order**, because a box takes values and not collections — which is fine, and
gives you something: arrival order isn't reproducible across runs, but once
collected the order is fixed, so the same collection always formats the same
way. And its output can't be wired back into the map, or termination wakes more
work and "final" stops meaning anything.


**One real conflict, written into the issue rather than papered over.**
Composition was chosen *because* a sub-map has no lifetime — its stations are
just more stations, and nothing tracks when it's done. But "the parent's
importing station gathers the sub-map's output station" implies a moment when
the sub-map has finished, which is the completion concept composition was chosen
to avoid. Tracking it needs per-map outstanding-task counts, which is a
scheduler.


The resolution I wrote down: **the final act belongs to the outermost map
only.** For the map that owns the process, "every worker asleep, queue empty" is
well defined, and that's when its output station fires. A composed sub-map has
no separate completion — its output station is just a station the parent wires
from, and values flow as produced. If a sub-map genuinely needs to complete and
hand back a result, that's invocation semantics and deserves to be recognised as
a different issue rather than smuggled through this one.


## Optional inputs — your caveats, plus two refinements


Agreed on the sentinel-not-NULL point. Two things it forces:


**How the box declares it.** The generator parses C declarations, so the marker
has to be visible there — a macro that expands to nothing for the compiler and
means something to the generator, written into the parameter list. That part is
standard and works.


**What the box actually receives.** There's no spare value in an `int`'s range
to mean "deliberately absent," so the honest answer is a small generated wrapper
per type: a presence flag and the value. The author writes the optional
parameter as that type and checks the flag. The advantage is that an optional
parameter then *looks* optional in the C signature, rather than looking like an
ordinary int that might secretly be a sentinel.


**Where the crash belongs — earlier than you said.** A port set to "none"
feeding a non-optional parameter is detectable the moment the port's kind is
set: at load, or when the runtime configuration call is made. That names the
station and the port while there's still a person standing there to read it,
instead of on the first task built minutes into a run. The shim can keep a cheap
assertion as a backstop, but it never has to make a decision — which keeps the
hot path free of a check that can only fail because of a configuration error
made much earlier.


84 pages, zero unresolved references. The two reopened issues show in git as a
deletion plus an untracked file, since I moved them with a plain move rather
than staging anything. Still nothing committed.


--------------------------------------------------------------------------------

### User Request 10

  One real conflict, written into the issue rather than papered over.
  Composition was chosen because a sub-map has no lifetime — its stations

  are just more stations, and nothing tracks when it's done. But "the parent's
  importing station gathers the sub-map's output station"

  implies a moment when the sub-map has finished, which is the completion
  concept composition was chosen to avoid. Tracking it needs per-map

  outstanding-task counts, which is a scheduler.



whoa so true bestie. This reason is why we can't have a unified thread pool.
Darnit. The result is that we need to have submaps keep their own thread pools,
and every time a sub-map needs to enqueue a task in it's internal thread pool,
we create a router task in the main thread pool that points at the submap and
says "run the n'th task in this thread pool" and we'd make sure that only the
main thread pool could assign tasks, whether main map or submap tasks, and also
that we use atomics to only assign / add / remove tasks from thread pools. Do we
already? If no, then how is adding a task to a thread pool currently structured,
with respects to atomicity?


  Where the crash belongs — earlier than you said. A port set to "none"
  feeding a non-optional parameter is detectable the moment the port's

  kind is set: at load, or when the runtime configuration call is made. That
  names the station and the port while there's still a person

  standing there to read it, instead of on the first task built minutes into a
  run. The shim can keep a cheap assertion as a backstop, but it

  never has to make a decision — which keeps the hot path free of a check that
  can only fail because of a configuration error made much

  earlier.



good point. on map initialization or upon creating a new box's connection.


the idea of a map output box station collecting inputs then only pushing a value
out to the parent map when all of them are filled is just.... I love it.....


--------------------------------------------------------------------------------

### Assistant Response 10

The premise dissolves — and your own last sentence is what dissolves it. Let
me answer the atomicity question first, then show why.


**How pushing to the queue works today:** it takes the pool's mutex, grows the
ring if the tail would land on the head, writes the pointer, advances the tail,
broadcasts to sleepers, unlocks. So it's *mutually exclusive*, not lock-free.
Worth separating two meanings of "atomic" that are easy to run together: the
push is **indivisible** — no other thread can observe a half-written queue —
because the mutex enforces it. What it is not is **lock-free**, where
indivisibility comes from hardware compare-and-swap instead of blocking. The
station statistics use real hardware atomics, because those are incremented
outside any lock. The queue doesn't need them, because it's already inside one.


Now the important part.


## Keep the unified pool — you solved it in the same message


"A map output box station collecting inputs then only pushing a value out to the
parent map when all of them are filled" **is the one rule.** A station runs when
every one of its input ports holds a value. Give a sub-map's output station one
port per result it produces, and it becomes ready exactly when all those results
exist.


That readiness *is* the completion event. Nothing counts tasks. Nothing asks
which map a task belonged to. The sub-map is finished at the instant its output
station fires, by the same mechanism that decides everything else in the engine
— so the completion concept composition was chosen to avoid turns out to have
been sitting there the whole time, expressed in wiring instead of bookkeeping.


Which means no router tasks, no per-map queues, no donated workers, and the
termination proof untouched. Worth noting what the router design would have
cost, since it was a reasonable response to a real-looking problem: two queue
entries per sub-map task, an indirection per execution, and a pool that is a
queue with no workers of its own being drained by another pool's workers —
which is a work-stealing scheduler, arrived at sideways.


Issue 209 now records this, and the section that was "an open conflict" is now
the resolution. Two output stations, only one of them new:


- **A sub-map's output station is an ordinary station.** One port per result,
  delivering one value into the parent when all have arrived. No new station
  kind, no final act, no pool change.

- **The outermost map's output station is the accumulating kind** — never
  ready during the run, collects into its port, drains at termination. That one
  has to exist because a top-level map gathering an unbounded stream has no "all
  ports filled" moment; its completion is "everything has gone quiet," which is
  only well defined for the map that owns the process.


The general shape here is worth keeping in view: twice now, a problem that
looked like it needed new scheduling machinery turned out to be expressible in
the rule that already exists. The gather-versus-enqueue question ended the same
way — the answer was a wire, not a mechanism.


Optional-input check placement noted as settled: at map initialization, and at
the moment a connection is made. There's no issue file for the four-value port
flag yet — say the word and I'll write it, since it now has three settled
decisions to record (no preference chain, the wrapper type with a presence flag,
and check-at-configuration).


84 pages, zero unresolved references. Nothing committed.


--------------------------------------------------------------------------------

### User Request 11

  - The outermost map's output station is the accumulating kind — never ready
    during the run, collects into its port, drains at termination.

  That one has to exist because a top-level map gathering an unbounded stream
  has no "all ports filled" moment; its completion is "everything

  has gone quiet," which is only well defined for the map that owns the process.



we should treat it like any submap's output station. The box function will
determine how we save the data as it comes in piece-by-piece. The box function
should only run when every input slot is filled.


--------------------------------------------------------------------------------

### Assistant Response 11

That's simpler, and there's a property that makes it clearly right: since the
pool doesn't stop until every task has run, and every complete input set
produces a task, **the output is already finished by the time the pool declares
completion.** The final act was solving a problem that doesn't exist.


Rewritten. The issue lost most of its weight, which is the sign it's right:


- **No new station kind** — the routing dispatch table gains nothing.

- **No final act** — the termination proof is untouched, not "untouched but
  with a hook."

- **No accumulating storage, no drain, no once-only flag** — three
  implementation steps deleted.

- **No "may not re-enter the map" rule** — that constraint only existed to
  protect the final act. A cycle through the output station is now just an
  ordinary push cycle, which is legal and always was.


What's actually left is a *name*: a map declares which station its results come
from, the loader checks it, and the dump carries it. Everything else was already
there. The engine gains a noun, not a subsystem.


Two things I wrote into it that you should know about, since they follow from
making it ordinary:


**The output box runs concurrently with itself.** Two invocations of one station
can be in flight at once, so an output box lives under exactly the same rule as
a gatherer — safe to run from several threads simultaneously. Open, append,
close is fine; holding a file handle between calls is not, and is already
forbidden by "a box cannot remember" anyway. That rule is reused verbatim rather
than invented, which is why the issue points at 403 for it.


**Write order isn't production order, and isn't reproducible.** You'd already
accepted unstable arrival order, but this is a slightly different instability
worth naming: previously the collection order was at least fixed once drained,
and now even the writes interleave. A map that needs ordered output has to put
the ordering in the map, by routing through a station that imposes it. The
demonstration in the steps runs the same map twice and shows the same set in a
different order, so the non-guarantee is written where it can't rot.


Standing offer unchanged: the four-value port flag has three settled decisions
now (no preference chain, a wrapper type carrying a presence flag, and
check-at-configuration rather than in the shim) and no issue file to hold them.


--------------------------------------------------------------------------------

### User Request 12

  The output box runs concurrently with itself. Two invocations of one station
  can be in flight at once, so an output box lives under exactly

  the same rule as a gatherer — safe to run from several threads
  simultaneously. Open, append, close is fine; holding a file handle between

  calls is not, and is already forbidden by "a box cannot remember" anyway. That
  rule is reused verbatim rather than invented, which is why

  the issue points at 403 for it.



we should make file handles atomic when modifying them for the output box
function. things like file handles (for writing to log files perhaps?) can be
stored as a static value on the box station. It's in shared memory, remember?


  Write order isn't production order, and isn't reproducible. You'd already
  accepted unstable arrival order, but this is a slightly different

  instability worth naming: previously the collection order was at least fixed
  once drained, and now even the writes interleave. A map that

  needs ordered output has to put the ordering in the map, by routing through a
  station that imposes it. The demonstration in the steps runs

  the same map twice and shows the same set in a different order, so the
  non-guarantee is written where it can't rot.



that's fine.


so, question, how is the output station marked as such? Is there like, some data
in the map file for any return value locations it should write to, essentially
turning every map file into a box that can be instantiated as a box station?
What would need to change to make maps and boxes the same thing?


--------------------------------------------------------------------------------

### Assistant Response 12

Two things there — one is a good idea with a gap worth recording, and the
other is a genuinely deep question. Let me put the first into the issue.


## How the output station gets marked


A line in the map file naming it, carried through the loader and the dump so a
dumped map still round-trips. That's all issue 209 adds.


But you've spotted what that line actually is: **declaring the output is half a
signature.** Declare the inputs too and the map file stops being a description
of a structure and becomes a description of a *callable thing*.


## What would have to change


Here's what a box is, to the engine — a registry record holding a name, a
function pointer, a parameter list with each type and size, a return type and
size, the exact allocation one invocation needs, and optionally a comparison
function.


A map already has a name. With 209 it has an output station, whose box's return
type is the map's return type. What it's missing is **a declared input list**
— which stations' ports are the map's parameters, in order. Add that and a map
file has a signature: parameters in, one value out, sizes known. It can be a
registry entry beside the boxes, and its shim is generic — one function that
knows how to instantiate and run a map, not something generated per map.


Then you hit the real mismatch, and it's not about types. **A box invocation is
synchronous: the shim runs, returns a value, done. A map is asynchronous: values
go in, results come out later, on other threads.** You can't make a box out of
that without deciding which one wins.


And here's the thing — **the engine already has both modes, and has from the
beginning.**


**A map used in the push direction is a splice.** When the importing station's
inputs are all present, its values are delivered into the sub-map's entry ports,
and the sub-map's output station is wired onward to whoever needed the result.
Fully parallel, no blocking, no completion tracking. The cost is that it isn't
really a box: its result emerges at a different place in the graph than where
its inputs went in. This is composition, and it's nearly free — it's what
we've already designed.


**A map used in the pull direction is genuinely a box.** A gatherer already runs
an upstream station inline, on the calling worker's stack, and returns a value.
A map gathered the same way would evaluate to completion on that one thread and
hand back one value. That *is* a box, indistinguishable from a compiled one at
the call site.


And it inherits the gatherer's constraint for precisely the same reason. A
gather cycle is a call that never returns; a synchronously-evaluated map with a
push cycle is the same infinite call. So **the acyclicity rule that already
exists for gather wires is exactly the rule a map-as-box needs** — including
for recursion, where a map instantiating itself is fine in the push direction
and fatal in the pull direction, checked by the walk that already exists.


The price of the pull direction is real: evaluating a map without a pool means
walking readiness synchronously on one thread, which is a second execution
engine, and it only works on acyclic maps. That's the expensive half.


Two more things either direction needs, both cheap once stations are
individually allocated rather than living in one flat array:


- **Instantiation.** A box can sit at many stations, each with its own buffers.
  Placing a map at many stations means building a fresh copy of its station
  graph per placement.

- **Qualified names.** A station inside an instantiated sub-map needs a name
  that doesn't collide with the same station in another instance, and the dump
  has to write those back out in a form that reads.


**My recommendation:** build the push direction, which makes a map *usable* like
a box without *being* one, and costs almost nothing beyond the input
declaration. Treat the pull direction as the thing that would make them
literally the same thing, and note that its price is a second evaluator plus a
real restriction on which maps qualify.


Two issue files are now waiting on your word rather than on any open design
question — the four-value port flag, and this one, which I'd write as "a map
file has a signature" covering the input declaration and the registry entry,
with the pull-direction evaluator named as out of scope and why.


--------------------------------------------------------------------------------

### User Request 13

so, I want to write this down or else I'll forget. If a map is just a box,
then... it might have multiple output ports. We should be able to create and
provide inputs for multiple output box stations simultaneously. It should be a
tag on a box that directs it's output ports. This means we might have multiple
output ports on a box, and they could be wired to separate destinations.


  Then you hit the real mismatch, and it's not about types. A box invocation is
  synchronous: the shim runs, returns a value, done. A map is

  asynchronous: values go in, results come out later, on other threads. You
  can't make a box out of that without deciding which one wins.


the second one. Let's do the second one.


  The price of the pull direction is real: evaluating a map without a pool means
  walking readiness synchronously on one thread, which is a

  second execution engine, and it only works on acyclic maps. That's the
  expensive half.


tell me more?


--------------------------------------------------------------------------------

### Assistant Response 13

Writing the multi-output idea down before it evaporates.


Written as issue 506. The mechanism that makes it cheap: **a box returns one
value, and that value may be a struct** — so the output ports are the fields
of the return struct, with a tag saying which field leaves by which port. The
generator already emits a field table for every struct with every offset as a
compiler-computed expression, so delivery just copies from an offset it's told
instead of from the start of the value. A single-output box is the same path
with offset zero and the full size, so there's one walk, not two.


It also makes "port" mean one thing everywhere: *one of the things this station
produces*. The routing kinds then become what they always were underneath — a
rule about which port is **used** on a given run. Plain uses port zero,
comparator uses one of three, multi-output uses all of them. The issue refuses
the comparator/iterator combination at first and says so by name, because a
comparator routing on one field while fanning others is describable and
unreadable.


On "the second one" — I'm reading that as the asynchronous side winning: a map
keeps its nature, and the importing station is a splice. Say otherwise if you
meant the pull direction.


## The pull direction — and I overstated its price


Two corrections to what I told you.


**The evaluator is cheap, not a second engine.** Today a value delivered into a
station that becomes ready builds a task and pushes it to the pool. Synchronous
evaluation is the same delivery walk pushing to a **local worklist** instead:
deliver the inputs, then loop — pop a task, run it, run the delivery walk,
which may add more — until the list is empty. Termination is "the list is
empty," decided on one thread, which is *simpler* than the sleeper-counting
proof the pool needs. It's maybe fifty lines and one indirection: where does
this map's tasks go.


**And push cycles are fine.** I said a cycle would be "the same infinite call"
as a gather cycle. That was wrong. A gather cycle overflows the stack because
gathering *recurses*; a worklist loop doesn't recurse, so a cycle terminates
exactly when it terminates in the pool — when values stop being produced. The
only thing that must stay acyclic for the recursion reason is the gather
relation *between* maps, and the existing check already covers that.


**The real price is state, and it's worse than what I claimed.**


A gathered box today has no ring buffers at all — that's the rule, and it's
precisely what makes it safe to run from several threads at once. There's
nothing to trample. **A gathered map is full of buffers.** Two workers pulling
the same sub-map instance at the same instant would both deliver into the same
stations and both drain them, and values would cross between the two
evaluations. Not a crash — a silent wrong answer, which is the worst kind.


Three ways out, none free:


- **Instantiate fresh per invocation.** Allocate the whole station graph —
  every station, every port, every buffer — each time the gather runs. This is
  what kills it: today a gather is a function call, and this makes it a graph
  construction.

- **One lock per instance.** Every user of that sub-map serializes. Fine when
  it's pulled from one place, disastrous when it's the shared utility everyone
  pulls from — which is exactly what a reusable sub-map is for.

- **A free-list of pre-built instances.** Amortizes the first, and is a resource
  manager with a sizing decision, which is new machinery of a kind the engine
  has none of.


And here's where acyclicity actually earns its place, for a different reason
than I gave: **an acyclic map that drains fully returns to its initial state**,
every buffer empty, so an instance can be reused at all. A push cycle leaves
residue — a counter loop parks a value in a buffer by design — so the next
evaluation starts contaminated by the last one. Acyclicity isn't about
termination here. It's the condition for reuse.


So the honest summary: the machinery is small, and the semantics are expensive.
The push direction gives you a sub-map used from many places concurrently with
no instance management at all, because tasks just go in the same pool. The pull
direction gives you a genuine box, and hands you the instance-lifetime problem
the push direction never has.


--------------------------------------------------------------------------------

### User Request 14

● Written as issue 506. The mechanism that makes it cheap: a box returns one
value, and that value may be a struct — so the output ports are

  the fields of the return struct, with a tag saying which field leaves by which
  port. The generator already emits a field table for every

  struct with every offset as a compiler-computed expression, so delivery just
  copies from an offset it's told instead of from the start of

  the value. A single-output box is the same path with offset zero and the full
  size, so there's one walk, not two.



I don't think this will work? Well, it could work, but it'd be wasted most of
the time. It's inefficient, and I'll explain why.


if we have two box output stations inside of a boxmap, and one of them gets the
inputs filled in the first half of the program and the other gets their inputs
filled in the second half of the program, then for the first half of the
program's execution cycle we'll be outputting structs that are filled with
values in the first half of their outputs, and empty for the other half. Then,
in the last 50% of the program's execution cycle, it's the opposite - we're
emitting structs to the parent map that are half empty, just, the other half.


I think it'd be better not to treat them as a strict "return" value at all, but
rather to point their "returned" values to a receiving input box, whose inputs
are either filled by a parent map instantiating this box as a sub-box and
connecting up it's input values, OR, which is in this case, or by a sub-box
"returning" values to the parent map, where the parent map is ready to collect
them into input box stations. Does that make sense? We are treating them as just
another box, encapsulating behavior and with output ports that are wired as they
are, and the encapsulated behavior returns values along those wires - we don't
care how. (the truth is, they are running internal boxes, and putting the
results into their output box's, each of which correspond to an output buffer)


  A gathered box today has no ring buffers at all — that's the rule, and it's
  precisely what makes it safe to run from several threads at

  once. There's nothing to trample. A gathered map is full of buffers. Two
  workers pulling the same sub-map instance at the same instant

  would both deliver into the same stations and both drain them, and values
  would cross between the two evaluations. Not a crash — a silent

  wrong answer, which is the worst kind.



sorry I missed something - why don't we instantiate them with separate memory
locations? Why would two sub-maps of the same type share anything in terms of
memory?


Also, we're changing the "only ring buffers" rule, remember? Now every station
has all the machinery. Or did you not write that down? The reason we're doing
this is to liberate the user from needing to keep that in mind.


--------------------------------------------------------------------------------

### Assistant Response 14

Two real catches. The port design isn't written down anywhere — you offered
it, I argued the preference chain out of it, and then I never recorded what
survived. Fixing that first.


Both catches land. Taking them in order.


## The port record — you're right, I never wrote it down


Issue 210, and it supersedes the port half of three completed issues that each
designed one kind of input in isolation. What survived the argument:


Four-value tag — ring, gather, static, **none** — with **exactly one live**
and storage for all three present. No preference chain, because that's how a
typo becomes behavior. What the uniform storage actually buys isn't ambiguity,
it's that changing a port's source is a **field write** under the station's
mutex instead of a free-and-retag, which is what makes runtime reconfiguration
cheap enough to do while values are flowing.


"None" means unconfigured, and a station holding one can never be ready —
which is what makes a map assemblable from nothing: a station is born with every
port unconfigured, can't run, and becomes runnable as its ports get sources.
Optional parameters are the separate feature, with the generated wrapper
carrying a presence flag so an optional parameter *looks* optional in the C
signature, and the check at configuration time rather than in the shim.


And on the rule: **what changes is where the storage lives, not the rule
itself.** It restates as *a station that is gathered from may have no port whose
live tag is ring.* Every port still has ring storage; what matters is which
source is selected. It's still doing the same work — a gathered station has
nothing to consume and nothing to trample, which is what lets several threads be
inside it at once.


## The struct return — you're right, and here's the exact boundary


The reason it works for a compiled box is narrow: **a C function returns once,
so all its outputs happen at the same instant.** There's no such thing as a box
that fills port one now and port two later. The struct isn't a container of
unrelated results, it's one result with parts, and fanning its fields states
exactly that.


A map has no such moment. Its output stations fire on their own schedules, so a
struct return would emit half-filled values in both directions across a run —
exactly your first-half/second-half case — *and* impose a synchronization that
isn't real, since nothing says two outputs belong to the same instant.


So a map's output ports are simply wires: each output station delivers into the
parent whenever it's ready, independently. No mechanism needed, it's
composition.


And the interface is identical from outside, which is the whole point. A station
with several output ports is a station values come out of, on numbered wires.
Whether two values came from one run of a compiled function or from two internal
stations that finished minutes apart is the producer's business, and nothing
downstream can tell or needs to. 506 now says this rather than implying the
struct is the universal answer.


## The memory question — my framing was wrong, the cost isn't


You're right: two placements of the same sub-map share nothing. Each placement
gets its own stations and its own buffers, exactly as a box placed at two
stations gets two sets. I wrote "the same sub-map instance" and then argued as
if that meant "the same sub-map type," which was sloppy.


The actual collision is narrower. **One placement, pulled by two concurrent
invocations of the same puller.** Two invocations of one station may run
simultaneously — that's the engine's central parallelism, not an edge case —
and both would reach into the same instance. So the fix isn't per-placement
instantiation, which we already have. It's per-*invocation*, which is the
expensive one: building the whole station graph every time the pull happens.


And here's the sharper version of the same point, using the restated rule. What
you'd be gathering from a map is its output station — whose input ports are
**live rings**, fed by the map's internal stations. The gatherable rule forbids
exactly that. So gathering a map isn't a special case of gathering; it's
**suspending the rule that makes gathering safe**, and the instance-state
problem is the bill for suspending it.


Which is the whole argument for the push direction, and you've already chosen
it. The pull direction stays interesting as the thing that would make a map
*literally* a box rather than merely usable as one — and now the price has a
one-line statement.


87 pages, zero unresolved references. Four open issue files now carry the
design: the port record, the output station, the map signature work still
unwritten, and multi-output boxes.


--------------------------------------------------------------------------------

### User Request 15

  The memory question — my framing was wrong, the cost isn't


  You're right: two placements of the same sub-map share nothing. Each placement
  gets its own stations and its own buffers, exactly as a box

  placed at two stations gets two sets. I wrote "the same sub-map instance" and
  then argued as if that meant "the same sub-map type," which

  was sloppy.


  The actual collision is narrower. One placement, pulled by two concurrent
  invocations of the same puller. Two invocations of one station

  may run simultaneously — that's the engine's central parallelism, not an
  edge case — and both would reach into the same instance. So the

  fix isn't per-placement instantiation, which we already have. It's
  per-invocation, which is the expensive one: building the whole station

  graph every time the pull happens.


  And here's the sharper version of the same point, using the restated rule.
  What you'd be gathering from a map is its output station — whose

  input ports are live rings, fed by the map's internal stations. The gatherable
  rule forbids exactly that. So gathering a map isn't a

  special case of gathering; it's suspending the rule that makes gathering safe,
  and the instance-state problem is the bill for suspending

  it.


  Which is the whole argument for the push direction, and you've already chosen
  it. The pull direction stays interesting as the thing that

  would make a map literally a box rather than merely usable as one — and now
  the price has a one-line statement.



please explain this in more detail


--------------------------------------------------------------------------------

### Assistant Response 15

Let me build it from the mechanics, because the whole thing turns on one
property that's easy to miss.


## Why gathering is safe today


A port tagged as a gatherer holds an upstream station's identity. When a task is
being assembled and hits that port, the gather path runs the upstream station
**inline, on the assembling thread's own stack**: it lays out that station's
argument values in a stack array, fills them (a static is copied, a nested
gatherer recurses), builds a borrowed task struct pointing at those stack
values, and calls the shim. The return value lands directly in the assembling
task's input slot.


No mutex is taken on the upstream station. No pool, no allocation, no
bookkeeping.


That's safe for exactly one reason: **a gathered station holds no state that the
gather touches.** Its ports are statics or other gatherers. Reading a static
copies bytes and advances nothing. Recursing writes nothing. The only mutable
memory in the whole operation is the calling thread's own stack frame, which no
other thread can see.


So two workers can be inside the same gathered station at the same instant and
never notice each other. The station is effectively a pure function with some
constants attached. That's the whole basis of the rule, and it's why the rule is
phrased as "no live ring port" — a ring buffer is precisely the thing that has
a head index, a tail index, and mutation.


## What breaks when the gathered thing is a map


A map's internal stations have live ring ports. Delivery writes at the tail and
advances it; claiming reads at the head and advances it. Those are mutations,
protected by each station's mutex — which works perfectly when the map is
driven by the pool.


Now here's the scenario, concretely.


Parent station **P** has a gather port pointing at sub-map **M**. P becomes
ready twice in quick succession — two deliveries complete its ring inputs —
so two tasks are being assembled on two different workers at the same moment.
Nothing exotic; that's the normal case for a station with a fast upstream.


Both assembling threads reach P's gather port. Both begin evaluating M.


Evaluating M can't be the current inline recursion, because M's stations need
values delivered into ring buffers, not returned up a call stack. So it means:
deliver the caller's inputs into M's entry stations, run a local worklist until
empty, then read M's output.


Thread A delivers its inputs into M's entry stations. Thread B delivers its
inputs into the **same** entry stations. Now an internal station has two values
queued — one from A, one from B. It becomes ready twice and produces two
tasks. A's worklist gets one, B's gets the other, or one thread gets both,
depending on who pushed when.


Then A's loop runs a task and delivers its result into M's internal stations —
the ones B is also reading and writing. A's value completes a station whose
other input came from B. The two evaluations braid together through shared
buffers, and each caller receives an output computed from a mixture of both
callers' inputs.


**Nothing crashes.** Every mutex holds. No memory is corrupted. Every individual
operation is correct. The answers are simply wrong, in a way that depends on
thread timing — so it reproduces intermittently, or on one machine and not
another. Every guarantee the engine offers is still technically satisfied while
it happens.


## Why per-placement instantiation doesn't help


Per-placement means: if stations P and Q both pull M, they get two copies.
That's right, and it does solve P-versus-Q.


But the collision above is **inside P**. One placement, two concurrent
invocations of it. The placement count is one; the count of things running at
once is two. Instantiating per placement hands one copy to a station that can be
running twice.


## The three ways out, and what each costs


**Instantiate per invocation.** Build a fresh copy of M every time P hits its
gather port — every station, every port, every buffer allocation, then tear it
down. Today a gather is a function call costing nanoseconds. This makes it a
graph construction costing one allocation per station in M. That's the cost that
ends the idea for any sub-map worth composing.


**One lock per instance.** P's two invocations serialize at M. Correct and
trivial to implement — but the point of pulling a shared sub-map from many
places is that many places can pull it at once, and today a gathered box running
concurrently is a *feature*. This removes it exactly where it matters most. And
there's a second, worse consequence: while A holds M's lock, B's worker is
**blocked**. A blocked worker is one not running the ten other things that are
ready — the engine would be violating its own "nothing blocks" rule from the
inside, which is the rule that makes deadlock impossible in the first place.


**A free list of pre-built instances.** Amortize the first: keep some copies,
hand one out per invocation, return it after. Now there's a sizing decision —
how many? — and undefined behavior when they run out: block, allocate, or
fail. That's a resource scheduler, in a machine whose whole character is that
readiness is the only scheduler.


## Where acyclicity actually earns its place


If an instance is reused — options two and three — reuse requires that M
returns to its starting state after each evaluation: every buffer empty, every
index home. An acyclic map that drains fully does exactly that, because every
value produced gets consumed.


A map with a push cycle does not. A counter loop parks a value in a buffer **on
purpose** — that's how the engine carries state at all. So the next evaluation
starts contaminated by the last one, and its answer depends on how many times
the map has been used before.


So acyclicity here isn't about termination, which is what I originally said and
was wrong about. It's the condition under which an instance is clean enough to
use twice.


## And why push has none of this


In the push direction the sub-map's stations are simply more stations in the
same graph, and its tasks go in the same pool. Two parent invocations delivering
into M produce two flows through M's buffers that **cannot** braid — because
each value is claimed by exactly one task, under the station's mutex, and that
is precisely what ring buffers are for.


That's the deep version: push composition inherits the mechanism that already
solves concurrent flows through shared structure. Pull composition has to solve
it again, from scratch, without the buffers that solve it.


If you want this kept, it's a natural implementation note — the pull
direction, why it's coherent, and the exact bill. Say the word.


--------------------------------------------------------------------------------

### User Request 16

  Evaluating M can't be the current inline recursion, because M's stations need
  values delivered into ring buffers, not returned up a call

  stack. So it means: deliver the caller's inputs into M's entry stations, run a
  local worklist until empty, then read M's output.


okay I was with you until we got to "then read M's output", and I walked back a
step and realized "run a local worklist" is also false.


when we "call" a "submap" what we're really doing is composing it with the
current map. So, we fill in the input values of the sub-map's input boxes, and
then we walk away. They should run normally. The output ports of the submap are
what the map's "box station" has it's wire's connected from.


  Thread A delivers its inputs into M's entry stations. Thread B delivers its
  inputs into the same entry stations. Now an internal station

  has two values queued — one from A, one from B. It becomes ready twice and
  produces two tasks.


yes this is what ring buffers are for. The misunderstanding is here:


A's worklist gets one, B's gets the other,

  or one thread gets both, depending on who pushed when.


  Then A's loop runs a task and delivers its result into M's internal stations
  — the ones B is also reading and writing. A's value completes

  a station whose other input came from B. The two evaluations braid together
  through shared buffers, and each caller receives an output

  computed from a mixture of both callers' inputs.



we are not running the submap like a function. We are not using "worklists". We
are simply directing inputs to outputs, and queueing tasks when all input values
are present.


any other concerns?


--------------------------------------------------------------------------------

### Assistant Response 16

You're right, and it goes further than a correction of detail.


The worklist was an artifact of the pull model — and once you say "we're
composing, not calling," **the pull direction stops being an expensive option
and becomes a category error.** Gathering means *run this now and hand the value
back up the call stack*. A map doesn't hand values back; its outputs emerge
along wires whenever they emerge. There's no moment to return at. So "a map used
as a gathered box" isn't costly, it's incoherent, and the instance-state
analysis I gave was analyzing something that can't exist. Push composition isn't
the cheaper of two options. It's the only one.


Let me record the concern that *does* survive, because it's real and it isn't
about sub-maps.


## The one that survives: values aren't correlated


You said "this is what ring buffers are for," and you're right that they keep
two values from being claimed twice. But they don't keep the *right two*
together.


A task pairs whatever is at the head of each port. Nothing marks which flow a
value came from, because the engine has no notion of a flow — only values in
buffers. That's fine in a chain. It bites where a graph **splits and rejoins**:
one station fans to two paths, and those paths later meet at a third station.


Send two values through. The tasks on path X and path Y run on different threads
at different speeds, so they can finish out of order — X2 delivered before X1
is entirely ordinary. The rejoining station then pairs X2 with Y1. Type-correct,
every buffer behaved, run completes normally, answer built from two different
inputs.


This is not a sub-map problem — it's the engine as it stands today, and I
should have named it earlier. But composition makes it far likelier to matter,
because a sub-map is exactly a large subgraph where a caller's inputs travel
independently and rejoin at the output station. Feed a sub-map twice in quick
succession and its output station can pair the first call's result-A with the
second call's result-B.


Recorded in the guarantees document under what is *not* guaranteed, including
the mitigations that don't require changing anything: keep one flow in the graph
at a time, or route both paths through a single station so they can't reorder
relative to each other. The general fix is a tag riding with every value and
ports matching tags instead of taking heads — that's a different engine, not
an adjustment to this one, and it's worth knowing it exists so nobody reinvents
it badly.


## Three smaller ones


**Two parent stations wired into one sub-map instance** mix their flows inside
it, and its output station can pair one parent's result with the other's. Same
hazard, sharper. It's not wrong — the author wired it that way — but it's
the case where "give each placement its own instance" stops being an
optimization and becomes correctness. Worth a load-time warning, or at minimum a
sentence in the composition documentation.


**A sub-map has no completion the engine can see.** Under composition there's no
invocation boundary at all, just flow. That's the design and it's why the
unified pool works — but it also means the mispairing above has no possible
detector. Nothing can notice that a pair came from two calls, because nothing
knows what a call is.


**Buffer growth moves inside the sub-map.** Feed one faster than it drains and
its internal buffers absorb the imbalance, exactly as they do anywhere else.
Already visible in the growth reporting, so this is a "know where to look"
rather than a defect.


Nothing else concerns me. The composition design holds up — I'd just want the
correlation hazard written into the composition documentation when it gets
built, since that's where someone will meet it.


--------------------------------------------------------------------------------

### User Request 17

yes this is intended behavior. Anything flowing along a wire should be
considered atomic, meaning, it doesn't depend on any correlations between other
data values. If you wanted to keep them together, you should build them into a
struct and pass that around.


so, the third station that receives the fan-in of the first and second station
might receive them in the wrong order. That's absolutely fine. Each of the first
two station's output values should be considered independent of the other
station's output values.


--------------------------------------------------------------------------------

### Assistant Response 17

That's a cleaner principle than the way I wrote it down — I framed it as a gap
with mitigations, and it's actually a rule. Let me check where it belongs.


Reframed. It was in the wrong section of the wrong document — filed as a gap
with workarounds, when it's a rule with a consequence.


**In the guarantees document** it's now a new section at the top, ahead of
boxes, because it's the most fundamental thing and everything else handles
values:


- **A value on a wire is atomic and independent** — no relationship to any
  other value, none to whatever run produced it. What it costs: anything that
  belongs together must *be* one value, a struct on one wire, rather than two
  values that happen to arrive near each other.

- **A station pairs whatever is at the head of each of its ports** — there's
  no notion of a round, a batch, or a matching set, and nothing could implement
  one without a tag riding on every value.


The prose says the diamond case plainly and then says it isn't a defect: a
station that fans to two paths has produced two unrelated values, and the
station where they meet takes the oldest available of each, which is the only
thing it could do without being told about a relationship nobody expressed. **If
two things were meant to stay together, they were meant to be one value.**


**In the overview** it's a new section between the one rule and what that rule
buys, because a reader needs it before they start reasoning about graph shapes.
It ends on the parallel that makes it stick: correlation is something the map
author builds out of values, exactly as state is something the map author builds
out of wiring. Both are cases of the same move — the engine gives you no
ambient anything, and what you want you construct.


That parallel is worth noticing generally. A box can't remember, so state
becomes a loop through a buffer. A value can't be correlated, so grouping
becomes a struct. Both times the engine's refusal to provide something quietly
pushes the thing into the map, where you can see it.


87 pages, zero unresolved references. Nothing committed — the tree now has
three new issue files, two reopened ones, and edits to four documents plus the
Makefile fix.


--------------------------------------------------------------------------------

### User Request 18

Okay. Describe the shape to me?


--------------------------------------------------------------------------------

### Assistant Response 18

Three things, with decreasing lifetimes. A **box** is a compiled C function —
it lives as long as the binary. A **station** is one placement of a box in a
structure — it lives as long as the program. A **task** is one invocation —
it lives for one call. Everything else is arrangement.


The structure is bolted down. Stations don't move, wires don't move, boxes are
compiled in. **The only thing in motion is values**, copied from one station's
output into another's input buffer. And the copying is the scheduling: whoever
writes a value into a buffer then checks whether that station's other buffers
are full, and if they are, builds a task and drops it on the pile. Nothing
polls. Nothing looks for work. The act of finishing is the act of starting
something else, so the graph propagates the way fire does — each completion
discovers the next.


The workers aren't assigned to stations. They're a crew taking whatever ticket
is on top of the pile. When the pile is empty and every one of them has checked
and found nothing, the day is over — and that's the whole termination story,
one sentence with a careful proof under it.


## What this session did to the shape


Something consistent, which I only see clearly now that you've asked.


Every time we found something the engine was holding centrally, it turned out to
belong on a station, and moving it there made the center smaller:


- Constants lived in a table on the map. They belong on the port that reads
  them. → the map holds no state.

- Wires named stations by number, which only means something relative to a
  table. They should point at stations. → the map leaves the delivery path
  entirely.

- Completion looked like it needed per-map bookkeeping. It's just a station
  whose inputs are all full. → no counters, one pool for every map.

- Output looked like it needed a collection with its own storage and a drain at
  the end. It's a station. → no new machinery, and the results are finished
  before the pool stops because they were always ordinary tasks.

- Correlation looked like it needed tags riding on values. It's a struct. →
  the engine never learns what a group is.


So the map is dissolving into its stations. What's left of it is a name, a list
of what belongs to it, and a pool — a label for a connected set of stations
rather than a container that owns them. And the pool was never an authority
either: it reads one field of a task and knows nothing about what a station is.


**Two things that looked like central authorities turn out to be bookkeeping
around a set of stations that can already run themselves.** That's the shape.


## The through-line


The engine refuses to provide ambient anything, and each refusal pushes a
capability out into the map where it becomes visible.


A box can't remember, so state becomes a loop through a buffer — you can see
it in the wiring. A value can't be correlated, so grouping becomes a struct —
you can see it in the type. A constant can't be shared, so a shared knob becomes
a station everyone pulls from — you can see it as five wires instead of five
references to a number that appears nowhere.


Each of those is the same trade: the engine gets simpler, and the map gets more
honest. Nothing happens that isn't drawn.


## Where it points


A sub-map is more stations bolted to the same floor, sharing the same crew.
That's why composition costs almost nothing and why the unified pool survives
— nothing about finishing requires knowing which map a task came from.


And a map with declared inputs and outputs is a box: a name, a parameter list, a
result. Not because a map is implemented like a function, but because from
outside, both are a thing values go into and come out of on numbered wires.
What's behind the ports is the producer's business.


--------------------------------------------------------------------------------

### User Request 19

... okay can you describe the shape of the thing we've been discussing, not the
entire system?


--------------------------------------------------------------------------------

### Assistant Response 19

Right — the shape of the *design work*, not the engine.


It's a stack, and it has one thing at the center: **the map is being taken out
of the middle.**


## Two foundations


Nothing above these can happen first.


**One shape for every input port.** Today a port is a different thing depending
on what feeds it, and converting between them is surgery. Making every port the
same record — room for a buffer, a constant, and a pull, with a tag saying
which is live — is what makes reconfiguring a port a field write instead of an
allocation. Everything about assembling a map at runtime rests on this.


**A station is found directly, not looked up.** Today a task says "station
seven," and seven only means something relative to a table, which means the
delivery path needs the map to interpret it. Point at the station instead and
the map has no job on that path at all.


## What falls out of them


A constant stops living in a table on the map and lives on the port that reads
it — so the map holds no state. That removes the only real reason anything
reaches for a process-wide map pointer, which is the thing that quietly permits
exactly one map per process.


With the map off the delivery path and no state in it, several maps can exist at
once and share one pool. Which is composition: a sub-map is more stations in the
same graph, its tasks in the same queue.


## What it was all for


A map that declares its inputs and its outputs has a signature — parameters
in, results out — and can be used exactly like a box. Two pieces support that:
naming which station a map's results come from, and letting a compiled box have
several output ports so it has what a map already has.


That's the top of the stack, and everything below it exists to get there.


## The side track


Separate from the stack, running alongside: making the thing usable by someone
who isn't us. The generator in C so a build needs nothing but a compiler; the
single-file shape so internal names stop escaping; errors that die loudly but
let a host know why. And the record of what's always true, which is what makes
any of it safe to build on.


## The method, which kept repeating


Five times, something that looked like it needed new machinery turned out to be
sayable in machinery that already exists.


Completion looked like per-map bookkeeping — it's a station whose inputs are
full. Output looked like a collection with its own storage and a drain at the
end — it's a station. A shared constant looked like a table — it's a station
everyone pulls from. Keeping two values together looked like tagging every value
— it's a struct. And the gather timing question, where the answer was to keep
what was already built and let the guarantee decide.


Each time, the issue got *smaller* as we talked. That's the strongest signal in
the whole shape.


One branch got pruned rather than shrunk: running a sub-map like a function,
synchronously, returning a value. That one isn't expensive — it's incoherent.
Gathering means "run now and hand it back"; a map's outputs emerge along wires
whenever they emerge. There's no moment to return at.


--------------------------------------------------------------------------------

### User Request 20

  A station is found directly, not looked up. Today a task says "station seven,"
  and seven only means something relative to a table, which

  means the delivery path needs the map to interpret it. Point at the station
  instead and the map has no job on that path at all.


yes, with the functionality that if we need to move the station, there's a
pointer left behind at the exact spot the old station was. When a task struct
with an obsolete station pointer (after running a long box function and trying
to return it's outputs to a station that has been moved) checks and sees that
there's another pointer at the location where there should be a station, then it
checks the location of that pointer, as many times as it needs to, until it
finds the station at the end of the list. Though we should make sure that we
don't do any loops, so during this process store all the pointers you gather and
each time you get a new one, make sure it's unique, and keep searching until you
find the box station you need, then record your outputs in their inputs and... I
dunno, die I guess, because you're a task struct and your job is done. I guess.


--------------------------------------------------------------------------------

### Assistant Response 20

Before the mechanism — **what forces a station to move?**


I ask because I don't think anything currently does, and if that's right the
mechanism has no customer. Every variable-size part of a station already hangs
off a pointer: the buffer storage, the port array, the destination lists.
Growing a ring buffer reallocates only the storage the port points at — the
station doesn't budge. Adding a port reallocates the port array, not the
station. And under individually-allocated stations, adding a *new* station
doesn't disturb existing ones the way growing a flat array would.


So the move you'd be forwarding around is one I can't find a cause for. If
you're thinking of compacting for cache locality, or of something in the
runtime-assembly work I haven't seen yet, name it — because the mechanism
should be designed for the real case, and right now I'd be designing it for a
hypothetical.


If moves do happen, the design is close to right. Three things about it.


**You need a discriminator, and it costs the hot path.** The memory at the old
address holds either a station or a forwarding pointer, and something has to say
which. That's a tag in the first word and a branch on every delivery to every
destination — which is the hottest path in the engine. Cheap, but paid always,
for an event that may never occur.


**Loops can't happen, so the visited-set check isn't needed.** A forwarding
record is written once, when a station moves, and it points at freshly allocated
memory that has never been anything before. So the chain is strictly
older-to-newer, and a cycle would require a forwarder pointing backwards, which
nothing can create. It's the same induction as the gather cycle check: refuse
nothing, because the construction can't produce one. That means no allocation
and no bookkeeping during delivery — just follow until the tag says "station."


**The hazard is address reuse, and it's worse than a loop.** If a forwarding
record were ever freed, the allocator could hand that same address to a newly
created station. A task holding the stale pointer would then arrive, see a valid
station tag, and deliver into **the wrong station** — type-correct, silent, no
crash. So "never free the forwarding record" isn't a leak you tolerate to keep
things simple. It's the thing that makes the scheme correct, and it should be
written down that way, because the first person to notice the leak will try to
fix it.


The cost of that is bounded and small — a couple of words per move, forever.


**One refinement that shrinks the window a lot.** Right now a moved station
leaves stale pointers in two places: in-flight tasks, and every *destination
record* upstream that points at it. If each station kept a list of its
**inbound** wires, a move could update every upstream destination immediately,
and forwarding would only ever be needed for tasks already in flight. That's a
much smaller and more bounded population than "everything that ever pointed
here."


And a station knowing who points at it is useful independently — disconnecting
cleanly needs it, and so does any dump that wants to report a station's
neighbourhood without scanning the whole graph.


On the last part: yes. Deliver into the destinations and die is exactly the
existing lifecycle — the worker runs the task, hands it to delivery, frees it.
Your instinct there matches what's already built, including the "I guess" part,
which is genuinely all there is to it.


--------------------------------------------------------------------------------

### User Request 21

● Before the mechanism — what forces a station to move?


  I ask because I don't think anything currently does, and if that's right the
  mechanism has no customer.


if we need to grow the ring buffers. Or if we want to delete a station.


you say the ring buffers are pointers to ring buffers, and that's alright. Who
can hold pointers to box stations? I need to know so we can decide which shape
would be better.


  You need a discriminator, and it costs the hot path. The memory at the old
  address holds either a station or a forwarding pointer, and

  something has to say which. That's a tag in the first word and a branch on
  every delivery to every destination — which is the hottest path

  in the engine. Cheap, but paid always, for an event that may never occur.



nuh-uh! It's just a discriminator when recording output values in input slots.


  The hazard is address reuse, and it's worse than a loop. If a forwarding
  record were ever freed, the allocator could hand that same address

  to a newly created station. A task holding the stale pointer would then
  arrive, see a valid station tag, and deliver into the wrong

  station — type-correct, silent, no crash. So "never free the forwarding
  record" isn't a leak you tolerate to keep things simple. It's the

  thing that makes the scheme correct, and it should be written down that way,
  because the first person to notice the leak will try to fix

  it.



eh... don't worry about this


  One refinement that shrinks the window a lot. Right now a moved station leaves
  stale pointers in two places: in-flight tasks, and every

  destination record upstream that points at it. If each station kept a list of
  its inbound wires, a move could update every upstream

  destination immediately, and forwarding would only ever be needed for tasks
  already in flight. That's a much smaller and more bounded

  population than "everything that ever pointed here."



why are we doing this again?? because we don't want to have a map that has
pointers to each box station?


--------------------------------------------------------------------------------

### Assistant Response 21

## Why we're doing it


Not to stop the map from knowing its stations — **the map still holds a
pointer to every station**, for iteration, teardown, reporting, and names. That
was never the problem.


The problem is narrower: a *wire* records "station seven," and seven only means
anything relative to a particular table. So when a worker finishes a task and
goes to deliver, it must consult a map to turn the number into an address —
which means the delivery path needs to know which map it's working for. That's
the single fact that makes one map per process, blocks two maps sharing a pool,
and therefore blocks composition. Put an address in the wire instead and
delivery needs no map at all.


So: the map keeps its list. The wires stop using indices.


## Who holds a station reference


| holder | today | can it be found and updated when a station moves? |

|---|---|---|

| the map's own station list | the array itself | **yes** — single owner, one place |

| a destination record on some upstream port | station index | only by scanning every port of every station, unless each station keeps a list of its inbound wires |

| a gatherer port's source | station index | same — findable only by scanning |

| a task | station index | **no.** Tasks sit in the queue and on worker stacks. Nothing can enumerate them |

| a worker mid-delivery | a local pointer on its stack | **no, and worse** — it has already resolved the address and is using it |


That last row is the one that decides the shape. Delivery takes a station's
mutex, snapshots its destinations, releases the mutex, and then walks them.
During that walk it holds a raw pointer to a station it is no longer locking. A
forwarding pointer cannot help there — forwarding fixes a stale reference that
gets *re-resolved before use*, and this one has already been dereferenced.


## Neither of your two causes actually moves a station


**Growing a ring buffer doesn't.** The port record holds a *pointer* to the
buffer's storage. Growth allocates fresh storage, copies the contents, frees the
old block, and updates that pointer. The port record doesn't move; the station
doesn't move. Same for growing the number of ports — the port array hangs off
a pointer too, so reallocating it moves the ports, not the station. This is
deliberate and it's why wires and in-flight values survive growth today.


**Deleting a station doesn't move it either — it removes it.** A forwarding
pointer answers "where did it go," and the answer here is "nowhere, it's gone."
That's a different question with a different mechanism: a tombstone saying
*dead*, plus a decision about what a delivery into a dead station does. Discard
silently? Stop the program? Both are defensible and they're very different maps
to debug.


And deletion runs straight into the last table row: you cannot free a station
that a worker might be inside. Which points at the answer you'd already reached
from the other direction — don't free, ever. Mark it dead and leave it.


So the shape I'd take: **stations allocated individually, never moved, never
freed. The map holds a list of pointers. Wires hold pointers. Deletion writes a
tombstone.** No chains, no discriminator on the delivery write, nothing to
follow — because the case that needs following doesn't arise.


You were right about the discriminator being narrow, and I over-weighted it.
It's one load and a predictable branch, on a path that's already taking a mutex
and doing a memcpy. That's noise. My objection should have been that it's
machinery for a case that doesn't occur, not that it's expensive.


--------------------------------------------------------------------------------

### User Request 22

  The problem is narrower: a wire records "station seven," and seven only means
  anything relative to a particular table. So when a worker

  finishes a task and goes to deliver, it must consult a map to turn the number
  into an address — which means the delivery path needs to know

  which map it's working for. That's the single fact that makes one map per
  process, blocks two maps sharing a pool, and therefore blocks

  composition. Put an address in the wire instead and delivery needs no map at
  all.



this design makes sense if we need to move box stations. Right now we only need
to be able to create, modify, and delete them. Moving doesn't really have a
use-case, because we're using pointers to the ring buffers right? Would it be
faster to instead keep the ring buffers in the station, and grow the actual
station itself, with moving and... hmmm... maybe it's better to have pointers to
ring buffers. What do you think?


--------------------------------------------------------------------------------

### Assistant Response 22

Pointers to the storage — but you can have most of the locality anyway,
without ever moving a station.


## Why inline-and-grow is the wrong trade


Growing a buffer inline means reallocating the station, which moves it. And
moving is not merely awkward here — it's unsolvable for one specific holder.
When a worker delivers, it takes the station's mutex, snapshots the destination
list, **releases the mutex**, then walks those destinations. Throughout that
walk it holds a raw pointer to a station it no longer locks. A forwarding
pointer can't rescue that: forwarding fixes a stale reference that gets
re-resolved before use, and this one was resolved already and is being used
right now.


So inline-and-grow buys locality and pays with a correctness problem that has no
cheap fix. Bad trade.


## But your instinct about locality is right


Look at what a single delivery actually touches today. The station record, for
the mutex and the port count. Then the **port array**, which is a separate
allocation. Then, for the readiness walk, one storage block per port. A station
with three inputs touches five separate allocations to do one delivery, and
they're scattered wherever the allocator put them.


That's worth fixing, and it doesn't require moving anything.


## The shape I'd take


**Size the station's single allocation at placement, and never resize it.** One
block: the station header, then the port records, then a small run of inline
cells for each port — enough for a handful of values, sized from the element
size the registry already knows. A port that overflows its inline cells spills
to a heap block and sets a pointer, exactly as today.


The common case never leaves the station's own allocation. Deep backlogs pay a
dereference — and a deep backlog is already the exceptional case, which is
precisely why buffer growth is a reported statistic rather than routine.


This is also the house pattern rather than a new idea. A task is already one
allocation sized exactly for its box — the struct, the pointer array, the
input bytes, the output bytes — computed from the registry before the task
exists. Doing the same for a station is the same trick at a longer lifetime.


## The one condition it depends on


A station's port count must be fixed when the station is placed. That's true
today, because the port count is the box's parameter count and the registry
supplies it.


It stays true as long as **a station's box is chosen at placement and never
swapped**. If "modify" is ever meant to include pointing a live station at a
different box, the parameter list can change, the allocation has to grow, and
we're back to moving.


I'd draw that line explicitly: changing which box a station runs is a *delete
and create*, not a modify. You keep create, modify, and delete — swapping a
box is just spelled with two of them instead of one. In exchange, a station
never moves, wires stay valid forever, no forwarding chains, no discriminator,
and one allocation per station instead of two plus one per port.


--------------------------------------------------------------------------------
