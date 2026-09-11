# 802 — A program you can watch

A program built one way runs and says nothing. Built the other way it
runs identically and **leaves a trail somebody else can read while it
happens** — every station that ran, every value that moved, every
buffer that grew — without the program waiting on anybody, noticing
anybody, or behaving differently because somebody is there.

The trail is the deliverable here. What reads it is
[803](803-the-viewer.md), and it is deliberately not this issue.

## Current behaviour

**Built.** A program compiled with `CERA_WATCH` writes a fixed-size
event into a ring in shared memory at each thing worth seeing, and
anybody who can open the file can read them. The program never waits for
a reader, never learns one is there, and cannot be slowed by one.

Ten kinds of event: the program came up, a task became due, a station
ran and for how long, a value moved from one station's port to another's,
a buffer grew, a station was placed or removed, a wire was drawn or cut,
and the program finished. Each is emitted at the place the thing
actually happens rather than near it.

**The emitting is not in an ordinary build**, and that is read from the
object file rather than asserted: without the flag neither the emitter
nor the clock it reads exists as a symbol, and the object is about three
kilobytes smaller. **The reading is always compiled**, because a watcher
is a different program with no reason to have been built for watching,
and having both halves in one place is what keeps the ring's shape
written down once.

### What the flag has to cover, learned the hard way

The arguments to an emit vanish when watching is compiled out. **Anything
computed on the line above it does not.**

Two places got that wrong: the closing event's tally of how many tasks
ran in all, and the check for whether a write grew a buffer. Both left
their working behind — an unwatched program was reading an atomic per
station on every teardown and comparing a growth count on every
delivery, feeding events it would never send. That is the rule this
issue is built on, broken by the code that implements it: a watched
program may be slower, an unwatched one pays nothing.

It surfaced as a build failure on somebody else's machine, because their
compiler said *variable set but not used* where the one here said
nothing at all. The warning was the smaller half of it.

`tests/131-test-a-second-opinion.sh` now compiles the engine under every
compiler on the machine, with the flag and without, warnings as errors.
**One compiler is one opinion**, and the half of a flag that is usually
not compiled is exactly where a mistake can sit unseen. It was proved by
putting the fault back and watching the second compiler find it.

### What the test holds down

Five things, and the last two are the ones worth having:

- A program writes events and a reader that is not the program reads
  them back in sequence.
- Its account and the program's own counters agree exactly — two
  independent tallies of one run, and they match.
- **A ring already being written is refused, naming the process that
  owns it.** The same process is not exempt: two maps in one process are
  two programs, and a second seizing the first's ring truncates it under
  a reader that was following it. That was found by writing the test —
  the first version exempted the same process, and the rival promptly
  wiped the ring the reader was reading.
- **A reader too slow for the ring loses events and is told how many**,
  and the arithmetic is checked rather than trusted: it lost 18,980, it
  kept 1,024, and the first one it kept is number 18,981. A count that
  did not add up would mean a reader quietly showing a shorter story
  than the one that happened.
- The trail says when the program it describes has ended, so a viewer
  can stop waiting on a stream that will never move again.

## Intended behavior

### It is a build-time choice, and the cost is zero when it is off

**A flag on the build says this program can be watched.** With it, the
engine emits an event at each thing worth seeing. Without it, the
emitting compiles out entirely — not a branch that is usually false, an
absence.

This is the shape [702](702-station-statistics.md) already
chose for timing, and the reasoning is quoted rather than re-derived:
*a measurement apparatus whose cost is unmeasured is a rumour, and one
that cannot be removed is a tax.* A watched program is allowed to be
slower than an unwatched one; an unwatched one is not allowed to pay
anything at all.

**Why a build flag rather than a run-time switch.** A run-time switch
is a branch on the delivery path, taken on every value that moves, to
answer a question whose answer never changes during a run. The engine's
standing habit is to resolve such a question while somebody can still
read an error message about it — the same reasoning that turned box
records into placement functions ([311b](311b-placement-instead-of-records.md)).

### Watching must never change what is watched

**This is the constraint the whole issue is held to**, and everything
below follows from it.

- **The program never waits for a reader.** No lock a reader can hold,
  no buffer a reader can fill, no handshake. A reader that stops
  reading is a reader that stops receiving.
- **A slow reader loses events and is told how many.** Losing them is
  correct: the alternative is a reader that can slow the program by
  being slow itself, which is the failure this rule exists to prevent.
  Being told is what keeps it honest — a view quietly missing events is
  a view somebody will trust.
- **Any number of readers, none of them known to the program.** A
  reader attaches, reads, and detaches without the program learning
  that any of it happened. Two readers do not interfere with each
  other, because neither has anything the other needs.
- **A reader cannot write.** Nothing it does can reach the program.
  This is a window, not a door; [801](../801-browser-workbench.md) is the
  door and shares nothing with this.

