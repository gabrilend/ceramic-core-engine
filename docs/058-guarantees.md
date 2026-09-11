# 058 — Guarantees

Things that are always true about a running map, collected from the
documents, the source comments, and the tests that hold them up.

A guarantee is worth more than a measurement. A measurement tells you
what happened once on one machine; a guarantee tells you what you may
build on without measuring anything.

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
as long as work exists, as completely as the design can manage.

**What is sold** is every promise that would require a worker to wait.
Ordering values would mean a slow write holding finished ones behind it.
Pairing across ports would mean a ready value waiting for its partner. A
batch or a round would mean the cores that finished early standing still
until the cores that finished late caught up. Each is a worker not
running one of the ten things that are ready, which is the one cost this
design will not pay.

**Which is why values must stand on their own.** A value passing through
this engine has to be usable by anyone, interchangeably with any other
value of its type at the same port. Nothing is anybody's in particular.
That is what lets whichever worker is free take whatever is ready, and it
is the same sentence as V1 read from the scheduler's side rather than
the data's.

**How it is held, honestly.** By structure everywhere except the one
place: a box that blocks (B4) parks a worker on something the engine
cannot see, and the guarantee is withdrawn for as long as that box runs,
silently. This is the strongest reason B4 deserves a linter.

## Values

| # | always true | what it costs | held by |
|---|---|---|---|
| V1 | A value on a wire is atomic and independent. It carries no relationship to any other value, and none to whatever run produced it. | Anything that belongs together must **be** one value — a struct on one wire — rather than two values that happen to arrive near each other. | structure |
| V2 | A station pairs whatever each of its ports happens to hand over. | There is no notion of a round, a batch, or a matching set, and nothing could implement one without a tag riding on every value. A port hands over the first ready slot a search finds, not the oldest; the guarantee is not about which value, but about there being no pairing rule at all. | structure |

These are the same fact stated twice, and the place it becomes visible is
a graph that splits and rejoins — one station fanning to two paths that
later meet at a third. Send two values through. The tasks on the two
paths run on different threads at different speeds and can finish out of
order, so the rejoining station may pair the first value's result from
one path with the second value's result from the other.

That is not a defect to be mitigated. It is what independence means. A
station that fans to two paths has produced two unrelated values, and the
station where they meet takes whatever each port hands over, which is the
only thing it could do without being told about a relationship that was
never expressed. **If two things were meant to stay together, they were
meant to be one value.**

The consequence for a map author is a design rule rather than a hazard:
correlation is something you build, by putting the related parts in a
struct and sending it down one wire. The alternative — tagging every
value and matching tags at every port — is a different engine, and
knowing it exists is mostly useful so that nobody reinvents it badly.

## Boxes

| # | always true | what it costs | held by |
|---|---|---|---|
| B1 | A box cannot remember anything between calls. **A station can.** | The memory is a value sitting on a **static port**, and the only way a box reaches it is by **returning a value that a wire carries into it** — usually the station's own output arrow pointing back at its own static input port. So state belongs to the *placement*, not to the function: one box at three stations is three memories. And it is **drawn**: an accumulator is visible in the map file as an arrow, rather than hidden inside C where no picture shows it. | structure |
| B2 | A box receives private copies of its arguments, never shared pointers. | Every value is copied at least once per invocation. | structure |
| B3 | Two invocations of the same station may run simultaneously on different threads. | Follows from B1 — the box stores nothing, so two invocations share nothing. **A station's memory does not endanger this**: it lives on a port and is written by the delivery path under the station's mutex (G6), never by C code executing inside the box, so two simultaneous invocations cannot race over it. Each reads whatever was there when its own inputs were claimed, which is T3 working exactly as written. | structure |
| B4 | A box never blocks. | Nothing may wait on a value, a lock, or a device. A worker that cannot progress is a worker not running the ten things that are ready. | **discipline** |
| B5 | A box that returns void is a sink; the engine needs no support for it. | A sink's output is simply not delivered anywhere. | structure |

B1 and B4 are the two load-bearing guarantees held by nothing but
discipline, and almost everything else rests on them. B1 is what makes B3
safe, and B3 is what makes the whole engine parallel. B4 is what makes
deadlock impossible (see P6). A single box with a `static` counter or a
blocking socket read quietly withdraws both, and no part of the system
will say so.

