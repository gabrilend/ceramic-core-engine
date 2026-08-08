# 106 — Stopping on purpose

The sibling of [104](completed/104-termination-by-last-sleeper.md).
That issue built the one way a program ends by itself: work runs out,
the last worker to fall asleep looks once more, finds nothing, and
stops everyone by broadcast. This is every other way a program ends —
because it was told to, or because it found something it refuses to
continue past.

## Current behavior

**A program has exactly one way to stop, and it is the happy one.**

Running out of work is handled and proven. Nothing else is. There is no
signal handling anywhere in the engine: a person pressing ctrl+C gets
the kernel's default action, which terminates the process immediately
with nothing written, no report, and no indication of which part of the
program was doing what. A service manager sending the polite shutdown
signal gets the same abrupt nothing. The counters, the buffer
high-water marks, the growth counts, and the live map — all of which
exist and are readable — go with it.

**Failure is handled two different ways, and the disagreement is
recorded as deliberate.** The loader dies on a bad map file, collecting
every failure in the file first so its author gets the whole list at
once ([604](completed/604-load-time-validation.md)). Runtime rewiring
does the opposite: it returns minus one, names the reason on the error
stream, changes nothing, and leaves the program running
([704](completed/704-runtime-rewiring.md)). The reasoning was that a
loader that dies serves its author while a running engine that dies for
one bad control instruction takes the plant down with it. The
first-pass report books the cost of that choice as a debt in plain
words: **a caller can ignore a return value.**

**Under one construction surface those became the same call.** Issue
[212](212-one-way-to-build-a-program.md) collapses loading and editing
into one act, so the two policies can no longer coexist. It resolves
in favour of dying, and this issue is where that resolution is built.

## Intended behavior

**An invalid operation ends the program, having first said everything
it can about what went wrong.**

Not a return value a caller may discard. The debt the first-pass report
identified is paid off rather than serviced: there is no surviving path
in which a refused instruction leaves a program running that somebody
believes they just edited successfully. This is also what makes
"complete or visibly incomplete, never half-built" true without the
construction surface having to enforce it — a program cannot be left
partly built by ignored refusals when a refusal ends the process.

**The surface still returns a refusal rather than dying where the
failure happens.** The loader's rule of collecting every failure in a
file and presenting them together survives, and it needs a refusal to
travel upward rather than terminate on the spot. So a refusal is
reported, accumulated, and the stop happens once with the whole list. A
single instruction arriving alone produces a list of one.

**And the program answers three signals, differently.**

They form a progression, and the ordering is the design: each needs
less cooperation from the program than the one before it, and each is
the right answer for a program in worse condition than the last.

| | polite shutdown | interrupt | quit |
|---|---|---|---|
| who sends it | a service manager | a person at a terminal | a person who wants evidence |
| what it means | wind down, there is time | stop, and tell me why | stop now, leave the body |
| diagnostics | none | everything | only what needs no lock |
| drains the queue | yes | yes, around one task | no |
| waits for running boxes | yes | no | no |
| needs a free worker | needs healthy ones | needs exactly one | needs none |
| how it exits | zero, by the existing rule | non-zero, explicitly | aborts, leaving a core |

### The polite shutdown writes nothing

Stop accepting new work and let the program end the way it would have
ended on its own. Boxes already running finish, delivery does its
ordinary work, the queue empties, workers park themselves, and
**the last-sleeper rule from 104 fires unmodified.** Exit zero.

No diagnostics, because nobody asked for any and a supervisor stopping
a healthy program does not want a report it did not request.

This path adds no mechanism at all. It is the ordinary ending,
triggered early, and that is the argument for it.

**It borrows a clock rather than inventing one.** If the program is
wedged this path never completes — which is correct, because whatever
sent the signal already has a timer and will escalate to the
uncatchable kill signal when it expires. The supervisor's clock is the
only clock in the system that knows how long is too long for this
particular deployment. The engine must not guess at one; a guess is
wrong on a slow machine and wrong differently on a fast one.

### The interrupt gathers everything