### Where the trail lives

**A ring in shared memory**, under the same RAM tier logs already use
— the readable one, since this is read and never executed.

Chosen over the alternatives for what it does *not* need: no socket to
listen on, no port to allocate, no protocol, no dependency, and no
second thread inside the program. A reader opens a file and maps it.

**A writer that catches up with a reader overwrites it**, which is the
only arrangement in which the program cannot be held up. The ring is
sized by the build; a reader that cannot keep up with a given size is a
reader that should ask for a bigger one, and until then it is told what
it missed.

**Multiple writers, because every worker emits.** What is written is
small and fixed-size, so a slot is claimed with one atomic step and
filled afterwards — the same shape a port's slot state already has
([210c](210c-a-state-on-every-slot.md)), for the same reason.

### What counts as an event

Small, fixed-size, and about the graph rather than about values.

| event | what it carries |
|---|---|
| a station ran | which station, how long it took |
| a value was delivered | from which station and port, to which station and port |
| a task became due | which station |
| a buffer grew | which station, which port, how deep it is now |
| the program came up | how many stations, how many workers |
| the program finished | how many tasks in total |
| a station was added, removed, or rewired | which, and into what |

**Values are not carried, and that is a decision rather than an
omission.** A value is any size at all, including sizes that would not
fit in a fixed-size slot, and copying one onto the trail is a cost paid
on the delivery path for something a watcher usually does not need — it
wants to see *that* the graph is moving and *where* it is stuck.
Somebody who wants values has [712](712-capturing-a-running-program.md),
which writes them all, at a moment nothing is running.

**The station's name is not carried either**, because a name is a
string and this is fixed-size. A reader that wants names reads them
once, from a dump the program can be asked for, and holds them against
the indices — which is the same trick a wire already is.

## Suggested implementation steps

1. The shared ring: its shape, its file, one writer and one reader in a
   test, with the reader losing events on purpose and being told.
2. The build flag, and a demonstration that an unwatched program
   contains none of the emitting — read from the binary's symbols
   rather than asserted.
3. The events, one at a time, each with the emit at the place the thing
   actually happens rather than near it.
4. A reader in C, in the test suite, that follows a running program and
   reconstructs how many times each station ran — checked against the
   counters the program itself keeps.
5. The map file's own reading of the trail: a program watched for its
   whole life should be reconstructible from the events alone, and
   comparing that reconstruction against a dump is the strongest test
   available.

## What was undecided, and is now

**The ring's path is the caller's, and the ring outlives the program.**
`cera_watch_open` takes a path the way the crash report already takes
one. A caller that passes the same path every run overwrites its own
ring and accumulates nothing; a caller that wants yesterday's last
moments picks a different name and keeps it. The engine does not choose,
because it has no way to know which of those anybody wants — and a file
in the RAM tier that the engine deleted on exit could never answer *what
happened just before it wedged*, which is most of the point.

**The ring's size is derived, and can be overridden.** Slots enough for
a few hundred events per station, rounded up to a power of two, with a
floor for tiny programs. A number in a build flag puts the decision on
somebody with no way to make it; a guess from the station count is a
guess with better manners, and a reader that cannot keep up is told
exactly how much it missed and can ask for more.

**Two programs cannot share one ring.** The header records the writing
process, and opening a ring whose recorded process is still alive is
refused, naming it. Allowing it would mean every event carrying which
program it came from, for a situation nobody wants; refusing costs one
check at startup.

**Every event is the same kind of thing, in one stream, in one order.**
A rewire and a delivery are one record shape discriminated by a field,
because the only way a reader can know a wire was cut *before* a value
tried to cross it is for both to be in the same sequence.

**The emitting is behind the build flag; the reading is not.** A watcher
is a different program from the one being watched, and it has no reason
to have been built with watching turned on. Both halves live in the
engine so that the ring's shape is defined once — two readers of one
format are two things that must agree.

## Open questions

- **How much history does a viewer hold?** The ring holds what it holds;
  what a *page* keeps is [803](803-the-viewer.md)'s question.

## Related

- [803 — The viewer](803-the-viewer.md), which reads this and does
  nothing else
- [702 — Station statistics](702-station-statistics.md), whose
  build-time switch this copies and whose counters a reader checks
  itself against
- [701 — Buffer growth reporting](701-buffer-growth-reporting.md),
  which already notices the one thing most worth watching for
- [106 — Stopping on purpose](106-stopping-on-purpose.md),
  whose report is what this replaces for the living case
- [712 — Capturing a running program](712-capturing-a-running-program.md),
  which is the same question asked of a program standing still
- [801 — The workbench in the browser](../801-browser-workbench.md), which
  is a door where this is a window, and shares nothing with it
