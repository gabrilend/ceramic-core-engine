# 058 — Guarantees

Things that are always true about a running map, collected from the
documents, the source comments, and the tests that hold them up.

A guarantee is worth more than a measurement. A measurement tells you
what happened once on one machine; a guarantee tells you what you may
build on without measuring anything. This page exists so that the list
is a list — visible, countable, and arguable — rather than a dozen
sentences scattered across nine documents where nobody can see them at
the same time.

**How each one is held** is the column that matters most, because it
says how much the guarantee is worth:

| held by | meaning |
|---|---|
| **structure** | it cannot be otherwise without changing the shape of the data. The strongest kind. |
| **build** | the generator or the C compiler refuses the alternative. |
| **load** | validation rejects a map that would break it, before anything runs. |
| **runtime** | checked at the moment of the action, and the program stops if it fails. |
| **discipline** | nothing enforces it. A box author can break it and find out later. The weakest kind, and the ones worth staring at. |

---

## The bargain

One trade, made once, in one direction — and nearly every entry in the
*not guaranteed* section at the bottom is the price of this single
purchase. It is stated first because a list of guarantees read without
it looks like a set of unrelated concessions rather than one decision.

| # | always true | what it costs | held by |
|---|---|---|---|
| U1 | No worker sits idle while a task is ready to run. | Order, timing, pairing, batches, rounds — everything in the closing section. | discipline (via B4) |

**What is bought** is the processor. Every core is kept doing work for
as long as work exists, as completely as the design can manage. This is
the thing the engine is for, and it is the reason to accept any of the
rest.

**What is sold** is every promise that would require a worker to wait.
Ordering values would mean a slow write holding finished ones behind
it. Pairing across ports would mean a ready value waiting for its
partner. A batch or a round would mean the cores that finished early
standing still until the cores that finished late caught up. Each is a
worker not running one of the ten things that are ready, which is the
one cost this design will not pay.

**Which is why values must stand on their own.** A value passing
through this engine has to be usable by anyone, interchangeably with
any other value of its type at the same port. Nothing is anybody's in
particular. That is what lets whichever worker is free take whatever is
ready, and it is the same sentence as V1 read from the scheduler's side
rather than the data's.

**How it is held, honestly.** By structure everywhere except the one
place: a box that blocks (B4) parks a worker on something the engine
cannot see, and the guarantee is withdrawn for as long as that box
runs, silently. This is the strongest reason B4 deserves a linter.

## Values

| # | always true | what it costs | held by |
|---|---|---|---|
| V1 | A value on a wire is atomic and independent. It carries no relationship to any other value, and none to whatever run produced it. | Anything that belongs together must **be** one value — a struct on one wire — rather than two values that happen to arrive near each other. | structure |
| V2 | A station pairs whatever each of its ports happens to hand over. | There is no notion of a round, a batch, or a matching set, and nothing could implement one without a tag riding on every value. This used to say *the head of each port*, which stopped meaning anything when a head index was replaced by a search from a hint (issue 210d) &mdash; a port hands over the first ready slot the search finds, not the oldest. The guarantee is unchanged and was never about which value; it is about there being no pairing rule at all. | structure |

These are the same fact stated twice, and the place it becomes visible
is a graph that splits and rejoins — one station fanning to two paths
that later meet at a third. Send two values through. The tasks on the
two paths run on different threads at different speeds and can finish
out of order, so the rejoining station may pair the first value's result
from one path with the second value's result from the other.

That is not a defect to be mitigated. It is what independence means. A
station that fans to two paths has produced two unrelated values, and
the station where they meet takes the oldest available of each, which is
the only thing it could do without being told about a relationship that
was never expressed. **If two things were meant to stay together, they
were meant to be one value.**

The consequence for a map author is a design rule rather than a hazard:
correlation is something you build, by putting the related parts in a
struct and sending it down one wire. The alternative — tagging every
value and matching tags at every port — is a different engine, and
knowing it exists is mostly useful so that nobody reinvents it badly.

## Boxes

| # | always true | what it costs | held by |
|---|---|---|---|
| B1 | A box cannot remember anything between calls. | State has to live on the wires: to count, you route a box's output back into its own input. | **discipline** |
| B2 | A box receives private copies of its arguments, never shared pointers. | Every value is copied at least once per invocation. | structure |
| B3 | Two invocations of the same station may run simultaneously on different threads. | Follows from B1 — anything a box stored would be shared between them. | structure |
| B4 | A box never blocks. | Nothing may wait on a value, a lock, or a device. A worker that cannot progress is a worker not running the ten things that are ready. | **discipline** |
| B5 | A box that returns void is a sink; the engine needs no support for it. | A sink's output is simply not delivered anywhere. | structure |

