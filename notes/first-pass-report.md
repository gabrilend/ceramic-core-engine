# First-pass report — lessons learned while building

This document is the deliverable of the first pass over minimal-soramech.
The build proceeded issue by issue, in order, and every pain point,
contradiction between documents, and gap the design left unstated is
recorded here as it was met. The second pass reads this before touching
anything.

Entries are append-only, grouped by the phase that surfaced them. Each
entry says what was expected, what was found, and what the second pass
should do differently.

## Found before building anything

**The launcher's hard-coded root pointed at a machine that isn't this
one.** The `run-demo` script carried a project root of
`/home/ritz/programs/sora/minimal-soramech`; the project actually lives at
`/mnt/mtwo/programming/ai-playground/minimal-soramech`. The script's
override-by-argument saved it from being broken, but a hard-coded default
that is wrong is a fallback in disguise. Lesson: the hard-coded `${DIR}`
convention needs a stated rule for what happens when a project moves —
probably a check that refuses loudly when the default does not exist,
rather than quietly relying on the caller to pass the right thing.

**The vision file asks for its own edit, and the table of contents seals
it.** The vision's final paragraph says "please inform the user then
delete this paragraph when we figure it out"; document 000 says the
vision is sealed and never edited. Both instructions cannot be followed.
First pass resolution: the paragraph's question (input-less boxes) was
answered by the seed sweep and gatherers, the user is informed here, and
the paragraph stays — the seal wins because history outranks tidiness.
The second pass should decide the precedence rule explicitly instead of
leaving two documents in quiet disagreement.

**The docs and the global project conventions disagree about where
indexes come from.** The docs are numbered 000–010 but no
file-index-counter existed to record it. One was created holding 010
before any source file was named. Lesson: the counter must be created
the moment the first indexed file is, or two files will eventually claim
one number.

## Phase 1 — the pool

**Issues 102, 103, and 104 describe one function, not three.** The run
loop, the sleeping rule, and the termination rule are all the same
dozen lines under the same mutex; the staging the issues prescribe — a
busy-return placeholder, later replaced by sleeping, later extended
with termination — would have meant building two throwaway
intermediate states of a function whose final shape was already fully
designed. The implementation landed as one unit and each issue closed
by proving its aspect with its own test. Lesson for the second pass:
when several issues each specify a facet of a single small mechanism,
say so in the issues and stage the *tests*, not the code.

**The documented termination race is an artifact of an assumed lock
discipline.** The design docs walk through a lost-wakeup race (worker
checks empty, is preempted, another worker wakes nobody, everyone
sleeps over a full queue) and prescribe the last-sleeper re-scan as its
repair. With one mutex guarding queue, sleeper count, and stop flag —
and a condition wait that releases that mutex atomically — the race
has no window at all: check-and-register is indivisible. The re-scan
degenerates into the last sleeper's final look, which is still the
termination *decision*, but the doc's dramatic justification is
unreachable, and the test issue 104 demands (delay a worker between
check and registration) cannot be written. Lesson: docs 006 specifies
mechanism where it should specify invariant. Say "the count and the
decision to sleep must be atomic with respect to push" and let the
implementation prove it however it locks.

