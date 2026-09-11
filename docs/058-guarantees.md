# 058 — Guarantees

Things that are always true about a running map. A guarantee tells you
what you may build on without measuring anything.

**How each one is held** says how much it is worth:

| held by | meaning |
|---|---|
| **structure** | it cannot be otherwise without changing the shape of the data |
| **build** | the generator or the C compiler refuses the alternative |
| **load** | validation rejects a map that would break it, before anything runs |
| **runtime** | checked at the moment of the action; the program stops if it fails |
| **discipline** | nothing enforces it. A box author can break it and find out later |

---

## The bargain

One trade, made once. Nearly every entry in the *not guaranteed* section
is the price of this single purchase.

| # | always true | what it costs | held by |
|---|---|---|---|
| U1 | No worker sits idle while a task is ready to run. | Order, timing, pairing, batches, rounds — everything in the closing section. | discipline (via B4) |

**What is sold** is every promise that would require a worker to wait.
Ordering values would mean a slow write holding finished ones behind it.
Pairing across ports would mean a ready value waiting for its partner. A
batch or a round would mean cores that finished early standing still
until the ones that finished late caught up.

So a value has to be usable by anyone, interchangeably with any other
value of its type at the same port. That is V1 read from the scheduler's
side rather than the data's.

The one leak is B4: a box that blocks parks a worker on something the
engine cannot see, and U1 is withdrawn, silently, for as long as that box
runs.

## Values

| # | always true | what it costs | held by |
|---|---|---|---|
| V1 | A value on a wire is atomic and independent. It carries no relationship to any other value, and none to whatever run produced it. | Anything that belongs together must **be** one value — a struct on one wire. | structure |
| V2 | A station pairs whatever each of its ports happens to hand over. | There is no round, batch, or matching set, and nothing could implement one without a tag riding on every value. A port hands over the first ready slot a search finds, not the oldest. | structure |

These are one fact stated twice, and it becomes visible in a graph that
splits and rejoins. The two paths run at different speeds and can finish
out of order, so the rejoining station may pair the first value's result
from one path with the second value's from the other. **If two things
were meant to stay together, they were meant to be one value.** The
alternative — tagging every value and matching tags at every port — is a
different engine.

## Boxes

| # | always true | what it costs | held by |
|---|---|---|---|
| B1 | A box cannot remember anything between calls. **A station can.** | The memory is a value on a **static port**, reached only by the box returning a value that a wire carries into it. So state belongs to the *placement*: one box at three stations is three memories. And it is **drawn** — visible in the map file as an arrow rather than hidden inside C. | structure |
| B2 | A box receives private copies of its arguments, never shared pointers. | Every value is copied at least once per invocation. | structure |
| B3 | Two invocations of the same station may run simultaneously on different threads. | Follows from B1. A station's memory does not endanger it: that lives on a port, written by the delivery path under the station's mutex (G6), never by C inside the box. | structure |
| B4 | A box never blocks. | Nothing may wait on a value, a lock, or a device. | **discipline** |
| B5 | A box that returns void is a sink; the engine needs no support for it. | A sink's output is simply not delivered anywhere. | structure |

B1 and B4 are held by nothing but discipline and almost everything rests
on them. B1 makes B3 safe, B3 makes the engine parallel, B4 makes
deadlock impossible (P6). One box with a `static` counter or a blocking
socket read withdraws both, and nothing will say so.

**B1 does not forbid counting.** It forbids the *box* keeping the count.
The station keeps it on a static port, written by a wire from the box's
own output, so the memory stays somewhere the engine can lock, dump and
revive.

---

## Stations and wiring

Field by field, all of this is [002](002-stations-and-ports.md).

