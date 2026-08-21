# 211 — Growing the station table

## Current behavior

**Done.** The table is shelves, it grows one station at a time, and a
removed station's place is reused before it grows.

A shelf is one allocation holding sixty-four station records, and the
table is a short array of pointers to shelves. Station *n* sits at
position *n* within shelf *n* divided by the shelf size — one shift and
one mask. Growing means allocating one more shelf and writing its
pointer, so **nothing already placed is ever copied**, mutex included,
and guarantee S1 is kept rather than argued with.

**The measurement step 1 asked for.** Indexing costs one shift, one
mask and one dereference where a flat array had one add, and that lands
on the delivery path. Measured on the fan-in apparatus across several
runs, it sits **inside the run-to-run noise** — the 200-byte figure
moves between roughly 870 and 1410 nanoseconds per delivery from run to
run, which is a wider spread than the change could account for. A
visible cost here would have been a finding; its absence is the other
kind of finding and is worth writing down as one.

**The count is atomic, only grows, and is published last.** A thread
reading a stale, smaller count does not see the newest station, which
is harmless: a station nothing is wired to cannot be reached by
delivery, and the wire that will reach it is drawn after the station
exists.

**Adding is exclusive under the rewiring lock**, and the hand-raising
ring the note asked for is not built. That is the answered question
below made real: adding a shelf is one allocation and one pointer
write, so there is no long stretch for anybody to raise a hand during.
The pattern is kept in `strategems/raise-your-hand.md` with the lesson
that displaced it — before building machinery to let people contribute
during a long exclusive operation, ask whether the operation can stop
being long.

**Reading a map file is now the same act as growing a program.** The
loader starts from an empty table and adds one station per line,
instead of counting lines and allocating once. That is the step that
proves the mechanism, because every existing test loads a program.

**One thing the plan did not distinguish**, found in building it: a
place nobody has filled and a place somebody emptied look identical,
because both are a record with no shim. `map_add_station` hands back
the first unfilled place, so asking twice without filling in between
gives the same answer twice — which is correct for what it means
("somewhere to put one thing") and wrong for what map_create wanted
("N places reserved"). Creation reserves directly; only growth reuses.

**Proven**, in `tests/078-test-growth.c`:

- a table grown across four shelves, with the addresses of stations
  placed beforehand checked afterwards — **not one of them moved**,
  which is the entire reason for the shape
- a station added to a running program, wired in afterwards, receiving
  values, with the station that was always there losing none of its
  hundred
- eight threads asking for places at once, and two hundred places
  asked for and filled with none ever handed twice

Under a leak checker: 1,544 allocations and 1,544 frees, no leaks and
no errors.

## Intended behavior

**Stations are added one at a time, and adding one never moves a
station that already exists.**

**And a removed station's place is reused before the table grows.**
[216](216-removing-a-station.md) makes removal possible by removing the
wires to a station before the station, so a freed position holds
nothing stale and the next station placed can simply take it. That
makes this issue the *growing* half of a table that also shrinks:
placement asks for a free position first and adds a shelf only when
there is none. A program that adds and removes stations forever
therefore reaches a steady size rather than climbing.

### The table stops being one array

A flat array grows by reallocation, and reallocation is what moves the
mutexes. A table built out of **shelves** does not.

A shelf is one allocation holding a fixed number of station records,
and the table becomes a short array of pointers to shelves. Station
number *n* lives at position *n* within shelf *n divided by the shelf
size* — one shift and one mask when the shelf size is a power of two.
Growing means allocating one more shelf and writing its pointer into
the array. Every station already placed stays exactly where it was,
mutex included, because nothing that holds a station is ever copied.
The invariant in the header is kept rather than argued with.

*(The word is deliberate. "Run" would have been the ordinary term for a
fixed span of records, and this project already spends that word four
ways — a station runs, a station's completed-task count is its runs, a
program is running, and rewiring happens at runtime. A shelf is a place
things sit, which is what this is.)*

Indexing costs one shift, one mask, and one extra dereference instead
of one add, which lands on the delivery path and is therefore worth
measuring rather than assuming free. The only thing ever copied is the
short array of shelf pointers, which holds addresses rather than
mutexes — the same kind of copy the pool's ring already does safely.

**The shelf size is a fixed constant, chosen once and named.** It does
not have to be guessed well, for the same reason the ring capacity does
not: getting it wrong is cheap in both directions. Too small and the
pointer array grows a little more often, and that array holds pointers,
so growing it is safe and fast. Too large and the last shelf holds some
records nobody uses — a few kilobytes at worst. Nothing is copied
either way and no station ever moves either way. Sixty-four is a
reasonable place to start, and it lives beside the ring capacity as one
more number that can be changed in one place.

The alternative is to lift the mutex out of the station and hold it
elsewhere, so the record becomes movable. That trades a shift-and-mask
on the delivery path for a pointer chase on the delivery path, and it
breaks the sentence in the header rather than keeping it. It is written
down here so the choice reads as a choice.