**B1 does not forbid counting.** What it forbids is the box keeping the
count. The station keeps it, on a static port, written by a wire from the
box's own output — so the box stays a function of its arguments and the
memory stays somewhere the engine can see, lock, dump and revive.

---

## Stations and wiring

Field by field, all of this is [002](002-stations-and-ports.md); what is
here is what may be relied on.

| # | always true | what it costs | held by |
|---|---|---|---|
| S1 | A station never moves once placed. | It has to be kept: a station holds its own mutex, and a mutex is identified by where it lives, so moving one would leave every thread parked on it waiting at an address nobody unlocks. The price is one shift, one mask and one dereference where a flat array had one add, measured on the delivery path and found to sit inside the run-to-run noise. | structure |
| S2 | A wire never names a station that is not there. | A wire is an index, never a pointer, and **removing a station is what removes the wires to it** — a walk of every output port, paid during a rare operation rather than on every delivery. A removed place does not come free until the sweep clears its record, because a task is built from a station's fields *after* the readiness check released the mutex. | structure |
| S3 | Growing an input buffer invalidates nothing. | **Nothing is reallocated at all.** A worker copying a value out of a slot it has claimed holds no lock, because a claimed slot belongs to it alone, and relocating that slot underneath it is the one thing ownership does not protect against. | structure |
| S4 | An input buffer is never full when a value arrives. | It grows before a writer can fail to find an empty slot. Memory absorbs any imbalance between input sides. | runtime |
| S5 | A station's **slot states** are read and written only under that station's mutex; its value **bytes** are protected by ownership instead. | One lock per station, covering every port's states together, so a claim holds exactly one lock and no lock ordering exists to get wrong. A slot in *reserved* or *claimed* belongs to exactly one worker, so the copies into and out of it happen outside the lock — which is what keeps the expensive half of a delivery off the serialized path. A delivering writer takes no lock at all. | structure |
| S6 | The station kind is consulted at exactly one moment — choosing an exit on the way out — and nowhere else. | A new kind is a row in one table, never a branch in several functions. | structure |
| S7 | A program is reached only through its marked input and output ports. | Everything else — a station index, a pointer into its buffers, anything a caller held on to — is **undefined** to reach across a program boundary. **It is enforced rather than merely undefined**: delivering from outside refuses any port a program did not mark as an argument. Being an argument is derived where it has to be, so an enclosing map wiring into a sub-map needs no bookkeeping and nothing can go stale. The outward mark adds **no** rule of its own: a marked output discards like any other until a caller registers somewhere to put the values. | structure |
| S8 | A delivery walk takes no lock and copies nothing. | A port's destinations are one immutable array behind a pointer; a rewire builds a whole new one and swaps the pointer in a single atomic write, so a walker reads the pointer once and walks something nobody will ever modify. | structure |
| S9 | Nothing a rewire replaces is freed while a worker might be inside it. | Each worker keeps an epoch on its own cache line, **odd while inside a task and even while not**. A replaced set is filed with a snapshot of every epoch and freed once every worker is either even or has moved on. Nothing waits and nothing spins — an idle worker is asleep and therefore even, which is what would otherwise deadlock a sweep against a quiet pool. The epoch spans the whole task rather than the walk, so the same mechanism answers the same question about a box's compiled code and a removed station. | runtime |

---

## Tasks

| # | always true | what it costs | held by |
|---|---|---|---|
| T1 | A task is self-contained once built. Nothing another thread can change affects it. | Every value in it is a copy, taken before it existed. | structure |
| T2 | A task's run time is its box's run time. Nothing hidden rides along. | Per-box timings measure what their name says, and throughput is runs times per-box cost. | structure |
| T3 | One invocation's inputs are claimed atomically with respect to each other — **all of them**, of every kind, under a single hold of the station's mutex. | No other thread can interleave between them, so an input set is always mutually consistent. **What happens under the hold is the claiming, not the copying**; the bytes are copied afterwards, outside the lock. A static is the exception in the other direction — it is peeked rather than taken, so nothing owns it, and its copy stays inside the hold where the only protection it has lives. | structure |
| T4 | One allocation per task, sized exactly for its box; one free. | The size comes from the **station**, which was told it when its box was placed — every number a `sizeof` the compiler folded into an immediate. | build |
| T5 | A task comes into existence by exactly one path. | Everything that starts a station — a delivery arriving, a static being written, the statics bound while a program is built, a captured queue being put back — reaches the same readiness check and the same task build. | structure |
| T6 | **A station that is ready several times over starts that many tasks.** | Asking once is enough when values arrive one at a time, because each arrival asks again — and stops being enough when a port fills *all at once*, which is what binding a constant, writing one, or reviving a captured program all do. Thirty values stacked on one port and a constant arriving on the other means thirty tasks, not one. A station whose every port is a constant is asked exactly once per change instead: a constant is never consumed, so such a station is ready forever and draining it would never finish. | structure |
| T7 | A ring value claimed by one task cannot be claimed by another. | The claim runs under the station's mutex, and a slot moves out of *ready* there exactly once. | structure |
| T8 | An iterator's exit is decided under the mutex and rides inside the task. | Two tasks assembled a moment apart carry different exits no matter which finishes first. | structure |