B1 and B4 are the two load-bearing guarantees held by nothing but
discipline, and almost everything else rests on them. B1 is what makes
B3 safe, and B3 is what makes the whole engine parallel. B4 is what
makes deadlock impossible (see P6). A single box with a `static` counter
or a blocking socket read quietly withdraws both, and no part of the
system will say so.

---

## Stations and wiring

| # | always true | what it costs | held by |
|---|---|---|---|
| S1 | A station never moves once placed. | The table is **shelves**: a short array of pointers to fixed runs of records. Growing means allocating one more shelf and writing its pointer, so nothing already placed is ever copied; only the array of addresses is, which is the same kind of copy the pool's ring already does. Removing one (issue 216) empties its record in place and never shifts anything, so every index keeps meaning what it meant. It has to be kept: a station holds its own mutex, and a mutex is identified by where it lives, so moving one would leave every thread parked on it waiting at an address nobody unlocks. The price is one shift, one mask and one dereference where a flat array had one add, measured on the delivery path and found to sit inside the run-to-run noise. Issue 211. | structure |
| S2 | A wire never names a station that is not there. | A wire is an index into the map's station array, never a pointer. Stations were once never removed at all, which held this trivially; issue 216 allows removal and holds it properly instead &mdash; **removing a station is what removes the wires to it**, walking every station's output ports first, so no stale wire can survive to be followed. That walk is the price, paid during a rare operation rather than on every delivery, which is what a version tag on every wire would have cost. A removed place does not come free until the sweep clears its record, because a task is built from a station's fields *after* the readiness check released the mutex. | structure |
| S3 | Growing an input buffer invalidates nothing. | **Nothing is reallocated at all**: growth adds a page of slots to a list, so no slot that already exists ever moves (issue 210e). That stronger statement had to be reached rather than merely preferred &mdash; a worker copying a value out of a slot it has claimed holds no lock, because a claimed slot belongs to it alone, and relocating that slot underneath it is the one thing ownership does not protect against. The weaker version, that only the storage a port points at is reallocated, was true and sufficient while the station's mutex covered every copy. | structure |
| S4 | An input buffer is never full when a value arrives. | It grows before a writer can fail to find an empty slot. Issue 210c replaced the head-and-tail indices with a per-slot state, so "full" stopped being a computed condition and became a question asked directly &mdash; nothing answers a search for an empty slot &mdash; and issue 210e makes the growth an appended page rather than a doubling copy, because a claimer copying bytes outside the lock would not survive having its slot relocated. Memory absorbs any imbalance between input sides. | runtime |
| S5 | A station's **slot states** are read and written only under that station's mutex; its value **bytes** are protected by ownership instead. | One lock per station, covering every port's states together, so a claim holds exactly one lock and no lock ordering exists to get wrong. A slot in *reserved* or *claimed* belongs to exactly one worker, so the copies into and out of it need no exclusion and happen outside the lock — which is what keeps the expensive half of a delivery off the serialized path. A delivering writer takes no lock at all, because writing touches one slot and one slot is already atomic. Issue 210d. | structure |
| S8 | A delivery walk takes no lock and copies nothing. | A port's destinations are one immutable array behind a pointer; a rewire builds a whole new one and swaps the pointer in a single atomic write, so a walker reads the pointer once and walks something nobody will ever modify. What it replaced was a linked list a rewire could free under a walker's feet, which is why the walk used to copy every pair onto its own stack under the station's mutex &mdash; a lock acquisition and a copy proportional to fan-out on every value the engine moved. Issue 214. | structure |
| S9 | Nothing a rewire replaces is freed while a worker might be inside it. | Each worker keeps an epoch on its own cache line, bumped at the start and end of a whole task, so it is **odd while inside one and even while not**. A replaced set is filed with a snapshot of every epoch; a later sweep frees it once every worker is either even or has moved on. Nothing waits and nothing spins &mdash; an idle worker is asleep and therefore even, and passes without moving, which is what would otherwise deadlock a sweep against a quiet pool. The counter spans the whole task rather than the walk, so the same mechanism answers the same question about a box's compiled code (issue 310) and a removed station (issue 216). Issue 214. | runtime |
| S6 | The station kind is consulted at exactly one moment — choosing an exit on the way out — and nowhere else. | A new kind is a row in one table, never a branch in several functions. | structure |
| S7 | A program is reached only through its input and output stations. | Everything else about a program &mdash; a station index, a pointer into its buffers, anything a caller held on to &mdash; is **undefined** to reach across a program boundary, and reaching one across a removal is undefined behaviour rather than a defended case. This is what makes issue 216's wire walk sufficient: a wire lives on an output port and can be found, and a caller outside the map cannot, because the input station deliberately does not remember who called (issue 213). **It is now enforced rather than merely undefined**, which moved this from a discipline to a structure: delivering from outside refuses any station a program did not declare as its entrance, and that refusal is the whole of what turns internals that happen to be reachable into a surface (issues 209, 213). The one rule the outward door adds is that results are **held** when nobody is wired to them, rather than discarded &mdash; discarding is right for every other port and wrong for the one case where the values are the point of the program. What is still undefined is a caller that held on to something and reaches past the doors anyway; the engine no longer offers it a way to. | structure |

