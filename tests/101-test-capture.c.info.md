# 101-test-capture.c — a running program put down and picked up

Proves that a dump is becoming an **image** and not only a schematic.
A schematic says what a program is shaped like; an image says what it
currently holds, so it can be revived rather than merely rebuilt.

## The shape of each scene

Every scene fills a station's buffer with work that **cannot run** —
because a second port of the same station is still empty — and then
writes the program down. That constraint is not incidental: a station
runs the instant every port holds a value, so anything that *could*
run already has. What waits in a buffer is precisely what is waiting
for something else, and that is the state worth capturing.

Each program carries a `keep` station marked as the entrance, feeding
nothing. It is there only so the program has a declared entrance and
is therefore allowed to sit waiting rather than being refused as a
program that can never start.

## What each scene proves

**work_in_flight_survives** — three values queue up, the program is
written down, and a fresh program reads it back with the same three
values waiting and nothing consumed on the way. Written down again, the
text is identical, which extends the round trip the dump has always
promised to cover what a program holds and not only what it is.

**the_revived_program_finishes_the_work** — the missing port is
finished on the revived program, and all three pieces of work in flight
run and produce the answers the first program was waiting to give. A
capture nobody can go on from is not worth having.

**a_struct_queue_survives** — two struct values, each with commas
inside it, in a comma-separated list. This is the case that decides
whether the format can be read at all: only reading one value at a time
can tell an outer comma from an inner one.

## And three scenes about a question asked once too few times

Proving the revival surfaced a gap in the engine, so the scenes that
pin it live here beside the work that found it. A station is ready
whenever every port holds a value, and asking once is enough only while
values arrive one at a time. Three doors let a port fill **all at
once**, and each is a different call path into the same question:

**a_deep_buffer_drains_when_the_constant_arrives** — thirty values
stacked on one port, a constant bound to the other, thirty runs. This
is the scene that fails loudly if the drain is ever removed.

**writing_a_constant_drains_what_was_waiting** — the constant already
exists and is changed while the program runs. The write and the first
check happen inside one lock hold on purpose, so there is no gap
between the value changing and the question being asked; what this pins
is that continuing from there strands nothing and starts nothing twice.

**a_station_of_only_constants_runs_once_per_change** — the exception,
and the reason the drain declines to touch such a station rather than
relying on a count: a constant is never consumed, so a station made
only of them is ready forever and a drain would never finish. Once when
it becomes complete, once more per change.

## Two scenes about what counts as a change

**writing_the_same_value_still_counts** — a constant written its own
value over again runs the station every time. A write is a statement,
not a report of a difference: whether the bytes match is a fact about
the previous value, which the caller said nothing about.

What this catches is the comparison placed *before* the readiness
check, which is where somebody would really write it. A comparison
inside the copy itself does not fail this scene, because the check runs
either way — worth knowing, because it says the thing being protected
is the asking and not the copying.

**a_write_drains_whatever_is_waiting** — and the rule is not relaxed
for constants. Ten values stacked on one buffer, a second buffer empty,
a constant written: nothing runs, because a port is empty and that is
the whole of the rule. Four values on the second buffer make four
pairs, and six stay waiting.

## And the one memory a station keeps

**an_iterator_remembers_where_it_was** — an iterator takes its exits in
turn, and which one is next is the only thing about a station that is
neither its shape nor a value sitting on a port. Two values go through
a three-exit iterator, the program is written down, and the revived one
is pointing at the third exit.

A capture that reset it would produce a file of exactly the right shape
whose next value went somewhere it was never going — the worst way for
a written-down program to be wrong, because nothing about the file
looks incorrect.

## And three about putting the program down

**draining_produces_a_complete_capture** — the polite door. Shut the
entrance, let everything in flight finish, wait for the workers to go
home, write. What comes out says nothing about being incomplete,
because by construction nothing was running, and it reads back through
the ordinary door.

**an_incomplete_capture_says_so_and_is_refused** — the other door,
which matters more: a program that cannot drain is exactly when a
capture is worth most. A worker goes into the box that never returns,
in a forked child because it stays there for the life of the process,
and the artifact is written anyway with a header naming the station
whose work was lost.

The child waits for the worker to actually be inside the wedge by
asking the pool what it is doing, rather than guessing. Nothing here
invents a clock.

**reviving_a_lossy_capture_is_refused** — reading such an artifact the
ordinary way is fatal, proven in a child; salvaging the same file
through the door with a different name works. The file is written by
hand rather than captured, so what is under test is the reading and not
the writing.

## And one about a program that grew

**a_grown_program_captures_whole** — a box arrives after the program
started, and the program is captured into a directory. A description
alone would not do: it would be a perfectly good file naming a function
that exists nowhere on the machine reading it, because the box came as
text after the build.

So the directory holds the description and every source the program is
made of — the late arrival and the ones the build compiled in, since a
capture that stands alone cannot assume which half somebody already
has. The scene finds the late box's file by asking the running program
what it is filed under, rather than by knowing where the compiler
happened to put it.

The same scene checks the report written beside the description: that
it names and counts the box which arrived while the program ran, and
says what each station did by the name its author gave it. Nothing in
the report is measured for it — every number was already being kept.