**The design forbids what its own demos require.** Termination assumes
nothing outside the pool pushes after startup; the sleeping issue's own
trickle test — and any interactive use, and phase 7's control surface —
pushes from outside. Issue 104 waves at this ("must register itself in
the count") without designing it. The first pass built the missing
piece: outside-submitter registration, with the last deregistration
nudging an all-asleep pool so termination becomes decidable. The second
pass should promote this from a footnote to a designed part of the
pool's interface, because rewiring (704) and any live control surface
will stand on it.

**Two naming conventions collide at the demos.** The launcher
discovers demos by the pattern `phase-*` with the executable bit set;
the project convention wants every source file carrying a numeric
index. A demo's shell script cannot satisfy both. First-pass
resolution: the script keeps the launcher's name, the C source behind
it carries the index. The second pass should either teach the launcher
to read indexed names or exempt launcher-discovered entry points from
indexing, in writing.

**The pool needed a join/destroy ownership rule the design never
states.** Who collects the worker threads — the join call, the destroy
call, or both? First build had destroy join again after the caller
already had, and the platform's error for joining a collected thread
is noise. Resolved by making join idempotent. Small, but exactly the
kind of edge a blueprint should own: state the lifecycle (create →
release → join → destroy, join implied by destroy, both callable).

## Phase 2 — stations and the push path

**The docs tell the wrong story about where a backlog lives.** Docs
002, issue 203, and issue 701 all say a growing ring buffer means "a
consumer slower than its producer." Building the demo disproved the
simple reading: a single-input station is ready on every write, so its
values are claimed instantly and its slot depth never exceeds one —
the backlog of a slow single-input consumer accumulates as tasks in
the *pool's ring*, not in the slot. Slot growth is specifically the
signature of a *multi-input* station fed unevenly, one side waiting
for its siblings. Docs 002 now carries the correction. The second pass
should design phase 7's reporting around both piles from the start —
queue high-water and slot high-water are two different diagnoses and
the original text conflates them.

**Claiming statics and gathered values under the station mutex was
quietly wrong, and the issues almost prescribe it.** Issue 204 says
"claim one value from each" under the lock; issue 403 separately says
gathering happens outside the lock. Following 204 literally for all
three slot kinds would run user code (a gatherer) inside a station's
contended section. The build resolved it by making the claim dispatch
table honest about the split: ring slots claim under the mutex,
gatherer and static rows are null meaning "resolved at task build,
outside the lock." The second pass should state the rule once, in the
delivery doc: no user code and no cross-structure locks inside a
station's critical section, and the claim table's null rows are how
that is enforced.

**Nothing in phase 2 can stop a push loop, and nothing says so.** The
docs celebrate push cycles as the engine's only memory (the counter
pattern), but until phase 5's comparator exists there is no way for a
loop to ever terminate — a phase 2 cycle runs forever by construction.
The roadmap never mentions that the counter pattern is unusable for
three phases. Harmless here, but a reader building phases in order and
trying the documented counter as their first map would produce an
unstoppable program. A sentence in docs 004 or the roadmap would have
prevented the surprise.

**The demo had to invent an outside-feeder idiom the design lacks.**
Showing backpressure requires feeding a running map at a metered rate
from the main thread — which is exactly the outside-submitter case
issue 104 declared illegal-unless-registered. Every test and demo that
trickles ended up using the registration API built in phase 1. It
works, but it is load-bearing scaffolding the original design never
drew; the second pass should give "feeding a live map from outside"
a designed front door (and it is the same door phase 7's rewiring
control surface will need).

## Phase 3 — the build path

**The docs' own shim example contains an alignment bug.** Docs 007 and
issue 302 show shims loading arguments with pointer casts —
`int a = *(int *)t->in[0]` — and the task layout packs values back to
back with no padding, so a double following an int sits at a
misaligned address and the cast is undefined behavior off x86. The
generator emits memcpy loads instead. Second pass: either align the
task's value area per member (wastes a few bytes, allows casts) or
bless memcpy loads in the doc; either way the example code should not
model the bug.