**The count is the thing readers race on, and it only ever grows.** A
thread reading a stale, smaller count does not see the newest station,
and that is harmless: a station nothing is wired to yet cannot be
reached by delivery, and the wire that will reach it is added after the
station exists. The ordering to enforce is exactly one — the station is
completely built before the count that reveals it is published.

### Waiting your turn without stopping the line

The second half of this is a pattern rather than a feature, and it
applies to every growable list in the engine.

Growing by holding a lock for the whole operation means everyone who
arrives during it waits for all of it. Instead: the thread that finds
no room becomes the builder and publishes that a new container is under
construction. A thread arriving while that happens does not block until
the end. It joins a short ring of waiting contributors — raises its
hand — and when the builder reaches a convenient point it lets that
contributor add its own entry and update the running counts, then
carries on. The builder finishes when the ring of raised hands is
empty.

The image is the specification: you show up, you raise your hand, you
say *excuse me, can you do this thing for me?*, they say *sure, put it
over there*, they update their count, and they trust you to do it right
rather than checking your work.

Two things make this affordable rather than merely clever. The
machinery already exists — a ring of waiting parties is the pool's own
queue with a different payload. And adding a shelf is short, so the
builder is never holding anyone up for long.

**Where else this belongs.** The pool's task ring is the other
candidate, and the one where the win is largest, because pushes are
constant and the ring doubles under load exactly when pushes are most
frequent. The pattern generalises to any list that grows: find no room,
become the builder or join the queue of hands, add your part, carry on.

**The wider principle, worth stating because it shapes the rest.** Try
never to block anybody. If you are blocked, wait your turn in line
rather than spinning on the thing you wanted. Prefer taking work that
is not blocked. Very few tasks should discover mid-flight that they are
blocked, and when they do it should be for a reason like this one —
memory ran out at a moment nobody could have predicted.

### What adding a station has to check

Every rule the loader applies to a station applies here, because a path
that skips validation is a way to reach a state the loader would have
refused — and under one surface it is not merely *like* the loader's
path, it *is* the loader's path. The box name must resolve in the
registry. The kind must be one the engine knows, and a comparator's
return type must have a compare function. Input count, element sizes,
and type names come from the registry rather than from the caller, so a
caller cannot describe a station the generator never saw.

**A station is created with its name**, the same as its box and its
kind. Names already live on the program for the life of the run —
[703](703-map-dump.md) forced that when it needed to write a
program back out as text, having found that the design had proudly
discarded them at load. So there is nothing new to keep; a station
added at runtime supplies a name the way it supplies everything else,
and refusing a duplicate is one lookup in a table that already exists.

**An unwired station is ordinary.** A station whose buffered inputs no
arrow feeds simply never becomes ready, which costs nothing at runtime
— the readiness check only runs when a delivery arrives, and no
delivery ever arrives. Somebody assembling a program may deliberately
place boxes before wiring them, and under one construction surface that
is a normal sequence rather than a half-finished load. The loud warning
this used to produce moves into the whole-program pass in
[212](212-one-way-to-build-a-program.md), where a caller asks for it
when a caller wants it.

Refusal follows the decision 704 already made and reasoned: a running
engine returns a failure and names the reason rather than dying,
because a loader that dies serves its author while a plant that dies
for one bad control instruction takes the plant down.

**Removing a station is not in scope**, for the same reason 704 kept
adding one out of its scope: indices are positions, and reclaiming one
means either leaving a hole that every walk must learn to skip, or
renumbering, which invalidates every wire at once. Growth only. If
removal is ever built it needs its own issue and probably a different
representation of an index.

## Suggested implementation steps

1. Change the table's representation to shelves with no growth yet —
   same capacity, same behaviour, one shift-and-mask where there was an
   add. Measure the delivery path before and after. A visible cost here
   is a finding worth having before anything is built on it.
2. Publish the count as an atomic that only increases, and establish
   the single ordering rule: build the station fully, then publish.
3. Add a shelf when the current ones are full, under the rewiring lock
   that already exists, with no hand-raising yet — a plain exclusive
   grow, correct and dull.
4. Validate a new station against the registry rules, refusing in the
   manner 704 established.
5. Let the loader start from an empty table and add stations one at a
   time instead of counting lines and allocating once. This is the step
   that proves the mechanism, because every existing test loads a
   program.
6. A test that a station added mid-run receives values down a wire
   drawn to it afterwards, and that everything already running is
   undisturbed across the growth.
7. A test that several threads adding stations at once all succeed and
   the table ends with all of them, no two sharing an index.
8. Only then, the hand-raising ring, as its own change with its own
   measurement: contention and time-to-add under many simultaneous
   growers, against the exclusive version from step 3.
9. Apply the same pattern to the pool's task ring, which is where it
   pays most.