---

## Tasks

| # | always true | what it costs | held by |
|---|---|---|---|
| T1 | A task is self-contained once built. Nothing another thread can change affects it. | Every value in it is a copy, taken before it existed. This was nearly given up to buy fresher pulled values; removing the pull path keeps it outright. | structure |
| T2 | A task's run time is its box's run time. Nothing hidden rides along. | Same as T1 — nothing runs inside a task but the box it names, so per-box timings measure what their name says and throughput is runs times per-box cost. | structure |
| T3 | One invocation's inputs are claimed atomically with respect to each other — **all of them**, of every kind, under a single hold of the station's mutex. | No other thread can interleave between them, so an input set is always mutually consistent. Statics used to be excluded, because their values lived in a table with a lock of its own that could not be nested inside a station's, so each was read at its own later moment; issue 401 moved the value onto the port and the exclusion went with the second lock. **What happens under the hold is the claiming, not the copying** &mdash; a ring port's slot is moved to *claimed* there and its bytes are copied afterwards, outside the lock, because a claimed slot belongs to one worker and nothing else may touch it. The set is therefore exactly the one that existed at the instant of the hold, which is what this promises. A static is the exception in the other direction: it is peeked rather than taken, so nothing owns it, and its copy stays inside the hold where the only protection it has lives. | structure |
| T4 | One allocation per task, sized exactly for its box; one free. | The size comes from the **station**, which was told it when its box was placed &mdash; every number in that placement a `sizeof` the compiler folded into an immediate (issue 311b). It used to come from a stored record read at build time; the record was read exactly once, at placement, so generated code could just as well do the writing. Either way it is known before the task exists. | build |
| T5 | A task comes into existence by exactly one path. | Everything that starts a station — a delivery arriving, a static being written, the statics bound while a program is built — reaches the same readiness check and the same task build. One door, not several. | structure |
| T6 | A ring value claimed by one task cannot be claimed by another. | The claim runs under the station's mutex, and a slot moves out of *ready* there. After issue 210c a slot carries its own state rather than being implied by a read index, so "claimed" is a fact written on the slot rather than a position somebody computed &mdash; which is what lets the claimer walk away with it and copy outside the lock. | structure |
| T7 | An iterator's exit is decided under the mutex and rides inside the task. | Two tasks assembled a moment apart carry different exits no matter which finishes first. | structure |

---

## The pool

| # | always true | what it costs | held by |
|---|---|---|---|
| P1 | Enqueuing never blocks on user code. | A full ring doubles — a bounded memory copy — rather than making the pusher run a task. | structure |
| P2 | The ring's storage can move with nothing dangling. | Nothing holds a pointer *into* the ring; the ring holds pointers *out* to tasks, and a worker carries its pointer away. | structure |
| P3 | First in, first out. A waiting task cannot be starved by newer arrivals. | No prioritization is possible. | structure |
| P4 | Every task is run exactly once, handed to the finish hook, and freed — three unconditional steps. | A task cannot have phases. A two-trip task must be two tasks. The pull path spent a long time looking for a way around this and never needed one: the pool has not changed for any of it. | structure |
| P5 | When the pool declares completion, no task remains and nobody could enqueue one. | The last worker to fall asleep re-scans before anyone sleeps. Without that re-scan the program can exit silently having not done part of its work. | runtime |
| P6 | Deadlock is impossible. | Bought entirely by B4. Nothing waits, so a worker is always running, delivering, or asleep with nothing to do — and the last is completion, not a stall. | discipline (via B4) |
| P7 | The pool knows nothing about boxes, stations, or maps. | It reads one field of a task. Nothing about a map can influence scheduling. | structure |