---

## The pool

| # | always true | what it costs | held by |
|---|---|---|---|
| P1 | Enqueuing never blocks on user code. | A full ring doubles — a bounded memory copy — rather than making the pusher run a task. | structure |
| P2 | The ring's storage can move with nothing dangling. | Nothing holds a pointer *into* the ring; the ring holds pointers *out* to tasks, and a worker carries its pointer away. | structure |
| P3 | First in, first out. A waiting task cannot be starved by newer arrivals. | No prioritization is possible. | structure |
| P4 | Every task is run exactly once, handed to the finish hook, and freed — three unconditional steps. | A task cannot have phases. A two-trip task must be two tasks. | structure |
| P5 | When the pool declares completion, no task remains and nobody could enqueue one. | The last worker to fall asleep re-scans before anyone sleeps. Without that re-scan the program can exit silently having not done part of its work. | runtime |
| P6 | Deadlock is impossible. | Bought entirely by B4. Nothing waits, so a worker is always running, delivering, or asleep with nothing to do — and the last is completion, not a stall. | discipline (via B4) |
| P7 | The pool knows nothing about boxes, stations, or maps. | It reads one field of a task, and ferries four it does not interpret — the owning program, the station, the exit port, and which station each worker is inside. Interpreting none of them is what keeps this true. | structure |
| P8 | A program that has run out of work says so, whether or not anybody was already waiting to hear it. | Asking to be told raises the signal immediately when it has already happened. Without it, a short program that finished between being released and the waiter sitting down would leave that waiter waiting forever. | runtime |
| P9 | A running box cannot be stopped, and nothing pretends otherwise. | There is no safe way to interrupt executing C: cancellation acts only at cancellation points, does not unwind C, and leaves held mutexes locked forever. So halting honestly means **stop starting new things**. | structure |

---

## Statics

| # | always true | what it costs | held by |
|---|---|---|---|
| G5 | A static is always present and never consumed. | It cannot participate in readiness, so a station with only statics is always ready — which is what makes writing one enough to run it. | structure |
| G6 | A statics write never tears. | One mutex, one copy — a struct half-overwritten while a claim reads it would yield fields from two different worlds. The mutex is the station's own, which the claim already takes. | runtime |
| G7 | Push cycles are legal. | They must be: a box cannot remember (B1), so **every** way of carrying state is a cycle. A loop through a ring buffer carries it as a queued value; an arrow from a station's output back into its own static port carries it as a remembered one (G13). A blanket cycle check would forbid both, which is to say all of the engine's mechanisms for state. | structure |
| G8 | Writing a static runs the ordinary readiness check on its station. | Nothing extra is needed to make recalculation propagate, and nothing can be made to run that could not run anyway: an empty ring port still answers no, because the engine will not invent a value for it. | structure |
| G9 | Every box runs on a worker that picked up a task for it. | There is no path that executes a box anywhere else. | structure |
| G10 | A static value belongs to exactly one port. | A constant is written on the input line of the port that holds it, so there is no way for two ports to name one value in a file. Sharing is drawn as a wire instead, which costs a station and gains something visible in the picture. | structure |
| G12 | A process may run any number of maps at once, and they cannot see each other. | Nothing in the engine is process-wide. Held by a test that runs two and writes into one. A box that reaches a map does so through a handle it was *given* as a value, never through a pointer the process holds. | structure |
| G13 | A wire may deliver into a static port, and the arriving value **overwrites** the constant rather than queueing. | **Two arrows into one static port is last-writer-wins, nondeterministically** — stated here rather than left as a surprise. What it buys is a constant that is *computed* rather than written down: a station that runs once, wired into a downstream static port, and every invocation afterwards reads what it produced. The box is untouched and remembers nothing; the **wire** is what says this value overwrites a static, and a wire is visible in the map file, in the dump, and on a canvas. | structure |