10. Confirm a grown program dumps to a file that loads to the same
    shape — the cheapest proof that growth produced a real program
    rather than an almost-real one.

## Open questions

**Answered:**

- *Does the hand-raising ring have a bound?* Moot here — the ring is
  not being built. Growing by adding a shelf makes the exclusive
  operation one pointer write, so there is no long stretch for anyone
  to raise a hand during. The pattern is kept in
  `strategems/raise-your-hand.md` along with the lesson that displaced
  it: before building machinery to let people contribute during a long
  exclusive operation, ask whether the operation can stop being long.
- *Should out-of-memory read differently from a misspelled box name?*
  Yes, and the distinction has to be visible to code rather than only
  to a reader: a misspelled name is something a caller can fix and
  retry, and out-of-memory is not, so anything that retries on failure
  would otherwise spin forever on the second kind.

## The note that started this

Kept verbatim, because the reasoning in it is the specification:

> I noticed this in 704:
>
> Adding a station is a different problem and is not in scope here. The
> station table is allocated once at load; growing it means
> reallocating the array, and while every wire holds an index rather
> than a pointer — so nothing dangles — every thread reading the table
> needs to see the new base. That is a real design question and
> deserves its own issue rather than being smuggled in alongside
> rewiring.
>
> here's the solution we're going to use:
>
> if the station table needs to be re-allocated, like for example if
> one of it's members needs to resize itself and there's no room at the
> end or in the middle (checked in that order) then we do a double
> buffer system structured like this:
>
> first, we create space - larger than the previous version. Then, we
> start copying everything over - this is just for stations, and
> they're mostly static, so it's okay if it's a bit slow. There is only
> one worker thread hanging on this so might as well take our time.
> Resizing it is rare, so once it's fully copied we can go through each
> of the stations that already exist and A. atomically ensure that each
> of their values is correct with the 2nd edition, and B. move the
> pointer in the box station list to point to the correct location. We
> do this each time, without removing an entry in the box station list
> unless it is guaranteed that no other boxes wire into it or draw from
> it gatherer style.
>
> actually, we should just create a new spot in memory and make a
> thread pool task to copy them all over. If there's already a thread
> pool task being constructed when another task needs to allocate,
> which we check with a little flag at the top of the box station list,
> or rather, if we need to allocate and there's no room, not in
> general. Then we find the under-construction buffer/page/list, and we
> first set a flag that is like a mutex but is just a ring buffer of
> those who want to add a box into the new container while it's under
> construction. This is absolutely doable with the machinery we already
> have, just gotta use it as a template going forward. Anyway first it
> adds itself to that flag, then it waits until the main constructor
> thread (or whoever is currently working) finishes adding the part
> that it's currently working on. Then, it spins while the guest adds
> their part and increments the relevant counters. Then, the main
> thread continues once the ring buffer is empty. The idea is, if you
> have to interrupt someone, then you first show up, raise your hand,
> say "excuse me, can you do this thing for me?" then they say "sure,
> put it over there." and updates their running count. Then you do, and
> they either wait until your done, or just move on without validating
> that what you're doing is fine. Because they trust you.
>
> The same pattern should be used for all the growable lists. The task
> list I think could benefit from this design pattern. Essentially, try
> not to block anyone. If you are blocked, then wait your turn in line.
> Also, try and only take tasks that are not blocked. Very few tasks
> should be discovered as blocked partway through, and typically only
> for memory issues like this where there's not enough space to do
> something and you only just realized.

**One departure from the note, and why.** The note reaches for a double
buffer: allocate larger, copy everything across, then repoint. That is
the right instinct for a list of pointers and it is what the pool's
ring already does. It does not survive contact with the station table
as it stands, because a station record carries its own mutex and a
mutex cannot be relocated while anyone might be holding or waiting on
it — the copy would produce a table full of locks that no unlock can
reach. The note's own phrasing anticipates the escape: it speaks of
*moving the pointer in the box station list to point to the correct
location*, which is a list of pointers to stations rather than a list
of stations. Taking that literally is the fix. Shelves are the same
idea with the pointer array kept short, and they mean nothing holding a
station is copied at all.

The hand-raising ring is adopted exactly as written. It is the more
interesting half of the note and it is independent of how the table is
laid out.

## Related

- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which makes this the mechanism underneath every program's first
  moment rather than a capability a running one gains
- [704 — Rewiring while it runs](704-runtime-rewiring.md),
  which named this problem and deliberately left it
- [201 — The station table](201-station-table.md), which
  fixed the record's size and wrote down the invariant this must keep
- [101 — The task queue ring](101-task-queue-ring.md), the
  other growable list, and the one the hand-raising pattern helps most
- [703 — The map dump](703-map-dump.md), the cheapest proof
  that a grown program is a real program
- [604 — Load-time validation](604-load-time-validation.md),
  the rules an addition has to satisfy
- [058 — Guarantees](../../docs/058-guarantees.md), which currently says
  the station count is fixed for the life of a run