| # | always true | what it costs | held by |
|---|---|---|---|
| S1 | A station never moves once placed. | A station holds its own mutex, and a mutex is identified by where it lives — moving one leaves every thread parked on it waiting at an address nobody unlocks. The price is one shift, one mask and one dereference where a flat array had one add. | structure |
| S2 | A wire never names a station that is not there. | A wire is an index, never a pointer, and **removing a station is what removes the wires to it** — a walk of every output port, paid during a rare operation rather than on every delivery. | structure |
| S3 | Growing an input buffer invalidates nothing. | **Nothing is reallocated at all.** A worker copying out of a slot it has claimed holds no lock, and relocating that slot underneath it is the one thing ownership does not protect against. | structure |
| S4 | An input buffer is never full when a value arrives. | It grows before a writer can fail to find an empty slot. Memory absorbs any imbalance between input sides. | runtime |
| S5 | A station's **slot states** are read and written only under that station's mutex; its value **bytes** are protected by ownership instead. | One lock per station, so a claim holds exactly one lock and no lock ordering exists to get wrong. A slot in *reserved* or *claimed* belongs to one worker, so its copies happen outside the lock. A delivering writer takes no lock at all. | structure |
| S6 | The station kind is consulted at exactly one moment — choosing an exit on the way out. | A new kind is a row in one table, never a branch in several functions. | structure |
| S7 | A program is reached only through its marked input and output ports. | Everything else — a station index, a pointer into its buffers — is **undefined** to reach across a program boundary, and enforced rather than merely undefined: delivering from outside refuses any port a program did not mark. Being an argument is derived, so an enclosing map wiring into a sub-map needs no bookkeeping. A marked output discards like any other until a caller registers somewhere to put the values. | structure |
| S8 | A delivery walk takes no lock and copies nothing. | A port's destinations are one immutable array behind a pointer; a rewire builds a new one and swaps the pointer in a single atomic write. | structure |
| S9 | Nothing a rewire replaces is freed while a worker might be inside it. | Each worker keeps an epoch on its own cache line, **odd inside a task and even outside**. A replaced set is filed with a snapshot of every epoch and freed once every worker is even or has moved on. Nothing waits and nothing spins — an idle worker is asleep and therefore even, which is what would otherwise deadlock a sweep against a quiet pool. | runtime |

---

## Tasks

| # | always true | what it costs | held by |
|---|---|---|---|
| T1 | A task is self-contained once built. | Every value in it is a copy, taken before it existed. | structure |
| T2 | A task's run time is its box's run time. | Per-box timings measure what their name says; throughput is runs times per-box cost. | structure |
| T3 | One invocation's inputs are claimed atomically with respect to each other — **all of them**, under a single hold of the station's mutex. | What happens under the hold is the claiming, not the copying; the bytes are copied outside the lock. A static is the exception in the other direction — peeked rather than taken, so nothing owns it, and its copy stays inside the hold. | structure |
| T4 | One allocation per task, sized exactly for its box; one free. | The size comes from the station, told it at placement — every number a `sizeof` the compiler folded into an immediate. | build |
| T5 | A task comes into existence by exactly one path. | A delivery arriving, a static being written, the statics bound during construction, a captured queue put back — all reach the same readiness check and the same task build. | structure |
| T6 | **A station ready several times over starts that many tasks.** | Asking once is enough when values arrive one at a time; it stops being enough when a port fills *all at once*, which binding a constant, writing one, and reviving a capture all do. Thirty values on one port and a constant on the other means thirty tasks. A station whose every port is a constant is asked once per change instead — a constant is never consumed, so draining it would never finish. | structure |
| T7 | A ring value claimed by one task cannot be claimed by another. | A slot moves out of *ready* under the mutex, exactly once. | structure |
| T8 | An iterator's exit is decided under the mutex and rides inside the task. | Two tasks assembled a moment apart carry different exits no matter which finishes first. | structure |

---

## The pool

| # | always true | what it costs | held by |
|---|---|---|---|
| P1 | Enqueuing never blocks on user code. | A full ring doubles — a bounded memory copy — rather than making the pusher run a task. | structure |
| P2 | The ring's storage can move with nothing dangling. | Nothing holds a pointer *into* the ring; it holds pointers *out* to tasks, and a worker carries its pointer away. | structure |
| P3 | First in, first out. | No prioritization is possible. | structure |
| P4 | Every task is run once, handed to the finish hook, and freed — three unconditional steps. | A task cannot have phases. A two-trip task must be two tasks. | structure |
| P5 | When the pool declares completion, no task remains and nobody could enqueue one. | The last worker to fall asleep re-scans before anyone sleeps. Without it the program can exit silently having not done part of its work. | runtime |
| P6 | Deadlock is impossible. | Bought entirely by B4. A worker is always running, delivering, or asleep with nothing to do — and the last is completion, not a stall. | discipline (via B4) |
| P7 | The pool knows nothing about boxes, stations, or maps. | It reads one field of a task and ferries four it does not interpret: the owning program, the station, the exit port, and which station each worker is inside. | structure |
| P8 | A program out of work says so, whether or not anybody was already waiting. | Asking to be told raises the signal immediately when it has already happened. Without it, a program that finished between being released and the waiter sitting down would leave that waiter waiting forever. | runtime |
| P9 | A running box cannot be stopped, and nothing pretends otherwise. | Cancellation acts only at cancellation points, does not unwind C, and leaves held mutexes locked forever. Halting honestly means **stop starting new things**. | structure |

