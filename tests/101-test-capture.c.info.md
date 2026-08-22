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

## What it does not cover

Draining a running pool before the capture, and a program that grew
boxes while it ran. Both are steps of their own on
[712](../issues/712-capturing-a-running-program.md).