---

## Statics

**Four guarantees left this section rather than being weakened.** The
gather graph being acyclic, gather chain depth being a static property,
a gatherer's port always holding a value, and a gatherable station
having no ring-buffer ports were all about a pull path that no longer
exists — nothing is pulled, so none of them can be stated.
[056](implementation-notes/056-no-pull-path.md) records what they were
for and what removing them cost.

| # | always true | what it costs | held by |
|---|---|---|---|
| G5 | A static is always present and never consumed. | It cannot participate in readiness, so a station with only statics is always ready — which is what makes writing one enough to run it. | structure |
| G8 | Writing a static runs the ordinary readiness check on its station. | Nothing extra is needed to make recalculation propagate, and nothing can be made to run that could not run anyway: an empty ring port still answers no, because the engine will not invent a value for it. | structure |
| G9 | Every box runs on a worker that picked up a task for it. | There is no longer any path that executes a box anywhere else. This was the one exception in the engine, and it is gone. | structure |
| G6 | A statics write never tears. | One mutex, one copy — a struct half-overwritten while a claim reads it would yield fields from two different worlds. The mutex is now the station's own, which the claim already takes, rather than a second one belonging to a table. | runtime |
| G10 | A static value belongs to exactly one port. | Two ports written from one entry in a file are independent from the moment they are written; sharing is drawn as a wire instead, which costs a station and gains something visible in the picture. | structure |
| G11 | ~~A box cannot write a static.~~ **Retired.** A map wrapped in its input and output stations *is* a box, so a box reaching a map is a box reaching a box, and the prohibition was protecting a distinction that no longer exists. | What it originally bought was G12, and G12 survives without it: a map handle travels as an ordinary value on an ordinary wire, arriving through the input station like anything else from outside, so no process-wide pointer returns. A box that builds stations therefore has a side effect far larger than any other box, and that is now deliberate rather than forbidden &mdash; it is what lets a program restructure itself while it runs. | structure |
| G12 | A process may run any number of maps at once, and they cannot see each other. | Nothing in the engine is process-wide. Held by a test that runs two and writes into one. This is the guarantee G11 was retired in favour of keeping directly: a box that reaches a map does so through a handle it was *given* as a value, never through a pointer the process holds, so several maps stay invisible to each other even though boxes can now build them. | structure |
| G7 | Push cycles are legal. | They must be: since a box cannot remember (B1), a loop through a ring buffer is the only way to carry state. A blanket cycle check would forbid the engine's sole mechanism for state. | structure |

---

## The build path

| # | always true | what it costs | held by |
|---|---|---|---|
| C1 | Every size and offset is computed by the C compiler, never by the generator. | They are emitted as `sizeof` and `offsetof` expressions. The generator cannot guess wrong because it does not guess. **Where they live has changed and the rule has not**: they were rows in a table read at placement, and they are now expressions inside a generated placement function, folded into immediates and stored nowhere at all (issue 311b). | build |
| C2 | A box that exists is a box the generator saw. | Box sources are discovered, never listed, so a box cannot exist that the build silently ignores. | build |
| C3 | You can never compile against a stale registry. | A failing generator writes nothing into place. | build |
| C4 | A map that loads is a map that can run. | Validation happens at load, so a mistyped wire is a startup error rather than a surprise at the first delivery. | load |
| C5 | The dump round-trips: dumping a dump yields the dump. | The live map can always be written back out as a map file that reads back in. Held by a test. | runtime |

---

## Errors

| # | always true | what it costs | held by |
|---|---|---|---|
| E1 | There are no fallbacks. Every error stops the program and names what failed and where. | The engine owns its process. This is what makes it hostile to embed in someone else's — see [057](implementation-notes/057-packaging.md). | discipline |
| E2 | A wrong answer never keeps flowing. | Follows from E1. | discipline |

---

## Not guaranteed — and worth knowing

These are the places where somebody could reasonably expect a guarantee
and not get one. Naming them is the same work as naming the guarantees.