---

## Statics

| # | always true | what it costs | held by |
|---|---|---|---|
| G5 | A static is always present and never consumed. | It cannot gate readiness, so a station with only statics is always ready — which is what makes writing one enough to run it. | structure |
| G6 | A statics write never tears. | One mutex, one copy. The mutex is the station's own, which the claim already takes. | runtime |
| G7 | Push cycles are legal. | They must be: a box cannot remember (B1), so **every** way of carrying state is a cycle. A blanket cycle check would forbid all of them. | structure |
| G8 | Writing a static runs the ordinary readiness check on its station. | Nothing can be made to run that could not run anyway: an empty ring port still answers no. | structure |
| G9 | Every box runs on a worker that picked up a task for it. | There is no path that executes a box anywhere else. | structure |
| G10 | A static value belongs to exactly one port. | A constant is written on the input line of the port that holds it, so no two ports can name one value. Sharing is drawn as a wire. | structure |
| G12 | A process may run any number of maps at once, and they cannot see each other. | Nothing in the engine is process-wide. A box reaches a map through a handle it was *given* as a value. Held by a test. | structure |
| G13 | A wire may deliver into a static port, and the value **overwrites** the constant rather than queueing. | **Two arrows into one static port is last-writer-wins, nondeterministically.** What it buys is a constant that is *computed*: a station runs once, wired into a downstream static port, and every invocation afterwards reads what it produced. The **wire** says this, not the box — so it is visible in the map file, in the dump, and on a canvas. | structure |

---

## The build path

| # | always true | what it costs | held by |
|---|---|---|---|
| C1 | Every size and offset is computed by the C compiler, never by the generator. | Emitted as `sizeof` and `offsetof` inside a generated placement function, folded into immediates and stored nowhere. | build |
| C2 | A box that exists is a box the generator saw. | Box sources are discovered, never listed. | build |
| C3 | You can never compile against a stale emission. | A failing generator writes nothing into place. | build |
| C4 | A map that loads is a map that can run. | Validation happens when a caller declares the program finished, so a mistyped wire is a startup error rather than a surprise at the first delivery. | load |
| C5 | The dump round-trips: dumping a dump yields the dump. | Held by a test. | runtime |
| C7 | A value written into a caller's collection array never lands outside it. | The bound is the **reservation**: one atomic add, and a worker at or past the room writes nothing. Winding down when an array fills is an optimisation and can never be what keeps it in bounds, because workers are still inside boxes when the last slot goes. The count keeps climbing past the room deliberately, so a caller can tell a program that filled the array from one that ran dry. | delivery |

---

## Errors

| # | always true | what it costs | held by |
|---|---|---|---|
| E1 | There are no fallbacks. Every error stops the program and names what failed and where. | The engine owns its process, which is what makes it hostile to embed — see [057](implementation-notes/057-packaging.md). | discipline |
| E2 | A wrong answer never keeps flowing. | Follows from E1. | discipline |
| E3 | An invalid operation ends the program. **A refusal cannot be ignored into a half-built program.** | The refusal still *travels* — handed back so a caller reading a description can collect faults — but nothing may discard it and continue. | structure |
| E4 | How a program died is legible to a shell. | Distinct exit codes: malformed input, wrong call, a resource no edit can fix, an interruption. Anything that retries can tell a fault it can correct from one it cannot, in code rather than in prose. | structure |
| E5 | A person can always stop a program, and always get out of stopping it. | Three signals, each needing less cooperation than the last, and a second interrupt that leaves immediately — the only signal handler in the engine, because a thread gathering a report is not asking for signals and the report is the thing that may never finish. | runtime |

---

## Not guaranteed — and worth knowing

**Two results of one program are not synchronised with each other.** Two
stations, two threads, two unrelated moments. A caller registers one
array per result, and **two arrays filling side by side look like columns
of a table** — they are not. Entry three of one and entry three of
another did not come from the same input. **Make the outputs fungible.**

**A program is not required to declare a result.** A map with no result
mark produces nothing outward. What is checked instead is that the marked
ports are numbered without gaps and without repeats.

