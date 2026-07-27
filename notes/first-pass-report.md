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

(appended as built)

## Phase 3 — the build path

(appended as built)

## Phase 4 — the pull path and configuration

(appended as built)

## Phase 5 — routing kinds

(appended as built)

## Phase 6 — the map file

(appended as built)

## Phase 7 — seeing inside it

(appended as built)

## Cross-cutting lessons

(appended at the end of the pass)
