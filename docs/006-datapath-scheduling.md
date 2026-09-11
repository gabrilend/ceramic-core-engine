# 006 — Datapath: scheduling

Delivery decides *what* runs. The pool decides *when*, and by whom.

It is a fixed set of worker threads, one queue of tasks, and one rule
about sleeping that turns out to also be the rule about when the
program is finished.

## The task queue

A ring of pointers. Each entry points at a task struct sitting on its
own in the heap; the ring holds only the pointers.

Two indices: one at the oldest task, one at the port where the next
task goes. Both wrap at the end. It is first-in-first-out, so a task
that has been waiting cannot be starved by newer arrivals.

**When the ring fills, it doubles.** If the writing index would land on
the reading index, the storage is reallocated to twice the size, the
wrapped portion is copied so the queue reads contiguously again, and
the indices are corrected. This happens while the queue's mutex is
held, so nothing else is inside.

Growth is safe by construction, and for a reason worth stating: nothing
in the program holds a pointer *into* the ring. The ring holds pointers
*out* to task structs, and a worker that takes one carries the pointer
away with it. The ring's own storage can move underneath everyone
without a single dangling reference.

This is reachable, not theoretical. One box wired to a hundred
destinations produces a hundred tasks from a single run, so the queue
can go from nearly empty to overflowing during one delivery.

The alternative — a worker that finds the queue full runs a task itself
and then retries — was rejected. Growth delays an enqueue by a bounded
memory copy; running a task delays it by however long an arbitrary user
function takes.

Task structs are allocated fresh per invocation and freed by the worker
that ran them, after its outputs have been delivered. Each is sized
exactly for its box, since the generator knows what that box needs.
Swapping in a free list later is invisible to everything else.

## Workers

A fixed number of threads, decided at startup. Each loops: take a task
from the queue, run its shim, deliver its output, free the task, repeat.

A worker that finds the queue empty sleeps rather than spins. Spinning
burns a core per idle worker; sleeping costs a trip into the kernel,
which is expensive but paid only when there is nothing to do anyway.

A count of sleeping workers is maintained. When a task is pushed, every
sleeping worker is woken — not a chosen one. A worker that has just
finished enqueuing does not sleep, because the queue is not empty; it
just put something in it.

## Termination

The rule is one sentence: **the worker whose own registration brings
the sleeper count up to the worker total re-scans the queue before
anyone actually sleeps.**

If it finds a task, it deregisters itself, wakes everyone, and goes to
work. If it finds nothing, the program is finished — a stop flag is
set, every worker is woken, each sees the flag and returns, and the
main thread joins them.

The re-scan is not decoration. Without it there is a race that ends in
a silent wrong answer rather than a hang:

1. Worker A scans the queue, finds it empty, decides to sleep — and is
   preempted right there, before registering itself.
2. Worker B finishes a task, delivers, enqueues a new one, checks the
   sleeper count to decide whom to wake, sees zero, and wakes nobody.
3. Worker A resumes and registers itself asleep.

Every worker is now asleep, the count reads "all of them," and there is
a task sitting in the queue. Without the re-scan, that state is
indistinguishable from completion, and the program exits cleanly having
silently not done part of its work.

With the re-scan, worker A is the one whose registration completed the
count, so it looks again and finds B's task. And when the re-scan does
come up empty, the conclusion is sound: every other worker has already
finished its task and found nothing, so there is nobody left who could
enqueue anything.

Termination is a clean broadcast, never a break out of the loop. A
worker left parked in a wait that never ends turns shutdown into a hang
at the join.

## The other ways a program ends

Running out of work is one way and it is the happy one. There are
three more, and every one of them arrives from **outside**, because a
program cannot tell from inside that it should stop
([106](../issues/completed/106-stopping-on-purpose.md)).

**No handler is installed anywhere.** The three signals are blocked in
every thread and the thread that started the program waits for one to
arrive as an ordinary value. That removes the hardest constraint the
design had: a handler may call almost nothing, while a thread holding
a number may take locks, format text, and walk the station table. The
pool's own ending arrives at the same waiting point as one more
signal, so there is one place to wait, woken for two reasons, told
apart by which number came back.

| | polite shutdown | interrupt | quit |
|---|---|---|---|
| who sends it | a service manager | a person at a terminal | a person who wants evidence |
| what it means | wind down, there is time | stop, and tell me why | stop now, leave the body |
| diagnostics | none | everything | only what needs no lock |
| waits for running boxes | yes | no | no |
| how it ends | zero, by the rule above | 130, explicitly | aborts, leaving a core |

**The polite path adds no mechanism.** It shuts the entrance — the one
way the outside can push work through — and the ordinary ending does
the rest. It writes nothing, because a supervisor stopping a healthy
program did not ask for a report.

**The interrupt gathers on the waiting thread itself**, not as a task.
That is what makes it work on a program whose every worker is wedged:
the queue guarantees nothing to anybody, and the one piece of work
that must happen cannot be the one standing in line.

**The quit takes no locks**, because the reason it arrived may be that
a lock is held by something that will never release it.

### There is no watchdog, and there cannot be

Detecting a wedge from inside is not possible, and the reason is worth
keeping. **The last-sleeper rule is structurally incapable of it**: it
fires when workers go to *sleep*, and a worker in an infinite loop
never sleeps. It answers "does everybody have nothing to do," while a
wedge is the opposite condition.

A progress counter comes closer — every station carries an always-on
run count, so a wedge shows up as that sum not moving while every
worker is busy. But **the threshold is unavoidably a guess**: sixteen
workers each legitimately running a very long box look identical to
sixteen wedged ones, and no cleverness distinguishes them. That is the
halting problem wearing work clothes.

And a detector would itself be a task, so when every worker is wedged
there would be no thread left to run it. **The system would go silent
exactly when the observation mattered.** So the engine does not guess;
the signal comes from outside, where somebody who knows how long is
too long is the one deciding.

## What the pool does not do

**It does not detect deadlock**, because there is nothing that can
deadlock. No box blocks. Nothing waits on a value. A worker is always
either running a box, delivering, or asleep with nothing to do — and
the last of those is completion, not a stall.

**It does not prioritize.** Tasks run in the order they were created.

**It does not know anything about boxes, stations, or maps.** It moves
opaque task structs between threads. Everything about what a task
*means* lives on the delivery path.

A task carries a station number, an exit number, and **which program it
belongs to**, all three ferried without being looked at. The last of
those is what lets one pool serve several programs at once, which is how
a program can set another going beside itself.

## Related

- [003 — Delivery](003-datapath-delivery.md), which produces the tasks
- [009 — Loading](009-datapath-load.md), which produces the first ones