**A wire is checked by width, not by shape, and never by name.** A struct
of four integers wires into a struct of two integers and a double, and
delivery copies the bytes as asked. The gain is that two structs with
identical layouts and different names connect; the cost is that types
which merely happen to be the same size connect too. Shape comparison —
matching kind, size and offset field by field — would close it, is
designed in issue 309, and the field tables it needs are emitted and
consulted by nothing.

**An address travelling down a wire is not checked, and cannot be.** The
construction operations are boxes, so a program can build a program; such
a box is told which program to act on as a number, because a box takes
its arguments by value and no global pointer to a map exists. An address
is eight bytes, and so is a `double`, a `long`, and a file offset — so
the engine will accept a wire feeding any eight-byte value into that
argument, and what follows is a write through whatever those bytes were.

The consequence differs in kind from an ordinary mis-wire: wrong data is
a wrong number, visible and local, while a wrong address is a fault with
no useful location. The construction boxes refuse a null, which catches a
port never given a value; past null nothing distinguishes a real address
from any other eight bytes. Wrapping the address in a deliberately
odd-sized struct was rejected, because it makes the type system lie about
sizes.

**A box may write a static, and a map handle is an ordinary value.** A
map with marked ports *is* a box. Such a box has a side effect far larger
than any other, which is what lets a program restructure itself while it
runs; G12 survives because the handle travels as a value on a wire.

**One process-wide variable is left: the last load's timings.** A second
load overwrites the first's breakdown.

**No value is ever fresh at the moment it is used**, so this engine is
not for timing-critical work. A static holds whatever was last written,
and every invocation between two writes reads the same thing. A box that
needs the current time asks for it inside the box.
[056](implementation-notes/056-no-pull-path.md) is why.

**A value recomputes only when something upstream changes**, never
because somebody read it. Anything that should be re-read per use needs a
wire back from whatever consumes it, which will look like a loop because
it is one.

**A ring port with a backlog is drained by static writes.** Each write
completes an input set, so a fast writer against a deep queue empties it
faster than the producer alone would have.

**Nothing about the order values leave a port.** A reader scans for a
usable slot rather than computing where the oldest must be, and slots are
released individually wherever they sit. A second route to an
out-of-order delivery: a port converted away from being a buffer and back
keeps what was waiting in it.

**What this costs is positional pairing.** A station with two ring ports
cannot assume the fifth from the left meets the fifth from the right. A
program that needs two streams matched has to carry the matching in the
values.

**Nothing about ordering across stations.** Tasks run in creation order
within the queue (P3), but which station's task is created first, when
two become ready at once, is whatever the threads did.

**Nothing about how many times a station runs relative to its readers.** A
value computed once into a static port is read by every invocation that
follows, so a station a million invocations depend on runs once.

**A `static` inside a box source is not how a station remembers, and it
is not safe.** Nothing checks a box source for it, so somebody can write
one anyway, and three things go wrong at once:

- **Two invocations of one station run simultaneously (B3) and share
  it.** No lock covers it — the station's mutex guards its ports and is
  released before the box is called.
- **Capture would not carry it.** A capture walks every value on a port
  and every iterator's place in its exits. A `static` inside a C function
  is invisible to that walk, so the artifact looks complete and the
  revived program has silently forgotten.
- **It would not be in the picture.** An accumulator built the supported
  way appears in the map file, the dump and on a canvas as an arrow
  returning to its own station. One built with a `static` appears
  nowhere.

**B1 and B4 are unenforced.** A linter refusing `static` storage and
blocking calls inside box sources would move both from discipline to
build, and the generator already parses those files.

---

## Retiring a guarantee

1. **Find what rests on it.** P6 rests on B4; B3 rests on B1; T2 and T3
   rest on T1. Removing one of those is removing several.
2. **Find what it paid for.** Dropping it should refund the cost column.
   If it does not, the guarantee was free.
3. **Find the tests that hold it.** One that still passes afterwards was
   never testing the thing.
4. **Say what replaces it.** "No longer guaranteed" is worse than either
   "guaranteed" or "explicitly not guaranteed" — the space between them
   is where somebody builds on an assumption nobody wrote down.
5. **Move it to the section above** rather than deleting the line.

---

## Related

- [001 — Overview](001-overview.md), the one rule everything falls out of
- [002 — Stations and ports](002-stations-and-ports.md), where the
  structural ones live
- [006 — Scheduling](006-datapath-scheduling.md), the termination
  argument in full
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md)
- [057 — Packaging](implementation-notes/057-packaging.md)