---

## The build path

| # | always true | what it costs | held by |
|---|---|---|---|
| C1 | Every size and offset is computed by the C compiler, never by the generator. | They are emitted as `sizeof` and `offsetof` expressions inside a generated placement function, folded into immediates and stored nowhere. The generator cannot guess wrong because it does not guess. | build |
| C2 | A box that exists is a box the generator saw. | Box sources are discovered, never listed, so a box cannot exist that the build silently ignores. | build |
| C3 | You can never compile against a stale emission. | A failing generator writes nothing into place. | build |
| C4 | A map that loads is a map that can run. | Validation happens when a caller declares the program finished, so a mistyped wire is a startup error rather than a surprise at the first delivery. | load |
| C5 | The dump round-trips: dumping a dump yields the dump. | The live map can always be written back out as a map file that reads back in. Held by a test. | runtime |
| C7 | A value written into a caller's collection array never lands outside it. | The bound is the **reservation**, not the winding down: a worker takes the next index with one atomic add and writes nothing when the index is at or past the room. Winding down when an array fills happens alongside and is an optimisation — it can never be what keeps the array in bounds, because it is asynchronous and workers are still inside boxes when the last slot goes. The count keeps climbing past the room deliberately, so a caller comparing it against the size can tell a program that filled the array from one that ran dry. | delivery |

---

## Errors

| # | always true | what it costs | held by |
|---|---|---|---|
| E1 | There are no fallbacks. Every error stops the program and names what failed and where. | The engine owns its process. This is what makes it hostile to embed in someone else's — see [057](implementation-notes/057-packaging.md). | discipline |
| E2 | A wrong answer never keeps flowing. | Follows from E1. | discipline |
| E3 | An invalid operation ends the program. **A refusal cannot be ignored into a half-built program**, because there is no surviving path that reaches one. | The refusal still *travels* — it is handed back so a caller reading a description can collect faults — but nothing may discard it and continue. | structure |
| E4 | How a program died is legible to a shell, not only to somebody reading the message. | Distinct exit codes: a malformed input, a wrong call, a resource no edit can fix, an interruption. Anything that retries on failure can tell a fault it can correct from one it cannot, in code rather than in prose. | structure |
| E5 | A person can always stop a program, and always get out of stopping it. | Three signals, each needing less cooperation than the last, and a second interrupt that leaves immediately — which needs the only signal handler in the engine, because a thread that is gathering a report is not asking for signals and the report is exactly the thing that may never finish. | runtime |

---

## Not guaranteed — and worth knowing

Places where somebody could reasonably expect a guarantee and not get
one. Naming them is the same work as naming the guarantees.

**Two results of one program are not synchronised with each other.** Two
stations, two threads, two unrelated moments. An embedding caller
registers one array per result, and **two arrays filling side by side
look like columns of a table** — they are not. Entry three of one and
entry three of another did not come from the same input. The rule for
anyone building on it: **make the outputs fungible.** If two values have
to stay together, they have to be one value.

**A program is not required to declare a result.** A map with no result
mark produces nothing outward. What is checked instead is that the marked
ports are numbered without gaps and without repeats.

**A wire is checked by width, not by shape, and never by name.** Two
types of the same size connect regardless of what is inside them: a
struct of four integers wires into a struct of two integers and a double,
and delivery copies the bytes exactly as asked.

The gain is that two structs with identical layouts and different names
connect, where name comparison would have forced an author to rename one
or write a box that took one and returned the other. The cost is that
types which merely happen to be the same size connect too. Shape
comparison — matching kind, size and offset field by field — would close
it honestly, is designed in full in issue 309, and the field tables it
needs are emitted and consulted by nothing. It was not taken because
comparing one integer against another is what a wire check should cost.

**An address travelling down a wire is not checked, and cannot be.** The
construction operations exist as boxes, so a program can build a program;
such a box is told which program to act on as a number, because a box
takes its arguments by value and no global pointer to a map exists. On
the machines this runs on an address is eight bytes, and so is a
`double`, a `long`, and a file offset — so the engine will accept a wire
feeding any eight-byte value into that argument, and what follows is a
write through whatever those bytes were.