Somebody is standing there and wants to know what happened. Empty the
task queue, put one diagnostics task into it, and keep emptying around
that task so no new work starts. Let it write the full picture: per
station the completed-run and produced counts, per buffer the
high-water mark and growth count, per worker the station it is
currently inside, and the live map rendered as a map file. Then exit
non-zero.

**The drain needs no new flag, because an empty queue is already the
brake.** Workers that find nothing go to sleep by the mechanism 104
already built. Nothing about the run loop changes.

**A second interrupt skips all of it and exits immediately.** Ctrl+C
twice has to always work. A diagnostic path that can itself get stuck
would otherwise trap the person it was written for, which is the exact
failure it exists to prevent.

### The quit takes no locks at all

Maximum evidence, no cooperation. Do not drain, do not enqueue, do not
wait for any thread, and above all **do not take a single lock** —
because the reason this signal arrived may be that a lock is held by
something that will never release it.

Write only what is readable without cooperation: the per-station
completed-run and produced counts, which are atomic and readable from
any thread, and each worker's current station index. Then abort, which
leaves a core dump, so a debugger attached to the core shows every
thread's stack including the box that is not returning.

**This is the path that works when the other two cannot.** A program
with every worker wedged has no thread free to run a diagnostics task
and no queue that will ever drain. It can only be examined from
outside, and this hands the outside a copy of the inside on the way
past.

## What cannot be done, and why it keeps being attempted

**A running box cannot be stopped.** There is no safe way to interrupt
executing C. The cancellation facility only acts at cancellation
points, so a tight loop containing no system call never reaches one;
even when it does fire it does not unwind C code, and any mutex the
thread was holding stays locked forever — which would freeze every
thread delivering into that station, a worse outcome than the wedge.
So "halt everything" honestly means **stop starting new things**, and
that is achievable, and stopping the ones already running is not.

**Destroying the pool does not reach the box either, and makes things
worse.** Two versions of this idea have been considered and both are
recorded here because both are tempting:

Freeing the pool does not take anything away from the threads using
it. Every worker holds the pool's address in its own record and on its
stack, and freeing only tells the allocator those bytes may be reused.
The workers keep reading the same addresses while some unrelated
allocation begins rewriting them — including the bytes that are the
mutex. The pool does not stop; it starts behaving unpredictably, which
turns a clean crash into an unreproducible one.

Overwriting the pool with zeroes is worse in a more interesting way,
because it looks like it worked. The stop flag becomes false, which
tells every worker *not* to stop. The ring capacity becomes zero, and
the ring arithmetic divides by it, so some workers die of a hardware
exception at an arbitrary instruction. The storage pointer becomes
null, so others die dereferencing it. And a zeroed mutex is, on this
platform, a valid *unlocked* mutex — so the lock is silently released
while a thread is inside the critical section, and two threads can then
hold it at once, each believing it is alone. It also erases the
counters and the ring, which are the things worth gathering.

**The unifying reason all three fail: a wedged thread is inside user
code that never looks at the pool.** A box spinning on its own
arithmetic reads no flag, no capacity, no pointer. Nothing done to that
structure reaches it.

## Why there is no watchdog

Detecting a wedge from inside cannot be done, and the reason is worth
keeping.

The last-sleeper rule is structurally incapable of it: it fires when
workers go to *sleep*, and a worker in an infinite loop never sleeps,
so the count never completes and the rule never runs. It answers "does
everybody have nothing to do," while a wedge is the opposite condition.

A progress counter comes closer. Every station already carries an
atomic completed-run count that is always on and readable without a
lock, so summing them gives one number meaning "how much this program
has ever finished," and a wedge shows up as that number not moving
while every worker is busy and work waits behind them. But **the
threshold is unavoidably a guess.** Sixteen workers each legitimately
running a very long box look identical to sixteen wedged workers, and
no amount of cleverness distinguishes them — this is the halting
problem wearing work clothes.

And under the rule that everything runs as a task inside the engine,
the detector would itself be a task, so when every worker is wedged
there is no thread left to run it. **The system goes silent exactly
when the observation would matter**, which is not a flaw in that rule
so much as what that rule means.