**"Delete the hand shims" assumed shims lived in product code; they
lived in tests.** Issue 302's instruction to delete issue 207's shims
could not be followed literally — the hand shims were written inside
phase 2's tests and demo, wrapping boxes that mutate test counters,
which no generated box could reach (generated boxes cannot see a test
binary's globals). They stayed as marked harness instrumentation.
Second pass: decide where instrumented test boxes live — likely a
designated test-box source directory with extern counters, so even
harness shims are generated.

**The registry includes box sources whole, which works and has
sharp edges.** The generated file `#include`s each box .c so types
are visible and boxes inline into shims. Consequences: box sources
share one translation unit (name collisions across files are link
errors moved to compile time; statics are shared-visible), and a box
source cannot be compiled standalone. Fine at this scale; the second
pass should either bless this ("box sources are one compilation
unit, by design") or emit extern declarations and compile separately.

**LuaJIT as the generator's language was frictionless.** Pattern
matching plus a hand brace-walker handled everything the ground rules
allow; the whole generator is one readable file. The one real parsing
bug of the pass (a function name preceded by `*` with no space) was a
pattern subtlety, found by the shell tests in minutes. The
fail-don't-guess posture — refuse plain structs, comma fields,
unnamed parameters, string returns — turned every ambiguity into a
one-line rule instead of a guess.

## Phase 4 — the pull path and configuration

**The statics table is two designs wearing one name, and the docs
never chose.** Docs 008 says entries are text "read into bytes at the
moment a slot claims it" and that two slots of different types may
read one entry each their own way; issue 405 says entries can be
byte-written at runtime, size-checked. Text-at-claim and
mutable-bytes cannot both be the table: per-claim parsing on the hot
path is absurd, and byte-writes have no meaning against an entry that
is authoritative text. First pass chose bytes-at-bind (first binder's
type shapes the entry; same-size sharing only; mutation is
byte-level), recorded the narrowing, and moved on. The second pass
must pick one story in the docs — likely "text is the load-time
serialization; bytes are the runtime truth" — and design two-type
sharing away or in, explicitly.

**A box cannot reach the engine, and issue 405 quietly requires it
to.** Boxes receive values and return one; they hold no map pointer.
"Make the statics write reachable from a box" therefore forced an
ambient global — the active map — with a one-live-map-per-process
assumption the design never states. Second pass: either bless the
ambient map (and say one-per-process out loud), or give boxes an
explicit capability argument, which touches the shim shape. This same
door is where phase 7's control surface will want to enter.

**Static binding needs the slot's type, so hand-built maps cannot
bind statics.** Text becomes bytes only when the slot knows its type
name, which only registry placement supplies. The scaffolding
construction calls therefore cannot express phase 4 maps without a
type-name backdoor (one test grants a name by hand). Harmless — the
loader always places by name — but the issues present construction
calls and statics as freely composable, and they are not.

**Gather cost is paid wherever the task is assembled — including
outside the pool.** The demo's first timing of the gather tax
measured nothing, because seeded values assemble their tasks on the
seeding thread before the workers ever release: the gather ran, at
full cost, on the main thread, outside the timed window. Issue 702
says "charge gather time to the pulling station"; the first pass adds:
and note that the paying *thread* can be any deliverer, seeders
included. Phase 7's attribution should follow the station, not the
worker.

**The engine-side type classifier duplicates the generator's.** The
statics reader must classify type names (int-like, unsigned-like,
float, string, struct) and so must the Lua generator; two lists must
now agree forever. Second pass: emit the classification into the
registry (a kind byte per param/field, which field tables already
half-carry) so the engine never parses a type name at all.

**Optimizers eat demonstration code.** Two demo scenes were silently
hollowed out at -O2: a burn loop whose result folded to zero was
deleted whole, and a deliberately endless recursion was tail-call
optimized into a loop that overflows nothing. Both needed
compiler-visible consumption to stay real. Lesson for any measured
demo: prove the cost is still there (the numbers made it obvious).

## Phase 5 — routing kinds

**The port-gap rule was a plain-only assumption wearing a principle's
clothes.** Phase 2 decided ports must be wired in order with no gaps
("a silently invented port is a wire the author did not draw") — and
that was correct while every station had one port whose index meant
nothing. A comparator's port index *means* an outcome, and wiring
only "greater" is a legitimate map, so the rule became per-kind:
plain wires port zero only, comparators up to three with empty
intermediates created freely, iterators unlimited. Lesson: a rule
justified by one kind's semantics should be stated as that kind's
rule, or the next kind pays to renegotiate it.

**"Fails at build time" appears three times in the design and is
impossible each time.** Issues 305, 502, and 503 all want the
comparator-without-compare refusal at build; the generator never sees
a map, so there is nothing at build time to check against. Issue 503
even notices this and re-lands the check at load. The second pass
should stop promising build-time map checks anywhere, and say once:
wire checks happen at the last moment the information exists, which
is placement/load.

**The byte-order lie needed care to demonstrate honestly.** memcmp on
a little-endian machine compares the *low* byte first, so the demo's
first "what bytes would say" agreed with the correct answer by
accident. The canonical wrong answer is reading the values as
unsigned words (sign bit as top bit). A tiny thing, but exactly the
kind of demo detail that quietly proves the wrong claim if unchecked
— the demo now asserts that the lie lies.

**Comparator threshold rides inside the task as a hidden extra
input.** The shim reads in[0..n-1]; routing reads in[n]. It works
cleanly, but the task's n_in now means "slots" not "box parameters",
and any future code that iterates task inputs to feed a box must know
the difference. Worth one sentence in docs 003; nothing needed it
yet.

## Phase 6 — the map file

**The format had no comment syntax, and the dump requires one.**
Issue 703 wants derived facts written "as comments" in a dump that
reads back as a map; docs 008 defines no comments. Added `#` to end
of line, with the doc updated. A format designed alongside its dump
would have caught this on day one — the lesson is to design a
serialization and its round-trip together.

**Names were discarded exactly when they became necessary.** The
docs are proud that the name table dies when loading ends — and then
issue 703 needs a dump that writes names, and any live view needs to
speak them. The load now retains station names on the map (the
resolving table still dies; keeping-for-speaking and
keeping-for-resolving are different needs). Second pass: state name
lifetime explicitly — resolved once, retained for diagnostics.

**Issue 602's pass-split doesn't survive contact with gather lines.**
"Apply the input lines" in the first pass cannot apply a gather line:
its source is a *name*, and names may point forward — the very
problem two passes exist for. Statics bind in pass one, gathers in
pass two. The doc should split input lines by kind, not by section.

**Seeding rules needed one word changed.** "No ring inputs and
output feeds a ring buffer" excludes an input-less sink, which has no
output at all yet plainly should run once. Landed as "no ring inputs
and not gathered from". Also: an iterator fed by seeds alone
receives exactly as many values as there are seeded sources — maps
wanting an iterator to actually iterate need multiple value sources
or a loop, which no doc mentions and the everything-map had to be
designed around.

**The whole-map validation runs O(stations²) walks.** Every station
scans every other station's ports and slots to learn who pushes and
pulls it. Fine at map scale (hundreds), quadratic all the same;
phase 7's rewiring wants these checks per-edge anyway. Second pass:
build the reverse index (who feeds whom) once and let validation,
seeding, and rewiring all read it.

**Where phase boundaries bit: the demo wanted rewiring one phase
early.** The capstone's edit-between-runs scene is a restart; true
live rewiring is 704. The scene works, but the natural demo
("change the wire *while* it runs") had to wait — a sign 704 might
belong in phase 6, or the capstone demo in phase 7.

## Phase 7 — seeing inside it

**Rewiring needed a delivery-path retrofit the "quietly preparing"
list missed.** Issue 704 celebrates the design decisions that made
rewiring cheap (indices, immovable stations, connection-time cycle
checks) — but delivery walked live destination lists with no lock,
and a removed wire would have left a walker holding a freed node.
Delivery now snapshots the chosen port's destinations under the
station's mutex before delivering. Second pass: design the snapshot
in from phase 2, or state that destination lists are immutable until
phase 7 makes them otherwise.

**A slow box is not a bottleneck; the demo had to learn it live.**
The bottleneck scene split a slow station's load across two stations
and throughput barely moved — because the pool was already running
the "bottlenecked" station's invocations on every core. In this
engine, per-station concurrency means hot *boxes* parallelize away;
what actually bottlenecks is contention (one mutex everyone must
pass) and serial chains. Docs 002/702 talk about "the station too
many things point at" — right instinct, but the second pass should
say plainly: run counts and box time locate cost, mutex wait locates
bottlenecks, and a slow box alone is neither.

**"Fatal on nothing-to-seed" and "outside delivery is legitimate"
contradict.** Validation warns that an unfed buffered station may be
fed from outside — and then the seed sweep aborts any map with no
seedable stations, which is exactly what a fully outside-driven map
looks like. The rewiring test hit it immediately. Second pass:
either an explicit map-level marker ("this map is driven from
outside"), or downgrade empty-seed to the same loud warning.

**Refusal-versus-death is now split across two regimes, on purpose.**
Load-time errors abort; runtime rewiring refusals return -1 with a
message. Deliberate (a control surface must not kill the plant), but
the -1 is ignorable, and an ignored refusal is a silent divergence
between intended and actual shape. Second pass: consider a refusal
log the dump includes, so ignored refusals leave a trace.

> **Settled, and not the way this paragraph guessed.** The two regimes
> became one and the debt was paid off rather than serviced: an
> invalid operation ends the program. The refusal still *travels*
> rather than dying where it happens, so a caller collecting faults
> can still collect them — but there is no surviving path in which an
> ignored refusal leaves a program running that somebody believes they
> just edited. The refusal log this suggested would have been a way to
> notice the divergence afterwards; there is no divergence to notice.
> The argument that killed the -1 was the one this paragraph made and
> then set aside, given force by a program becoming able to edit
> another: a box's caller is a *wire*, and a wire ignores everything
> it is not attached to. See
> [106](../issues/completed/106-stopping-on-purpose.md).

**Statistics attribution wants a design pass.** Box time is recorded
by the shims into the active map via a process global; gather time is
charged to the puller at the call site; produced-counts ride the
walk. It works and measures true, but three mechanisms feed one
table, and the active-map global quietly forbids two maps in one
process — the same restriction the statics back-channel already
imposed. One engine-context pointer through the task would fix both.

**The dump cannot re-serialize mutated statics.** Bytes cannot be
turned back into text without the per-entry type the table refuses
to store — the text-versus-bytes decision again, now costing dump
fidelity. The dump says so in a comment, which is honest but is
still a hole in "the only accurate description of the program".

## Cross-cutting lessons

**The recurring thread: the engine has no "context", and three
features hit the wall.** A box receives values and nothing else. So
the statics write needed an ambient active-map global (phase 4), the
statistics needed the same global (phase 7), and both quietly impose
one-live-map-per-process — a restriction no document states. The
single highest-leverage second-pass change is an engine-context
pointer riding the task (the task already carries the station index;
it could carry the map), which dissolves the global, permits several
maps in one process, and gives phase 7's control surface a front
door.

**The second thread: the outside world is undesigned.** Termination
assumes nothing pushes from outside; every trickle test, every demo
visual, and any control surface pushes from outside. The
submitter-registration API was invented in phase 1 and leaned on by
every later phase; the nothing-to-seed rule then refused fully
outside-driven maps outright in phase 7. Feeding, observing, and
steering a running map from outside is a real design surface the
docs treat as a footnote. Design it once: registration, an
outside-driven map marker, the refusal log.

**The third thread: the statics table never decided what it is.**
Text-at-claim versus bytes-at-bind surfaced in phase 4 (mutation),
phase 6 (loading), and phase 7 (the dump cannot re-serialize mutated
entries). One decision, three phases of interest. Decide it first in
the second pass; everything else in the table follows.

**Docs that specify mechanism where they should specify invariants
aged worst.** The termination re-scan defends a lock discipline the
implementation doesn't have; "claim one value from each" under the
mutex almost prescribed running user code under a station lock; the
first-pass split of loader input lines didn't survive gather's
forward references. The docs that specified *invariants* — a station
runs when and only when its slots are full; nothing polls; values in
flight are copies — survived contact with every phase untouched.

**What worked, and should be kept exactly as it is.** Fail-don't-
guess turned every ambiguity into a one-line rule instead of a
guess, and made the refusal messages testable word for word — the
error gallery is arguably the product's best surface. The registry
as single source of truth held perfectly: not one type bug crossed a
wire all pass. Dispatch tables over conditionals kept every "add a
kind" change to a row. And the demos-as-measurement-instruments rule
caught four genuine design insights that tests alone missed: the two
kinds of backlog, gather cost paid at assembly, the slow-box-is-not-
a-bottleneck property, and the endianness subtlety in the byte-lie
scene. Demos that only display would have caught none of them.

**Convention frictions, small but real.** The launcher's `phase-*`
discovery pattern and the file-index naming rule collide at every
demo script; `.info.md` per source file doubles up on header/impl
pairs (interface + internals worked better); and building staged
intermediate code states (busy-return before sleeping) would have
been theater — staging tests, not code, is the honest unit.

For live numbers — test counts, line counts, commit history — run
`make test` and read `git log --oneline`; this report deliberately
stores none, so it cannot go stale.

## Second-pass priorities, most valuable first

1. Decide the statics model (text vs bytes) — unblocks mutation,
   loading, and dump fidelity at once.
2. Thread an engine-context pointer through the task — kills the
   active-map global, allows multiple maps, opens the control door.
3. Design the outside-world surface: submitter registration as a
   first-class concept, outside-driven maps, a refusal log the dump
   includes.
4. Build the reverse index (who feeds whom, who pulls whom) once —
   validation, seeding, and rewiring all currently re-derive it
   quadratically.
5. Emit type classification from the generator instead of keeping a
   twin classifier in the engine.
6. Decide task value-area alignment (memcpy loads versus aligned
   casts) and make the docs' example code match.
7. Rewrite mechanism-docs as invariant-docs where the pass found
   them over-specified; add the push-loop termination note.
8. Finish the documentation set's last two widgets and deep links.
9. Restructure issues so one mechanism is one issue with staged
   tests, and design formats beside their round-trips.