The consequence differs in kind from an ordinary mis-wire rather than in
degree: wrong data is a wrong number, visible and local, while a wrong
address is a fault with no useful location. The construction boxes refuse
a null before touching anything, which catches a port never given a
value. Past null nothing distinguishes a real address from any other
eight bytes. Wrapping the address in a deliberately odd-sized struct was
considered and rejected, because buying the distinction that way makes
the type system lie about sizes.

**A box may write a static, and a map handle is an ordinary value.** A
map with marked ports *is* a box, so a box reaching a map is a box
reaching a box. Such a box has a side effect far larger than any other,
and that is what lets a program restructure itself while it runs. G12
survives it, because the handle travels as a value on a wire.

**One process-wide variable is left, and it is the last load's timings.**
A second load overwrites the first's breakdown.

**No value is ever fresh at the moment it is used, and this engine is not
for timing-critical work.** A static holds whatever was last written into
it, and every invocation between two writes reads the same thing. A box
that needs the current time asks for it inside the box.
[056](implementation-notes/056-no-pull-path.md) is why the engine makes
this narrowing rather than the opposite promise.

**A value recomputes only when something upstream changes, never because
somebody read it.** For something that genuinely should be re-read per
use, the drawing has to say what makes it re-read — a wire back from
whatever consumes it, which will look like a loop because it is one.

**A ring port with a backlog is drained by static writes.** Each write
runs the readiness check and a waiting ring value completes the input
set, so a fast writer against a deep queue empties it faster than the
producer alone would have.

**Nothing about the order values leave a port.** A reader scans for a
usable slot rather than computing where the oldest one must be, and slots
are released individually wherever they sit. A second route to an
out-of-order delivery: a port converted away from being a buffer and back
keeps whatever was waiting in it, so a value that arrived before the
conversion may be served after values that arrived during it.

**What this costs is positional pairing.** A station with two ring ports
cannot assume the fifth from the left meets the fifth from the right. A
program that needs two streams matched has to carry the matching in the
values.

**Nothing about ordering across stations.** Tasks run in creation order
within the queue (P3), but which station's task is created first, when
two become ready at once, is whatever the threads did.

**Nothing about how many times a station runs relative to its readers.**
A value computed once into a static port is read by every invocation that
follows, so a station a million invocations depend on runs once.

**A `static` inside a box source is not how a station remembers, and it
is not safe.** Nothing checks a box source for `static` storage, so
somebody can write one anyway, and three things go wrong at once:

- **Two invocations of one station run simultaneously (B3) and share
  it.** No lock in the engine covers it — the station's mutex guards its
  ports and is released before the box is called. The supported
  mechanism has no such problem, because the write happens on the
  delivery path under that same mutex.
- **Capture would not carry it.** A capture walks every value sitting on
  a port and every iterator's place in its exits. A `static` inside a C
  function is invisible to that walk, so the artifact looks complete and
  the revived program has silently forgotten.
- **It would not be in the picture.** An accumulator built the supported
  way appears in the map file, in the dump and on a canvas as an arrow
  returning to its own station. One built with a `static` appears
  nowhere, and the drawing of the program is a drawing of something
  else.

**B1 and B4 are unenforced.** The two guarantees the most is built on are
the two that nothing checks. A linter that refused `static` storage and
blocking calls inside box sources would move both from discipline to
build, and the generator already parses those files.

---

## Retiring a guarantee

A guarantee is load-bearing whether or not anyone remembers leaning on
it, so taking one out means:

1. **Find what rests on it.** Follow the "bought with" and "follows from"
   notes above. P6 rests on B4; B3 rests on B1; T2 and T3 rest on T1.
   Removing one of those is removing several.
2. **Find what it paid for.** Every guarantee has a cost column, and
   dropping the guarantee should refund that cost. If it does not, the
   guarantee was free and there was no reason to drop it.
3. **Find the tests that hold it.** A guarantee held by a test that still
   passes after it is retired was never really the thing under test.
4. **Say what replaces it.** "No longer guaranteed" is a worse state than
   either "guaranteed" or "explicitly not guaranteed" — the third is a
   fact, the first two are facts, and the space between them is where
   somebody builds on an assumption nobody wrote down.
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
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md)
- [057 — Packaging](implementation-notes/057-packaging.md), where E1 and
  the one-map restriction become somebody else's problem