So the engine does not guess. The signal comes from outside, where
somebody who actually knows how long is too long is the one deciding.

## Suggested implementation steps

1. A signal disposition installed during startup for the three
   signals, blocked in every worker at creation so delivery always
   lands on the initial thread. This is the one piece that must exist
   before the engine is running, and it is small.
2. A handler that does exactly one thing: record which signal arrived
   in a flag of the type the standard permits a handler to write, and
   return. It may not take the pool mutex — a handler interrupts an
   arbitrary thread at an arbitrary instruction, possibly one already
   holding that mutex, and locking there deadlocks against itself
   immediately, producing neither diagnostics nor an exit.
3. The polite path: reuse termination unchanged. Prove it by a program
   stopped mid-run whose in-flight work all completes and whose exit is
   indistinguishable from having run out of work.
4. The interrupt path: drain, enqueue one diagnostics task, drain
   around it, exit non-zero when it finishes. Prove it by a program
   with deliberate backlog whose report names every station.
5. The second-interrupt escape, proven by an interrupt arriving while
   the diagnostics task is deliberately blocked.
6. The quit path, taking no locks, proven by a program with a station
   mutex deliberately held — the report must still appear.
7. The construction surface's refusals become fatal, with the
   accumulate-then-stop behaviour the loader already has, and the
   rewiring path loses its ignorable return.
8. A test that an ignored refusal cannot leave a half-built program,
   which is the property this buys and the reason for the change.

## What this change reaches

The refusal policy is stated in eight places that were deliberately
kept in agreement, and all of them move together:

- [704](completed/704-runtime-rewiring.md), which decided it
- [211](211-growing-the-station-table.md), which inherits it explicitly
- [212](212-one-way-to-build-a-program.md), which now answers it
- the first-pass report, which books it as a debt
- the observe interface file and the observe header
- the rewire source, in a comment explaining the return value
- **scene five of the phase 7 demo**, which argues the old position at
  length and measures it — its image is a lever with a mechanical
  interlock, and one of its correspondences pairs a refusal that
  returns with a light saying why, on the grounds that both inform
  without punishing. That scene cannot survive this change. It was
  already scheduled for rewriting in
  [710](710-demos-after-the-pull-path.md) because the illegal operation
  it uses is a gather cycle and gathering is being removed, so it now
  needs rewriting for two independent reasons.

## Open questions

- Where does the diagnostic report go — the error stream, or a file
  named at startup? A person at a terminal wants it on screen; a
  program being stopped by a supervisor has nowhere useful to put
  screen output, and that is also the case where nothing is written.
- What exit code does the interrupt path use? The convention of
  reporting the signal number added to a fixed offset exists and is
  understood by shells, but it collides with the engine wanting to
  distinguish "stopped by a person" from "refused an instruction."
- Does the fatal policy hold everywhere the engine is embedded? A
  program compiled into the browser workbench
  ([801](801-browser-workbench.md)) that takes its whole host down on a
  bad edit is a much worse experience than one that reports, and the
  workbench's whole purpose is people making bad edits and seeing what
  happens.
- Should the interrupt path's diagnostics task be an ordinary task at
  all? It is the one task in the system that must be guaranteed to run,
  and the queue makes no such guarantee to anybody.

## Related

- [104 — Termination by last sleeper](completed/104-termination-by-last-sleeper.md),
  the other way a program ends, unchanged by this
- [102 — Workers and the run loop](completed/102-workers-and-run-loop.md),
  whose stop flag is already the brake and needs no addition
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which decided the refusal policy this builds
- [704 — Rewiring while it runs](completed/704-runtime-rewiring.md),
  whose ignorable return is what changes
- [702 — Station statistics](completed/702-station-statistics.md) and
  [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md),
  which already gather everything the report needs
- [710 — The demos after the pull path](710-demos-after-the-pull-path.md),
  which now inherits a second reason to rewrite the same scene
- [006 — Scheduling](../docs/006-datapath-scheduling.md), which needs a
  section on the ways a program ends