**A wire is checked by width, not by shape, and never by name (issue
309).** Two types of the same size connect regardless of what is
inside them: a struct of four integers wires into a struct of two
integers and a double, and delivery copies the bytes exactly as asked.

That is a deliberate trade with a real gain on one side. Under name
comparison, two structs with **identical** layouts and different names
could not be connected at all — an author's only options were to
rename one or to write a box that took one and returned the other and
did nothing. Under width comparison they connect, which is the
capability this bought, and so do types that merely happen to be the
same size, which is what it cost.

**The hole widens rather than closing** as boxes start arriving
compiled at runtime, since two separately written sources are likelier
to disagree about a struct than one build is with itself. It is the
same failure either way and it was already on the books. Shape
comparison — matching kind, size and offset field by field — would
close it honestly, is fully designed in issue 309, and the field
tables it needs have been emitted since phase 3 and consulted by
nothing. It was not taken because comparing one integer against
another is what a wire check should cost.

**Where it bites hardest is a value that is a handle.** A wrong wire
between two data types produces a wrong number, which is visible and
local. A wrong wire into a port expecting a map handle or a function
pointer produces a call through whatever those bytes were. Every
pointer is the same width as every other pointer, and as a `double`,
and as a file offset.
and not get one. Naming them is the same work as naming the guarantees.

**One process-wide variable is left, and it is the last load's
timings.** A second load overwrites the first's breakdown, which
matters to nobody except somebody loading two maps and then asking how
long the first took.

The serious one that used to sit here is gone. A process-wide pointer
named the active map, so a second map silently took the first's place
as the target of any box-initiated statics write and of every
box-timing sample. That pointer existed for exactly one feature — a
box reaching out to write a static — and issue 405 removed the
feature, which removed the reason for the pointer, which removed the
restriction. See G11 and G12.

**No value is ever fresh at the moment it is used, and this engine is
not for timing-critical work.** Nothing produces a value at the instant
it is consumed. A static holds whatever was last written into it, and
every invocation between two writes reads the same thing.

This is a deliberate narrowing rather than a gap. There was a pull path
that existed to make the opposite promise, and it was removed —
[056](implementation-notes/056-no-pull-path.md) says what it was for
and what ending it cost. A box that needs the current time asks for the
current time *inside the box*. Anything that must match the instant it
is used should not be reaching through a dataflow graph to get it.

**A value recomputes only when something upstream changes, never
because somebody read it.** For a recalculation graph that is right.
For something that genuinely should be re-read per use, the drawing has
to say what makes it re-read, which means a wire back from whatever
consumes it — and that will look like a loop, because it is one.

**An invocation's inputs are all read at one instant, and this used to
be a warning here.** Ring values were claimed under the station's
mutex while a static was read afterwards, outside it, so a box reading
two statics could get values that were correct at two different times
and never together. The gap existed because a static's value lived in
a table with a lock of its own, and that lock could not be taken while
the station's was held. With the value on the port there is no second
lock, so the claim moved inside the same window as the ring pops and
the warning became guarantee T3.

A static still holds whatever was last written into it, so it may be
*old*. What it can no longer be is inconsistent with its siblings in
the same invocation.

**A ring port with a backlog is drained by static writes.** Each write
runs the readiness check, and a waiting ring value completes the input
set, so it is consumed. That is real work on real values rather than an
error — but a fast writer against a deep queue empties it faster than
the producer alone would have.

**An address travelling down a wire is not checked, and cannot be.**

The construction operations exist as boxes, so that a program can
build a program (issue 212). Such a box has to be told which program
to act on, and it cannot reach one — a box takes its arguments by
value and the last global pointer to a map was deleted so that two
programs could run in one process without seeing each other. So a
program is named by **where it lives**, carried as a number.

A wire is legal when both ends count the same bytes. On the machines
this runs on an address is eight of them, and so is a `double`, a
`long`, and a file offset. **So the engine will accept a wire feeding
any eight-byte value into the argument that says which program to
build into**, and what follows is not a wrong answer. It is a write
through whatever those bytes were.

This is the same accepted cost as everywhere else — width tells two
types apart and nothing else does — and it is recorded separately
because the consequence is different in kind. When values are data, a
mis-wire produces a wrong number: visible, local, eventually noticed.
When the value is an address the engine will build through, a mis-wire
produces a fault with no useful location, because the engine did
exactly what it was told.

