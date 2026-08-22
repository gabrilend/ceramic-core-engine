# 802 — A program you can watch

A program built one way runs and says nothing. Built the other way it
runs identically and **leaves a trail somebody else can read while it
happens** — every station that ran, every value that moved, every
buffer that grew — without the program waiting on anybody, noticing
anybody, or behaving differently because somebody is there.

The trail is the deliverable here. What reads it is
[803](803-the-viewer.md), and it is deliberately not this issue.

## Current behavior

**Everything the engine knows about itself is told afterwards, or on
the way out.**

- **Counters exist and are read at the end.** Every station keeps how
  many tasks it ran and how many it made due elsewhere
  ([702](completed/702-station-statistics.md)); every port keeps how
  deep it got and how often it grew
  ([701](completed/701-buffer-growth-reporting.md)). Something has to
  ask, and asking is a walk of the whole table.
- **A capture writes a report** of the same numbers when a program is
  put down ([712](completed/712-capturing-a-running-program.md)).
- **A dying program writes a diagnostic report**
  ([106](completed/106-stopping-on-purpose.md)), which is the closest
  thing to a live view and requires the program to be in trouble.
- **The observer notices piles forming** and prints a line when results
  stack up somewhere nobody is taking them from. That line goes to
  stderr, mixed in with everything else, and cannot be read by a
  program.

So the shape of what a program *did* is recoverable, and the shape of
what it is *doing* is not. Watching one means printing from inside a
box, which changes the program to look at it.

**And nothing outside the process can see in at all.** There is no
socket, no file, no shared area — the only channels out are stderr and
whatever a program's result station is wired to.

## Intended behavior

### It is a build-time choice, and the cost is zero when it is off

**A flag on the build says this program can be watched.** With it, the
engine emits an event at each thing worth seeing. Without it, the
emitting compiles out entirely — not a branch that is usually false, an
absence.

This is the shape [702](completed/702-station-statistics.md) already
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
records into placement functions ([311b](completed/311b-placement-instead-of-records.md)).

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
  This is a window, not a door; [801](801-browser-workbench.md) is the
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
([210c](completed/210c-a-state-on-every-slot.md)), for the same reason.

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
Somebody who wants values has [712](completed/712-capturing-a-running-program.md),
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

## Open questions

- **How is the ring's size chosen?** A build flag with a number is the
  obvious answer and it puts the decision on somebody who has no way to
  know what a good number is. A size derived from the station count is
  a guess with better manners. Undecided.
- **Does the trail survive the program that wrote it?** A file in the
  RAM tier outlives the process that made it, so a reader could attach
  after a program died and read its last moments — which sounds
  valuable and means the file cannot be removed on exit, and therefore
  accumulates. Undecided.
- **What does a second program watching the same map do?** Nothing
  stops two programs writing one ring if they are handed the same path.
  Refusing needs a lock; allowing needs the events to say which program
  they came from. Undecided, and worth deciding before anybody hits it
  by accident.
- **Is the event for a rewire the same kind of thing as the event for a
  delivery?** One says the graph changed shape and the other says
  something moved through it. A reader has to handle both, and a single
  stream in one order is the only way it can know which happened first.

## Related

- [803 — The viewer](803-the-viewer.md), which reads this and does
  nothing else
- [702 — Station statistics](completed/702-station-statistics.md), whose
  build-time switch this copies and whose counters a reader checks
  itself against
- [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md),
  which already notices the one thing most worth watching for
- [106 — Stopping on purpose](completed/106-stopping-on-purpose.md),
  whose report is what this replaces for the living case
- [712 — Capturing a running program](completed/712-capturing-a-running-program.md),
  which is the same question asked of a program standing still
- [801 — The workbench in the browser](801-browser-workbench.md), which
  is a door where this is a window, and shares nothing with it