**What is done about it, and what is not.** The construction boxes
refuse a null before touching anything, which catches the likeliest
mistake — a port never given a value delivers zero. Past null, nothing
distinguishes a real address from any other eight bytes, and nothing
here pretends to. The honest fix is comparing types by *layout*
instead of by width, which is designed in full and whose tables have
been emitted since phase 3, unconsulted; it was not taken, and this is
the cost of not taking it. Wrapping the address in a deliberately
odd-sized struct was considered and rejected, because buying the
distinction that way makes the type system lie about sizes, which is
worse than what it prevents.

**Nothing about the order values leave a port.** A value delivered
first is not necessarily claimed first. A reader scans for a usable
slot rather than computing where the oldest one must be, and slots are
released individually wherever they sit, so gaps open and "oldest"
stops being cheap to find.

*(An earlier draft of this blamed a claim that rolls back. There is no
roll-back path: the claim checks every port before taking from any of
them, so a partial claim never exists. The scan is the reason on its
own.)*

This was true and tested before it was given up. Two things made it
worth losing. Values reaching one port from two upstream stations were
already in whatever order the threads happened to produce them, so the
order being preserved was arbitrary to begin with. And keeping it would
have meant a slow write blocking every finished value behind it, which
is a real cost paid on every delivery to buy an order nobody could rely
on anyway.

**A second reason, and it is not the scan.** A port that is converted
away from being a buffer and back keeps whatever was waiting in it
(issue 210f) — nothing is destroyed on any path through a conversion.
So a value that arrived before the conversion may be served after
values that arrived during it. This costs nothing that was still being
promised, but somebody tracing an out-of-order delivery should know
there are two ways to get one.

**The test that held it is not deleted, it is narrowed.** Several
hundred values through a growing, wrapped buffer still have to arrive
untorn and arrive exactly once; what has gone is the assertion that the
fifth value on one side met the fifth on the other. Tearing and loss
are the failures worth catching, and neither of them was ever the
ordering.

**What this costs is positional pairing.** A station with two ring
ports takes one value from each, and it can no longer be assumed that
the fifth from the left meets the fifth from the right. A program that
needs two streams matched has to carry the matching in the values —
which is what it should have done regardless, since ordering across
stations was never promised either.

**Nothing about ordering across stations.** Tasks run in creation order
within the queue (P3), but which station's task is created first, when
two become ready at once, is whatever the threads did. Two runs of the
same map on the same input may deliver in different orders.

**Nothing about how many times a station runs relative to its
readers.** A value computed once into a static port is read by every
invocation that follows, however many that is — so a station a million
invocations depend on runs once, not a million times. Under the pull
path this was the other way round, and the change is a saving rather
than a loss: a shared computation is computed once and read by
everyone.

**B1 and B4 are unenforced.** The two guarantees the most is built on
are the two that nothing checks. A linter that refused `static` storage
and blocking calls inside box sources would move both from discipline to
build, and the generator already parses those files.

---

## Retiring a guarantee

Since some of these should probably go, here is what taking one out
involves — the point being that a guarantee is load-bearing whether or
not anyone remembers leaning on it.

1. **Find what rests on it.** Follow the "bought with" and "follows
   from" notes above. P6 rests on B4; B3 rests on B1; T2 and T3 rest on
   T1. Removing one of those is removing several.
2. **Find what it paid for.** Every guarantee has a cost column, and
   dropping the guarantee should refund that cost. If it does not, the
   guarantee was free and there was no reason to drop it.
3. **Find the tests that hold it.** A guarantee held by a test that
   still passes after it is retired was never really the thing under
   test.
4. **Say what replaces it.** "No longer guaranteed" is a worse state
   than either "guaranteed" or "explicitly not guaranteed" — the third
   is a fact, the first two are facts, and the space between them is
   where somebody builds on an assumption nobody wrote down.
5. **Move it to the section above**, rather than deleting the line. A
   retired guarantee is more useful as a stated non-guarantee than as an
   absence.

---

## Related

- [001 — Overview](001-overview.md), the one rule everything else falls
  out of
- [002 — Stations and ports](002-stations-and-ports.md), where the
  structural ones live
- [006 — Scheduling](006-datapath-scheduling.md), the termination
  argument in full
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md),
  which nearly cost T1 and T2 and ended up keeping both
- [057 — Packaging](implementation-notes/057-packaging.md), where E1 and
  the one-map restriction become somebody else's problem
