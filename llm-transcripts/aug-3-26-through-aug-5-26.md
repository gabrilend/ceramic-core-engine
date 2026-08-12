# Conversation Summary: ebd500b5-7b20-437d-a9c4-fd93380f69b0

Generated on: 2026-08-12 11:45:58

--------------------------------------------------------------------------------

### User Request 1

hello claude can you examine the issue files in this directory and sort them
into proper locations and formats?


--------------------------------------------------------------------------------

### Assistant Response 1

I'll look at the issue files first before touching anything.


There are loose, unnamed issue files in the project root. Let me read them.


I've read all five loose notes in the project root and checked them against what
already exists. Here's what I found before I move anything:


| loose file | disposition |

|---|---|

| `issue-2` | already absorbed — quoted verbatim inside the demos-as-word-problems issue in phase 7 |

| `new-issue-please-sort` | gathering at worker pickup — already analyzed as "option 3" in the gather-timing implementation note, but **no issue file exists for it** |

| `update-to-issue-704` | growing the station table — the rewiring issue explicitly said this "deserves its own issue"; none exists |

| `new-issue-4` | slideshow + transcripts as a book in the HTML set — extends the documentation issue, not covered by it |

| `new-issue-3` | in-browser canvas that draws maps and downloads map + C source — nothing covers this at all |


One decision is genuinely yours before I write:


**[Asked the user]**


*Q1 — Canvas phase:* The browser canvas note describes a whole authoring
surface — place boxes, draw wiring, fill statics, download a map file plus the
C to compile it, with a drawer of bundled functions. That is a different cluster
of functionality from anything phases 1-7 cover. Where should it land?

- New phase 8 — the workbench (Recommended) — Opens a phase for authoring
  tools that run outside the engine. I'd write it as issue 801, and add the
  phase to the roadmap and table of contents. Leaves room for the editor to grow
  siblings later (a map validator in the browser, a shareable link format)
  without cramming them into the observability phase.

- Phase 7, alongside the HTML docs — Treats the canvas as another interactive
  piece of the documentation set, numbered after the slideshow issue. No roadmap
  change. Risks conflating 'pages that explain the engine' with 'a tool that
  authors programs' — they share a delivery medium and nothing else.

- Hold it as a note, not an issue — Files it under notes/ as a design sketch
  rather than a ticket, because it is large and speculative. Nothing gets
  scheduled until you decide it is real.

→ **Selected:** New phase 8 — the workbench (Recommended)


*Q2 — Raw notes:* What should happen to the five raw notes in the project root
once their content is preserved? The existing convention (from the demos issue)
is to quote the note verbatim inside the issue under a 'The note that started
this' heading.

- Quote verbatim, then delete the scrap (Recommended) — Follows the convention
  already set. Your words survive inside the issue file, which is the immutable
  record; the root stays clean. These files are untracked, so the issue quote is
  the only record either way.

- Quote verbatim, leave the scraps in place — I write the issues and touch
  nothing in the root. You delete them yourself once you have read the issues
  and agree they captured everything.

- Quote verbatim, move scraps to notes/ — Keeps the originals as dated
  artifacts in notes/ alongside the first-pass report, with their original
  filenames. Costs a little clutter; gains a record of what the inbox looked
  like.

→ **Selected:** Quote verbatim, then delete the scrap (Recommended)


Phase 8 it is. Let me gather the specifics each issue needs to be accurate.


Now I have what I need. Writing the four issues.


Now the roadmap, table of contents, and the one piece of the canvas note that
belongs to the documentation issue.


Before deleting the originals, let me verify each one's words survived into its
issue word-for-word.


All 1,771 words accounted for. Removing the scraps.


Sorted. Here's what landed where.


## Four new issue files


**`issues/407-gather-at-pickup.md`** — from `new-issue-please-sort`. Letting
the worker that runs a box do its own gathering, rather than the thread that
scheduled it. This one already had a design document written about it — the
gather-timing implementation note lays out three possible answers and recommends
shipping the one that exists — but no ticket existed for the work itself. The
note asked for what that document calls option 3, and the document independently
concluded that option 3 is the right second one to build, so the issue is
written as the follow-through rather than as a fresh proposal.


Your own note has a line in it — *"Or maybe we're doing that automatically? I
forget"* — that has an answer: the generated shim already unpacks the task's
bytes into typed arguments, which is exactly the layer of indirection you were
reaching for. So the fallback you sketched (a second pointer in the task struct)
isn't needed; the hook is already there.


**`issues/708-growing-the-station-table.md`** — from `update-to-issue-704`.
The rewiring issue said adding a station "deserves its own issue"; this is it,
built around your double-buffer-plus-hand-raising design.


One departure, and it's the reason the file says so out loud: a station record
carries its mutex *inside itself*, and the station header writes down as an
invariant that growing must never move a station. A mutex is identified by its
address — a thread blocked on the old one is not woken by an unlock at the new
one — so copying the array to a bigger allocation would produce a table full
of locks nothing can reach. Your note actually anticipates the escape without
quite taking it: you wrote about *moving the pointer in the box station list*,
which is a list of pointers to stations rather than a list of stations. Taking
that literally fixes it. Fixed-size runs, appended rather than copied, and
nothing existing moves at all. The hand-raising ring is adopted exactly as you
wrote it, and the issue points it at the pool's task ring too, where it pays
most.


**`issues/709-slideshow-and-transcripts.md`** — from `new-issue-4`. Both
halves depend on a fix the HTML documentation issue already names as
outstanding: discovery is a fixed list of directories read one level deep, and
`llm-transcripts/` is invisible to it — invisibly invisible, since a missing
page leaves no gap to see.


**`issues/801-browser-workbench.md`** — from `new-issue-3`, opening phase 8.


Your question about running a C compiler in JavaScript has a real answer, and
it's in the file. Clang compiled to WebAssembly is a working thing — tens of
megabytes, but a download rather than an install. What doesn't follow is the
native binary: a compiler has to know its target's instruction set, calling
convention, C library headers, and something to link against, so shipping
executables means shipping a full sysroot per platform and picking the right
one, plus that platform's pthreads. WebAssembly output is tractable instead,
except that real threads in wasm need shared memory, which browsers only grant a
cross-origin-isolated page — two response headers, which is a hosting concern
rather than a server. So: source download is the deliverable, wasm-in-the-page
is the interesting stretch, native binaries is a promise worth not making.


The strongest thing in that issue is a dependency: once the generator is a
standalone C program that reads text and writes text, it compiles to wasm
without ceremony, and the page gets *the build's own parser* instead of a
JavaScript one that would eventually disagree about what a box is.


## Everything else


- `issue-2` needed no new file — it was already quoted verbatim inside the
  demos-as-word-problems issue.

- The first three lines of the canvas note were a different request from the
  rest (the docs should be more artistic, keep the colour scheme). That went
  into the HTML documentation issue's intended behavior, where it belongs, and
  the workbench issue says where it went.

- Phase 8 added to the roadmap and the table of contents.

- All five scraps deleted from the project root — after a script checked that
  all 1,771 of your words survived into the issues word-for-word, since
  re-wrapping made a line diff useless. It passed on all five.


## Two things I did not do


**I did not regenerate the HTML.** The four new issues have no pages. The
generator finds issues by listing the directory, so it's one run and no editing
— but the demos issue says explicitly that the generator is mid-edit under the
documentation issue and running it now would mix unfinished work into the
output. Say the word when that's settled.


**I did not commit.** The two files I edited that were already tracked — the
table of contents and the HTML documentation issue — were both dirty before I
started, and the repo is carrying a large body of somebody's in-progress work:
regenerated HTML across seventy pages, rewritten demos, four new demo-engine
files, and a re-opening of the static-slots and statics-mutation issues that
pulled them back out of `completed/`. My four new files are cleanly separable,
but committing means deciding what to do about the rest. Whose is that work, and
do you want me to stage only the four issues plus the roadmap and contents
edits?


## The open questions


The sort is done; these are not. Sixteen questions surfaced while writing, all
recorded in their issues:


- **407** (3) — is the strategy per-station, per-map with override, or chosen
  by the loader from the map's shape; what happens to a task whose gather fails
  halfway when the task is already the worker's; does the sleeper count need to
  tell a worker inside a gather chain from a worker inside a box.

- **708** (4) — the loader discards its name-to-index table, so how does
  anything name a station added later; is the run size fixed or derived; does
  the hand-raising ring have a bound before it becomes a livelock with good
  manners; is an unwired station an error, a warning, or ordinary.

- **709** (4) — generated frames or live drawing; does the slideshow narrate
  the engine or the project; whole transcripts or edited ones; does a build
  publish whatever was said that day.

- **801** (5) — is the workbench documentation or its own thing; does a map
  file carry layout as comments; what's in the bundled drawer; is a wasm engine
  build in phase 8 at all; does an in-progress map survive a closed tab.


The one I'd start with, because it decides whether phase 8 is one issue or
several: **what's in the bundled drawer?** You described it as functions
interfacing with software you've made and hardware you've worked with — that's
a list only you can write, and its length is the phase's size.


--------------------------------------------------------------------------------

### User Request 2

<local-command-caveat>Caveat: The messages below were generated by the user
while running local commands. DO NOT respond to these messages or otherwise
consider them in your response unless the user explicitly asks you
to.</local-command-caveat>


--------------------------------------------------------------------------------

### User Request 3

<command-name>/model</command-name>

            <command-message>model</command-message>

            <command-args></command-args>


--------------------------------------------------------------------------------

### User Request 4

<local-command-stdout>Set model to [1mOpus 5 (1M context)[22m and saved as
your default for new sessions</local-command-stdout>


--------------------------------------------------------------------------------

### User Request 5

alright can you give me a summary and critical path through all the available
issue files? Make sure you touch all of them.


--------------------------------------------------------------------------------

### Assistant Response 5

Let me read every issue file rather than summarize from memory. Starting with
the open ones I haven't read in full.


Now the forty completed issues — titles and what each one established.


I've read all fifty-two issue files — forty completed, twelve open. Here is
the whole thing.


## First, the vocabulary everything below is written in


The engine builds programs out of ordinary C functions wired together, where the
wiring decides what runs and when. Seven nouns carry the whole design:


| term | what it actually is |

|---|---|

| **box** | a plain C function the map author writes. It takes arguments, returns one value, and remembers nothing between calls |

| **station** | one *placement* of a box in a running program. Holds its own mutex, its input ports, its output ports, a kind (plain, comparator, iterator), and a cursor. Two stations can place the same box and be wired completely differently |

| **input port** | where one argument comes from. Three sources exist: a **ring buffer** that arrows push into, a **static** value that is simply always there, or a **gatherer** that reaches upstream and pulls a fresh value on demand |

| **output port** | one exit from a station, holding a linked list of destinations. A destination is two 32-bit integers — a station index and a slot index — never a pointer |

| **the claim** | the instant every input port has a value and one value is taken from each ring buffer under the station's mutex. This is when an invocation becomes real |

| **task** | one invocation, in one allocation: the shim to call, the producing station's index, the iterator's chosen port, the claimed input bytes, and a landing place for the return value. Everything in it is a *copy*, so it depends on nothing another thread can change |

| **the pool** | worker threads and a ring of task pointers. Its three unconditional steps are: pop a task, call one function pointer, hand it to the finish hook and free it. It knows nothing about stations, and that ignorance is load-bearing |


Two more that are generated rather than written: the **shim** is glue with one
uniform signature that unpacks a task's packed bytes into typed arguments and
calls the box; the **registry** is a table mapping a box's *name* in text to its
shim pointer, its parameter types and sizes, its return type and size, its exact
task size, and its comparison function if its return type has an ordering. Every
size in it is a `sizeof` expression the compiler computes, never a number the
generator worked out.


And the **map file** is a line-oriented text schematic: one line per station
giving its name, its box function, and its kind; input lines only for the
exceptional ports (a port is a ring buffer unless a line says otherwise); output
lines for the arrows.


---


## What is built — the forty completed issues


## Phase 1 — The pool (five issues, all done)


**The task queue as a ring of pointers that doubles.** One mutex, doubling when
the tail would land on the head, with the wrapped portion unwrapped during the
copy so contents stay in arrival order. Push and pop never block. It records its
own high-water occupancy and growth count, placed early so demos could report
measured numbers rather than constants.


**Worker threads and the run loop.** A fixed set of threads spawned at creation
and parked at a starting gate, released by a separate call — that gap is the
window where a map gets seeded. Each worker loops: take the oldest task, run its
call, hand it to the finish hook, free it. Thread count obeys the creation
argument, then an environment variable, then one per processor.


**Sleeping and waking.** A worker finding the queue empty registers itself
asleep and waits on a condition variable; a push wakes everyone. The sleeping
count is exact because the check, the registration, and the wait all happen
inside one hold of the queue mutex — the wait releases it atomically. Wakes
are treated as rumors: a woken worker re-checks before believing anything.
Measured at three milliseconds of processor time across four-fifths of a second
of near-idleness.


**Termination by the last sleeper.** The worker whose registration brings the
sleeping count to the worker total looks at the queue one final time; finding
nothing, it sets the stop flag and wakes everyone, and each worker returns on
its own. Shutdown is a broadcast, never a break-out. The issue records something
honest: the race it was written to repair *cannot occur* in the lock discipline
as built, so the re-scan survives as a final look rather than as a fix, and the
test meant to reproduce the race is unwritable.


**The phase 1 demo.** Four scenes: a fan-out burst doubling the queue six times
from eight cells; four uneven chains whose termination decision costs a
twentieth of a millisecond; the idleness measurement; and the same two hundred
thousand tasks at every power-of-two worker count, where the single queue
mutex's ceiling becomes visible as the throughput line flattens.


## Phase 2 — Stations and the push path (eight issues, all done)


**The station table.** One flat allocation of identical fixed-size records.
Slots, ports, and destinations hang off pointers; every cross-reference is an
index. Ports and destinations append at their list tails so wiring order
survives — which the dump's round-trip later leans on. The kind field and
iterator cursor were placed here and left inert until phase 5.


**Ring-buffer input slots.** Cells are exactly the size of the parameter the
slot feeds, so every write is a memory copy into a fixed offset with no
allocation on the hot path. Occupancy is head-differs-from-tail, answerable
under the station's mutex alone.


**Slot buffer growth.** When the tail would land on the head, the storage
doubles under the station's mutex and the wrapped portion is copied so contents
read contiguously from cell zero. Only the storage the slot *points at* is
reallocated — never the slot, never the station. Proven by flooding one slot
through seven doublings and finding every station address unchanged.


**The readiness check and claiming values.** Readiness walks the slots through a
dispatch table keyed on slot kind: ring buffers answer head-differs-from-tail,
gatherer and static rows answered "always" from the very beginning. Claiming is
a *second* table, where the gatherer and static rows are deliberately null,
meaning "resolved during task construction, outside the lock" — so user code
can never run under a station's mutex.


**The delivery walk.** Riding the pool's finish hook: choose a port through the
routing dispatch, then for each destination — lock, write, check readiness,
claim, unlock, build a task, push, next. A port wired to nothing discards, which
an unwired comparator branch later wants.


**The task struct.** One allocation laid out as struct, then pointer array, then
input bytes, then output bytes, sized exactly for the box and freed with one
call by the worker that ran it. Constructed *after* the station's mutex is
released, on the assembling thread's own time.


**Hand-built maps and hand-written shims.** Deliberate scaffolding, kept minimal
enough to be irritating — placement demanded every element size spelled out by
hand, which is precisely the tedium phase 3 deleted.


**The phase 2 demo.** Five scenes, including the two kinds of backlog side by
side — a slow single-input consumer piling tasks into the pool's ring while
its slot stays one deep, versus a pairing station's starved side growing its
buffer to four hundred waiting values. That distinction was not in the design
documents; the demo surfaced it and the documents were corrected.


## Phase 3 — The build path (seven issues, all done)


**The box-source parser.** A LuaJIT script. Box sources are designated by
*location* — anything under the boxes directory — so a box cannot exist that
the generator does not see. It blanks comments, string contents, and
preprocessor lines while preserving newlines, then stops at every top-level
brace and classifies what led to it: a struct typedef, a function (non-static
becomes a box, static is a private helper, a `__compare` suffix is an ordering
and never a box), or an error naming file and line.


**Shim emission.** One shim per box, each carrying a comment naming the
declaration it came from. Loads and stores inside a shim are memory copies
rather than pointer casts — the task's value area packs values back to back,
so a double following an int sits unaligned, and copying is what keeps that
defined on every platform. The issue's own example code used direct casts and
would have been wrong.


**The registry.** Type names ride along as text specifically for error messages,
because "four bytes versus four bytes" is not a message. Placement-by-name is
the moment phase 2's hand-supplied sizes became lookups.


**Struct field tables.** Name, offset, size, and kind per field, with nested
entries pointing into the shared table and fixed strings carrying their length.
Every offset an `offsetof`, every size a `sizeof`. Proven against the compiler's
own answers for a deliberately awkward padded struct with a seven-byte hole.


**Compare functions.** Every primitive some box returns gets a generated
three-way comparison; every author-written struct ordering gets a wrapper. Both
copy bytes into real typed variables first, because raw bytes lie — the tests
prove minus five below three where unsigned bytes disagree, and minus two below
one half where the float sign bit reads backwards.


**Build integration.** Regeneration whenever any box source or the generator is
newer. A failing generator writes to a temporary name and never moves it into
place, so the build can never compile against yesterday's registry.


**The phase 3 demo.** Shell-driven on a scratch copy of the box sources: one
ordinary function appended appears in the registry with real types and sizes and
not a single other edit.


## Phase 4 — The pull path (three of five done; two reopened, see below)


**Struct constants.** One generalized reader walking a field table and brace
text together, recursing into nested tables — never one parser per type. A
string field is a fixed-size character array: the bytes live inside the struct,
nobody owns anything, and a value longer than the field is fatal rather than
truncated.


**Gatherer slots and inline gathering.** A gatherer slot holds an upstream
station index; at task construction the upstream box runs *inline on the
assembling thread's own stack*, its value landing straight in the task. No pool,
no mutex on the source. This is the named exception to "boxes only run from the
pool", and it forces a rule: a gathered box must be safe on several threads at
once — open, read, close, never a kept handle.


**Gather chains and the cycle check.** Chains recurse inline; depth is recorded
on the map at wiring time. Every new gather wire runs a forward walk from its
proposed source, and reaching back to the destination refuses the edge naming
both stations, so the graph stays acyclic by induction. The push direction stays
unchecked and legal — and a finding the design never anticipated: a push loop
needs a finite companion input to ever stop.


**The phase 4 demo.** The file read both ways sits first: the frozen side keeps
reporting 111 after the file turns to 999 mid-run while the gathered side
follows.


## Phase 5 — Routing kinds (five issues, all done)


**Routing dispatch.** A three-entry table at step one of delivery and nowhere
else. Nothing else in the engine asks what kind a station is. Port creation
became kind-aware in the same stroke — intermediate ports are created *empty*
rather than refused, because wiring only some outcomes is legitimate and an
empty port discards.


**The comparator.** Placement gives it one slot more than its box has
parameters: the threshold, at the end of the slots array, typed to the box's
return value. The shim reads only the real parameters; the threshold rides the
task and is consulted *after* the box returns, on the delivery path. There is no
operator setting anywhere — every comparison operator is a fact about wiring.


**Three-way comparison on the delivery path.** The compare function is resolved
from the registry once, at placement, and stored on the station, so delivery
does one call and a switch on the sign.


**The iterator.** The cursor advances inside the readiness path, under the
station's mutex, at the moment a task becomes due; the chosen port is written
into the task and the box never sees any of it. Proven by twenty-four hundred
values from eight feeders landing exactly eight hundred on each of three ports.


**The phase 5 demo.** One comparator rewired six ways produces every comparison
operator in numbers, and then a seventh shape no operator can name: equals one
way, greaters another, lessers discarded.


## Phase 6 — The map file (six issues, all done — the capstone)


**The parser**, which reads and never constructs. **The loader's first pass**,
placing stations by name from the registry. **The second pass**, resolving
arrows by name including forward references — which is the entire reason there
are two passes — and type-checking each wire at the first moment both ends are
known, producing messages like *"head → wrong.1: box returns int, slot takes
double"*. **Validation that needs the whole map**, with failures collected and
printed together before one stop, because someone fixing a new map wants the
whole list. **The seed sweep**, enqueueing every station with no ring-buffer
inputs that nobody gathers from, through the very same task-construction path
delivery uses — one way a task comes into existence, not two.


**The phase 6 demo.** One compile at the top and never again: the same binary
runs three map files into three behaviours, then a single text edit removes a
hop and another changes a threshold, with the compiler untouched throughout.


## Phase 7 — Seeing inside it (five of eight done)


**Buffer growth reporting**, with the phase 2 lesson written into the output:
slot piles mean uneven inputs, ring piles mean slow consumers, two different
diagnoses. **Per-station statistics** — counts always on as atomics costing a
fetch-add, timing only when compiled in, with the report offering three
orderings as a dispatch table because the interesting station is a different one
under each. **The map dump**, which walks the live station table and never any
remembered file text, and required giving the format a comment syntax and
required loaded maps to retain their station names, which the design had proudly
discarded. **Runtime rewiring**, with check and change under one lock, proven by
fifty rounds of two threads drawing individually-legal jointly-cyclic edges with
exactly one refused every round.


**The phase 7 demo** caught a finding worth repeating: a slow box alone is not a
bottleneck here, because the pool already runs one station's invocations on
every core. The reports exist to show who pays, not to promise speedups.


---


## What is open — the twelve


| issue | phase | what it changes | state |

|---|---|---|---|

| **What an input port is** | 2 | one port record with room for all three sources | not started |

| **The output station** | 2 | a map declares where its results come from | not started |

| **The generator, in C** | 3 | removes LuaJIT from the build path | not started |

| **Static input values** | 4 | moves statics from a map table onto the port | **reopened** |

| **Changing a static while running** | 4 | kills the process-wide map pointer | **reopened** |

| **Gathering at pickup** | 4 | the worker gathers, not the scheduler | not started |

| **Boxes with several output ports** | 5 | return-struct fields fan to different ports | not started |

| **The HTML documentation set** | 7 | generated cross-linked site | **in progress** |

| **The demos as word problems** | 7 | thirty-five scenes rewritten | **all but one step done** |

| **Growing the station table** | 7 | add a station to a running map | not started |

| **Slideshow and transcripts** | 7 | an introduction, and the logs as a book | not started |

| **The browser workbench** | 8 | draw a map instead of typing it | not started |


Two of these were **reopened** rather than newly written, and the reason is the
most interesting thing in the whole set. Both were built, both work, and both
are wrong about *where the value lives*.


The statics table is a numbered table held on the map behind one mutex. Reaching
it from inside a box landed as a bare-name call against a process-wide "active
map" pointer, because a box receives only values and has no handle to anything.
**That pointer is the singleton.** It exists for this one feature plus one
optional timing hook, and nothing else in the engine needs it — and it is the
entire reason a process can hold exactly one map. A feature the design already
distrusted as a back channel was quietly charging the whole engine its ability
to compose.


---


## The critical path


Hard blockers only — X genuinely cannot be built before Y:


```

  ┌─ THE SPINE ─────────────────────────────────────────────────┐

  │                                                             │

  │  what an input port is                                      │

  │        │  (gives the port room to hold its own value)       │

  │        ▼                                                    │

  │  static values move onto the port                           │

  │        │  ("or this has nowhere to land")                   │

  │        ▼                                                    │

  │  changing a static from outside the graph                   │

  │        │  ── deletes the process-wide map pointer ──        │

  │        ▼                                                    │

  │  TWO MAPS IN ONE PROCESS                                    │

  │        │                                                    │

  └────────┼────────────────────────────────────────────────────┘

           │

           ├──► the output station ──► boxes with several output ports

           │         (a map has a named result)   (a box catches up to a map)

           │                    │

           │                    ▼

           │            MAPS AND BOXES ARE THE SAME KIND OF THING

           │

           └──► packaging: handing the engine to someone else



  ┌─ THE PARALLEL TRACK (no dependency on the spine) ───────────┐

  │                                                             │

  │  the generator, in C ──────────► the browser workbench      │

  │       (build needs only cc)        (page and build share    │

  │                                     one parser, via wasm)   │

  │                                                             │

  │  HTML docs: walk directories ──┬──► close the demos issue   │

  │                                └──► slideshow + transcripts │

  │                                                             │

  │  growing the station table  ─── unblocked already (rewiring │

  │                                 landed the lock it needs)   │

  │                                                             │

  │  gathering at pickup ────────── unblocked, but should       │

  │                                 follow the port record      │

  └─────────────────────────────────────────────────────────────┘

```


**The spine is three issues long and every link is stated in the issues
themselves.** The statics issue's first implementation step is "give the input
port room to hold its own value" — which is the port-record issue's first
step, word for word. The mutation issue opens its steps with *"Follow issue 401:
the value moves to the port first, or this has nowhere to land."* These three
are not three pieces of work that happen to be related; they are one piece of
work that got written down three times as its shape became clear.


**What finishing that spine actually buys.** Right now a static value lives in a
numbered table on the map, and a box that wants to write one reaches it through
a global pointer to "the active map". Delete that global and three things become
possible at once that are impossible today: a process can hold more than one map
(which is what makes a map usable *as a box inside another map*); the claim
takes one lock instead of two, so the static half of an input set becomes as
mutually consistent as the buffered half already is; and the engine stops having
any map-level mutable state at all. Two of the three phase-7 documents and the
packaging note all name this pointer as the thing standing in their way.


**The cheapest high-leverage item is not on the spine.** The documentation
generator discovers documents by consulting a fixed list of directories, each
read exactly one level deep, and a directory nobody told it about is missing
from the site *silently* — the missing page leaves no gap to see. Replacing
that with a downward walk is a small change to one Lua script, and it is the
sole blocker on closing the demos issue (which is otherwise finished —
thirty-five scenes, a hundred and forty-two justified correspondences, four live
steerable panels) and the sole blocker on starting the slideshow and transcript
work.


**The workbench has exactly one hard prerequisite**, and it is worth naming
because it is not obvious: the canvas has to know each box's parameter types and
count in order to refuse an illegal wire while it is being dragged. If the page
parses C in JavaScript, there are then two opinions about what a box is, and
they will eventually disagree — at which point the page hands people maps that
do not load. Once the generator is a standalone C program that reads text and
writes text with no dependency on the engine, it compiles to WebAssembly without
ceremony and the page gets the build's own parser. So the generator translation
is not merely about dropping LuaJIT; it is what makes the workbench honest.


**Two issues converge on something neither one claims alone.** The port-record
issue introduces a fourth tag value, **none**, meaning unconfigured — a
station comes into existence with every port unset, cannot become ready, and
becomes runnable as its ports are given sources. The station-table issue makes
it possible to add a station to a running map. Together those are a map
assembled from nothing while the program runs, which is the shape the workbench
would eventually want to push edits into.


## If you want one order to work in


1. **Walk the documentation directories.** Small, unblocks two issues, and lets
   the site show the four new tickets.

2. **The port record, then statics onto ports, then mutation from outside.** The
   spine. Three issues, one shape, and the payoff is the singleton's deletion.

3. **The generator in C.** Independent of everything above, and the workbench's
   only hard prerequisite.

4. **The output station, then multi-output boxes.** These are what "two maps in
   one process" is *for*.

5. Everything else — gathering at pickup, growing the station table, the
   slideshow, the workbench — is unblocked at that point and can go in
   whatever order interests you.


---


## The unfinished part


Sixteen open questions are recorded across the four issues I wrote yesterday,
and none has been answered. Per your own rule those issues are in progress
rather than delivered, and I said so at the time. The one that gates the most is
still the same one: **what is in the bundled drawer of functions the workbench
ships with?** You described it as things interfacing with software you've made
and hardware you've worked with — that's a list only you can write, and its
length decides whether phase eight is one issue or five.


I can fold this summary into the documentation set as a document rather than
leaving it in a terminal, if that's useful — it would want to be generated
from the issue files rather than written by hand, though, or it becomes the
second thing that has to be maintained in parallel.


--------------------------------------------------------------------------------

### User Request 6

Okay let's iterate through the critical path until we're done. Let's go one
issue at a time - for each issue, I want you to first describe it as if I hadn't
read it and I know what soramech is. Connect everything to the core terms of the
project, so I know what touches what. Then, describe the current behavior,
intended behavior (two short paragraphs), and the intended implementation (one
paragraph, no code). I will offer corrections as I see them.


--------------------------------------------------------------------------------

### Assistant Response 6

Starting at the head of the spine — the longest hard-dependency chain, where
each issue genuinely cannot be built before the one above it. The documentation
directory walk is off-spine and can slot in any time you like.


---


# Issue 1 of the critical path — What an input port is


## What it is, and what it touches


Every argument a box function receives has to come from somewhere, and soramech
has exactly three somewheres: a **ring buffer** that upstream stations push
values into, a **static** value that just sits there and is always available, or
a **gatherer** that reaches upstream and runs another box on demand to produce a
fresh value. Those three were designed in three separate issues, months apart in
project time — ring buffers in phase 2, statics and gatherers in phase 4 —
and each one designed its own idea of what an input needs to remember.


The result is that a station's per-input record is a single fixed struct
carrying the union of three designs that never met each other:


| field | type | who uses it |

|---|---|---|

| `kind` | `unsigned char` | all — says which of the three this input is |

| `elem_size` | `int` | all — bytes of the parameter this input feeds |

| `storage` | `void *` | ring buffer only — the cells |

| `capacity` | `int` | ring buffer only — how many cells |

| `head` | `int` | ring buffer only — where reads come from |

| `tail` | `int` | ring buffer only — where writes go |

| `source` | `int` | gatherer only — index of the upstream station to pull from |

| `static_id` | `int` | static only — which numbered entry in the map's statics table |

| `type_name` | `const char *` | all — the parameter's type as text, from the registry |

| `growths` | `int` | ring buffer only — doublings so far |

| `high_water` | `int` | ring buffer only — deepest the backlog ever got |


A gatherer input carries a `storage` pointer that is always null, a `capacity`,
`head`, and `tail` that are always zero, and growth counters that will never
move. A static input carries all of that plus an unused `source`. Roughly half
of every input record is dead weight determined by which of three tags it wears.


**A naming collision you should know about before reading any of this in the
source.** The code calls this record a *slot*. The vocabulary the design settled
on later — written down in the gather-timing note — reserves "slot" for the
transient bytes one value occupies inside one task, and calls this standing
thing on the station an **input port**. So: a station has ports, a task has
slots. The source has not caught up, and renaming it touches the station header,
delivery, gathering, statics, the loader, the dump, and the stations-and-slots
document, so it is deliberately being left as a separate deliberate act rather
than smuggled in here.


**What this record touches, which is nearly everything on the push path.** The
readiness check walks these records through a dispatch table keyed on the tag to
ask "is there a value here?" — ring buffers answer *head differs from tail*,
gatherers and statics answer *always, without looking*. The claim walks them
through a *second* dispatch table to take one value from each, where the
gatherer and static rows are deliberately null, meaning "resolved later, during
task construction, outside the station's mutex." The delivery walk writes into
the ring-buffer ones. The gather module reads the `source` field to know which
station to run. The statics module reads `static_id` to index the map's table.
The loader fills all of them at load. Runtime rewiring repoints the `source`
field on a live map. The dump reads them back out to reconstruct a map file. And
the generated registry supplies `elem_size` and `type_name` at placement, which
is why hand-placed stations cannot bind statics — nobody told them their
types.


**Why this is the head of the critical path.** The statics-onto-ports issue and
the mutation issue both need a port that can hold its own value, and this is the
record that would hold it. Their first implementation step and this issue's
first implementation step are the same sentence.


## Current behavior


Converting a port from one source to another is surgery rather than a setting.
Turning a ring-buffer port into a gatherer means freeing its cell storage,
zeroing its head and tail, writing the upstream index into `source`, and
re-tagging it. Doing that on a running map means doing it while another thread
might be inside the readiness check reading the very same record — so the
capability that runtime rewiring built for gatherers is a repoint of the
`source` field only, never a change of kind.


There is also a rule the loader enforces that exists purely because the record
cannot express the situation cleanly: a station that anybody gathers from may
have no ring-buffer inputs at all. The reason is sound — gathering runs the
upstream box inline, right now, on the calling thread's stack, and a ring buffer
that is empty at that instant has no way to become full while somebody waits on
it. But it is a rule the map author has to carry in their head while wiring, and
the error it produces arrives at load time, far from the moment and the line
where the mistake was actually made.


## Intended behavior


**Every input port has room for all three sources at once, and carries a tag
saying which one is live.** The tag takes four values: *ring*, *gather*,
*static*, and *none*. Exactly one is in effect at any moment; the storage
belonging to the other two exists and sits idle. What that buys is not ambiguity
— it is that changing where an argument comes from becomes a field write
instead of an allocation dance. A port can become a static, then a gatherer,
then a ring buffer again, while the program runs, under the station's own mutex,
with nothing freed and nothing allocated.


**There is no preference chain, and that is the central decision.** An earlier
draft had a port try its ring buffer first, then fall back to its static, then
fall back to gathering — which would mean that wiring an arrow into a port
configured as a static stops being an error and starts being a silent choice
between two sources decided by timing. One tag, one live source, and an arrow
into a port that is not a ring buffer is refused by name. The fourth value,
*none*, means unconfigured: a station holding one can never become ready, which
is a state rather than a value — no null is invented and nothing is ever
handed to a box. That is what would let a map be assembled from nothing, a
station coming into existence with every port unset and becoming runnable as its
ports are given sources one at a time.


## Intended implementation


The port record grows to hold all three storages plus the four-value tag, with
accessors that read whichever one is live, and ring cells allocated lazily on
the first transition into being a ring buffer so a port that is never a ring
never pays for one. The readiness and claim dispatch tables each gain a fourth
row for *none* — the readiness row answers "not filled" and the claim row can
never be reached. Conversion between tags becomes a single operation taken under
the station's mutex, replacing today's free-and-retag path. The gatherable rule
is restated rather than removed: a station that is gathered from may have no
port whose *live tag* is ring, which is the same safety property doing the same
work, since every port still owns ring storage and what matters is which source
is selected. Separately and deliberately, a box may declare a parameter optional
— and because there is no spare value inside an `int`'s range that could
honestly mean "deliberately absent," an optional parameter's type becomes a
small generated wrapper carrying a presence flag beside the value, so the C
signature *looks* optional rather than being an ordinary parameter that might
secretly be a sentinel; the check that a non-optional parameter is not left
facing an unconfigured port happens at configuration time, where a person is
still standing there to read the error, with the shim keeping only a cheap
assertion as a backstop.


---


Corrections welcome. When you're satisfied, say so and I'll move to the next one
on the spine — statics moving onto the port.


--------------------------------------------------------------------------------

### User Request 7

  A naming collision you should know about before reading any of this in the
  source. The code calls this record a slot. The vocabulary the design settled
  on later — written down in the gather-timing note — reserves "slot" for
  the transient bytes one value occupies inside one task,

  and calls this standing thing on the station an input port. So: a station has
  ports, a task has slots. The source has not caught up, and renaming it touches
  the station header, delivery, gathering, statics, the loader, the dump, and
  the stations-and-slots document, so it is

  deliberately being left as a separate deliberate act rather than smuggled in
  here.



an input port has various machinery attached to determine how to store the value
in the slot. The slot is the actual location in the ring buffer where the data
is stored. The data is stored in the slot in a way defined by the port. We
should correct our notes if they differ.


  A gatherer input carries a storage pointer that is always null, a capacity,
  head, and tail that are always zero, and growth counters that will never move.
  A static input carries all of that plus an unused source. Roughly half of
  every input record is dead weight determined by which

  of three tags it wears.


it's not dead weight, because it allows us to switch input types quickly and
easily without re-allocating memory.


  What this record touches, which is nearly everything on the push path. The
  readiness check walks these records through a dispatch table keyed on the tag
  to ask "is there a value here?" — ring buffers answer head differs from
  tail, gatherers and statics answer always, without

  looking. The claim walks them through a second dispatch table to take one
  value from each, where the gatherer and static rows are deliberately null,
  meaning "resolved later, during task construction, outside the station's
  mutex." The delivery walk writes into the ring-buffer ones.

  The gather module reads the source field to know which station to run. The
  statics module reads static_id to index the map's table. The loader fills all
  of them at load. Runtime rewiring repoints the source field on a live map. The
  dump reads them back out to reconstruct a map

  file. And the generated registry supplies elem_size and type_name at
  placement, which is why hand-placed stations cannot bind statics — nobody
  told them their types.


we should use an enum instead of NULL, one for "GATHER" and one for "STATIC" so
that we know, in the task, how to treat the data - as a function pointer that
returns the gathered value, or as a value itself.


also, what does it mean when you say "hand-placed stations cannot bind statics"?
That seems like an arbitrary limitation. Why don't we just... fix the problem
instead of accepting it? What even is the problem? Also, can you explain this
entire paragraph for me, it feels like we're talking a different language:


  What this record touches, which is nearly everything on the push path. The
  readiness check walks these records through a dispatch table keyed on the tag
  to ask "is there a value here?" — ring buffers answer head differs from
  tail, gatherers and statics answer always, without

  looking. The claim walks them through a second dispatch table to take one
  value from each, where the gatherer and static rows are deliberately null,
  meaning "resolved later, during task construction, outside the station's
  mutex." The delivery walk writes into the ring-buffer ones.

  The gather module reads the source field to know which station to run. The
  statics module reads static_id to index the map's table. The loader fills all
  of them at load. Runtime rewiring repoints the source field on a live map. The
  dump reads them back out to reconstruct a map

  file. And the generated registry supplies elem_size and type_name at
  placement, which is why hand-placed stations cannot bind statics — nobody
  told them their types.


Doing that on a running map means doing it

  while another thread might be inside the readiness check reading the very same
  record


the readiness check is atomic, so we pop the values correctly. This can't
happen.


  There is also a rule the loader enforces that exists purely because the record
  cannot express the situation cleanly: a station that anybody gathers from may
  have no ring-buffer inputs at all. The reason is sound — gathering runs the
  upstream box inline, right now, on the calling

  thread's stack, and a ring buffer that is empty at that instant has no way to
  become full while somebody waits on it. But it is a rule the map author has to
  carry in their head while wiring, and the error it produces arrives at load
  time, far from the moment and the line where the

  mistake was actually made.



sorry, why is this an error? Aren't we making it so gather boxes run when the
task is picked up from the thread pool?


--------------------------------------------------------------------------------

### Assistant Response 7

Good corrections. Let me check the source before I agree or push back on any of
them.


All six corrections check out against the source. Let me take them in order, and
the paragraph you flagged gets rebuilt from the ground up.


## 1. Slot versus port — you're right, and the note is wrong


Your model: **the port is the machinery that decides how a value is stored; the
slot is the actual location where the value sits.** The source agrees with you.
The record on the station owns a `void *storage` pointing at an array of cells,
and each cell is `elem_size` bytes — those cells are the slots. The record
around them is the port.


The gather-timing note says something different and stricter: it defines a slot
as "the physical bytes one value occupies for one invocation, inside a task,"
and then states flatly *"A station has ports; a task has slots."* That sentence
is wrong under your definition, because the ring buffer cells are places a value
sits and they live on the station.


The reconciliation that keeps both halves true:


- **A slot is any place one value physically sits.** A cell in a ring buffer is
  a slot. The bytes reserved for one argument inside a task are also a slot.

- **A port is the standing interface for one input of one station** — where
  the value comes from, what type it is, how many bytes it is, and how it gets
  stored. A port owns its ring slots.

- So a station has ports, ports have slots, and a task has slots too. The word
  travels; the port doesn't.


That is a correction to the gather-timing note, not to the issue. I'll make it.


## 2. The unused fields are not dead weight — and today's code is worse than I said


Accepted, and the issue already argues your side: it says what the uniform
storage buys "is not ambiguity — it is that changing a port's source is a
field write rather than an allocation dance." I framed it as waste and
pre-judged the answer.


But there's something worse than I described, and it strengthens your point.
Today the engine does not merely leave the ring fields unused on a gatherer port
— it **frees the storage**. Both conversion paths do the same three lines:
free the cell array, null the pointer, flip the tag. So converting a port to a
gatherer today destroys the buffer, and converting it back would have to
allocate a new one. The redundancy you're describing isn't a nice-to-have; it is
the repair for something the current code actively does wrong.


One honest limit: a port that has *never* been a ring buffer has no cells to
keep. So the first transition into being a ring still allocates. After that,
switching away and back is free.


## 3. The push-path paragraph, rebuilt


That paragraph was compressed to the point of being code-shaped. Here is the
same content as a walk through one value's life.


**A worker has just finished running a box.** The box returned a value, which is
sitting in the task's output area. The delivery walk now begins, and it is the
only thing that moves values in soramech.


**Choosing an exit.** The station has one or more output ports. Which port this
value leaves by depends on the station's kind: a plain station always uses port
zero; a comparator compares the returned value against its threshold and picks
the first, second, or third port for less, equal, or greater; an iterator uses
whichever port was written into the task back when the task was created. That
choice is one table lookup on the station's kind and it happens in exactly one
place in the engine.


**Following the arrows.** The chosen port holds a linked list of destinations.
Each destination is two 32-bit integers: a station index and a slot index. The
walk visits them one at a time.


**Arriving at a destination.** For each one, the walk takes that station's
mutex. Then it copies the value into the ring buffer — a `memcpy` into the
cell at `tail`, then `tail` advances. If advancing `tail` would land it on
`head`, the buffer doubles first and the wrapped portion is unwrapped so the
contents read in order from cell zero.


**Asking whether the station can now run.** Still holding the mutex, the walk
asks every one of that station's input ports the same question: *do you have a
value?* This is the first of the two dispatch tables. Rather than a chain of
if-else on the port's kind, there is an array of function pointers indexed by
the kind tag, and the walk calls `table[kind](port)`:


| kind | how it answers "do you have a value?" |

|---|---|

| ring | `head` differs from `tail` — there is at least one unclaimed cell |

| gather | **yes, always.** The value doesn't exist yet; it will be produced on demand |

| static | **yes, always.** The value is simply sitting there |


**Taking one value from each.** If every port answered yes, the invocation is
real, and the walk claims one value from each port — the second dispatch
table. The ring row pops from `head` into a buffer on the delivering thread's
own stack. The gather and static rows are **null**, which is the thing you
flagged, and the reason for it is worth stating: resolving a gather means
*running somebody's C function*, and running user code while holding a station's
mutex would mean one slow box freezes every other thread trying to deliver into
that same station.


**Letting go, then building.** The mutex is released. Only then is the task
allocated and filled: the claimed ring values are copied in, the static values
are copied from the map's statics table, and the gather values are produced by
running the upstream box inline, right there, on this thread's stack. Then the
task is pushed into the pool, and the walk moves to the next destination.


**Where the rest of the engine touches these records.** The gather module reads
the port's `source` field — a plain `int`, the index of the upstream station
— to know whose box to run. The statics module reads `static_id`, also an
`int`, to find its entry in the map's numbered table. The loader fills every
field at startup. Runtime rewiring can change `source` on a live map, which is
how a gatherer gets repointed at a different upstream station. The dump reads
all of it back out to reconstruct a map file from the running table. And the
generated registry is what supplies `elem_size` and `type_name` when a station
is placed by name.


## 4. An enum instead of null — accepted, and it's your own standing rule


You're right on the mechanism and right on principle. The claim table today is
literally this shape: one row filled in for ring buffers, the other two left
implicitly null, and the caller guarded by `if (table[kind])`. That is a null
check standing in for a decision, and your own rule on nulls says to find out
why it would be null and fix the real thing. The real thing is that "claim"
means two different operations depending on *when* the value can be produced,
and the table is being asked to express that with an absence.


Your proposal — a tag the **task** carries, per slot, saying whether these
bytes are a value or a function pointer to call — is a larger idea than a
tidier dispatch table, and it lands somewhere useful. Right now a task is always
complete: every slot holds real bytes by the time it reaches the pool. If a slot
can instead say *"I am a thunk, call me,"* then a task can be pushed with its
gathered slots unresolved, and the worker that picks it up resolves them before
running the box. That is the entire mechanism of the gathering-at-pickup issue,
arriving as a consequence of fixing this record rather than as its own
construction. I'll write that connection into both issues.


## 5. Hand-placed stations and statics — not arbitrary, but not a design limit either


Here is the actual mechanism.


A static value in a map file is **text**: `0 = 5`, or `3 = { 5, 2.0, { 0, 0, 0
}, "hey there", 2 }`. To turn that text into bytes, something has to know what
shape those bytes are — is `5` a four-byte signed integer or an eight-byte
double? Is that brace list a struct with a `char`, then seven bytes of padding,
then a `double`? The generator answers this: it emits a field table for every
struct with every offset written as an `offsetof` expression the compiler
computes, and the registry carries every box parameter's type as text.


So binding a static needs the port to know its `type_name`. There are two ways a
station gets created:


- **Placement by name**, which the loader uses. It looks the box up in the
  registry and copies each parameter's type name onto the corresponding port.
  This works.

- **Hand placement**, phase 2 scaffolding that takes an array of `int` element
  sizes and nothing else. It never sets `type_name`, so the field stays null and
  the static text parser has nothing to walk.


**It is a leftover, not a decision.** Phase 2's hand-built map calls were marked
in the header as scaffolding with the issues that would replace them named, and
they were deliberately kept irritating. Phase 3 and phase 6 replaced them on the
product path. What survives is inside tests only, wrapping test counters that no
generated box can reach.


So: yes, fix it. The clean fix is that hand placement takes type names alongside
sizes — one extra parameter, and then there is one placement contract instead
of two. The alternative is deleting the hand path entirely, which would mean
giving the test harness some other way to reach its counters. I'd take the
first; it's smaller and it removes a second class of station rather than a
capability. Either way it belongs in this issue, because we're redesigning the
record that holds `type_name` anyway.


## 6. The readiness race — I was wrong


You're right and I'll correct the issue too, since it makes the same claim.
Readiness and the ring-buffer pop both happen inside a single hold of the
station's mutex, and the task is not built until after the unlock. So a port
conversion that also takes that mutex cannot collide with a readiness check or a
claim. That window does not exist.


The window that **does** exist is the next few instructions: static values are
copied and gather values are produced during task construction, which happens
deliberately outside the lock. A conversion landing in that gap is racing the
resolution, not the readiness check. The gather-timing note already identified
this and proposed the fix — snapshot the port's wiring into the task during
the claim, under the mutex that is already held — which gives one sentence of
semantics: *wiring as of scheduling, values as of gathering.*


## 7. Why the gatherable rule survives pickup-time gathering


Good question, and the answer is that the rule is not about *when* the gather
happens — it's about the gather being unable to wait.


A gather runs the upstream box **inline, on whichever thread is doing the
gathering, right now**, and it cannot decline. Moving that from the delivering
thread to the worker that picked up the task changes who pays and how fresh the
value is. It does not give the gather the ability to stop halfway, park, and
resume later. So if the upstream station has a ring-buffer input and that buffer
is empty at the moment of the pull, the gatherer is standing in front of an
input that no one can fill for it — the thread that would deliver into it
might be this very thread. There is nothing to do but deadlock or lie. Hence the
rule.


**There is exactly one design where the rule could relax, and it's the one the
note argued against.** If a gather were its own pool task rather than an inline
call, a gather that found an empty buffer could go back in the queue and be
resumed when a value arrived — which is precisely what readiness already does
for pushed values. At which point a gather from a buffered station stops being a
pull and becomes a deferred push, and the distinction between the two paths
starts dissolving. That's interesting and I don't think it should be settled in
passing. I'm recording it as an open question on the gathering-at-pickup issue
rather than answering it.


---


## What I'm changing in the files


- **Gather-timing note** — the ports-and-slots vocabulary table, and the
  sentence declaring slots exclusive to tasks.

- **The input-port issue** — the conversion race restated to name the real
  window; the null dispatch rows replaced by explicit kinds; the task carrying a
  per-slot tag for value-versus-thunk, with the link to deferred gathering
  drawn; hand placement gaining type names so the two placement paths stop
  differing in what they can bind.

- **The gathering-at-pickup issue** — the thunk tag noted as arriving from the
  port record, and the "could a queued gather park and resume" question added.


Want me to make those edits now, or keep walking the spine first and batch the
file changes at the end?


--------------------------------------------------------------------------------

### User Request 8

  - A slot is any place one value physically sits. A cell in a ring buffer is a
    slot. The bytes reserved for one argument inside a task are also a slot.

  - A port is the standing interface for one input of one station — where the
    value comes from, what type it is, how many bytes it is, and how it gets
    stored. A port owns its ring slots.

  - So a station has ports, ports have slots, and a task has slots too. The word
    travels; the port doesn't.


  That is a correction to the gather-timing note, not to the issue. I'll make
  it.



great.


  2. The unused fields are not dead weight — and today's code is worse than I
     said


  Accepted, and the issue already argues your side: it says what the uniform
  storage buys "is not ambiguity — it is that changing a port's source is a
  field write rather than an allocation dance." I framed it as waste and
  pre-judged the answer.


  But there's something worse than I described, and it strengthens your point.
  Today the engine does not merely leave the ring fields unused on a gatherer
  port — it frees the storage. Both conversion paths do the same three lines:
  free the cell array, null the pointer, flip the tag.

  So converting a port to a gatherer today destroys the buffer, and converting
  it back would have to allocate a new one. The redundancy you're describing
  isn't a nice-to-have; it is the repair for something the current code actively
  does wrong.


  One honest limit: a port that has never been a ring buffer has no cells to
  keep. So the first transition into being a ring still allocates. After that,
  switching away and back is free.



we should allocate space for ring buffers for each of the inputs when we're
instantiating the box station. Ring buffers can grow, so their location doesn't
*really* matter that much. But the growing operation has a rigidly defined
procedure, and it's considered safe. For the switching operation, we know
beforehand how much space each type of input takes to store - so, we might as
well have a small ring buffer there and ready just-in-case. If we need to grow
it later then we can, but that'll only happen if we experience intense load of
inputs.


  Taking one value from each. If every port answered yes, the invocation is
  real, and the walk claims one value from each port — the second dispatch
  table. The ring row pops from head into a buffer on the delivering thread's
  own stack. The gather and static rows are null, which is

  the thing you flagged, and the reason for it is worth stating: resolving a
  gather means running somebody's C function, and running user code while
  holding a station's mutex would mean one slow box freezes every other thread
  trying to deliver into that same station.


this clarified something for me. We might need to remove the issue file related
to it... If a slot is input-only, meaning it's only used as input for this box
(all it's upstream boxes eventually terminate in a leaf node that has no ring
buffer inputs) then it can ONLY gather it's value AFTER being picked up from the
thread pool. There's I think an issue file, and maybe some documentation that
plans for other options - gathering the value when creating the task struct to
be placed in the thread pool, and also beforehand sometime I think - the truth
is, we can ONLY do it AFTER the task has been assigned FROM the thread pool. I
think that's option 3, correct me if I'm wrong. Can you make the necessary
amendments to the files to reflect that truth? Don't spend too much time talking
about what was - just build the new pattern as if it always was.


  It is a leftover, not a decision. Phase 2's hand-built map calls were marked
  in the header as scaffolding with the issues that would replace them named,
  and they were deliberately kept irritating. Phase 3 and phase 6 replaced them
  on the product path. What survives is inside tests

  only, wrapping test counters that no generated box can reach.


  So: yes, fix it. The clean fix is that hand placement takes type names
  alongside sizes — one extra parameter, and then there is one placement
  contract instead of two. The alternative is deleting the hand path entirely,
  which would mean giving the test harness some other way to

  reach its counters. I'd take the first; it's smaller and it removes a second
  class of station rather than a capability. Either way it belongs in this
  issue, because we're redesigning the record that holds type_name anyway.



sounds like we need to clean all that up. We don't have map files anymore, just
boxes. There's no static table, the values are written directly into boxes when
instantiated as stations.


  - Placement by name, which the loader uses. It looks the box up in the
    registry and copies each parameter's type name onto the corresponding port.
    This works.

  - Hand placement, phase 2 scaffolding that takes an array of int element sizes
    and nothing else. It never sets type_name, so the field stays null and the
    static text parser has nothing to walk.


we should unify these, so that the system loads the initial state of the program
using the exact same methods we use to modify it at runtime.


  A gather runs the upstream box inline, on whichever thread is doing the
  gathering, right now, and it cannot decline. Moving that from the delivering
  thread to the worker that picked up the task changes who pays and how fresh
  the value is. It does not give the gather the ability to

  stop halfway, park, and resume later. So if the upstream station has a
  ring-buffer input and that buffer is empty at the moment of the pull, the
  gatherer is standing in front of an input that no one can fill for it — the
  thread that would deliver into it might be this very thread.

  There is nothing to do but deadlock or lie. Hence the rule.



this behavior is deprecated, and we should work on pruning it, to replace with
the design described above, where the values are gathered when the task is
picked up from the thread pool.


  There is exactly one design where the rule could relax, and it's the one the
  note argued against. If a gather were its own pool task rather than an inline
  call, a gather that found an empty buffer could go back in the queue and be
  resumed when a value arrived — which is precisely

  what readiness already does for pushed values. At which point a gather from a
  buffered station stops being a pull and becomes a deferred push, and the
  distinction between the two paths starts dissolving. That's interesting and I
  don't think it should be settled in passing. I'm

  recording it as an open question on the gathering-at-pickup issue rather than
  answering it.



I think this would involve too much memory shuffling. If we're making another
task just to gather data, then why not gather that data when we're going to use
it?


  What I'm changing in the files


  - Gather-timing note — the ports-and-slots vocabulary table, and the
    sentence declaring slots exclusive to tasks.



good


--------------------------------------------------------------------------------

### Assistant Response 8

Yes — that's option 3, gather at pickup, and your reasoning is the sharper
version of the argument: a pulled value's whole purpose is to be fresh *at the
moment it is used*, and any gather that happens before the task reaches a worker
has the entire queue sitting between the read and the use. That isn't a slower
variant of the same promise, it's a different promise wearing the same words.


Let me read what the note currently guarantees before I rewrite it, so I don't
quietly drop something load-bearing.


Option 3, yes. Let me check the two documents that state the old promise before
I rewrite anything.


Now I'll make the amendments. The gather-timing note gets rewritten around the
new design rather than patched, since its conclusion inverted.


The gather document and the guarantees document describe the engine as it runs
today, so I'm marking them rather than rewriting them — the project's
established pattern for a truth that's changing.


Now the input-port issue, with everything from this turn folded in.


Amendments made. Here's what changed and what it did to the shape of things.


## Confirmed — that's option 3


Your argument is the sharper form of the one the note was making. The note
treated freshness as a *quality* that option 3 maximized; you're treating it as
the *definition*, which makes the other two timings not slower variants but
different promises wearing the same words. A value that must match the instant
it is consumed can only be read at that instant. The note now opens with that
rather than arriving at it after nine pages of comparison.


Option 2 is gone on your reasoning, stated in the note in your terms: if a
second task is being built anyway just to fetch data, the data may as well be
fetched at the moment it's going to be used.


## Files changed


**The gather-timing note** — rewritten around gathering at pickup as the
design, not as a candidate. The ports-and-slots table now says a port is
machinery and a slot is a location, that a ring cell and a task's argument bytes
are both slots, and that the word travels between station and task while the
port doesn't. The two rejected timings are one short section near the end
instead of the body of the document.


**The gathering-at-pickup issue** — rewritten as the design of record. The
three-strategy dispatch table is gone; there's one timing, so there's nothing to
dispatch on. Added the per-slot value-or-thunk tag, credited to the port record
rather than invented there.


**The input-port issue** — six changes: ring cells allocated for every port at
station instantiation so switching source is always a field write; explicit
dispatch rows in place of nulls; the per-slot tag in the task; the conversion
race restated correctly; statics living on the port with no table; and one
construction surface used by both loading and runtime editing.


**The gather datapath document and the guarantees document** — marked, not
rewritten. Both describe the engine that is running right now, and rewriting
them would make them lie until the work lands. Two guarantees are flagged as
*being given up*: a queued task stops being self-contained, and a task's run
time stops being its box's run time. Both losses are the freshness seen from the
other end.


## One consequence worth seeing before we go further


Your instruction that loading should use the exact same methods as runtime
modification is the largest thing said this turn, and it reorders the critical
path.


If the loader adds stations one at a time through the same surface a control
socket uses, then the station table starts empty and grows during loading.
**Growing the station table stops being a phase-7 convenience and becomes the
thing loading is built on.** Today it's the reverse: the loader counts the
station lines in the file, makes one allocation of exactly that size, and fills
it — and adding a station afterward was explicitly left out of scope by the
rewiring issue. Under unification, the "add a station" operation is the *only*
way a station ever comes into existence, and the loader is just its first
caller.


That's a better design and it's a bigger one. It also means the fixed-size-runs
decision — appending a run rather than reallocating, so a station's mutex
never moves — stops being an optimization for a rare case and becomes the
normal path taken once per station at startup.


## One thing I decided rather than asked, flag it if I'm wrong


You said the deadlock behavior around gathering from a buffered station is
deprecated and should be pruned. I kept the **rule** and pruned the **timing**,
because your own sentence a paragraph earlier defines a gather chain as one
where "all its upstream boxes eventually terminate in a leaf node that has no
ring buffer inputs" — which is the rule, stated as a property of the whole
chain rather than of one station. The guarantees document already says the same
thing in stronger terms: it isn't imposed, it falls out, because a station with
no buffer can never be discovered by a delivery and being pulled is its only way
to run.


Moving the gather to pickup changes which thread pays and how recently the value
was read. It doesn't give a gather the ability to stop halfway and resume, so a
gatherer standing in front of an empty ring buffer is still asking for something
only a delivery can provide. I wrote it up that way. If you meant the rule
itself goes, say so and I'll rework it — but I'd want to know what a gather
does when the buffer is empty before I write that down.


**[Asked the user]**


*Q1 — Map files:* You wrote "We don't have map files anymore, just boxes." I
can read that three ways and they lead to materially different work, so I'd
rather ask than guess.

- The loader stops being its own path (Recommended) — A map file becomes a
  script of ordinary operations — add a station, configure a port, connect a
  wire — replayed through the same surface a debugger or control socket calls.
  The text format survives unchanged; what dies is the loader as a separate
  construction mechanism. This is what your next sentence about unifying the two
  placement paths describes.

- A map IS a box — the distinction dissolves — Maps and boxes stop being two
  kinds of thing. A map is a box built out of boxes, composes anywhere a box
  composes, and "map file" becomes "the text form of a box." This is where the
  output-station and multi-output-box issues were already heading; it would make
  them foundational rather than late.

- Both, and the second is the point — Unifying construction is the mechanism;
  making a map indistinguishable from a box is the reason for it. Loading,
  runtime editing, and composing a sub-map all become the same set of calls, and
  the text file is one way to drive them.

→ **Selected:** Both, and the second is the point


*Q2 — Ring capacity:* Ring cells now get allocated for every input port when a
station is instantiated, so that changing a port's source is a field write
rather than an allocation. That capacity is paid by every port, including ones
that will never be a ring buffer — so the starting number matters more than it
used to.

- Derive it from the element size (Recommended) — A port holding four-byte
  integers starts deeper than one holding a 200-byte struct, so every port costs
  roughly the same bytes rather than the same count. Growth handles anything
  that outgrows it. Costs one small calculation at placement.

- One fixed capacity for every port — Simplest to reason about and to report
  on — every buffer in a fresh map is the same depth, so the growth counters
  read as a clean signal about load rather than about type size. A map full of
  large structs pays more memory at rest.

- Let the map file say, per port — The author decides, with a default when
  unstated. Most expressive, and it puts a tuning number into a file that has so
  far deliberately carried no capacities — the format's current line is that
  ring buffers grow on their own and therefore need no capacity written down.

→ **Answered:** we do option 2, one fixed capacity for every port. This
capacity should be, oh, I dunno, 10 entries, sized according to the values that
will be stored there. Make sure this is like, a global variable or something so
we can change it later. Right now 10 is a magic number, but it doesn't really
matter because the system adjusts over time to the greatest capacity it
experiences. It'll only be slowed down on it's startup, which is fine. Maybe...
This might be crazy, but maybe when instantiating a box we take in a value "ring
buffer size" for each port. If there's no value, we default to 10. This value
can be written in the map file (box file haha) or provided as an argument to the
"Create a box" function. It must be defined for each port, any port that misses
the value will get 10.". Read the answers carefully — they may request
clarification, changes, or that you not proceed — and follow what they
actually say.


Both answers land. Let me put the capacity decision in the port record first.


Recorded: ten values per port from one named constant, overridable per port in
the file or at the call, growth covering any figure that turns out wrong. I
noted in the format document why this doesn't violate its "no capacities in the
file" rule — that rule was keeping out a *required* number an author had to
get right.


## Your answer moved the head of the spine


"Both, and the second is the point" means composition isn't the payoff at the
end of the chain — it's the organizing principle, and unified construction is
how you get there. That promotes two things that were sitting late and adds one
that doesn't exist yet:


```

  one way to build a program            ← new head, no issue file yet

        │   add a station · configure a port · connect a wire

        │   legal at any moment; loading is just the first caller

        ├──► what an input port is  ──► gathering at pickup

        └──► growing the station table


  what an input port is ──► statics onto ports ──► mutation from outside

                                                        │

                                                        ▼

                                             two maps in one process

        │

        └──► the output station ──► boxes with several output ports

                                                        │

                                                        ▼

                                                 A MAP IS A BOX

```


Growing the station table was a phase-7 convenience an hour ago. Under
unification it's the mechanism by which *every* station comes into existence,
including the ones loading creates — so the fixed-size-runs decision stops
being an optimization for a rare event and becomes the normal startup path.


---


# The new head of the spine — One way to build a program


*(No issue file yet. This is the draft; correct it and I'll write it.)*


## What it is, and what it touches


The shape of a soramech program is three facts: which stations exist, where each
station's input ports get their values, and which arrows connect which output
port to which input port. Today those facts come into existence through **three
unrelated paths that do not share code**, and each one can do something the
other two cannot.


**Hand placement** is phase 2 scaffolding. You say how many stations the map
has, then place each one by handing over a shim pointer, a kind, a count of
inputs, an array of `int` element sizes, and the size of the return value. Then
you connect ports to destinations. It works, it is marked in the header as
scaffolding, and it was deliberately kept irritating. It cannot bind a static
value to a port, because it is never told any type names — and turning the
text `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into bytes requires knowing the
field layout.


**The loader** reads a text file. It counts the station lines, makes one
allocation of the station table at exactly that size, then places every station
by name — which is the moment the registry supplies the shim, the parameter
count, the element sizes, and the type names that hand placement lacks. A second
pass resolves arrows by name, because a station may be wired to one declared
later in the file. Then validation, then the seed sweep, then the workers are
released. It can do everything, once, at startup, from a file.


**Runtime rewiring** works on a live map: connect a wire, disconnect a wire,
repoint a gatherer at a different upstream station. Every rule the loader
applies per edge is applied here too, with the check and the change under one
rewiring lock so two threads drawing individually-legal jointly-cyclic arrows
cannot both succeed. It can change arrows on a running program and it cannot add
a station, because the table was sized once and never grows.


So: the loader can bind statics and nothing else can. Rewiring works on a
running map and nothing else does. Nothing at all can add a station after
startup. The three paths produce identical structures — the same station
records, the same ports, the same two-integer destinations — by three
different routes that have to be kept in agreement by hand.


## Current behavior


Loading is a construction mechanism of its own, not a caller of anything. It
knows the file format, the registry, the table allocation, the two-pass name
resolution, the validation rules, and the seed sweep, and it performs them in a
fixed order that only makes sense at startup. The concept "still loading" is
real inside it: the table is sized before any station exists, forward references
are legal because a second pass will resolve them, and the workers are parked
until it finishes.


Runtime editing is a second implementation of a subset of the same work, sharing
the *rules* with the loader — the per-edge checks were deliberately made
callable per edge rather than only per map — but not the construction. And
hand placement is a third, which survives only inside tests, wrapping counters
that no generated box can reach.


## Intended behavior


**There is one surface, and it has four operations: create an empty map, add a
station, configure an input port, and connect or disconnect a wire.** Every one
of them is legal at any moment — while a program is being read from a file, or
on a running map with workers in flight — because there is no longer any such
thing as "still loading." Every one applies the same rules, refuses the same
illegal things by name, and leaves the map in a state that is either complete or
visibly incomplete, never half-built. A station comes into existence with every
port unconfigured, which is a state a station can hold indefinitely: it simply
cannot become ready, and it becomes runnable as its ports are given sources one
at a time.


**A map is a box.** Once a map declares where its results come from and
construction is uniform, a map placed inside another map is a station like any
other — the parent wires into its inputs and out of its output ports and has
no way to tell, and no reason to care, that the thing behind the port is a graph
rather than a C function. The text file stops being a special format that only
one mechanism can read and becomes one way among several to drive the surface,
which is why it is a box file rather than a map file. Reading a file, editing a
running program, and composing a sub-program become the same set of calls
arriving from different places.


## Intended implementation


The four operations become the only way any structure is ever created, with the
per-edge rules from runtime rewiring applied by all of them, and the checks that
genuinely need the whole program — the reachability the seed sweep depends on,
the warning for a buffered station no arrow feeds — moved into a separate
deliberate pass a caller runs when it believes the thing is complete, rather
than being a fixed step in a startup sequence. The station table starts empty
and grows by appending fixed-size runs, so that adding a station is the ordinary
path taken once per station at startup rather than a rare event with its own
machinery, and so that no existing station ever moves and no mutex is ever
relocated. The loader shrinks to a reader: it turns each line of text into one
call on the surface and stops at the first refusal, naming the file and the
line, which means the two-pass structure survives only as "resolve names after
all stations exist" rather than as two different kinds of pass. Hand placement
either gains the type names the registry already holds or disappears into the
surface entirely. And the dump becomes the exact inverse of the reader — it
already reconstructs a text file by walking the live station table, so the
round-trip test stops proving that two hand-matched pieces of code happen to
agree and starts proving that one construction surface and one reader are
inverses of each other.


---


Two things I want to flag before you correct this.


**The seed sweep gets awkward.** Today it runs as the last step of loading,
after validation and before the workers are released, and it enqueues every
station that nothing pushes into and nobody gathers from. Under one surface
there is no "last step of loading" — so seeding becomes either an operation
the caller invokes deliberately, or something that happens per-station as each
one becomes fully configured. The second is more uniform and means a station
added to a running program seeds itself, which is probably what you want; it
also means the count of seeded stations stops being a startup figure.


**"A map is a box" may want its own capstone issue.** The output-station and
multi-output-port issues are its two mechanical halves, but neither one says the
sentence, and placing a map *as* a station is a third thing that no issue
currently covers. Say the word and I'll draft it as the tail of the spine.


--------------------------------------------------------------------------------

### User Request 9

looks good


--------------------------------------------------------------------------------

### Assistant Response 9

Writing the construction-surface issue now. Its position also means the
station-table issue is numbered wrong — it's foundational under unification,
not a phase-7 convenience — so let me check what points at it before I move
it.


Nothing points at it but itself — clean move. Writing both now.


Written: the construction surface as the capstone of phase 2, and the
station-table issue moved out of phase 7 into phase 2 where it now belongs —
nothing referenced it but itself, so the move was clean. The roadmap says what
phase 2 now covers.


---


# Next on the spine — Growing the station table


## What it is, and what it touches


A soramech program is a **flat array of station records** plus a count. Every
station is the same fixed size, and everything variable about it — its input
ports, its output ports, its destination lists — hangs off pointers. Every
wire in the entire program is two 32-bit integers, a station index and a slot
index, so nothing anywhere stores a station's address. That was a deliberate
choice made back in phase 2 and it is what let runtime rewiring work at all: you
can redraw arrows on a running program and nothing dangles, because nothing was
ever pointing at anything.


The array is allocated once. Today the loader counts the station lines in the
file, allocates exactly that many records, and fills them. Under the
construction surface we just agreed on, that has to change — if adding a
station is the only way a station ever exists, the table starts empty and grows
while the program is being read.


**What stands in the way is one field, and it is the most-used lock in the
engine.** Each station record carries its `pthread_mutex_t` *inside itself*.
That is the mutex the delivery walk takes on every single value it hands over
— lock, write into the ring buffer, check readiness, claim, unlock.


And a mutex is identified by **where it lives, not by what it contains.** On
Linux a mutex is a small struct whose lock word the kernel addresses through the
futex mechanism: a thread that cannot take the lock calls into the kernel saying
*park me until the word at this address changes*, and the thread that unlocks
says *wake whoever is parked on this address*. The address is the identity. If
you copy those bytes to a new location, every thread already parked is still
waiting on the old address for a wake that will never be sent, and the new
copy's internal state describes a lock somebody may still be holding somewhere
else.


So reallocating the station array moves every station, which moves every mutex,
which breaks the one lock the entire push path is built on. The station header
already writes this down as an invariant: growing a buffer must never move a
station.


**What touches this:** the delivery walk resolves a station index on every
destination it visits; the gather path resolves an upstream station index per
pull; runtime rewiring takes the station's mutex for list surgery; the readiness
check and the claim both run under it; the dump walks the whole table; and the
observer's statistics read every record. Everything that resolves an index pays
whatever indexing costs.


## Current behavior


One allocation, sized at load, never changed. Adding a station is impossible,
and the rewiring issue said so explicitly rather than attempting it — it named
the problem, said every thread reading the table would need to see the new base,
and left it as deserving its own issue rather than being smuggled in alongside
arrows.


The engine's other growable list, the pool's task queue, doubles when full and
does not have this problem, because it is a ring of **pointers** to tasks.
Copying it moves addresses rather than the things at those addresses. It grows
while holding the queue mutex, so every thread wanting to push waits for the
entire copy to finish.


## Intended behavior


**The table stops being one array and becomes a series of fixed-size runs.**
Growing means appending a run — allocating a fresh chunk and writing one
pointer into a short array of chunk pointers — so every station already placed
stays exactly where it was, mutex included, and nothing is copied at all. The
invariant in the header is kept rather than argued with. Resolving an index
costs a shift and a mask instead of an add, which lands on the delivery path and
therefore gets measured rather than assumed free. The count of stations becomes
a value that only ever increases, and the single ordering rule is that a station
is completely built before the count that reveals it is published — a thread
reading a stale, smaller count simply doesn't see the newest station, which is
harmless, because nothing can be wired to it yet.


**Growing does not stop everyone else.** Rather than one thread holding a lock
for the whole operation, the thread that finds no room becomes the builder and
publishes that a new container is under construction; a thread arriving during
that joins a short ring of waiting contributors, raises its hand, and is let in
to add its own entry and update the counts when the builder reaches a convenient
point. The builder finishes when the ring of hands is empty. You show up, you
say *excuse me, can you do this thing for me?*, they say *sure, put it over
there*, they update their count, and they trust you rather than checking your
work. The same pattern belongs on the pool's task ring, where it pays most,
because pushes are constant and the ring doubles exactly when pushes are most
frequent.


## Intended implementation


Change the representation first with no growth at all — same capacity, same
behaviour, one shift-and-mask where there was an add — and measure the
delivery path before and after, because a visible cost there is a finding worth
having before anything else is built on it; then publish the count as an atomic
that only rises, with the build-then-publish ordering; then append a run when
the current ones fill, under the rewiring lock that already exists, as a plain
exclusive grow that is correct and dull; then validate a new station against the
same registry rules the loader applies, refusing by returning a failure and
naming the reason rather than dying, as runtime rewiring already decided; then
let the reader start from an empty table and add stations one at a time instead
of counting lines, which is the step that proves the whole mechanism because
every existing test loads a program; and only after all of that, the
hand-raising ring as its own change with its own measurement of contention and
time-to-add against the exclusive version, followed by the same pattern applied
to the task queue. Removing a station stays out of scope for the same reason
adding one was out of scope for rewiring: an index is a position, and reclaiming
one means either leaving a hole every walk must learn to skip or renumbering,
which would invalidate every wire in the program at once.


---


Three things I'd want your eye on:


**The run size has become harder to pick than it was.** When the loader counted
lines first, it knew how many stations were coming. Under the surface it doesn't
— that's the counting we're removing. So the run size is now a guess, and I
don't have a principled way to make it. Fixed, like the ring capacity? Or does
the reader get to peek at the file for a hint it treats as advisory?


**Station names may now have to live forever.** The loader builds a
name-to-index table and throws it away when loading ends. There is no "when
loading ends" anymore. The dump already forced names to be retained on the
program, so half of this is paid for — but adding a station at runtime raises
whether a name is required, optional, or generated.


**An unwired station used to be caught at the end of loading**, as a loud
warning that something buffered had no arrow feeding it. With no end of loading,
that check has nowhere to fire on its own. It moves into the deliberate
whole-program pass — which means a program nobody runs that pass on can
contain a station that will wait forever, silently.


--------------------------------------------------------------------------------

### User Request 10

  The run size has become harder to pick than it was. When the loader counted
  lines first, it knew how many stations were coming. Under the surface it
  doesn't — that's the counting we're removing. So the run size is now a
  guess, and I don't have a principled way to make it. Fixed,

  like the ring capacity? Or does the reader get to peek at the file for a hint
  it treats as advisory?


what is a run size? I've never used that term before.


  Station names may now have to live forever. The loader builds a name-to-index
  table and throws it away when loading ends. There is no "when loading ends"
  anymore. The dump already forced names to be retained on the program, so half
  of this is paid for — but adding a station at

  runtime raises whether a name is required, optional, or generated.


station names should be attached to the station boxes...? Sorry, what's the
issue here?


  An unwired station used to be caught at the end of loading, as a loud warning
  that something buffered had no arrow feeding it. With no end of loading, that
  check has nowhere to fire on its own. It moves into the deliberate
  whole-program pass — which means a program nobody runs that

  pass on can contain a station that will wait forever, silently.


there's no runtime performance drawback to this, so we should probably accept
that behavior. Maybe someone wants to define some boxes that aren't wired up at
the start, but which will be later on.


--------------------------------------------------------------------------------

### Assistant Response 10

Both of those were me making things sound harder than they are. Let me fix the
term first — it's my coinage and it collides with something the project
already uses.


Confirmed — "run" is already carrying four meanings in this file alone.
Replacing it.


## What a shelf is (and why I stopped calling it a run)


Today the station table is **one allocation**: N station records laid end to
end, so station number *n* sits at `base + n × record size`. One add, one
dereference. Growing it means asking for a bigger block and copying — which is
the thing that moves mutexes and breaks everything.


The alternative is **many small allocations plus a short list of where they
are.** Each allocation holds a fixed number of station records — call that a
**shelf** — and the table becomes an array of pointers to shelves. Station
number *n* now lives at position *n mod shelf size*, on shelf *n ÷ shelf size*.
If the shelf size is a power of two that's a shift and a mask instead of a
divide. Growing means allocating one more shelf and writing one pointer into the
array. **Nothing that holds a station is ever copied**, so no station moves and
no mutex is relocated.


I called it a "run" because that's the ordinary term for a fixed span of
records, and that was a bad choice — this project already spends "run" four
ways: a station *runs*, a station's completed-task counter is its *runs*, a
program is *running*, and rewiring happens at *runtime*. A shelf is a place
things sit, which is what this actually is. Renamed throughout.


**And I over-worried the size.** You were right to push. Since nothing is copied
and no station moves, guessing wrong is cheap in both directions: too small
means the pointer array grows a bit more often, and that array holds *pointers*,
so growing it is the safe kind of copy; too large means the last shelf has some
unused records, a few kilobytes at worst. So it's a fixed named constant sitting
beside the ring capacity — sixty-four to start, changeable in one place. No
peeking at files, no advisory hints.


## Station names — you're right, and I raised a non-issue


Names are already attached to stations and already live for the whole run. The
dump forced that: to write a running program back out as text it needed the
names, and it found the design had proudly thrown them away at load. So the
thing I flagged as a new cost was paid a phase ago.


What's left is trivial and I've just decided it rather than asking: a station is
created **with** its name, the same as with its box and its kind, and a
duplicate name is refused by one lookup in a table that already exists.


## Unwired stations — accepted, recorded


Written into both issues in your terms: it costs nothing at runtime because the
readiness check only fires when a delivery arrives and none ever arrives, and
someone may deliberately place boxes before wiring them. The old end-of-loading
warning survives as something the whole-program pass reports **when asked**,
which is where advice belongs rather than where refusals do.


One residue I left as an open question rather than deciding: a *file* does have
an end, and its author almost certainly does consider it complete — so should
the reader run that pass automatically when it hits the end of a file, even
though a program built by calls never gets one?


---


# Next on the spine — Static values move onto the port


## What it is, and what it touches


A **static** is the third way an argument reaches a box. Not pushed down an
arrow into a ring buffer, not pulled from an upstream station, just *there* —
a comparator's threshold, a file path for a read box, a tuning constant. The
port is always full, it never gates readiness, and claiming copies the value
without consuming it, so a station driven by its buffered side reads the same
static on every one of its runs.


Today all of those values live in **one numbered table on the program, behind
one mutex.** A port that wants a static holds an `int` — which entry. Every
claim indexes the table and copies out.


The text becomes bytes at bind time. The file's statics section holds text —
`0 = 5`, or `3 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` — and binding parses
that text into bytes shaped by the type of the **first port that binds the
entry**, walking the field table the generator emitted. That was a deliberate
departure from the original design, which planned to re-parse the text at every
claim: runtime mutation writes bytes, and a table that is sometimes text and
sometimes bytes is two tables wearing one name.


**What touches it:** the readiness dispatch, whose static row answers *always*
without looking; the claim dispatch, whose static row is one of the two absences
we're replacing; task construction, which copies from the table after the
station's mutex is released; the reader, which binds ports to entries; the
mutation call, which writes new bytes into an entry; the dump, which writes the
table back out; and the **process-wide "active map" pointer**, because a box
that writes a static receives only values and has no handle to anything, so it
reaches the table by a bare name.


That last one is why this issue is on the spine. The format document already
names two consequences of the table's shape as things not to build on: two ports
of different types may reference one entry and read it each their own way, and a
box may write to the table — a back channel around *a box cannot remember*,
invisible in the wiring, so a program showing no connection between two stations
might still have them talking.


## Current behavior


Built, correct, and proven — a station running fifty times off one entry
driven only by its buffered side, and an all-static station that delivery can
never wake. The locking analysis held up: reads constant, writes rare,
contention nil.


What is wrong is not the capability but **where the value lives.** The table is
mutable state held by the program, and program-level mutable state is what
forces a process to contain exactly one program. It is also a second mechanism
for something the wiring already expresses: one value read by several stations
is a thing the graph can say, by putting the value on a station and having
everyone gather from it, where a reader can see it.


## Intended behavior


**A static value belongs to the input port that reads it.** A station is one
instantiation of a box, wired its own way; its ports are its own, and what a
port holds is a property of that port on that station and of nothing else.
Binding copies the file's entry into the port, and from that moment the entry
has done its job — the running program holds no statics table at all. Claiming
then happens under the station's own mutex, in the same window as the
ring-buffer pop rather than after it, which is one lock instead of two and makes
the static half of an input set as mutually consistent as the buffered half
already is. Always full, never consumed: both survive untouched, because they
are the reason the kind exists.


**Sharing, when it is wanted, is drawn.** One station holds the value and
everyone who needs it gathers from it — which costs a station and buys
visibility, since a constant that five stations read appears in the wiring as
five wires rather than as five references to a number appearing nowhere in the
shape of the program. Statics stay slightly discouraged for the same reason: a
program carrying much of its behaviour in constants is a program whose behaviour
is not visible in its wiring.


## Intended implementation


The port gains room to hold its own value sized by its own type — which the
port-record work already provides, so this issue spends it rather than building
it — and binding becomes a copy into the port instead of an index into a
table. The claim moves into the readiness walk under the station's mutex, beside
the ring pop, so the claim dispatch table's static entry stops being an absence
and becomes an ordinary copy. The reader copies from the file's numbered entries
into each binding port as it goes and retains nothing afterward, at which point
the table, its mutex, and its teardown all leave the program. The existing
struct-constant reader is reused unchanged — it walks a field table and brace
text together, recursing into nested structs, with offsets coming from the
compiler through the generated tables — the only difference being that it now
writes into a port rather than into a shared entry. Two tests carry it: that two
ports bound to the same file entry are genuinely independent afterwards, since
writing one no longer disturbing the other is the observable difference from
today; and that the proven behaviours survive, meaning a station driven only by
its buffered side still reads the same value on every run, and an all-static
station is still never woken by a delivery.


---


One thing this quietly fixes that nobody asked it to: **the "first port to bind
shapes the bytes" hazard disappears.** Today two ports of different types can
point at one entry and each read those same bytes their own way, which is a
genuine footgun sitting in the format document as a warning. Once each port
parses the text into its own storage at its own type, there is nothing shared to
disagree about — the hazard stops being a rule people have to know and becomes
a thing that cannot be expressed.


--------------------------------------------------------------------------------

### User Request 11

● What a shelf is (and why I stopped calling it a run)


  Today the station table is one allocation: N station records laid end to end,
  so station number n sits at base + n × record size. One add, one dereference.
  Growing it means asking for a bigger block and copying — which is the thing
  that moves mutexes and breaks everything.


  The alternative is many small allocations plus a short list of where they are.
  Each allocation holds a fixed number of station records — call that a shelf
  — and the table becomes an array of pointers to shelves. Station number n
  now lives at position n mod shelf size, on shelf n ÷

  shelf size. If the shelf size is a power of two that's a shift and a mask
  instead of a divide. Growing means allocating one more shelf and writing one
  pointer into the array. Nothing that holds a station is ever copied, so no
  station moves and no mutex is relocated.



okay. after growing, we need to re-assign the pointer, and THEN we can copy the
values from the old ring buffers into the new ones. Each time we write a value
to a ring buffer (this is always true by the way, not just during the growing
phase) we don't need an atomic on the ring buffer, or the box. This is just for
the writing, when we're checking all the values and popping them off to create a
task struct, we DO need an atomic. But for writing, we don't need an atomic, and
the reason is simple - we ask the box each time we need a ring buffer index to
write into. When we ask the box, the box (running on the same thread that asked
of course) then gives an atomic to itself, briefly, and iterates the head/tail
of the ring buffer (I forget which). So... Actually, there is an atomic. I just
wanted to point out exactly where, to make sure we're on the same page.


  One residue I left as an open question rather than deciding: a file does have
  an end, and its author almost certainly does consider it complete — so
  should the reader run that pass automatically when it hits the end of a file,
  even though a program built by calls never gets one?


can you explain this question more? Also, can you walk me through the process of
starting a new program? When do we read from the "map file" which is just a box
really, and when does each part of that "map" file begin to run?


  Next on the spine — Static values move onto the port


yes please


--------------------------------------------------------------------------------

### Assistant Response 11

Three things, and one of them turned up a real problem with something I proposed
and you approved. Let me check the actual startup sequence first so the
walkthrough is accurate.


Now the statics issue, updated with what this walk settled.


All recorded. Three answers.


## 1. Where the lock actually is


We're on the same page about the shape and I want to say back exactly what I
understood, because it's a change from what runs today.


**Today** a delivery takes the destination station's mutex once and holds it for
the whole arrival: the memory copy of the value into a cell, the readiness walk
across every port, and the claim. One lock, everything inside it.


**What you're describing** is narrower. To write, you ask the station where to
write; the station takes the lock briefly, advances the index, hands back a
cell, and lets go. The copy then happens with **no lock held**. Checking
readiness and popping values to build a task still take the lock, because those
read every port at once and have to see a consistent set. So the held section
shrinks from *an arbitrary-size memory copy plus a walk of every port* down to
*one integer increment* — which pays off exactly where the engine hurts most,
at a station a hundred arrows fan into.


**Two things fall out of that split that don't exist today, and they're the
actual work.**


A cell whose index has been reserved but whose bytes haven't landed yet looks,
to a head-against-tail readiness test, exactly like a cell that's ready. So a
second thread can arrive, see the station as ready, and claim a value that's
still being copied — a torn read, on the hot path. The fix is a **published
position separate from the reserved one**: readiness reads published, and a
writer advances it only after its copy lands *and* only in reservation order, so
a fast writer for cell six can't expose a slow writer's half-finished cell five.


And growth reallocates the cells. A writer copying outside the lock is holding a
pointer into storage that another thread may have just freed. That cannot happen
today because growth and copy are under the same lock. Either growth waits for
outstanding writes to land, or a ring buffer grows by **adding** storage rather
than replacing it — the same move as the shelf, applied one level down, so old
cells never move.


On your ordering note: *reassign the pointer, then copy the old values across*
is safe only while nobody can look, which is true today because the whole growth
is under the station's mutex. Once the write path stops taking that mutex, the
order has to flip — copy into the new storage first, publish the pointer last
— or a reader gets a window where the buffer is empty.


Both are solvable and neither is small, so I've written them into the port
record with a note that this may want to be its own issue.


## 2. Starting a program, step by step


**Today.** Nine phases, and the thing to notice is that stations do not exist
for the first one and nothing runs for the first six.


1. **Reading.** The parser walks the file line by line and builds a description
   of it. It reads and never constructs — no stations, no program memory. A
   malformed line dies here naming the file, the line, and what was expected.

2. **First pass.** Count the station lines, allocate the station table at
   exactly that many records. Fill the statics table first so bindings have
   entries to parse. Then for each station line, look the box name up in the
   registry — which supplies the shim pointer, the input count, each input's
   element size, and each input's type name — and create the station. Every
   port defaults to a ring buffer; a comparator gets one extra port for its
   threshold, typed to the box's return value. Static input lines apply here.
   **Gather input lines do not**, because their source is a name that may belong
   to a station further down the file. A name-to-index table is built in file
   order.

3. **Second pass.** Every output line resolves its destination by name —
   forward references included, which is the entire reason there are two passes
   — checks the slot exists, and type-checks the wire at the first moment both
   ends are known. Gather input lines resolve here too, each running the cycle
   walk as it's bound. The name table is discarded.

4. **Whole-map validation.** The checks that need everything present, each its
   own routine, failures collected and printed together before one stop: an
   arrow onto a port that isn't a ring buffer, a station both written into and
   gathered from, a gathered station with ring inputs, and a loud non-fatal
   warning for a buffered station no arrow feeds.

5. **The pool comes into existence.** Worker threads are spawned immediately and
   park at a starting gate. Still nothing runs.

6. **The seed.** Walk every station and enqueue each one that has no ring-buffer
   inputs *and that nobody gathers from* — those would never run otherwise,
   since nothing can push to them and nothing will pull them. Each is announced
   by name. Seeded tasks enter through the same task-construction path delivery
   uses, so a task comes into existence by one door rather than two. Seeding
   nothing is fatal, with the reason spelled out.

7. **Release.** The gate opens. Workers wake and start popping. *This is the
   first moment any box runs.*

8. **Running.** A worker pops a task, calls its shim, and hands the finished
   task to the delivery walk, which picks an output port, visits each
   destination, writes, checks readiness, claims, builds tasks, pushes. That
   loop is the whole program.

9. **Termination.** The worker whose registration brings the sleeping count to
   the worker total looks at the queue one last time; finding nothing, it sets
   the stop flag and wakes everyone, and each worker returns on its own.


**Under one construction surface**, phases 1 through 4 collapse into "the reader
makes calls," and the interesting change is that stations become real as they
are read rather than all at once:


1. Create an empty program. The pool is created, workers parked.

2. The reader opens the file and turns each line into exactly one call. A
   **station line** adds a station — a shelf gets allocated if the current
   ones are full, the registry supplies everything about the box, ports come
   into existence unconfigured, and the reader immediately configures each as a
   ring buffer because the format's rule is that a port is a ring unless a line
   says otherwise. Ten cells get allocated per port here. An **input line**
   configures that port instead: a static with its value copied straight in, or
   a gatherer pointed at a named station. An **output line** connects a wire.

3. Forward references still need every station to exist before names resolve —
   either the reader holds those lines and replays them at the end, or the
   surface accepts a wire to a station that doesn't exist yet. Still open.

4. Somebody runs the completion pass: the whole-program checks, and the seed.

5. Release. From here it is identical to today.


**The question I flagged is step 4: who calls it.** A file has an end, and
whoever wrote that file almost certainly means "this is the whole program" —
so it's tempting to have the reader run the pass automatically when it hits the
last line. The cost of that is the reader becomes the one caller that secretly
does something extra, which is the exact asymmetry unification exists to remove;
and it makes reading three files into one program impossible, because the first
file's end would seed and check a program that isn't finished.


My recommendation, now written into the issue: **reading a file is only a
sequence of operations and never runs the pass on its own.** The convenience
call that most people use — the one that means "load this and run it" — does
both, so nobody has to remember, and the composable behaviour is still there
underneath for anyone who wants it.


## 3. Something I proposed that doesn't survive contact


Last turn I said seeding would become per-station: a station seeds itself the
moment it's fully configured and turns out to need nothing pushed to it. You
approved that. **It doesn't work**, and the reason is in the rule's second half.


The seed rule is *no ring-buffer inputs* **and** *nobody gathers from it*. The
first half is a property of the station. The second half is a property of the
whole program — whether anybody gathers from a station is decided when some
**other** station's port is configured as a gatherer, which can happen at any
point afterward. A station seeded the moment it looked self-sufficient would
already have run once, and produced a value nobody asked for, by the time the
gather wire showed up.


So seeding stays in the deliberate pass. Two consequences I've recorded: the
pass has to be repeatable without re-seeding what it already seeded, since a
growing program will run it more than once; and "nothing was seeded" stops
meaning "this program can never do anything" — on a second run it just means
"nothing new" — so the refusal that behaviour currently produces has to move
to the convenience call, which is the only caller that knows it's starting from
nothing.


## 4. Statics onto the port — settled


Folded in three changes from this walk: the port's own storage comes from the
port record rather than being built here; the claim's static row becomes an
ordinary copy rather than one of the two absences we're removing; and the reader
binds a static by making the same call a debugger would.


Next on the spine is **changing a static while the program runs** — which is
where the process-wide pointer dies and a process becomes able to hold more than
one program. Say go and I'll walk it.


--------------------------------------------------------------------------------

### User Request 12

  What you're describing is narrower. To write, you ask the station where to
  write; the station takes the lock briefly, advances the index, hands back a
  cell, and lets go. The copy then happens with no lock held. Checking readiness
  and popping values to build a task still take the

  lock, because those read every port at once and have to see a consistent set.
  So the held section shrinks from an arbitrary-size memory copy plus a walk of
  every port down to one integer increment — which pays off exactly where the
  engine hurts most, at a station a hundred arrows

  fan into.



yeah I think this is the right shape. That way, if we have to copy like 200
bytes of struct into an input, it'll take the same amount of time as if we need
to copy a 4 byte integer into the input buffer. This way, we can be copying and
someone else can be doing something else. Since we're the only ones who will be
writing to that particular ring buffer input, it should be fine.


  A cell whose index has been reserved but whose bytes haven't landed yet looks,
  to a head-against-tail readiness test, exactly like a cell that's ready. So a
  second thread can arrive, see the station as ready, and claim a value that's
  still being copied — a torn read, on the hot

  path. The fix is a published position separate from the reserved one:
  readiness reads published, and a writer advances it only after its copy lands
  and only in reservation order, so a fast writer for cell six can't expose a
  slow writer's half-finished cell five.


uh I'm confused about this - won't the reader pull from the tail, while the
writer is writing to the head? or rather, the index that the head gave to the
writer before incrementing itself so that the next writer doesn't write to the
same slot. I use slot there intentionally by the way.


  And growth reallocates the cells. A writer copying outside the lock is holding
  a pointer into storage that another thread may have just freed. That cannot
  happen today because growth and copy are under the same lock. Either growth
  waits for outstanding writes to land, or a ring

  buffer grows by adding storage rather than replacing it — the same move as
  the shelf, applied one level down, so old cells never move.


can't add size to the ring buffers because they're packed together. That just
won't work, sorry. The correct design is to allocate the newly needed space for
the entire ring buffer (or is it the entire box station?) then to copy the
values in, then to fix the pointer.


  1. Create an empty program. The pool is created, workers parked.

  2. The reader opens the file and turns each line into exactly one call. A
     station line adds a station — a shelf gets allocated if the current ones
     are full, the registry supplies everything about the box, ports come into
     existence unconfigured, and the reader immediately configures

  each as a ring buffer because the format's rule is that a port is a ring
  unless a line says otherwise. Ten cells get allocated per port here. An input
  line configures that port instead: a static with its value copied straight in,
  or a gatherer pointed at a named station. An output

  line connects a wire.

  3. Forward references still need every station to exist before names resolve
     — either the reader holds those lines and replays them at the end, or the
     surface accepts a wire to a station that doesn't exist yet. Still open.

  4. Somebody runs the completion pass: the whole-program checks, and the seed.

  5. Release. From here it is identical to today.



how about we load the file, create all the boxes, create all the wiring, and
then add all the boxes that we can? this will probably only be the boxes with
input-only upstreams. However, during the "create all the wiring" step, if any
running tasks can write to the boxes, then they will, and some boxes might get
added to the thread pool then. That's fine.


We'd run this procedure anytime we add a box to an already running map, and
whenever we start a map for the first time.


  The seed rule is no ring-buffer inputs and nobody gathers from it. The first
  half is a property of the station. The second half is a property of the whole
  program — whether anybody gathers from a station is decided when some other
  station's port is configured as a gatherer, which

  can happen at any point afterward. A station seeded the moment it looked
  self-sufficient would already have run once, and produced a value nobody asked
  for, by the time the gather wire showed up.


we don't configure box stations as "gatherers" anymore. We should update the
documentation to reflect that. Instead, we determine if it's a gather (when?) by
checking to see if there's no ring buffer inputs on any of it's upstream boxes.
If not, then the box is "input-only" and it can be run on demand when it's
outputs are needed.


--------------------------------------------------------------------------------

### Assistant Response 12

Three of these are settled and one has a hole in it that I can't close. Let me
record the settled ones first.


## The torn read — here's the case your model doesn't cover


You're right that the reader pops from head and the writer writes at tail. What
that misses is that **on an empty buffer they are the same cell.**


Concretely. A station with two inputs. Input one is empty — head and tail both
sitting at cell zero.


1. Thread A arrives with a value for input one. It reserves cell zero; tail
   moves to one. It starts copying two hundred bytes.

2. Thread B arrives with a value for input two. Four bytes, copied in an
   instant.

3. Thread B takes the station's lock and runs the readiness check. Input one
   reports head-differs-from-tail — cell zero, cell one — so it says it
   holds a value. Input two holds a value. **The station is ready.**

4. Thread B claims one value from each and pops cell zero of input one, which
   Thread A is still halfway through writing.


The gap between head and tail was one cell, so the cell the writer reserved *is*
the cell at head. Reserving advanced the count before the bytes existed.


So the reserved position and the readable position have to be two different
numbers. A writer reserves, copies, and only then advances the readable position
— and only after every earlier reservation has done the same, otherwise a fast
writer holding cell six would advance the readable position past a slow writer's
half-copied cell five.


Your other point holds and I had it backwards in emphasis: **several writers
into one port are fine.** Two threads reserving get two different cells and each
copies into its own, never touching the other's. The reservation is doing
exactly the job you described. The problem was never writer-against-writer; it's
writer-against-reader.


## Ring buffer growth — your design, with one ordering note


Agreed, and the shelf idea doesn't transfer — ring cells have to be contiguous
because the wrap arithmetic is modular, and the growth path deliberately unwraps
into a contiguous block so contents read in order from cell zero. Adding a
second chunk would break both.


**Answer to your parenthetical: just the cells.** Not the station, not the port
record. That invariant already holds today — only the storage the port points
at is ever reallocated, which is what let the station-table test flood one
buffer through seven doublings and find every station address unchanged.


One ordering correction from last turn, which you've already half-made yourself:
**allocate, copy across, publish the pointer, free the old.** Publishing before
copying would leave a window where a reader sees an empty buffer. That ordering
doesn't matter today because the whole growth happens under the station's lock
and nobody can look — but once writes happen outside the lock, it does.


And growth has to wait for outstanding writers. A thread that reserved cell
seven and is still copying holds a pointer into the old storage; if growth
copies across and frees before that lands, the write goes into freed memory.
Since reservations are handed out under the lock and growth holds the lock, no
*new* writers can start during the wait, and the writers being waited on need no
lock to finish — so the wait terminates and nothing deadlocks. It costs one
count of writers currently copying.


## The three-step procedure — taken, and it closed an open question


Create every station, then draw every wire, then enqueue everything that can be
enqueued. Same procedure for a fresh start and for adding to a running program.


**That killed the forward-reference question.** Names always resolve because
every station exists before any wire is drawn — which is all that survives of
the loader's two passes, and it survives as an ordering rather than as two kinds
of pass.


I also wrote down the thing you noted about step two, because it reads like a
race and isn't: on a running program, drawing a wire from a station that is
currently producing means values start arriving down it immediately, so stations
may become ready and enter the pool during wiring rather than waiting for step
three. A value arriving down a wire that exists is what a wire is for.


One residue: step three has to not re-enqueue what it already enqueued, since a
growing program runs the procedure repeatedly and a station seeded the first
time still satisfies the condition the second time. That wants a mark on the
station, not a recomputation.


## Deriving gatherers instead of declaring them — I can't close this one


What you're proposing buys something real, and more than you said. **Two
load-time rules stop being rules and become definitions.** Today the loader
checks that a gathered station has no ring-buffer inputs, and separately that no
station is both written into by arrows and gathered from. Under derivation, the
first *is* the definition of input-only, and the second cannot arise because a
station has one nature rather than one per wire. Two checks disappear rather
than getting better error messages. The map file also loses its `in <slot>
<name>` gather form — every connection becomes an arrow written on the
producer's side, and the engine decides what the arrow means.


**Here is where I get stuck.** The derivation says a station is input-only if
nothing upstream of it has a ring-buffer input. That recursion bottoms out at a
station with no inputs at all, which is trivially input-only. So input-only
propagates down from every leaf, and in a program where nothing declares a ring
buffer, *every* station is input-only and nothing is ever pushed. The property
needs a base case that says "this one pushes," and today that base case is the
file's default — a port is a ring buffer unless a line says otherwise.


I tried making the seed the base case: a seeded station runs on its own and
therefore pushes, anything downstream of a pushed station is pushed, and
whatever the seed can't reach is pullable. That's circular, because the seed
rule is "enqueue a station nothing pushes to *and nobody pulls from*" — and
whether anybody pulls from it is the thing being derived.


Underneath the circularity there's a distinction the graph genuinely cannot
express. Take a station that reads a clock, with one station wired downstream of
it. Two completely different programs draw identically:


- **Read the clock once at startup**, push that value downstream, and everything
  after that works from the timestamp of the run.

- **Read the clock every time** the downstream station needs a value, so each
  invocation sees the current time.


Same boxes, same arrow, same absence of ring buffers anywhere. One bit
distinguishes them, and no amount of walking the graph will find it, because it
isn't in the graph — it's in what the author meant.


So I think it needs one declaration, and the useful question is *where to put
it*. Today it's per-port on the consumer side: `in 0 reader` says this argument
is pulled. Given that a station now has one nature rather than one per wire, the
smaller and more honest place is **one word on the station line** — this
station is a service, run it when somebody needs it, rather than a source that
runs on its own. That's one declaration per station instead of one per gathered
port, it gives the derivation its base case, and everything else you described
still follows from it: the two rules still dissolve, and the port still doesn't
need a gather tag written anywhere.


I've held off editing the gather document and the guarantees until you land
this, because writing down a derivation with no base case would be worse than
leaving the current description in place.


--------------------------------------------------------------------------------

### User Request 13

  So the reserved position and the readable position have to be two different
  numbers. A writer reserves, copies, and only then advances the readable
  position — and only after every earlier reservation has done the same,
  otherwise a fast writer holding cell six would advance the

  readable position past a slow writer's half-copied cell five.


wow you're so right. This is totally true. So, instead of having 3 values (which
is redundant) then can we just... only increment the head/tail I forget which
when we finish writing...? Or does that mean that two writers that come in will
get the same index? I don't think that's true, because we specify where to slot
the data in before we start writing the data. Each ring buffer has a head/tail
value determining where we can read/write from it. when we want to write to an
input port, we say "hello input port, where should I write this?" and the input
port (atomically) increments the head/tail and returns the correct index to
write to. So... where's the issue? The readiness check on a different thread.
Hmmmm how about instead of a readiness check we just have an array of bools (or
even bitflags) that is the exact same size as the ring buffer and stored on the
input port. If we need a readiness check, we iterate through that "ready array",
starting at the "tail" or "head" value idk and pop from there? Or what if we
sampled from it randomly lmao. Okay okay how about this - there's no head/tail
anymore, and instead we iterate the ring buffer ready array to find out where to
store / where to read values. The problem is... "reserved" and "ready to pop"
and both separate things, and "empty" is also needed. Can't use bools or
bitflags. What if we use a 2 digit bitflag, with 4 possible values? What 4th
value could we put in there, maybe... gathered/static, so we know to treat it
that way? What do you think?


  And growth has to wait for outstanding writers. A thread that reserved cell
  seven and is still copying holds a pointer into the old storage; if growth
  copies across and frees before that lands, the write goes into freed memory.
  Since reservations are handed out under the lock and

  growth holds the lock, no new writers can start during the wait, and the
  writers being waited on need no lock to finish — so the wait terminates and
  nothing deadlocks. It costs one count of writers currently copying.


maybe we can iterate the ready array and see if there's any "reserved" values,
and if so... um, I dunno. Wait I guess?


  That killed the forward-reference question. Names always resolve because every
  station exists before any wire is drawn — which is all that survives of the
  loader's two passes, and it survives as an ordering rather than as two kinds
  of pass.


  I also wrote down the thing you noted about step two, because it reads like a
  race and isn't: on a running program, drawing a wire from a station that is
  currently producing means values start arriving down it immediately, so
  stations may become ready and enter the pool during

  wiring rather than waiting for step three. A value arriving down a wire that
  exists is what a wire is for.


  One residue: step three has to not re-enqueue what it already enqueued, since
  a growing program runs the procedure repeatedly and a station seeded the first
  time still satisfies the condition the second time. That wants a mark on the
  station, not a recomputation.



true good point okay so maybe we need a ring buffer for outputs as well, in-case
they get queue'd up with no wires attached. Then, when the wires are attached,
we should (ideally) attach them all at once and then drain them properly. If we
attach them one-at-a-time, then we might drain 15 values to one receiving box
before attaching the second wire, leaving only like 5 values for the second
receiving box. Not ideal. Better to have systems to attach an arbitrary count of
wires in one step.


note that we should only write TO an output buffer if there's no wires coming
out of it. We should have space for it, but we shouldn't use it unless we have
to.


  So I think it needs one declaration, and the useful question is where to put
  it. Today it's per-port on the consumer side: in 0 reader says this argument
  is pulled. Given that a station now has one nature rather than one per wire,
  the smaller and more honest place is one word on

  the station line — this station is a service, run it when somebody needs it,
  rather than a source that runs on its own. That's one declaration per station
  instead of one per gathered port, it gives the derivation its base case, and
  everything else you described still follows from

  it: the two rules still dissolve, and the port still doesn't need a gather tag
  written anywhere.



hmmmm I don't want to add more terms if we can help it.



  Deriving gatherers instead of declaring them — I can't close this one




something is "input only" if it doesn't have a ring buffer, but "doesn't have a
ring buffer" is a shortcut for something else. What it really means is that all
of it's input values are derivable at runtime, not at enqueue time - static
values are copied in, so they're present at runtime. gather boxes upstream of
the static box are just boxes whose runs are deterministic - they also bottom
out at having only static inputs. A program that is fully deterministic (no ring
buffers at all) should probably be written in a different paradigm - that is a
single box function, not a map of boxes.


... okay I kinda see the issue now. We want box functions to eventually BE maps
of boxes, so we should treat "an entire box map with no inputs" as a valid case.
Okay. I understand. So. to resolve that...


I see three cases. The first is, we start a mapgraph and it has no ring buffers
at all. It has a bunch of "input-only" boxes, and they're wired to... what? the
output of the box? Okay sure let's say that. Box output ports might have
instructions for what to do with their output ring buffers when values enter,
like writing to output wires or falling back to writing to a file, but that's
for another time. So we start this mapgraph and there's nowhere that's expecting
any values. Does this program just, not have any output? Why does it exist? Can
you think of examples? Oh wait you gave me one lemme examine that:


Take a station that reads a clock, with one station wired downstream of it. Two
completely different programs draw identically:


  - Read the clock once at startup, push that value downstream, and everything
    after that works from the timestamp of the run.

  - Read the clock every time the downstream station needs a value, so each
    invocation sees the current time.


  Same boxes, same arrow, same absence of ring buffers anywhere. One bit
  distinguishes them, and no amount of walking the graph will find it, because
  it isn't in the graph — it's in what the author meant.


so to do something like the first option, you'd need to have a box function that
writes to another box's static value. How? I have no idea! What would we
accomplish that goal with?


to do the second option, the current design should be fine.


I am so overwhelmed but I want to get this right!


--------------------------------------------------------------------------------

### Assistant Response 13

You're not overwhelmed because this is going badly — you're overwhelmed
because you've been making real decisions for six turns straight and three of
them changed the shape of the engine. Let me close things rather than open them.
Four of your five threads resolve cleanly, and the fifth got smaller because of
something you spotted.


## The ready array — yes, and it's better than what I proposed


Your instinct to kill head-and-tail and put the state on the cell is right, and
it dissolves the constraint I was worried about.


First, the thing you checked yourself: **you can't just increment on finish.**
Writer A asks where to write, gets cell zero. Writer B asks before A has
finished; the index hasn't moved yet; B also gets cell zero. So the increment
has to happen at reservation, which is what forces a second number. Unless the
number goes away entirely — which is your idea.


**Three states are required**, and they're exactly the three you named:


| state | meaning |

|---|---|

| **empty** | nobody has claimed this cell; a writer may take it |

| **reserved** | a writer owns it and is copying into it; nobody may read it |

| **ready** | the bytes have landed; a reader may take it |


A writer scans for an empty cell and flips it to reserved in one atomic step, so
two writers can never land on the same cell — that atomic flip *is* the
reservation, and it replaces the index handout entirely. It copies, then flips
to ready. A reader takes a ready cell and flips it back to empty.


**This removes the ordering constraint that was bothering me.** With a readable
position, a slow writer at cell five blocks cells six and seven from becoming
visible even though they're finished — head-of-line blocking. With per-cell
states there is no position to block: cell six goes ready the moment it's done,
independently.


But that same property is the cost, and it's the one thing to decide: **the
reader's scanning policy is what decides whether arrival order survives.** If
the reader scans from the oldest cell and stops at the first cell that isn't
ready, you keep first-in-first-out order and you keep the head-of-line blocking.
If it skips over a reserved cell and takes the next ready one, you get the
lowest possible latency and values can leave in a different order than they
arrived. That matters for a station that pairs two inputs positionally — the
fifth value from the left with the fifth from the right — because reordering
one side mismatches every pair after it. I'd stop at the first gap: the blocking
is bounded by one memory copy, and the reordering is unbounded and silent.


**Your fourth value shouldn't be gather or static.** Those are properties of the
*port*, not of a cell — a static port has no cells in play at all, and a
gathered one doesn't either, so storing that answer once per cell would be
storing the port's tag N times and inviting the N copies to disagree. The port
already carries its source tag.


If you want to spend the fourth value, there's one thing worth spending it on
and it's the mirror of the write side: **claimed** — a reader owns this cell
and is copying out of it. Today the claim happens under the station's mutex, so
the reader is protected by the lock. With a fourth state the read copy could
leave the lock too, exactly as the write copy did, and the lock would shrink to
the readiness scan alone. Same shape, same reasoning, other direction. Or leave
the fourth value unused; two bits cost the same either way.


**And your growth guess is right.** Scan the array for any cell in the reserved
state; if there is one, wait. That's better than the counter I proposed because
it needs no extra bookkeeping — the answer is already sitting in the array.
Growth holds the station's lock, so no new reservations can start while it
waits, and the writers being waited on don't need the lock to finish. The wait
ends.


## Output buffers — your batching point solves the actual problem


The window you're protecting against is real and narrow: during the wiring step,
a station can get its *input* wire attached from an already-running producer,
start running, and produce output before its own *output* wire has been drawn.
Those values would be discarded.


**Attaching all the wires as one step closes that window entirely**, which is
what you proposed, and it's the smaller fix — no buffer, nothing to drain, no
fairness problem to solve. Your 15-then-5 observation is exactly why draining
after one-at-a-time attachment is worse than not needing to drain at all.


Keeping the output buffer as well is a genuinely different feature: *let a
station run and hold its results for a consumer I'll attach later.* Worth
having, and it has one hazard I want on the record before it gets built. A port
wired to nothing currently discards, and that is **deliberate** — an unwired
comparator branch is the normal case, not an oversight. If unwired ports buffer
instead, a comparator with only its "greater" branch wired accumulates every
"less" and "equal" value forever. So "buffer when unwired" can't be the default;
it has to be something a port is told to do.


## No new term — accepted, and you were right to push back


Which brings us to the one that got smaller.


## The clock example was mine and it was wrong


You caught it: *"to do the first option, you'd need a box function that writes
to another box's static value. How? I have no idea!"*


Right. "Read the clock once at startup and everything afterward works from that
timestamp" means the value becomes **a constant for the run** — read many
times, by many stations, unchanging. That is a computed static, and computing a
static requires writing one, which is precisely the back channel being deleted.
So it was never a push-versus-pull question at all. It's a missing feature with
a different name — *a constant whose value is computed at startup rather than
written in the file* — and it deserves its own issue rather than distorting
this one.


Set it aside and the remaining question is much smaller. Given a wire from an
input-only station, when is it a push and when is it a pull?


**Answer: pull if the consumer already has some other reason to run. Push if it
doesn't.**


A station that has at least one buffered input runs repeatedly, on its own
schedule, driven by that input — so a wire from an input-only station into it
should be pulled, because pushing once would satisfy exactly one of those runs
and starve the rest. A station with no buffered inputs has no schedule of its
own; it can only run if something pushes it, so the wire has to be a push, and
the upstream gets enqueued to make it happen.


Test it against the pipeline shape: a source with no inputs feeding a transform
feeding a sink. The transform has no other buffered input, so the source pushes
and the transform's port is a ring buffer. The transform now has a ring buffer,
so it runs on its own schedule, so the sink's wire from it is a push too. The
source is input-only and nobody pulls it, so it gets enqueued. The chain runs.
Now add a clock to the transform: the transform already has a buffered input, so
the clock is pulled, fresh per run. Both correct, nothing declared.


And your case — an entire program with no ring buffers anywhere, used as a box
inside another program — stays valid: everything in it is input-only, nothing
is pushed, and the whole thing runs when its output is demanded. Which is
exactly what a pure function is.


That gives the derivation its base case without a new word, and it's computed by
the same walk that already checks for gather cycles when a wire is drawn.


**The one thing I'd want to check before writing it down** is what happens when
a late wire flips a station's nature. Draw an arrow into a station that had no
buffered inputs, and it acquires a schedule of its own — so every wire *out*
of the stations feeding it should turn from push to pull, and the ports
downstream of it may need to turn from pull to ring. That propagation is cheap
to compute and the port record was built to make the flip a field write, but it
means one wire can change how a distant part of the program behaves. Should I
work that through, or would you rather I write up everything we've settled first
and come back to it fresh?


--------------------------------------------------------------------------------

### User Request 14

  If you want to spend the fourth value, there's one thing worth spending it on
  and it's the mirror of the write side: claimed — a reader owns this cell and
  is copying out of it. Today the claim happens under the station's mutex, so
  the reader is protected by the lock. With a fourth

  state the read copy could leave the lock too, exactly as the write copy did,
  and the lock would shrink to the readiness scan alone. Same shape, same
  reasoning, other direction. Or leave the fourth value unused; two bits cost
  the same either way.


whoa that's kinda neat. Do you think it'd be faster? I think it might be,
especially if we're working with lots of threads accessing the same positions at
once!


  But that same property is the cost, and it's the one thing to decide: the
  reader's scanning policy is what decides whether arrival order survives. If
  the reader scans from the oldest cell and stops at the first cell that isn't
  ready, you keep first-in-first-out order and you keep

  the head-of-line blocking. If it skips over a reserved cell and takes the next
  ready one, you get the lowest possible latency and values can leave in a
  different order than they arrived. That matters for a station that pairs two
  inputs positionally — the fifth value from the left

  with the fifth from the right — because reordering one side mismatches every
  pair after it. I'd stop at the first gap: the blocking is bounded by one
  memory copy, and the reordering is unbounded and silent.



so, what's the proposed implementation? What are the alternative options?


  Keeping the output buffer as well is a genuinely different feature: let a
  station run and hold its results for a consumer I'll attach later. Worth
  having, and it has one hazard I want on the record before it gets built. A
  port wired to nothing currently discards, and that is

  deliberate — an unwired comparator branch is the normal case, not an
  oversight. If unwired ports buffer instead, a comparator with only its
  "greater" branch wired accumulates every "less" and "equal" value forever. So
  "buffer when unwired" can't be the default; it has to be

  something a port is told to do.



hmmmmmmmmmmmm good point let's scrap the output buffers entirely. Unless there
was another reason they exist...?


  Right. "Read the clock once at startup and everything afterward works from
  that timestamp" means the value becomes a constant for the run — read many
  times, by many stations, unchanging. That is a computed static, and computing
  a static requires writing one, which is precisely the

  back channel being deleted. So it was never a push-versus-pull question at
  all. It's a missing feature with a different name — a constant whose value
  is computed at startup rather than written in the file — and it deserves its
  own issue rather than distorting this one.


ummmmm good analysis, bad proposed solution. We should instead create a way to
modify static values at runtime, because we need to create ways to generate
boxes, connect wires, etc. so we might as well throw "editing static values" in
as well. In which case, the clock example just becomes a box whose "wire" is to
write to another box's static value. Though, I don't think that's how we should
do it, because that's a little silly. or is it? you tell me girl


  A station that has at least one buffered input runs repeatedly, on its own
  schedule, driven by that input — so a wire from an input-only station into
  it should be pulled, because pushing once would satisfy exactly one of those
  runs and starve the rest. A station with no buffered

  inputs has no schedule of its own; it can only run if something pushes it, so
  the wire has to be a push, and the upstream gets enqueued to make it happen.


keep in mind that an "input-only" box can never be wired into a ring buffer.
that is part of it's definition - that no upstream boxes have ring buffer
inputs. this means that wiring an "input-only" box to another box *does not use
the receiving box's ring buffer* at all. Meaning, an input-only box cannot be
wired to a ring buffer, it just can't happen because it's part of the process
that determines whether or not you even use the ring buffer at all.


  And your case — an entire program with no ring buffers anywhere, used as a
  box inside another program — stays valid: everything in it is input-only,
  nothing is pushed, and the whole thing runs when its output is demanded. Which
  is exactly what a pure function is.


pure functions are what we're trying to encourage.


  The one thing I'd want to check before writing it down is what happens when a
  late wire flips a station's nature. Draw an arrow into a station that had no
  buffered inputs, and it acquires a schedule of its own — so every wire out
  of the stations feeding it should turn from push to

  pull, and the ports downstream of it may need to turn from pull to ring. That
  propagation is cheap to compute and the port record was built to make the flip
  a field write, but it means one wire can change how a distant part of the
  program behaves. Should I work that through, or

  would you rather I write up everything we've settled first and come back to it
  fresh?



so what you're saying is we need to be able to distinguish between values stored
in an input ring buffer and made "stale" by a switching of the tag that
determines whether or not we even use a ring buffer at all, and the newly
provided inputs from the "static" or "gather" values that are now selected by
the tag? if that's the case, then I think we should include in the tag-switching
operation a system that checks for ring buffer input, and if there's any, then
we... hmmm actually maybe we don't need that system. Maybe we just, keep the
values in the ring buffer until we switch back. Yes I think that's the best case
scenario here.


--------------------------------------------------------------------------------

### Assistant Response 14

Four answers, one correction taken, and then I want to stop opening things.


## Would the fourth state be faster? Probably — with one thing that could eat it


The win is real and it's the same shape as the write side: today the claim
copies one value from every port *while holding the station's mutex*, so a
station with three 200-byte inputs holds the lock for 600 bytes of copying, and
every thread trying to deliver into that station waits behind it. Move the copy
out and the lock covers the scan and a few atomic flips. **The win scales with
bytes copied per claim × threads contending on that station** — so it's
largest exactly where you said, at a wide fan-in with fat values, and near zero
for a single `int` input.


The thing that could eat it: **false sharing.** Two bits per cell means a
ten-cell port's entire state array is a few bytes — one cache line, holding
every cell's state. Every writer scanning for an empty cell and every reader
flipping a cell is touching that same line, and every flip invalidates it on
every other core that has it. Today those flips happen under the mutex, so the
ping-pong is already serialized; moving them out means more cores hitting one
line concurrently.


That's usually still a win — a contended cache line costs tens of nanoseconds
while a contended mutex costs a trip into the kernel — but it is not free, and
it's measurable rather than arguable. The phase 2 demo already has a fan-in
scene built for exactly this question.


## The scanning policy — and a better shape than I proposed


Working through it, I hit something: **if you drop head and tail entirely, the
state array has no notion of "oldest."** It's a bag of cells with states.
There's no way to scan from the front because there's no front. Preserving
arrival order would need a sequence number per cell, which is a third thing
again.


So the proposal is to keep both and let each do one job:


**Head and tail stay as positions. The state array answers one question about
one cell.** Tail is where the next writer reserves; head is the oldest occupied
cell. A reader looks at head and *only* head: if its state is ready, take it and
advance head; if its state is reserved, this port isn't ready yet and the
station can't run. No scanning at all — readiness stays one check per port,
exactly as cheap as head-differs-from-tail is today.


What this buys over what I originally proposed is that **writers no longer
coordinate with each other.** My version had a shared readable position that
writers had to advance in reservation order, which is a protocol. Here each
writer marks its own cell and walks away; the reader checks the one cell it
cares about; ordering falls out of head moving one cell at a time. Your idea is
strictly better than mine and this is why.


It also collapses two rules into one: **a writer may reserve the cell at tail
only if that cell is empty.** If it isn't — because it still holds an
unclaimed value, or because a reader is mid-copy out of it — the ring is full
and grows. That one check covers both "full" and "someone is still reading that
cell," which the fourth state would otherwise need special handling for.


The alternatives, since you asked:


| | order | readiness cost | in-flight write blocks a read? |

|---|---|---|---|

| **head/tail + state array** (proposed) | preserved | one check per port | yes, bounded by one copy |

| drop head/tail, scan for any ready cell | **lost** | scan all cells | no |

| drop head/tail, add a sequence number per cell | preserved | scan all cells | no |

| head/tail + publish-in-order counter (my earlier one) | preserved | one check | yes, and writers must coordinate |


The second one is the only one that removes the blocking, and it pays for it by
silently reordering — which mismatches every pair after it for a station
pairing two inputs positionally. Not worth it for a stall bounded by one memory
copy.


## Output buffers — scrapped, and I looked for another reason


I checked the other candidates and none of them needs it. Backpressure doesn't,
because rings grow. A destination being rewired doesn't, because delivery
already snapshots the destination list under the mutex. A station with several
output ports doesn't, because each port delivers independently the moment the
box returns. The only motivation was the attach-later window, and batching the
wiring closes that without a buffer.


Your earlier idea — that an output port could carry *instructions* about what
to do with a value, like writing to a file when no wire wants it — survives
this untouched. That's behaviour attached to a port, not storage attached to
one.


## The static-write wire — not silly, and it's the honest version of the thing we deleted


Do it. Here's the distinction that makes it clean rather than a relapse.


The back channel was bad for one specific reason: **a box reached out and wrote
a value, and nothing in the wiring showed it.** Two stations could be talking
with no arrow between them, so the picture lied. That's what earned it the
suspicion a global variable deserves.


An arrow whose destination is a static port is the opposite of that. **The box
is still pure** — it takes its arguments, returns one value, remembers
nothing, and has no idea what happens next. The *wire* is what says "this value
overwrites that port's static instead of queueing into its ring." That's visible
in the file, visible in the dump, and drawable on the canvas. The rule that a
box may not write a static survives completely intact; what changes is that a
wire may deliver into one.


The semantics fall out of what a static already is. Writing one doesn't affect
readiness, because a static is always full — so an arrow into a static port
changes what the destination reads *next time* rather than making it run. The
write takes the station's mutex, which the claim already takes, so there are no
torn reads. And two arrows into one static port means last-writer-wins
nondeterministically, which should be written down rather than discovered.


And the clock case stops needing a new feature at all: a station that reads the
clock, seeded so it runs once, wired into a downstream station's static port. It
runs, the timestamp lands, and every invocation afterward reads it. "Read once
at startup and everything works from that timestamp," drawn.


## Your rule about input-only, taken over mine


You're right and mine was more complicated for no gain. **A wire out of an
input-only station is always a pull** — it never lands in a ring buffer,
because whether the receiving port is a ring at all is decided by what's
upstream of it. My "push if the consumer has no other reason to run" was me
trying to manufacture a base case, and it doesn't survive your version.


And your pipeline observation holds: a program with no ring buffers anywhere is
a pure function, pulled from the bottom, evaluated when its output is demanded.
That's the case that has to work for a program to be usable as a box, and under
your rule it does.


**The one thing still unresolved is where the first ring buffer comes from.** If
every wire out of an input-only station is a pull, and a station stops being
input-only only by acquiring a ring buffer, then nothing ever becomes a ring and
every program is a pure function evaluated once. Something has to be the source
of repetition. My suspicion is that it's the push loop — a station that feeds
itself, which is legal in the push direction and which phase 4 already found
needs a finite companion input to ever stop — but I haven't worked it through
and I don't want to guess at it while you're this deep in.


## Keeping ring values across a tag switch — taken


Simplest answer and the right one: switch the tag, leave the cells alone, and
they're waiting when you switch back. Nothing freed, nothing allocated, nothing
thrown away that a producer already handed over. It's the whole reason the
storage is uniform.


---


We've settled a great deal in the last few turns and almost none of it is
written down yet. Let me go do that — fold all of this into the port record,
the construction issue, the statics issues, and the map file format, and give
you back a short list of what's decided versus what's still open. Then the one
remaining question about where repetition comes from gets asked on its own, with
a clear head, instead of on the end of a long thread.


--------------------------------------------------------------------------------

### User Request 15

  The thing that could eat it: false sharing. Two bits per cell means a ten-cell
  port's entire state array is a few bytes — one cache line, holding every
  cell's state. Every writer scanning for an empty cell and every reader
  flipping a cell is touching that same line, and every flip

  invalidates it on every other core that has it. Today those flips happen under
  the mutex, so the ping-pong is already serialized; moving them out means more
  cores hitting one line concurrently.


so... move the mutex onto the state array? we shouldn't write while anyone else
is reading or writing, and we shouldn't read while someone else is writing. But
we can read while someone else is reading.


  It also collapses two rules into one: a writer may reserve the cell at tail
  only if that cell is empty. If it isn't — because it still holds an
  unclaimed value, or because a reader is mid-copy out of it — the ring is
  full and grows. That one check covers both "full" and "someone is

  still reading that cell," which the fourth state would otherwise need special
  handling for.


are we clearing the ring buffer values when we use them or are we just marking
them as "unfilled" and overwriting them, like a hard drive that de-allocates
memory but doesn't clear it? Because, well, what if we go to fill in a 200 byte
struct and one of the input values is missing. Would it re-use the stale value
from the previous time through? That's not ideal.


  The one thing still unresolved is where the first ring buffer comes from. If
  every wire out of an input-only station is a pull, and a station stops being
  input-only only by acquiring a ring buffer, then nothing ever becomes a ring
  and every program is a pure function evaluated

  once. Something has to be the source of repetition. My suspicion is that it's
  the push loop — a station that feeds itself, which is legal in the push
  direction and which phase 4 already found needs a finite companion input to
  ever stop — but I haven't worked it through and I don't

  want to guess at it while you're this deep in.



this sounds like a fun diversion! Let's come back to it when we're done with
this.


  We've settled a great deal in the last few turns and almost none of it is
  written down yet. Let me go do that — fold all of this into the port record,
  the construction issue, the statics issues, and the map file format, and give
  you back a short list of what's decided versus what's

  still open. Then the one remaining question about where repetition comes from
  gets asked on its own, with a clear head, instead of on the end of a long
  thread.


yes, please write things down!


--------------------------------------------------------------------------------

### Assistant Response 15

Two answers, then I'll write.


**On moving a mutex onto the state array** — your exclusion rules are exactly
right, and the four states already *are* that lock, at one cell of granularity
instead of one lock for the whole port. Empty→reserved is a writer taking
exclusive ownership of that cell; ready→claimed is a reader taking it; both
are a single atomic compare-and-swap, so two threads can never own the same
cell. And two readers never want the same cell anyway, since a reader takes the
cell at head and head advances once. So there's nothing left for a lock to do.


But a lock wouldn't have fixed false sharing regardless — that's not a race,
it's a hardware effect: when one core writes any byte in a cache line, every
other core holding that line has to refetch it. Serializing the accesses doesn't
stop the line from bouncing. **The fix is layout: put the state byte on the cell
rather than in a parallel array.** Then a writer at cell three and a reader at
cell zero touch different lines entirely — as long as the value is big enough
to span lines, which is exactly the case where the win is large. For four-byte
values they'd still share a line, and there the copy is trivial anyway, so the
contention scales inversely with how much it matters.


**On clearing cells** — we don't clear, and it's safe, because every write is
a memory copy of the port's *full* element size. There is no such thing as a
partial write into a cell, so stale bytes are always completely covered. A
struct with a field the box didn't set is the box's problem in C, not something
the engine can see or fix. The real guarantee isn't "the cell was cleaned," it's
"the cell's bytes are never read unless its state says ready" — which is the
state machine's whole job. Zeroing on release would cost a full memset per pop
and buy nothing.


Now let me write it all down.


Written down. Here's the ledger.


## Decided, and now on disk


**Gathering happens at pickup.** The worker about to run the box fills its
pulled slots on its own stack, immediately before the call — because freshness
*at the moment of use* is the definition of what a pulled value is, not a
quality it can have more or less of. The design note is rewritten around that;
the two rejected timings are a short section at the end instead of the body. The
issue is the design of record rather than "a second strategy," and the three-way
dispatch table is gone, because there's one right answer and nothing to dispatch
on.


**A port has room for all three sources at once, and switching is a field
write.** Ring cells are allocated for every port when the station is built —
ten values deep, from one named constant, overridable per port in the file or at
the call. Switching a port away from ring leaves its cells alone; they're
waiting if it switches back.


**A cell carries its own state, and that state is the lock.** Empty, reserved,
ready, claimed. Every transition is one atomic compare-and-swap, so two threads
can never own a cell, and no mutex is needed for the exclusion. Head and tail
stay — they're what give cells an order, which a bag of states can't. A reader
looks at head and only head, so readiness stays one check per port with no
scanning. The state lives on the cell rather than in a parallel array, so a
writer and a reader working at opposite ends touch different cache lines
whenever the value is large enough to matter. A writer may take the cell at tail
only if it's empty, which is one rule covering both "full" and "someone is still
reading that one." Cells are never cleared, because every write covers the full
element size and the guarantee is the state, not the cleanliness.


**Growth: allocate, copy across, publish, free — in that order**, waiting
first for every reserved cell to go ready. The wait is a scan of the states; no
counter needed, the answer's already there. Only the cells move.


**The station table becomes shelves.** Fixed-size allocations plus a short array
of pointers to them, so growing never moves a station and never relocates a
mutex. Sixty-four to a shelf, another named constant. I renamed it from "run"
because this project already spends that word four ways.


**One construction surface, three steps, one procedure.** Create every station,
draw every wire *as one batch*, enqueue what can be enqueued. Same procedure at
startup and mid-run. That killed forward references (every station exists before
any wire), killed the need for output buffers (no window where a station runs
before its outgoing wires exist), and kept unwired ports discarding — which
they must, since an unwired comparator branch is normal and buffering it would
grow forever.


**A wire may deliver into a static port.** The box stays pure and still may not
write a static; the *wire* decides that a value overwrites one. Visible in the
file, in the dump, on a canvas. It doesn't trigger the destination, because a
static never gates readiness. And it's how a constant gets computed instead of
typed: a clock station, seeded to run once, wired into a downstream static port.


**Statics live on the port.** No table, no ambient map pointer, claimed under
the station's own mutex beside the ring pop. A hazard disappears rather than
getting documented better — today an entry's bytes are shaped by whichever
port binds it first, and once each port holds its own copy at its own type
there's nothing shared to disagree about.


**Smaller things:** stations are created with their names (already retained
since the dump needed them); an unwired station is ordinary, not an error; a
station added at runtime is refused by name rather than killing the program.


## Still open


**Where the first ring buffer comes from.** Your rule — a wire out of an
input-only station is always a pull — is cleaner than the one I proposed and I
took it. But if nothing is ever pushed, every program is a pure function
evaluated once, and something has to be the source of repetition. My suspicion
is the push loop, and you called it a fun diversion, so it's waiting.


**Four smaller ones**, each recorded in its issue: whether the completion pass
re-seeds what it already seeded (wants a mark on the station); whether a refusal
for "out of memory" should read differently from a refusal for "you misspelled
the box name," since a caller that treats them alike will retry something that
can't succeed; what happens to a task whose gather fails halfway when the task
already belongs to a worker; and whether the sleeper count needs to tell a
worker inside a gather chain from a worker inside a box.


## Two things I'd flag before we continue


Everything above is uncommitted, and the repository is still carrying a large
body of someone else's in-progress work — seventy-odd regenerated HTML pages,
rewritten demos, four new demo-engine files. My changes are cleanly separable.
Say when you want them staged.


And the documentation site doesn't know any of this happened. None of the new
issues have pages, and the pages that exist describe the old design. That's one
run of the generator, but the generator is mid-edit under the documentation
issue, so I've left it alone.


--------------------------------------------------------------------------------

### User Request 16

  A cell carries its own state, and that state is the lock. Empty, reserved,
  ready, claimed. Every transition is one atomic compare-and-swap, so two
  threads can never own a cell, and no mutex is needed for the exclusion. Head
  and tail stay — they're what give cells an order, which a

  bag of states can't. A reader looks at head and only head, so readiness stays
  one check per port with no scanning. The state lives on the cell rather than
  in a parallel array, so a writer and a reader working at opposite ends touch
  different cache lines whenever the value is large

  enough to matter. A writer may take the cell at tail only if it's empty, which
  is one rule covering both "full" and "someone is still reading that one."
  Cells are never cleared, because every write covers the full element size and
  the guarantee is the state, not the cleanliness.



before we move on, let's talk about this one more?


  The station table becomes shelves. Fixed-size allocations plus a short array
  of pointers to them, so growing never moves a station and never relocates a
  mutex. Sixty-four to a shelf, another named constant. I renamed it from "run"
  because this project already spends that word four

  ways.


concerned about the 64 to a shelf thing - what's the point of shelves again?


  One construction surface, three steps, one procedure. Create every station,
  draw every wire as one batch, enqueue what can be enqueued. Same procedure at
  startup and mid-run. That killed forward references (every station exists
  before any wire), killed the need for output buffers

  (no window where a station runs before its outgoing wires exist), and kept
  unwired ports discarding — which they must, since an unwired comparator
  branch is normal and buffering it would grow forever.


and this is the procedure both when we start a mapgraph, and when we add a
boxmap to an existing and running mapgraph?


 a station added at runtime is refused by name rather than killing the program.


um tell me more about that?


  Where the first ring buffer comes from. Your rule — a wire out of an
  input-only station is always a pull — is cleaner than the one I proposed and
  I took it. But if nothing is ever pushed, every program is a pure function
  evaluated once, and something has to be the source of

  repetition. My suspicion is the push loop, and you called it a fun diversion,
  so it's waiting.


okay let's wait on the diversion, but um tell me, why does the map of pure
functions run only once? shouldn't it never run at all? AH DIVERSION


  Four smaller ones, each recorded in its issue: whether the completion pass
  re-seeds what it already seeded (wants a mark on the station);


no idea what this means, you gotta go slower with me I'm just a dumb human


whether a refusal for "out of memory" should read differently from a refusal for
"you misspelled the box name," since a caller that treats them

  alike will retry something that can't succeed;


um. yes? they should have different error messages?


 what happens to a task whose gather fails halfway when the task already belongs
 to a worker;


oh, um, it should gather the error and either crash the entire program (not
ideal!!) or it should... um... I don't know I'm just a girl! think, girl. this
is why you are menardi. AHHH fine okay um it should, uh, I dunno, turn around
and try again? and if it can't do it that time NO BAD WHAT IF THE GATHER
FUNCTIONS AREN'T IDEMPOTENT ahhhh I don't know!!!


and whether the sleeper count needs to tell a worker inside a gather chain from
a worker inside a box.


um. what is a sleeper count? is this determining whether or not the program is
still running? because if so, then we definitely definitely need to count the
gathering workers as workers. because y'know they're still working!


--------------------------------------------------------------------------------

### Assistant Response 16

Slow is fine — these are all good questions, and two of them have better
answers than the ones I gave.


## Shelves — why they exist at all


Start from the thing that forces it. **A station contains its own mutex**, and
that mutex is taken every single time a value is delivered into that station. On
Linux a mutex is identified by *where it lives*: a thread that can't get the
lock asks the kernel to park it until the word at that specific address changes,
and the thread that unlocks says "wake whoever is parked at this address."


So if you move a mutex, every thread parked on the old address waits forever for
a signal that now gets sent somewhere else.


Now: today the station table is one big allocation, all the stations in a row.
To make it bigger you allocate a bigger one and copy everything across — which
moves every station, which moves every mutex. That's the whole problem.


**Shelves are just "grow by adding instead of by copying."** Each shelf is one
allocation holding some stations; the table is a little list of where the
shelves are. Need more room? Make another shelf and add it to the list. Every
station already placed stays exactly where it was, mutex untouched, because
nothing gets copied.


The cost is that finding station number 500 is now two steps — look up which
shelf, then how far into it — instead of one.


**Sixty-four is arbitrary and I should have said so louder.** It's just how many
stations fit in one shelf. Bigger means fewer shelves and a shorter list, but a
five-station program still allocates space for sixty-four and wastes fifty-nine
slots — maybe ten kilobytes. Smaller wastes less and makes the list longer.
Nothing about the design depends on it. If ten kilobytes of waste on a tiny
program bothers you, sixteen is just as good.


## The cell states — let me walk one value through


Ten cells in a row. Every cell has a little state written on it.


A value arrives for this port. The writer takes the station's lock just long
enough to be pointed at a cell — say cell 3 — and to flip cell 3 from
**empty** to **reserved**. Then it lets go of the lock and copies its two
hundred bytes into cell 3 with nothing held. When the bytes have landed, it
flips cell 3 to **ready**.


Meanwhile a reader wants to know if this station can run. It looks at one cell
— the oldest one, wherever head is pointing — and asks what state it's in.
**Ready** means there's a value here, take it. **Reserved** means somebody is
mid-copy, so this port isn't ready and the station can't run yet. It never looks
at any other cell.


That's it. The state is doing two jobs at once: it's the "is there a value here"
answer, and it's the ownership marker that keeps two threads off the same cell.


Three things worth saying plainly about why it's shaped that way:


**The reason a cell can't just be "full or empty"** is that reserving and
finishing are two different moments. If you mark a cell full when you reserve
it, a reader sees "full" while the bytes are still arriving and reads garbage.
If you mark it full only when you finish, two writers both get sent to the same
cell because nothing moved in between. You need the middle state.


**The reason head and tail stay** is that a pile of cells with states on them
has no idea which one arrived first. Head and tail are what remember the order.
Without them you'd have to scan, and you'd hand values out in whatever order
they happened to finish.


**The reason a slow write blocks the reads behind it** is that head only moves
forward one cell at a time. If cell 3 is still being written, cells 4 and 5
might be ready, but nobody gets them until 3 lands. That's on purpose — it's
how values leave in the order they arrived. The wait is one memory copy long.


What did you want to dig into? I can go deeper on any of it.


## Yes — the same procedure both times


Starting a program and adding to a running one are the same three steps, in the
same order, through the same calls. The only difference is that on a running
program, step two has an effect you can watch: the moment a wire exists, values
start going down it.


## "Refused by name" — that was my jargon and it was bad


I meant: when you try to add a station and something's wrong — the box name
isn't in the registry, the kind isn't one the engine knows — the engine
**hands back a failure and prints what was wrong and which station**, instead of
killing the whole process.


The reasoning came from the rewiring work. A file being loaded should die
loudly: you're sitting right there, you want it to stop and tell you. But a
program that's already running, with real work in flight, shouldn't die because
a control socket sent one bad instruction — that takes everything down over a
typo.


## Why a program of pure functions runs once instead of never


Because something seeds it. The last station in the chain has nothing pushing
into it and nothing pulling from it, so it would never run at all — and the
engine's rule is: if a station could never run, give it one task to start it. It
runs, which pulls its inputs, which pulls theirs, all the way up. One answer
comes out.


And if that same program is used as a *box* inside a bigger program, nothing
seeds it, because something upstream is now pulling it. Then it runs whenever
it's asked. Both behaviours from the same rule.


## The re-seeding thing, said properly


Seeding is the engine dropping a first task into the queue for a station that
nothing else would ever trigger. Otherwise that station just sits there forever.


The problem: seeding happens in step three of the procedure, and you run that
procedure *again* every time you add boxes. The second time through, it looks at
the stations from last time, sees they still match "nothing will ever trigger
this," and seeds them again — so they run a second time for no reason.


Fix: write down on each station that it's already been started, and skip those.
That's all.


## Error messages — yes, and slightly more than messages


You're right that they need different text. The part underneath the text: a
misspelled box name is something the caller can *fix and try again*. Out of
memory isn't — trying again does exactly the same thing. So anything that
automatically retries on failure would spin forever on the second kind. The two
need to be distinguishable by the code, not just by a human reading the output.


## The gather that fails halfway — the answer is boring, and your instinct is why


You got to the right worry: **retrying is dangerous precisely because gather
functions aren't necessarily idempotent.** A gatherer that already pulled a
message off a socket can't pull it again.


But there's a second thing that closes the question. By the time a worker is
holding that task, **the values from the ring buffers have already been taken
out of them** — consumed, under the lock, gone. There's no putting them back:
other values were claimed after them, so the order would break. So abandoning
the task silently loses real data.


Can't retry, can't abandon. So it stays what it already is today: a gather that
fails stops the program and names what failed. Moving the gather to pickup
doesn't change that at all — it only changes which thread is standing there
when it happens. Nothing new to decide.


## The sleeper count, and you're right


When a worker finds nothing to do, it goes to sleep instead of spinning in a
loop burning processor. The pool counts how many are asleep. When that count
reaches *all of them*, the last one to fall asleep takes one more look at the
queue — and if it's still empty, that's how the program knows it's finished,
and it wakes everyone up to exit.


So yes: it is exactly the thing that decides whether the program is still
running. And your answer is right — a worker off gathering is working, so it
must not count as asleep.


**It already doesn't.** A worker that picked up a task never registered as
asleep; it's busy the whole time, gathering included. The program can't decide
it's finished while anyone is mid-gather.


So my question was worse than I made it sound, and it's not about correctness at
all — it's about reports. If a program looks stuck, can you tell whether a
worker is stuck inside *your* box or stuck inside the engine fetching something
on your behalf? That's a diagnostics nicety, not a hazard. I'll downgrade it in
the issue.


--------------------------------------------------------------------------------

### User Request 17

  Shelves — why they exist at all


  Start from the thing that forces it. A station contains its own mutex, and
  that mutex is taken every single time a value is delivered into that station.
  On Linux a mutex is identified by where it lives: a thread that can't get the
  lock asks the kernel to park it until the word at

  that specific address changes, and the thread that unlocks says "wake whoever
  is parked at this address."


  So if you move a mutex, every thread parked on the old address waits forever
  for a signal that now gets sent somewhere else.


  Now: today the station table is one big allocation, all the stations in a row.
  To make it bigger you allocate a bigger one and copy everything across —
  which moves every station, which moves every mutex. That's the whole problem.



ah I see. Re-explain this to me and ask how we're going to re-implement mutexes,
that'll really get me goin' :)


  The sleeper count, and you're right


  When a worker finds nothing to do, it goes to sleep instead of spinning in a
  loop burning processor. The pool counts how many are asleep. When that count
  reaches all of them, the last one to fall asleep takes one more look at the
  queue — and if it's still empty, that's how the

  program knows it's finished, and it wakes everyone up to exit.


  So yes: it is exactly the thing that decides whether the program is still
  running. And your answer is right — a worker off gathering is working, so it
  must not count as asleep.


  It already doesn't. A worker that picked up a task never registered as asleep;
  it's busy the whole time, gathering included. The program can't decide it's
  finished while anyone is mid-gather.


  So my question was worse than I made it sound, and it's not about correctness
  at all — it's about reports. If a program looks stuck, can you tell whether
  a worker is stuck inside your box or stuck inside the engine fetching
  something on your behalf? That's a diagnostics nicety, not

  a hazard. I'll downgrade it in the issue.



gather boxes are your boxes too. they're all the same. like a truck and a
minivan can both drive places.


  The re-seeding thing, said properly


  Seeding is the engine dropping a first task into the queue for a station that
  nothing else would ever trigger. Otherwise that station just sits there
  forever.


  The problem: seeding happens in step three of the procedure, and you run that
  procedure again every time you add boxes. The second time through, it looks at
  the stations from last time, sees they still match "nothing will ever trigger
  this," and seeds them again — so they run a

  second time for no reason.


  Fix: write down on each station that it's already been started, and skip
  those. That's all.



when we add a box, let's say it has sub-boxes, we should only "seed" the boxes
that we added.


  Why a program of pure functions runs once instead of never


  Because something seeds it. The last station in the chain has nothing pushing
  into it and nothing pulling from it, so it would never run at all — and the
  engine's rule is: if a station could never run, give it one task to start it.
  It runs, which pulls its inputs, which pulls

  theirs, all the way up. One answer comes out.


  And if that same program is used as a box inside a bigger program, nothing
  seeds it, because something upstream is now pulling it. Then it runs whenever
  it's asked. Both behaviours from the same rule.



I think we should just say a mapgraph that never runs is an error. We shouldn't
run it once just because. it should just... not compile, because it has nothing
to do.


  "Refused by name" — that was my jargon and it was bad


  I meant: when you try to add a station and something's wrong — the box name
  isn't in the registry, the kind isn't one the engine knows — the engine
  hands back a failure and prints what was wrong and which station, instead of
  killing the whole process.


  The reasoning came from the rewiring work. A file being loaded should die
  loudly: you're sitting right there, you want it to stop and tell you. But a
  program that's already running, with real work in flight, shouldn't die
  because a control socket sent one bad instruction — that

  takes everything down over a typo.



there will always be boxes the engine "doesn't know" - they aren't lego bricks
with specified shapes, they're paintings with unique characteristics.


  Shelves — why they exist at all


  Start from the thing that forces it. A station contains its own mutex, and
  that mutex is taken every single time a value is delivered into that station.
  On Linux a mutex is identified by where it lives: a thread that can't get the
  lock asks the kernel to park it until the word at

  that specific address changes, and the thread that unlocks says "wake whoever
  is parked at this address."


  So if you move a mutex, every thread parked on the old address waits forever
  for a signal that now gets sent somewhere else.


  Now: today the station table is one big allocation, all the stations in a row.
  To make it bigger you allocate a bigger one and copy everything across —
  which moves every station, which moves every mutex. That's the whole problem.



why don't we just have a multidimensional mutex array? that way we can grow it
easily. they're all the same size which is nice, but if we could say "please
lock page 3, index 4" then we can add more pages without resizing. Can you think
of anywhere else we could use this pattern?


  Sixty-four is arbitrary and I should have said so louder. It's just how many
  stations fit in one shelf. Bigger means fewer shelves and a shorter list, but
  a five-station program still allocates space for sixty-four and wastes
  fifty-nine slots — maybe ten kilobytes. Smaller wastes

  less and makes the list longer. Nothing about the design depends on it. If ten
  kilobytes of waste on a tiny program bothers you, sixteen is just as good.


hmmmmm what if they were multidimensional shelves


  Meanwhile a reader wants to know if this station can run. It looks at one cell
  — the oldest one, wherever head is pointing — and asks what state it's in.
  Ready means there's a value here, take it. Reserved means somebody is
  mid-copy, so this port isn't ready and the station can't

  run yet. It never looks at any other cell.


there were another two options for this, right?


--------------------------------------------------------------------------------

### Assistant Response 17

## The mutex thing again, and then the question you asked me to ask


A station has a mutex sitting inside it. Every value delivered to that station
takes it. On Linux, that mutex is a small piece of memory, and when a thread
can't get the lock it asks the kernel: *park me until the word at this exact
address changes.* When the holder releases, it says: *wake whoever is parked at
this address.* The address is the identity. There's nothing else — no handle,
no id, no registry of locks. Just a location.


Move it, and every thread parked at the old location is waiting on a doorbell
that's been unplugged.


Today all the stations sit in one block of memory. Making that block bigger
means getting a bigger block and copying — which moves every station, and
therefore every doorbell.


**Now the question you wanted.** We just spent an hour deciding that a ring
buffer cell doesn't need a lock, because its state field *is* the lock — four
states, one atomic flip to take ownership, nobody else can have it. So: what is
the station's mutex still doing? It's holding a decision that spans several
ports at once — *are all of these ready, and can I take one from each without
someone else taking the same ones?* That's a multi-object atomic decision, which
is the one thing a single compare-and-swap can't do.


Which means the real question isn't how to move a mutex. It's **whether that
decision can be made without one.** If it can, stations stop containing mutexes,
and the entire moving problem evaporates rather than getting solved. I have
thoughts and I'd rather hear yours first.


## Your paged mutex array — you already answered this an hour ago, backwards


The idea works. Page three, index four, add pages without resizing. It's exactly
right as a growable-thing pattern.


But it doesn't buy anything over shelves, because a shelved station *already*
never moves — its mutex is inside it and it's staying put. And it would cost
something real: **packing the mutexes together puts several of them in one cache
line.** Locking station three would invalidate the line holding station four's
mutex on every other core that was using it. Two unrelated stations would slow
each other down for no reason.


That's the same trap we walked around when you asked whether to put the cell
states in a parallel array — we put the state on the cell instead, precisely
so a writer at one end and a reader at the other touch different lines. **Same
lesson, opposite direction: keep the lock with the thing it protects, not in a
table of locks.**


## Where else the pattern goes — and it may have eaten your hand-raising ring


**The task queue.** It's a ring of pointers that doubles when full, and doubling
means one thread copying the whole thing while everybody who wants to push
waits. Make it a chain of pages instead — push into the tail page, pop from
the head page, add a page when the tail fills, drop a page when the head empties
— and **nothing is ever copied.** No doubling, no capacity, no pause.


Which is the awkward part: the hand-raising ring exists so that people arriving
during a long copy can contribute instead of waiting. If nothing copies anymore,
there's no long operation to contribute during — adding a page is a pointer
write. The pattern is still good and still worth having written down for
anything that genuinely needs a long exclusive stretch. It just might not have a
customer left in this engine.


**Where it doesn't go: ring buffer cells.** You were right to reject that.
Paging them would mean the number of cells changes, and head and tail are
positions *within* that number — so growing remaps where every existing value
lives. That's why growth unwraps into a fresh contiguous block. The copy is
load-bearing there.


## Multidimensional shelves


They'd work, and it's exactly what a processor does for memory: a page table of
page tables, because the address space is 2⁴⁸ and a flat table of that would
be larger than the memory it describes.


Our case is the opposite shape. One level of shelves for a thousand stations is
a list of sixteen pointers. Growing that list copies sixteen pointers — safe,
since moving a pointer doesn't move what it points at, and it happens maybe four
times in a program's entire life. A second level would buy us never copying
those sixteen pointers, and charge an extra memory hop on the delivery path
forever.


Right pattern, wrong scale. Worth remembering if we ever index something into
the millions.


## The other two options for the reader


Right, there were. All three keep the four cell states; they differ in what the
reader looks at.


**What we chose:** look at head, and only head. Ready means take it. Reserved
means this port isn't ready. Order is preserved because head moves one cell at a
time. The cost is that a slow write at head blocks cells behind it that are
already finished — for exactly as long as one memory copy.


**Skip the gap:** take any ready cell, ignore the one being written. Nothing
ever blocks. The cost is that values leave in a different order than they
arrived — and *silently*. A station pairing two inputs positionally would
mismatch every pair from the first reorder onward, and nothing would tell you.


**Sequence numbers:** put a number on each cell saying when it arrived, so order
survives without head and tail. Gets you both properties — order kept, no
blocking — and charges you a scan across the cells on every readiness check
instead of looking at one, plus a number per cell.


I picked the first because the blocking is bounded by one copy and the
reordering isn't bounded by anything. But the third is a real option if that
block ever shows up in a measurement.


## Four corrections, taken


**"Gather boxes are your boxes too."** Right, and my sentence was wrong. There's
no engine-versus-your-code distinction there — a gathered box is a box, and a
worker inside one is inside your code either way. A report should say which box,
and whether it was reached by being pulled is a separate fact about the same
event, not a different kind of event.


**Seed only what you added.** Better than the flag I proposed — no mark on the
station, no scan of the whole program, no chance of a stale mark. The procedure
operates on the set you handed it.


**"There will always be boxes the engine doesn't know."** Fair, and I should be
precise about what it *does* know: the generator reads your box sources and
writes down each box's name and the types it takes and returns. That's all. The
engine knows nothing about what a box does. So a refusal for an unknown name
means "nothing with that name was compiled in" — a typo or a missing file —
not a judgment about the box. You're right that they're paintings; the registry
is just the list of titles.


**"A program that never runs is an error."** I want to take this one but I need
to check what you mean, because it collides with something.


Today's rule is: if a station could never run, start it. That's the engine
inventing work, and I think that's what you're objecting to. But that same rule
is what starts *normal* programs — a reader station with no inputs gets one
task, runs, and pushes values downstream, and everything follows from that. Take
the rule away and nothing starts at all.


So which is it: **the rule is fine but a program where it fires zero times
should be refused**, or **the rule itself is wrong and something else should say
what starts**?


And I think this is your diversion wearing a different hat. "Where does the
first ring buffer come from" and "what starts a program" are the same question
— both are asking for the base case that everything else is derived from. It
might be worth taking the diversion now rather than later, since we keep walking
into it.


--------------------------------------------------------------------------------

### User Request 18

  Which means the real question isn't how to move a mutex. It's whether that
  decision can be made without one. If it can, stations stop containing mutexes,
  and the entire moving problem evaporates rather than getting solved. I have
  thoughts and I'd rather hear yours first.


oh! flip a value to "claimed" if it's valid, filled, etc, and keep going until
you get to the end. If you get to the end and there's one empty slot, then you
go back and flip those "claimed" values to "ready". If they're all ready, then
you go through and pop them each. Easy! It'll leave gaps in the ring buffer
maybe, and it might mess up ordering, and it might invalidate the head/tail
mechanic (when would we advance the head/tail in this proposed system?) but that
might be okay. Order is never something we guarantee. values might enter in one
box and enter in a different order. It's best if we can find a way to preserve
that but it's unlikely and not a real target - think of it like an extra credit
school assignment. not worth slowing down the system for.


  Your paged mutex array — you already answered this an hour ago, backwards


  The idea works. Page three, index four, add pages without resizing. It's
  exactly right as a growable-thing pattern.


  But it doesn't buy anything over shelves, because a shelved station already
  never moves — its mutex is inside it and it's staying put. And it would cost
  something real: packing the mutexes together puts several of them in one cache
  line. Locking station three would invalidate the

  line holding station four's mutex on every other core that was using it. Two
  unrelated stations would slow each other down for no reason.


  That's the same trap we walked around when you asked whether to put the cell
  states in a parallel array — we put the state on the cell instead, precisely
  so a writer at one end and a reader at the other touch different lines. Same
  lesson, opposite direction: keep the lock with the

  thing it protects, not in a table of locks.



reasonable! so, memory that's physically closer is slower. Hmmmmmmmmmmmmmmm


  Which is the awkward part: the hand-raising ring exists so that people
  arriving during a long copy can contribute instead of waiting. If nothing
  copies anymore, there's no long operation to contribute during — adding a
  page is a pointer write. The pattern is still good and still

  worth having written down for anything that genuinely needs a long exclusive
  stretch. It just might not have a customer left in this engine.


can you tell me about the hand raising ring? would the page buffer replace that
system well?


  Skip the gap: take any ready cell, ignore the one being written. Nothing ever
  blocks. The cost is that values leave in a different order than they arrived
  — and silently. A station pairing two inputs positionally would mismatch
  every pair from the first reorder onward, and nothing

  would tell you.


question... if we're looking for "ready" boxes after filling in a value, will we
have times where the "head" pointer is pointing at an empty cell, but there's a
valid value before and after it? what do we do in that case? the box would seem
unready, but it's actually totally ready to enter the task queue.


  "There will always be boxes the engine doesn't know." Fair, and I should be
  precise about what it does know: the generator reads your box sources and
  writes down each box's name and the types it takes and returns. That's all.
  The engine knows nothing about what a box does. So a

  refusal for an unknown name means "nothing with that name was compiled in" —
  a typo or a missing file — not a judgment about the box. You're right that
  they're paintings; the registry is just the list of titles.


we should be able to add boxes at runtime that weren't compiled in. If we need
to, we can shell out to a compiler teehee. We should be able to gather the name,
the arguments, and the return values, just from the function signature. That
should be enough right?


  "A program that never runs is an error." I want to take this one but I need to
  check what you mean, because it collides with something.


  Today's rule is: if a station could never run, start it. That's the engine
  inventing work, and I think that's what you're objecting to. But that same
  rule is what starts normal programs — a reader station with no inputs gets
  one task, runs, and pushes values downstream, and

  everything follows from that. Take the rule away and nothing starts at all.


  So which is it: the rule is fine but a program where it fires zero times
  should be refused, or the rule itself is wrong and something else should say
  what starts?


  And I think this is your diversion wearing a different hat. "Where does the
  first ring buffer come from" and "what starts a program" are the same question
  — both are asking for the base case that everything else is derived from. It
  might be worth taking the diversion now rather

  than later, since we keep walking into it.



hmmmmmmm, I see. How about we identify any boxes who have ring buffers that are
fed entirely by input-only boxes? Then, we create a task to start that
particular box. If we find multiple, great. I think that might be the only box
we can seed - a box with at least one ring buffer and only input-only inputs.
What do we do if one doesn't exist?


--------------------------------------------------------------------------------

### Assistant Response 18

## Your lock-free claim — it works, and there's one trap with a one-line fix


Walk the ports, atomically flip a ready cell to **claimed** at each. Get through
all of them and the invocation is real, so pop them. Hit a port with nothing
ready and walk back, flipping your claimed cells to **ready** again. That's a
complete design and it removes the station's mutex from the hot path.


**The trap is livelock.** Two threads arrive at a two-input station. Thread A
claims port one; thread B claims port two. A reaches port two — nothing ready,
B has it — so A rolls back. B reaches port one — nothing ready, A has it —
so B rolls back. Now both values are available again, both threads retry, and
they can hit the same interleaving forever. Nobody is blocked, nobody makes
progress, and the station has a complete input set sitting right there the whole
time.


**The fix is to always walk the ports in the same order**, lowest index first.
Then A and B both reach for port one, one of them wins outright, and the loser
fails at the first step having claimed nothing — no rollback, no wasted work,
and the winner sails through. It's the oldest deadlock-avoidance rule there is:
take your locks in a fixed order and two people can't grab them crosswise.


**What it costs:** finding a ready cell becomes a scan instead of a single look.
With ten cells that's nothing. Under a load that grew a port to four hundred
cells, scanning from zero every time starts to matter — so you'd want a "start
looking here" hint that advances opportunistically. Not a correctness invariant,
just a bookmark, so it can be wrong without breaking anything.


**And you're right about ordering, with one caveat worth writing down.** Values
arriving at a port from two different upstream stations are *already* in
whatever order the threads happened to produce them — so preserving arrival
order preserves an order that was arbitrary to begin with. It isn't a promise
worth slowing the fast path for.


The caveat: it is currently *true*, and there's a test that checks it —
several hundred deliveries of a struct-and-int pair, every pair byte-identical
and in order. So this isn't dropping a promise nobody made; it's dropping one
the engine quietly keeps. That test has to change and the guarantees document
needs a new line saying values may leave a port in a different order than they
arrived. Writing down "we stopped promising this" is much better than letting it
quietly become false.


**Now the interesting part.** If the claim goes lock-free, what's left for the
station's mutex? Growth, rewiring, static writes, tag changes — all rare, all
structural. And there's already a program-wide rewiring lock for exactly that
class of thing. If the rare operations move to it, **a station contains no mutex
at all** — and then a station can move, and then the station table can grow by
plain reallocation, and **shelves stop being necessary.**


I'm not certain of that yet. The piece I can't see through is the delivery walk,
which snapshots a station's destination list under that station's mutex so a
walker can't be left holding a freed wire. That needs some other answer before
the mutex can go. But the prize is real: an entire structural problem deleted
rather than solved.


## One correction, because you'll carry it forward


Memory that's physically closer is **faster**, not slower. What's expensive is
*sharing*.


Each core has its own small private cache, and it works in 64-byte chunks called
cache lines. When core A writes to a line, the hardware invalidates that line in
every other core's cache. Core B's next read of *anything* in those 64 bytes —
even a byte A never touched — has to go fetch it again. A hit in the private
cache is a few cycles; refetching a line another core just dirtied is tens to
hundreds.


So closeness is good for one core and expensive for several cores writing to the
same neighbourhood. That's the whole of it: **locality is fast, contended
locality is slow.** Which is why the cell states go on the cell, and why the
mutexes stay inside the stations.


## The hand-raising ring, and whether pages replace it


It's yours, from the note about growing the station table. The shape:


When something needs to grow and there's no room, the thread that noticed
becomes the **builder**. Anyone else who arrives during the build doesn't block
until it finishes. They add themselves to a small ring of waiting contributors
— raise a hand — and wait until the builder reaches a convenient point; the
builder then lets them add their own entry and update the running counts, and
carries on. The builder is done when no hands are up. Your words for it: you
show up, you say *excuse me, can you do this thing for me?*, they say *sure, put
it over there*, they update their count, and they trust you rather than checking
your work.


**Would paging replace it? For everything in this engine, yes — by making it
unnecessary rather than by doing its job.** The ring exists so people can
contribute *during a long copy*. Paged growth has no long copy; adding a page is
one pointer write. There's nothing to raise a hand during.


But the pattern is bigger than its customer here. It's the general answer to
"one thread must do something long and exclusive, and others need to get work in
meanwhile." That belongs in `strategems/` as a pattern rather than in this
engine as an implementation — which is what that directory is for.


## Your head-pointing-at-a-gap question answers itself, and it's a good one


Under strict head-and-tail: **it can't happen.** Head only advances when the
value at head is taken, and cells are only emptied at head, so head is always
sitting on the oldest occupied cell. There's never a hole behind a value.


Under the design you just proposed: **it happens constantly**, because rollback
releases cells wherever they are, so gaps open in the middle.


Which means the two can't be mixed — and that's the argument for your version
rather than against it. Once cells can be claimed and released out of order,
head and tail are lying about where the values are, so they stop being worth
maintaining and you scan. You asked the question that proves it.


## Runtime boxes and shelling out to a compiler — yes, and the signature isn't quite enough


A signature gives you names: the box's name, its parameter type names, its
return type name. What it can't give you is **sizes and offsets**, and those are
exactly what the engine runs on — how many bytes a cell holds, how far into a
struct a field sits, how big a task must be. The whole design's rule is that the
compiler computes every one of those and the generator never guesses.


So the signature gets you the paperwork and the compiler gets you the numbers,
which is what you said. The flow: take the C source, run the generator over it
to produce the shim and the registry entry, compile that into a shared library,
load it, look up the shim, add the row. All of it works on Linux today.


Two things it drags in. **The registry becomes a growable table** — and it's
indexed by name at load and by row afterward, so it's the same paging pattern
for the third time this conversation. And there's a hazard: **the engine
compares types by name.** Two structs both called `vec3`, compiled separately
with different field layouts, pass the check and then corrupt each other
silently. The generated field tables are the way out — compare the layouts
rather than the names — but nothing does that today because nothing needed it.


## Seeding — you found the hole yourself, and I think the output station fills it


Your rule is *a station with at least one ring buffer whose inputs all come from
input-only boxes.* And then: *what do we do if one doesn't exist?*


That question is the hole. Work the definition down: a station is input-only if
all of its inputs are constants or come from other input-only stations. That
bottoms out at stations with only statics or no inputs at all — which makes
them input-only, which makes everything downstream of them input-only, all the
way through. In a program where nothing declares otherwise, *every* station
qualifies, so nothing has a ring buffer, so your rule matches nothing. The base
case isn't in the graph.


**But there's already something outside the graph that says what the program is
for: the output station.** A program declares which station its results come
from — that's the issue sitting further down this same spine, and it exists so
a program can be used as a box inside another one.


Make that the base case. **The output station is what gets started.** It runs,
which pulls its inputs, which pull theirs, all the way up the tree. That's not
the engine inventing work to justify itself — it's the author having said
"this is the point of the whole thing," and the engine starting at the point.


And your instinct about refusing programs gets a much better error message out
of it. Instead of *"nothing to seed"* — which tells an author nothing about
what to do — it becomes **"this program never says where its results come
from."** That's a sentence someone can act on.


What it doesn't answer is where *repetition* comes from: this starts a program
once and lets it compute one answer. A program that runs forever needs something
else. That's still the diversion, and it's still waiting.


--------------------------------------------------------------------------------

### User Request 19

● Your lock-free claim — it works, and there's one trap with a one-line fix


  Walk the ports, atomically flip a ready cell to claimed at each. Get through
  all of them and the invocation is real, so pop them. Hit a port with nothing
  ready and walk back, flipping your claimed cells to ready again. That's a
  complete design and it removes the station's mutex

  from the hot path.


  The trap is livelock. Two threads arrive at a two-input station. Thread A
  claims port one; thread B claims port two. A reaches port two — nothing
  ready, B has it — so A rolls back. B reaches port one — nothing ready, A
  has it — so B rolls back. Now both values are available again,

  both threads retry, and they can hit the same interleaving forever. Nobody is
  blocked, nobody makes progress, and the station has a complete input set
  sitting right there the whole time.


  The fix is to always walk the ports in the same order, lowest index first.
  Then A and B both reach for port one, one of them wins outright, and the loser
  fails at the first step having claimed nothing — no rollback, no wasted
  work, and the winner sails through. It's the oldest

  deadlock-avoidance rule there is: take your locks in a fixed order and two
  people can't grab them crosswise.


  What it costs: finding a ready cell becomes a scan instead of a single look.
  With ten cells that's nothing. Under a load that grew a port to four hundred
  cells, scanning from zero every time starts to matter — so you'd want a
  "start looking here" hint that advances

  opportunistically. Not a correctness invariant, just a bookmark, so it can be
  wrong without breaking anything.



great sounds good


  Now the interesting part. If the claim goes lock-free, what's left for the
  station's mutex? Growth, rewiring, static writes, tag changes — all rare,
  all structural. And there's already a program-wide rewiring lock for exactly
  that class of thing. If the rare operations move to it,

  a station contains no mutex at all — and then a station can move, and then
  the station table can grow by plain reallocation, and shelves stop being
  necessary.


  I'm not certain of that yet. The piece I can't see through is the delivery
  walk, which snapshots a station's destination list under that station's mutex
  so a walker can't be left holding a freed wire. That needs some other answer
  before the mutex can go. But the prize is real: an

  entire structural problem deleted rather than solved.



pretty hyped about this ngl can we work through it together? maybe we can
cocreate a solution.


  The hand-raising ring, and whether pages replace it


  It's yours, from the note about growing the station table. The shape:


  When something needs to grow and there's no room, the thread that noticed
  becomes the builder. Anyone else who arrives during the build doesn't block
  until it finishes. They add themselves to a small ring of waiting contributors
  — raise a hand — and wait until the builder reaches a

  convenient point; the builder then lets them add their own entry and update
  the running counts, and carries on. The builder is done when no hands are up.
  Your words for it: you show up, you say excuse me, can you do this thing for
  me?, they say sure, put it over there, they update

  their count, and they trust you rather than checking your work.


  Would paging replace it? For everything in this engine, yes — by making it
  unnecessary rather than by doing its job. The ring exists so people can
  contribute during a long copy. Paged growth has no long copy; adding a page is
  one pointer write. There's nothing to raise a hand

  during.


  But the pattern is bigger than its customer here. It's the general answer to
  "one thread must do something long and exclusive, and others need to get work
  in meanwhile." That belongs in strategems/ as a pattern rather than in this
  engine as an implementation — which is what that

  directory is for.



great. how about we pick whichever seems faster and less complex, and write the
other down into strategems, to be applied later?


  Two things it drags in. The registry becomes a growable table — and it's
  indexed by name at load and by row afterward, so it's the same paging pattern
  for the third time this conversation. And there's a hazard: the engine
  compares types by name. Two structs both called vec3,

  compiled separately with different field layouts, pass the check and then
  corrupt each other silently. The generated field tables are the way out —
  compare the layouts rather than the names — but nothing does that today
  because nothing needed it.


well, we should make sure we compare by layout instead of name. I absolutely see
that being a common case. Plus, let's say there's a struct called "vec4" and a
struct called "stats" and one has x, y, z, and g values, while the other has
strength, agility, wisdom, and fondness. Of course, if they're all integers, we
should treat that the same. Sometimes we don't want to carry the same name from
one box to another - what matters is the pattern of data.


  But there's already something outside the graph that says what the program is
  for: the output station. A program declares which station its results come
  from — that's the issue sitting further down this same spine, and it exists
  so a program can be used as a box inside another one.


  Make that the base case. The output station is what gets started. It runs,
  which pulls its inputs, which pull theirs, all the way up the tree. That's not
  the engine inventing work to justify itself — it's the author having said
  "this is the point of the whole thing," and the engine

  starting at the point.



okay so... how about we look at the single output station. It might have many
inputs, and it might have many outputs, because output stations are just like
that. They aren't a function, they're just for transmitting results out of the
box - either to the OS as a file or something, or to another box as a
sub-map-box in a larger mapgraph. so, we start at the output box, of which there
is one, (by the way we might need to connect output box inputs to their
outputs?) and then we walk back along it's upstream until we find the input-only
leaf nodes. Then, we build a task for the box that those input-only leaf nodes
are connected to (input-only nodes never get tasks) and then it should propagate
if we get all of them. Right? What happens if it's not wired properly, does it
just... break? Maybe it should!


  What it doesn't answer is where repetition comes from: this starts a program
  once and lets it compute one answer. A program that runs forever needs
  something else. That's still the diversion, and it's still waiting.


to run a program forever, just have a mechanism like this: input-only box that
provides a static to a comparator. If the static is 1, keep running. otherwise,
break. Then, check that at the start of each "loop" through the program. Easy.
If you ever DO want to quit, then change the static to 0. Easy. Was there more
complexity I'm missing?


--------------------------------------------------------------------------------

### Assistant Response 19

Let me write the strategem first since that one's decided, then work the mutex
through with you.


## Working the mutex out — here's where I've got to, and where I'm stuck


**Step one: list what the station's mutex is still holding once the claim goes
lock-free.**


| what | how often | why it needs exclusivity |

|---|---|---|

| the destination list, during a delivery walk | **every value** | a rewire can free a node under a walker |

| ring buffer growth | rare | the cells move |

| a static write | rare | a claim must not read half of it |

| changing a port's source tag | rare | the port changes meaning |

| the iterator's cursor | every value | two tasks must not get the same port |


The cursor's easy — that's an atomic increment, no lock. So one hot-path user
is left: **the destination list.**


**Step two: my proposal for it.** Stop making the walker copy the list, and make
the list something that can't be pulled out from under it. A port's destinations
become **one immutable array**, and the port holds a pointer to it. Rewiring
doesn't edit that array — it builds a whole new one and swaps the pointer in a
single atomic write. A walker reads the pointer once and walks whatever it got,
which nobody will ever modify.


Three things fall out. The walker takes no lock and makes no copy — it reads
one pointer. Old and new readers both see a coherent set of destinations, never
a half-edited one. And it fits the decision you already made that wires get
attached in batches: you were going to rebuild the list anyway, so building an
array is no extra work.


**Step three is the part I'm least sure of, and it's the real question: when is
it safe to free the old array?** A walker might still be inside it. Three ways I
know:


- **Never free.** Leak one array per rewire. Honest, and for a program that
  rewires a handful of times it's genuinely fine. For a control loop that
  rewires continuously it's a slow leak.

- **Count the walkers.** An atomic increment when a walker takes the pointer, a
  decrement when it's done; the freer waits for zero. Simple, but that counter
  is touched on every single delivery by every core — the exact contended
  cache line we've been avoiding all afternoon.

- **Wait for a natural quiet moment.** This is the one I like for this engine
  specifically. A walker only holds an array between popping a task and
  finishing with it. So if each worker keeps a private counter of how many walks
  it has completed — its own line, never touched by anyone else, no contention
  at all — then a freer can note everyone's counter, and once every worker's
  number has moved, nobody can still be holding anything from before. Rare
  operation pays; the hot path pays nothing.


**Step four, which finishes it:** the four rare operations left still need
exclusivity, but they don't need a lock *inside the station*. Put a fixed array
of, say, sixty-four locks somewhere, and structural work on station N takes lock
number N modulo sixty-four. Two unrelated stations occasionally wait on each
other — for an operation that happens a handful of times in a program's life.
That array is allocated once, never grows, never moves. **And the station record
now contains no mutex at all.**


**But here's my honest doubt about the prize.** I said this would kill shelves.
Working it through, I don't think it quite does. Stations become *movable*, yes
— but a delivery resolves a station index into an address on every single
value, and if the table gets reallocated between the resolve and the use, you're
holding a dangling station instead of a dangling wire. Same problem, moved.
Shelves avoid it by never moving anything, and they cost one extra memory hop.


So I think these are two separate wins rather than one: **the mutex removal is
worth doing for the hot path no matter what, and shelves are worth keeping no
matter what.** Killing the mutex was never really about moving stations — it's
about not taking a lock on every value.


Where do you want to push on this? The reclamation step is the one with the most
room in it.


## Paging wins; the strategem is written


Paging is both faster and simpler — nothing to copy, nothing to coordinate,
one pointer write. The hand-raising ring is now in `strategems/`, written up as
a pattern with the manners intact, plus the thing that displaced it: **before
reaching for it, ask whether the long exclusive operation can be made short
instead.** That's the general lesson, and it's why the pattern had no customer
here.


## Structural typing — taken, and it's bigger than the bug it fixes


You're right that it fixes the silent corruption, and right that the interesting
part is the other direction: four integers are four integers, and a box that
works on them shouldn't care whether the author called them x,y,z,g or
strength,agility,wisdom,fondness. The field tables already carry everything
needed — kind, size, offset, per field, recursively — so the comparison is
available; nothing has ever asked for it.


**The trade to write down:** two structs with identical layout and different
meaning become interchangeable, silently. A four-float position and a four-float
colour would wire together happily. That's the cost of caring about the pattern
of the data rather than the label on it, and it's a deliberate choice rather
than an oversight — so it belongs in the guarantees document as a stated
non-guarantee, not buried in a commit.


This wants its own issue. It touches the registry, the loader's wire check, the
error messages, and the runtime-box-loading case, and it changes what the word
"type" means in this engine.


## The output station being a pass-through — that's a real catch


You're right and it resolves something. If an output station isn't a function
but a transmission point, then **its inputs are the map's outputs**. A parent
wiring into a sub-map's output port is reading whatever arrived on the
corresponding input of that station. No box runs, nothing is computed, values
just cross the boundary. That also lines up with the multi-output work: a map
with several output ports is several wires, not a struct.


Which means your parenthetical answers itself — yes, they connect, and the
connection is identity.


## The loop — it works, and it might be the base case we keep circling


Your mechanism is right and phase four already found half of it independently: a
push cycle is legal, and it needs a finite companion input to ever stop. Two
things you're missing, both small.


**Stopping is a drain, not a halt.** Flip the static to zero and values already
past the comparator keep going around one more time. The loop empties rather
than freezing. That's almost certainly what you want, but it means "stop" and
"stopped" are a few passes apart.


**Termination gets its meaning back.** While the loop runs, the pool never goes
idle, so the last-sleeper rule never fires — correct, since the program isn't
finished. When the comparator finally routes out instead of back, the loop
drains, the pool goes quiet, and termination fires normally. Nothing special is
needed.


And here's why I think this is more than a loop. **A ring buffer is where a push
cycle closes** — you can't pull around a cycle, because gather cycles are
refused as calls that never return. So a cycle *forces* a ring buffer, and
that's a property sitting right there in the graph, which is exactly the base
case the derivation has been missing every time we've circled it.


I'd like to take that away and work it through properly rather than half-solve
it in-line for the fifth time. If it holds, the whole push-versus-pull question
closes with no new terms — which is what you asked for four turns ago.


--------------------------------------------------------------------------------

### User Request 20

  Structural typing — taken, and it's bigger than the bug it fixes


  You're right that it fixes the silent corruption, and right that the
  interesting part is the other direction: four integers are four integers, and
  a box that works on them shouldn't care whether the author called them x,y,z,g
  or strength,agility,wisdom,fondness. The field tables

  already carry everything needed — kind, size, offset, per field, recursively
  — so the comparison is available; nothing has ever asked for it.


  The trade to write down: two structs with identical layout and different
  meaning become interchangeable, silently. A four-float position and a
  four-float colour would wire together happily. That's the cost of caring about
  the pattern of the data rather than the label on it, and

  it's a deliberate choice rather than an oversight — so it belongs in the
  guarantees document as a stated non-guarantee, not buried in a commit.


  This wants its own issue. It touches the registry, the loader's wire check,
  the error messages, and the runtime-box-loading case, and it changes what the
  word "type" means in this engine.



great let's make that issue file


  The loop — it works, and it might be the base case we keep circling


  Your mechanism is right and phase four already found half of it independently:
  a push cycle is legal, and it needs a finite companion input to ever stop. Two
  things you're missing, both small.


  Stopping is a drain, not a halt. Flip the static to zero and values already
  past the comparator keep going around one more time. The loop empties rather
  than freezing. That's almost certainly what you want, but it means "stop" and
  "stopped" are a few passes apart.


  Termination gets its meaning back. While the loop runs, the pool never goes
  idle, so the last-sleeper rule never fires — correct, since the program
  isn't finished. When the comparator finally routes out instead of back, the
  loop drains, the pool goes quiet, and termination fires

  normally. Nothing special is needed.


  And here's why I think this is more than a loop. A ring buffer is where a push
  cycle closes — you can't pull around a cycle, because gather cycles are
  refused as calls that never return. So a cycle forces a ring buffer, and
  that's a property sitting right there in the graph, which

  is exactly the base case the derivation has been missing every time we've
  circled it.


  I'd like to take that away and work it through properly rather than half-solve
  it in-line for the fifth time. If it holds, the whole push-versus-pull
  question closes with no new terms — which is what you asked for four turns
  ago.



let's talk about this a bit more? you seem concerned at the end.


  Step two: my proposal for it. Stop making the walker copy the list, and make
  the list something that can't be pulled out from under it. A port's
  destinations become one immutable array, and the port holds a pointer to it.
  Rewiring doesn't edit that array — it builds a whole new one

  and swaps the pointer in a single atomic write. A walker reads the pointer
  once and walks whatever it got, which nobody will ever modify.


  Three things fall out. The walker takes no lock and makes no copy — it reads
  one pointer. Old and new readers both see a coherent set of destinations,
  never a half-edited one. And it fits the decision you already made that wires
  get attached in batches: you were going to rebuild

  the list anyway, so building an array is no extra work.



great sounds good


  Step three is the part I'm least sure of, and it's the real question: when is
  it safe to free the old array? A walker might still be inside it. Three ways I
  know:


what if we write the destinations into the task struct when we're creating it?
what if the addresses change... okay that's not ideal. what if we, upon
finishing a box function, copy each destination address into the worker's memory
and then there's nothing to free? the problem is, what if they change partway
through the copying. hmmm what if we make a "list of destinations" into an
atomic unit that can only be copied completely? Then, we can safely copy the
list into a worker, they can deliver their goods even if there's a rewiring
happening during their delivery. They free their copied list as part of their
cleanup, and it's fine. Is that doable?


  Step four, which finishes it: the four rare operations left still need
  exclusivity, but they don't need a lock inside the station. Put a fixed array
  of, say, sixty-four locks somewhere, and structural work on station N takes
  lock number N modulo sixty-four. Two unrelated stations

  occasionally wait on each other — for an operation that happens a handful of
  times in a program's life. That array is allocated once, never grows, never
  moves. And the station record now contains no mutex at all.


I don't like this because it's hard to scale it... tell me again why we can't
just have an array with space for each instantiated box station's mutex, and
just use that, referred to by index, not pointer? We could use pages if we are
concerned about growing the list (which I am)...


  But here's my honest doubt about the prize. I said this would kill shelves.
  Working it through, I don't think it quite does. Stations become movable, yes
  — but a delivery resolves a station index into an address on every single
  value, and if the table gets reallocated between the

  resolve and the use, you're holding a dangling station instead of a dangling
  wire. Same problem, moved. Shelves avoid it by never moving anything, and they
  cost one extra memory hop.


tell me again why shelves don't need to move anything? what if we need to grow a
ring buffer?


--------------------------------------------------------------------------------

### Assistant Response 20

Let me make that issue first.


Issue written. Now your four questions.


## Why shelves don't move, when ring buffers do


Two different things live at two different levels, and only one of them is in
the shelf.


**The station record is fixed size.** Its kind, its counters, its cursor, its
pointers to its ports — all the same number of bytes for every station in the
program. That's what sits in a shelf, and that's what never moves.


**The ring buffer cells are not in the station.** The port holds a *pointer* to
them. When a buffer grows, you allocate new cells, copy across, and write the
new address into that pointer. The cells moved; the port that points at them did
not; the station holding that port did not.


So growth changes what a station points at, never where a station is. That's
already how it works today — the growth path is careful that only the storage
moves, which is what let the station-table test flood one buffer through seven
doublings and find every station's address unchanged.


**The general rule: fixed-size things live in shelves and never move.
Variable-size things live at the end of a pointer and may move whenever they
like.**


## The mutex array — you're right and I was wrong


My objection to a mutex array was false sharing: pack the locks together and
locking station three invalidates the cache line holding station four's lock.


**That objection only holds while the lock is on the hot path.** Once the claim
goes lock-free and the destination list is immutable, these locks are taken for
growth, rewiring, static writes, and tag changes — a handful of times in a
program's life. Nobody is contending for those cache lines because almost nobody
is touching them.


So: a paged array of mutexes, one per station, indexed by station number,
growing by adding a page. No striping, no collisions, scales exactly with the
program. The array never moves because pages never move. That's the fourth use
of the same pattern today, and it's better than my sixty-four-lock compromise in
every way I can find.


## Reclaiming the destination array — your instinct, and where it lands


Both of your ideas are right in a way that's worth separating.


**"Write the destinations into the task struct" — the addresses don't change,
so this is safer than you feared.** A destination is two integers, a station
number and a port number, never an address. Copying them is always safe. The
reason it doesn't quite work is different: *which* output port a value leaves by
isn't known until the box has returned — a comparator picks its port by
looking at the value. So at task-creation time you'd have to copy the
destinations of every port, which is more copying, not less.


**"An atomic unit that can only be copied completely" — you've described
exactly what the immutable array already is.** Nobody ever edits it; rewiring
builds a new one and swaps the pointer. So a copy out of it can never tear, no
matter what else is happening. There is nothing to protect *during* the copy.


The one thing neither idea removes: the old array has to be freed *sometime*,
and a worker might be reading it. Copying shortens the exposure from "a whole
delivery walk" to "one short copy," which is a real improvement, but it doesn't
make the question go away.


**The smallest thing that closes it: don't free during the run.** Rewiring
retires the old array onto a list and the list is emptied at teardown. A program
that rewires a few dozen times leaks a few kilobytes and nobody notices. That's
it — no counters, no epochs, no coordination.


If a program rewires continuously — a control loop turning knobs forever —
that leak becomes real, and *then* you want the quiet-moment trick: each worker
keeps a private count of walks it has finished, on its own cache line, touched
by nobody else. A retiring thread notes everyone's number and frees once every
worker's number has moved, because at that point nobody can still be holding
anything from before. It costs one uncontended write per delivery and it's
exact.


I'd build the first and leave a comment naming the second.


## What I was worried about — and I think it closes


Here's the thing I couldn't see past, and it's specific.


**A pull cannot cross a ring buffer.** Starting at the output station and
walking upward works beautifully right up until the walk reaches a station that
has a ring-buffer input — and there it stops, because that station's value
doesn't come from running something upstream, it comes from a delivery that
hasn't happened. A pure program has no such boundary, so the walk goes all the
way up and one seed at the output evaluates the whole thing. A program with a
loop has that boundary by definition, and the walk hits a wall.


So it looked like two different starting mechanisms for two different kinds of
program, which is exactly the sort of thing this design doesn't want.


**But it's one rule.** Start at the output station and walk up. Wherever the
walk reaches a station it *cannot* pull — because that station has a
ring-buffer input — **seed that station instead of pulling it.** If the walk
never hits one, seed the output station itself.


A pure program: nothing has a ring buffer, the walk reaches the top, the output
station is seeded, it pulls everything down through itself, one answer comes
out. A function, evaluated once.


A looping program: the walk goes up and hits the station where the cycle closes,
because that's where a ring buffer necessarily is — you can't pull around a
cycle, since a gather cycle is a call that never returns and is refused. So that
station is seeded. It runs, feeds the loop, the loop turns, values reach the
output.


Your version and mine are the same shape — you said walk back from the output
until you find the leaves and seed what they connect to. The only difference is
where the walk stops: yours stops at the input-only leaves, mine stops at the
first thing that can't be pulled. Mine seeds fewer stations and needs no notion
of a leaf.


And a ring buffer being *where a push cycle closes* is a fact about the drawing.
Nobody declares it, no new word appears in the file, and the loop you sketched
two messages ago is what supplies it.


That's the base case we've walked into five times. I'd like to write it up
properly — the derivation, the seeding rule, and what happens to a program
that has neither an output station nor a cycle, which I think is your "it just
shouldn't compile."


--------------------------------------------------------------------------------

### User Request 21

  The ring buffer cells are not in the station. The port holds a pointer to
  them. When a buffer grows, you allocate new cells, copy across, and write the
  new address into that pointer. The cells moved; the port that points at them
  did not; the station holding that port did not.


we should be careful here. allocate new cells, copy them across, and write the
new address to the pointer - however, if cells are popped while we're copying,
then we could accidentally duplicate them. What if we instead allocate new
cells, write the new address to the pointer, and then copy the old ones across?


  The general rule: fixed-size things live in shelves and never move.
  Variable-size things live at the end of a pointer and may move whenever they
  like.


great, write that down


  The mutex array — you're right and I was wrong


  My objection to a mutex array was false sharing: pack the locks together and
  locking station three invalidates the cache line holding station four's lock.


  That objection only holds while the lock is on the hot path. Once the claim
  goes lock-free and the destination list is immutable, these locks are taken
  for growth, rewiring, static writes, and tag changes — a handful of times in
  a program's life. Nobody is contending for those

  cache lines because almost nobody is touching them.


  So: a paged array of mutexes, one per station, indexed by station number,
  growing by adding a page. No striping, no collisions, scales exactly with the
  program. The array never moves because pages never move. That's the fourth use
  of the same pattern today, and it's better than my

  sixty-four-lock compromise in every way I can find.



is there a reason we don't just put it on the shelf with the station record?
those aren't supposed to move, right?


  If a program rewires continuously — a control loop turning knobs forever —
  that leak becomes real, and then you want the quiet-moment trick: each worker
  keeps a private count of walks it has finished, on its own cache line, touched
  by nobody else. A retiring thread notes everyone's

  number and frees once every worker's number has moved, because at that point
  nobody can still be holding anything from before. It costs one uncontended
  write per delivery and it's exact.


so... we don't want to leak memory ever, just-in-case. gotta protect our users
from themselves. so for this... we could increment the private count (integer
overflows are okay) and during the rewiring function, we'd look at each count at
the start, and at the end, we'd wait until they all have changed. do we... do we
need a mutex for the private count then? or can we just, keep checking until
it's changed? anyway if we wait until all of them change, then we can free it no
problem. We'd just increment that value every time we finish a task, off the hot
path, while we're cleaning up a worker's task they just finished. Does that
sound right?


  But it's one rule. Start at the output station and walk up. Wherever the walk
  reaches a station it cannot pull — because that station has a ring-buffer
  input — seed that station instead of pulling it. If the walk never hits one,
  seed the output station itself.


what do you mean by "seed that station" here? what values would we put in the
ring buffer to get the station to run it's box function?


--------------------------------------------------------------------------------

### Assistant Response 21

Your last question broke my rule, and fixing it closed the whole thing. Let me
take them in order.


## Ring growth ordering — you're right that copy-then-publish duplicates, and the fix is that there's no copy


Your duplication case is real: copy across, and a value popped from the old
buffer during the copy is already sitting in the new one, so it gets delivered
twice.


But flipping the order is worse, not better. Publish first and readers are
looking at an empty buffer while it fills — values that exist go missing, and
writers reserving cells in the new buffer take the very cells the copy is about
to overwrite. Duplication becomes corruption.


**Neither order is safe, because the real requirement is that nothing else
happens at all during the copy.** That's why growth holds the station's lock
today.


Which is a problem — right up until you notice something you already decided.
Ring cells had to be contiguous because head and tail are positions taken
*modulo the capacity*: change the capacity and every existing value is suddenly
at a different index. That's what the unwrap-and-copy is really for.


**Your lock-free claim design threw head and tail away.** Readers scan for a
usable cell now. And the moment nothing computes a position from the total size,
nothing cares that the total size changed — so a ring buffer can grow by
**adding a page**, exactly like everything else. No copy, no ordering question,
no exclusivity, nothing to get right.


That's the fifth use of the same pattern today, and it fell out of a decision
you made two messages ago for unrelated reasons. I've written the pattern into
`strategems/grow-by-adding.md` along with your rule about fixed-size versus
variable-size things — and the amendment that a variable-size thing which can
be paged stops moving too.


## The mutex on the shelf — yes, and I was overcomplicating


Right. Station records live in shelves, shelves never move, so a mutex *inside*
a station record never moves. There's no problem left for a separate array to
solve.


The separate array only ever mattered under the assumption that we wanted
stations to be movable — and we already concluded that buys nothing, because
delivery resolves an index into an address on every value, so a moving table
just relocates the dangling-pointer problem instead of removing it. Drop the
array. The lock stays where the thing it protects is, which is the same
conclusion as the cell states and for the same reason.


## The counter — no mutex, and don't wait


**No mutex.** Each worker writes only its own counter and nobody else ever
writes it, so there's no write-against-write race. The reader just needs the
read not to tear, which a properly declared atomic integer gives you with the
weakest possible ordering — no fences, no cost worth measuring.


**But don't spin waiting.** Better: the retiring thread doesn't wait at all. It
puts the old array on a retire list together with a snapshot of everyone's
counters, and walks away. Any *later* rewiring — or a periodic sweep —
checks that list and frees whatever has become safe. Nobody blocks, and the
memory comes back.


Three details that bite:


**Use 64-bit counters.** You're right that overflow is fine in principle, since
the test is "did it change" rather than "is it bigger." But a 32-bit counter can
wrap all the way around to the *same* value in a long run, and then it looks
unchanged when it isn't. At 64 bits that's unreachable.


**A sleeping worker never increments**, so waiting for its counter to move waits
forever. But a sleeping worker has finished its task and is holding nothing —
so the test is "changed, *or* asleep." Cleanest version: increment when a walk
starts and again when it ends, so the counter is odd while walking and even
while not. Then "even" means safe without any comparison at all.


**Increment where you said** — during cleanup, after the task is done, off the
hot path.


## Seeding — your question broke my rule, and the repair closes the whole thing


You asked what values we'd put in the ring buffer. **There aren't any, and
that's fatal to what I proposed.**


Seeding works today precisely because it only ever touches stations with *no*
ring-buffer inputs — every input is a static or a pull, both of which are
always available, so a task can be built needing nothing from anywhere. My rule
said to seed the station that *has* a ring input, which is the one station you
cannot possibly seed. You'd have to invent a value out of nothing.


So the rule reverts to what it already was: **enqueue every station that needs
nothing delivered to it and that nobody pulls from.** What I was actually
reaching for is one step earlier in the chain.


Here's the repair, and it's smaller than the thing it replaces.


**A cycle forces ring buffers.** You cannot pull around a cycle — a gather
cycle is a call that never returns, and the engine refuses those when the wire
is drawn. So a station in a push cycle has ring-buffer ports, necessarily, and
that's visible in the drawing rather than declared anywhere.


**A wire into a ring buffer is a push.** So whatever feeds a cycle from outside
is pushing into it, not being pulled by it.


**And that feeder is what gets seeded** — it has no ring inputs of its own,
and nobody pulls it, so it satisfies the existing rule exactly. It runs once,
drops a value into the cycle, and the cycle turns from there.


Walk your loop through it. A generator wired into an accumulator, and the
accumulator wired back to itself. The self-wire is a cycle, so that port is a
ring. The generator feeds the same port, so it pushes. The generator has no ring
inputs and nothing pulls it, so it's seeded. It runs, one value lands, the
accumulator becomes ready, runs, pushes its result back into its own port, and
goes around again — until your comparator's static flips and it routes out
instead of back. That's exactly the counter-through-a-ring-buffer that phase
four already built and tested, arrived at from the other direction.


And a program with no cycle has no ring buffers at all, so everything is pulled,
and the only station needing nothing and pulled by nobody is the output station.
It gets seeded, pulls the whole tree down through itself, and produces one
answer. A function, evaluated once.


**One rule, one seeding mechanism, no new words in the file.** The base case is
"a port in a cycle is a ring," and cycles are something you can see by looking.


Two things I'd still want to check before writing it up: what happens when a
program has neither a cycle nor an output station — I think that's your "it
shouldn't compile," and now there's a precise sentence for the error. And
whether a cycle that nothing feeds from outside can be detected and refused,
since it would sit there forever waiting for a first value that no one will ever
send.


--------------------------------------------------------------------------------

### User Request 22

  Which is a problem — right up until you notice something you already
  decided. Ring cells had to be contiguous because head and tail are positions
  taken modulo the capacity: change the capacity and every existing value is
  suddenly at a different index. That's what the

  unwrap-and-copy is really for.


whoa, hold on there partner. what do you mean, head and tail are modulus'd by
the capacity? what on earth for?


● Ring growth ordering — you're right that copy-then-publish duplicates, and
the fix is that there's no copy


  Your duplication case is real: copy across, and a value popped from the old
  buffer during the copy is already sitting in the new one, so it gets delivered
  twice.


  But flipping the order is worse, not better. Publish first and readers are
  looking at an empty buffer while it fills — values that exist go missing,
  and writers reserving cells in the new buffer take the very cells the copy is
  about to overwrite. Duplication becomes corruption.


got it. so we need like a flag or something that says "you can write into this
new buffer while we fill it, but if you iterate through looking for a value and
it's empty, then wait until we're done copying, then search again" or
something...


  Your lock-free claim design threw head and tail away. Readers scan for a
  usable cell now. And the moment nothing computes a position from the total
  size, nothing cares that the total size changed — so a ring buffer can grow
  by adding a page, exactly like everything else. No copy,

  no ordering question, no exclusivity, nothing to get right.


does this mean we need to iterate through the list looking for an empty slot
every time we write? Kinda seems like having at least one write-head cursor
could be a good thing here, so we don't have to do that. Then, we can always
pull from the beginning after iterating.


  But don't spin waiting. Better: the retiring thread doesn't wait at all. It
  puts the old array on a retire list together with a snapshot of everyone's
  counters, and walks away. Any later rewiring — or a periodic sweep —
  checks that list and frees whatever has become safe. Nobody

  blocks, and the memory comes back.


should we add the counter to the retire list when we start delivery in each
worker, indexed to the thread ID? then, when we start the rewiring process,
we... what, iterate through the retire list and make sure that everyone's
counter has changed, and if it has, then free the old list before doing the
rewiring process? what if the counters haven't changed? I could see that
happening if there's no work to do in the entire box for example. Do we just,
not free it then? i think that'd be acceptable, it's not a leak, it's just... a
scrapyard, waiting to be recycled.


  A sleeping worker never increments, so waiting for its counter to move waits
  forever. But a sleeping worker has finished its task and is holding nothing
  — so the test is "changed, or asleep." Cleanest version: increment when a
  walk starts and again when it ends, so the counter is

  odd while walking and even while not. Then "even" means safe without any
  comparison at all.


cool


  So the rule reverts to what it already was: enqueue every station that needs
  nothing delivered to it and that nobody pulls from. What I was actually
  reaching for is one step earlier in the chain.


so, a station with no inputs (or input-only inputs) that is delivering into a
ring buffer, which might include the output station?


  A cycle forces ring buffers. You cannot pull around a cycle — a gather cycle
  is a call that never returns, and the engine refuses those when the wire is
  drawn. So a station in a push cycle has ring-buffer ports, necessarily, and
  that's visible in the drawing rather than declared

  anywhere.


sorry, what do you mean by "cycle"?


  And a program with no cycle has no ring buffers at all, so everything is
  pulled, and the only station needing nothing and pulled by nobody is the
  output station. It gets seeded, pulls the whole tree down through itself, and
  produces one answer. A function, evaluated once.


brilliant. we could conceptualize any C program as a network of functions. Each
of those functions, if pure, could be considered a boxmapgraph. what a concept.


  Two things I'd still want to check before writing it up: what happens when a
  program has neither a cycle nor an output station — I think that's your "it
  shouldn't compile," and now there's a precise sentence for the error. And
  whether a cycle that nothing feeds from outside can be

  detected and refused, since it would sit there forever waiting for a first
  value that no one will ever send.


no output station = error.


And whether a cycle that nothing feeds from outside can be

  detected and refused, since it would sit there forever waiting for a first
  value that no one will ever send.


that's fine. We might wire it up later. it's not an infinite loop because it
isn't spinning. no worker thread picked it up, so... it just sits there, in
memory, waiting to be wired.


--------------------------------------------------------------------------------

### Assistant Response 22

## What "modulo the capacity" means, and why it's there


A ring buffer is an array pretending to be a circle. Say ten cells. Writes go in
at `tail`, reads come out at `head`, and both move forward as you use them. But
the array ends at cell nine — so when `tail` gets there, it wraps back around
to cell zero and keeps going. That wrap is the modulo: after cell nine comes
cell nine-plus-one-modulo-ten, which is zero.


That's the whole trick, and it's why it's called a ring. Without the wrap the
positions would climb forever and walk straight off the end of the array.


**And here's why growing it is painful.** Say capacity four, with head at cell
two and tail at cell one — meaning the values are physically sitting at cells
two, three, and zero, in that order, having wrapped around. Now grow to eight
cells. The values haven't moved, but the *wrap point* has: a reader starting at
cell two now walks two, three, four, five — and cell zero, which holds the
third value, is nowhere in that walk. The values are all still there and the
arithmetic can no longer find them.


So growth has to physically shuffle them back into order and reset the
positions. That's the unwrap, and it's real work rather than mere relocation.
It's the one thing that made a ring buffer look different from everything else
we've paged today.


## Your growth flag — good, and now unnecessary


The protocol you sketched is right in shape: let writers into the new storage
while the copy is happening, and make a reader that finds nothing wait and look
again rather than concluding the buffer is empty. That works.


It's just that once nothing computes a position from the capacity, **there's
nothing to copy** — you add a page and the existing cells stay exactly where
they are, still findable, still holding their values. The flag has no window to
guard. Worth keeping in your pocket for something that genuinely must copy.


## The write cursor — yes, and the distinction is everything


You're right that scanning for an empty cell on every write is unacceptable.
Keep the cursor.


But there's a difference between two things that look the same, and it's the
difference that makes paging work:


**A position must be exact.** "The next write goes at cell seven." If it's
wrong, you corrupt something. And because it's exact, it's computed from the
capacity — which is precisely what breaks when the capacity changes.


**A hint can be wrong.** "Start looking around cell seven." A writer begins
there, scans forward for an empty cell, takes the first one it finds, and nudges
the hint along. If the hint is stale, or points into an older page, or points at
a cell somebody else just took, nothing breaks — the scan simply walks a
little further. It never has to be recomputed, because nothing depends on it
being right.


So: a write hint and a read hint, both advisory. Typical cost is one look; worst
case is a scan; and neither cares what the capacity is or when it changed.
That's the whole reason a ring buffer can join the paging pattern — you gave
up the exact positions when you moved to per-cell states, and hints are what you
get back.


## The scrapyard — that's the right word for it and the right design


Almost exactly right, with one correction to where the counters live.


The counters live **per worker** — one number each, that worker's own, which
only it ever writes. The retire list holds *snapshots* of all of them: when you
retire an old array, you write down everybody's number as it was at that moment,
and file the array alongside it.


Then any later rewiring sweeps the scrapyard first. For each item, check each
worker: **is its number even now, or is it different from the snapshot?** Either
one means that worker has moved on from whatever it was doing at retirement. If
every worker passes, free it. If not, leave it filed and check again next time.


Your worry about counters that never change is handled by the odd-even half. A
worker with nothing to do is asleep, holding no task and therefore no array —
and asleep means even, so it passes without ever having to move. Nothing waits
on an idle worker.


And yes: an item that never becomes free just stays filed. Not a leak —
nothing is lost track of, and everything left in the yard is freed at teardown.


## What a cycle is


Follow the arrows out of a station, then out of whatever they reach, and so on.
If you ever arrive back at the station you started from, that's a cycle.


**Your loop is one.** The accumulator's output goes to the accumulator's input,
so following its arrow gets you straight back where you began. A longer one
works the same: A feeds B, B feeds C, C feeds A.


There are two kinds and the engine treats them oppositely. A **push cycle** —
values being delivered around the loop — is legal, and it's how anything
repeats. A **gather cycle** — a station pulling from something that eventually
pulls from it — is refused when the wire is drawn, because pulling is a
function call, and a call that loops back to itself never returns.


That difference is what does the work: since you can't pull around a loop, every
port in a push cycle must be a ring buffer. Nobody declares it; the drawing
forces it.


## Your restatement of the seed rule — nearly, with one amendment


You said: a station with no inputs (or input-only inputs) that is delivering
into a ring buffer.


The first half is exactly right, and "delivering into a ring buffer" is a good
concrete way to say the second half — because if something is pushing, nothing
is pulling it. But it doesn't quite cover the pure-function case, where the
output station has nothing downstream at all.


So: **a station that needs nothing delivered to it, and that nothing pulls
from.** In a looping program that's the feeder priming the cycle. In a pure
program it's the output station itself. Same sentence, both cases.


## And your two decisions are recorded


No output station is an error — with the message naming that, rather than
reporting some downstream symptom like "nothing to enqueue."


An unfed cycle is fine. Your reasoning is written down as you gave it: nothing
spins, no worker picks it up, it just sits in memory waiting to be wired. It's
the same call as an unwired station being ordinary, applied to a bigger shape.


---


On the C program observation — I think it goes further than an analogy. The
pure functions become graphs with no cycles, evaluated once when something asks.
The ones that *loop* become graphs with cycles. And the ones that touch the
world are the leaves that get pulled. Three kinds of C function, three shapes of
drawing, and nothing else needed to tell them apart.


The push-versus-pull derivation is now closed and I have the whole chain. Shall
I write it up — the design note, the seeding rule, and the corrections it
forces in the gather document and the guarantees?


--------------------------------------------------------------------------------

### User Request 23

Okay. what's left unwritten, and where do we go from here? any unanswered
questions?


--------------------------------------------------------------------------------

### Assistant Response 23

Let me check what's actually on disk rather than trust my memory of it.


Checked. Here's the honest state.


## The gap: roughly the last five turns aren't on disk


The port record still describes the design we had *before* you proposed the
lock-free claim. Everything after that point exists only in this conversation.


**Unwritten, in rough order of how much depends on it:**


1. **The push-versus-pull derivation and the seeding rule.** The whole chain —
   a cycle forces ring buffers, a wire into a ring buffer is a push, the feeder
   priming a cycle is what gets seeded, a program with no cycle is a pure
   function seeded at its output. This is the thing we circled five times and
   finally closed, and it's the load-bearing one: it decides what the map file
   needs to say, which is currently "declare your gatherers" and is about to be
   "don't."

2. **The lock-free claim.** Your optimistic walk with rollback, plus the
   ordered-ports fix for the livelock. The port record currently says the
   opposite — that the station's mutex covers readiness and the claim.

3. **Hints instead of positions**, and ring buffers growing by paging rather
   than copying. The port record still has head and tail as exact positions.

4. **Per-port arrival order is given up.** This one needs care: it isn't just
   undocumented, there's a *passing test* that checks several hundred deliveries
   arrive in order. That test will fail, and it should — but somebody has to
   decide that on purpose rather than discover it.

5. **Destination lists as immutable arrays**, published by pointer swap,
   reclaimed through the scrapyard with the odd-even counters.

6. **The mutex stays in the station record.** The separate paged mutex array is
   dropped, and the mutex's remaining job is the four rare structural
   operations.

7. **The output station is a pass-through** — its inputs are the map's
   outputs. That's a real change to how the output station issue describes
   itself.

8. **Runtime box compilation** — shelling out to a compiler, the registry
   becoming growable, structural typing making it safe. No issue file exists for
   it at all.


## What is written and current


The gather-timing note and the gathering-at-pickup issue, both rewritten around
pickup being the only correct timing. The station table issue, with shelves. The
construction surface issue, with the three-step procedure, batched wiring,
no-output-station-is-an-error, and unfed-cycles-are-fine. Statics moving onto
ports, and the static-write wire. Types compared by shape. Two strategems —
raising your hand, and growing by adding.


## What's stale rather than missing


**The stations-and-slots document has never been touched** and is now
substantially wrong — it uses the old vocabulary throughout and describes
ports that hold one kind of source.


**The gather document and the guarantees document are marked but not
rewritten.** They carry notes saying "this is changing, see the issue," which
was right when the change was one thing. It's now five things.


**Several recorded open questions have already been answered in conversation and
still read as open.** The partial-gather-failure one, the sleeper-count one, the
re-seeding one, the out-of-memory-versus-typo one — all settled here, all
still sitting in their files as unresolved. That's the worst kind of stale,
because a stale question looks like a live one.


**The documentation site knows none of it.** No pages for the new issues, and
the existing pages describe the old design.


**Nothing is committed**, and the repository is still carrying someone else's
in-progress work alongside mine.


## Genuinely unanswered


Beyond the bookkeeping, four things nobody has decided:


- **Does giving up arrival order break anything besides that test?** I don't
  know what else leans on it, and I haven't looked.

- **Where does changing a port's source sit relative to the rewiring lock?**
  Turning a port into a gatherer has to run the cycle walk, which suggests one
  lock rather than two.

- **Does an unconfigured port survive into a dump?** A program with one can't
  run, so writing it out is either a useful record of a half-built program or a
  file that won't load.

- **Should the file reader run the completion pass when it hits the end of a
  file?** A file has an end and its author probably means it; but making the
  reader special is what unification exists to avoid.


Plus the documentation and workbench questions, which are a different kind of
thing and can wait.


## Where I'd go


**First, write the derivation up.** It's the biggest unwritten piece, everything
else references it, and it's fresh. That's a design note plus corrections to the
gather document, the guarantees, and the map file format — the gather
input-line syntax stops existing.


**Second, rewrite the port record.** It's actively misleading right now,
describing a locking scheme we've replaced.


**Third, sweep the stale questions**, marking the answered ones with their
answers. Cheap, and it stops the files lying about what's settled.


**Then commit** — which needs you to tell me what to do about the other work
in the tree.


After that we're back on the spine with the output station and multi-output
boxes, which are the last two before "a map is a box" is actually true. Or we
take the diversion you've been promised twice.


Want me to start with the derivation?


--------------------------------------------------------------------------------

### User Request 24

How about we take each of these:


  Unwritten, in rough order of how much depends on it:


  1. The push-versus-pull derivation and the seeding rule. The whole chain — a
     cycle forces ring buffers, a wire into a ring buffer is a push, the feeder
     priming a cycle is what gets seeded, a program with no cycle is a pure
     function seeded at its output. This is the thing we circled

  five times and finally closed, and it's the load-bearing one: it decides what
  the map file needs to say, which is currently "declare your gatherers" and is
  about to be "don't."

  2. The lock-free claim. Your optimistic walk with rollback, plus the
     ordered-ports fix for the livelock. The port record currently says the
     opposite — that the station's mutex covers readiness and the claim.

  3. Hints instead of positions, and ring buffers growing by paging rather than
     copying. The port record still has head and tail as exact positions.

  4. Per-port arrival order is given up. This one needs care: it isn't just
     undocumented, there's a passing test that checks several hundred deliveries
     arrive in order. That test will fail, and it should — but somebody has to
     decide that on purpose rather than discover it.

  5. Destination lists as immutable arrays, published by pointer swap, reclaimed
     through the scrapyard with the odd-even counters.

  6. The mutex stays in the station record. The separate paged mutex array is
     dropped, and the mutex's remaining job is the four rare structural
     operations.

  7. The output station is a pass-through — its inputs are the map's outputs.
     That's a real change to how the output station issue describes itself.

  8. Runtime box compilation — shelling out to a compiler, the registry
     becoming growable, structural typing making it safe. No issue file exists
     for it at all.



  Genuinely unanswered


  Beyond the bookkeeping, four things nobody has decided:


  - Does giving up arrival order break anything besides that test? I don't know
    what else leans on it, and I haven't looked.

  - Where does changing a port's source sit relative to the rewiring lock?
    Turning a port into a gatherer has to run the cycle walk, which suggests one
    lock rather than two.

  - Does an unconfigured port survive into a dump? A program with one can't run,
    so writing it out is either a useful record of a half-built program or a
    file that won't load.

  - Should the file reader run the completion pass when it hits the end of a
    file? A file has an end and its author probably means it; but making the
    reader special is what unification exists to avoid.



and add each to your todo list. Then, walk through them one-by-one with me, just
to confirm that the design is correct before we write it to disk. Then, we can
get around to the stale documents and stuff.


--------------------------------------------------------------------------------

### Assistant Response 24

Thirteen tasks, and I'm starting on the first.


---


# 1 — The push/pull derivation and the seeding rule


Here's the whole chain. Each step follows from the one above it, so if it's
wrong somewhere, that's where.


**A pull cannot go around a cycle.** Gathering is a function call — to get a
value from a station you run its box right now, on your own stack. If its inputs
eventually lead back to itself, the call never returns. The engine already
refuses these the moment the wire is drawn.


**So a port in a cycle must be a ring buffer.** If a station's output eventually
comes back to its own input, that input cannot be satisfied by pulling. The only
other way a value arrives is by being delivered. Nobody declares this — the
shape of the drawing forces it.


**A wire into a ring buffer is a push**, by definition of what a ring buffer
holds.


**And push spreads forward.** A station with a ring-buffer input runs whenever a
value arrives, so it runs repeatedly on nobody's schedule but its inputs' —
which means it can't be pulled either, because a pull needs an answer *now* and
this station's answer depends on a delivery that hasn't happened. So everything
it feeds receives by delivery too.


**Everything not reached that way is a pull.** A station whose inputs are all
statics or pulls produces its value on demand and never on its own.


**Seeding: enqueue every station that needs nothing delivered to it and that
nothing pulls from.** The first half means its task can be built without waiting
on anyone. The second half means it would otherwise never run at all.


**Two cases, one rule.** In a program with a loop, the station feeding that loop
from outside has no ring inputs and pushes rather than being pulled — so it's
seeded, drops one value in, and the loop turns. In a program with no loop, there
are no ring buffers anywhere, everything is a pull, and the only station nothing
pulls from is the output station — so it's seeded, pulls the whole tree down
through itself, and produces one answer.


## How it's actually computed


Find the cycles — the same walk that already refuses gather cycles, run in the
other direction. Mark every port in a cycle as a ring. Then propagate: any
station with a ring port is pushed, and every wire out of a pushed station lands
in a ring port. Repeat until nothing changes. Everything still unmarked is a
pull.


That terminates and it's well-defined: ports only ever become rings, never stop
being them, and there are finitely many.


## What it buys


**The map file stops declaring gatherers.** The `in 0 reader` form disappears.
Every connection is an arrow drawn from the producer, and whether it means *push
when I run* or *run me when you need a value* is computed from the drawing.


**Two load-time checks stop being checks.** *A gathered station may have no
ring-buffer inputs* is now what "pulled" means. *A station may not be both
pushed-to and gathered-from* can't arise, because a station has one nature.
Neither needs a better error message; neither can happen.


## The consequence I want you to look hard at


**Repetition requires a visible loop.** A box returns one value per run, and a
station runs once per trigger, so a station that does something a thousand times
must be triggered a thousand times — which requires a cycle in the drawing.
There is no other source of repetition.


So a straight line — read, transform, write, no loop — is a *one-shot
evaluation*. It runs once and produces one answer. To process a thousand lines
you draw the loop, exactly as you sketched it: the work feeds back around, with
a comparator on a static deciding whether to go again.


I think that's right and I think it's good — every loop in a soramech program
is literally a loop you can see, and there's no hidden iteration anywhere. But
it's a real statement about how people will write programs with this, and it
deserves a deliberate yes rather than falling out of a derivation.


## One smaller thing worth confirming


With several output stations, an acyclic program gets several seeds — one per
output station, since nothing pulls any of them. If two of them share an
upstream station, that station gets pulled twice and **runs twice**, producing
two independently fresh values. That's correct pull behaviour rather than a bug,
but it means a shared expensive computation is computed per consumer, not once.


## What writing this up touches


A new implementation note for the derivation itself. The gather document loses
its "a gatherer is a station with no ring-buffer slots" definition in favour of
the derived one. The map file format loses the gather input line. The guarantees
document gains the derivation and loses two load-time rules. And the seed sweep
issue gets its rule restated in terms that survive runtime construction.


Confirm the chain and that consequence, and I'll write it.


--------------------------------------------------------------------------------

### User Request 25

I dunno, I don't really think we should make "cycles" a defined thing? The user
is going to wire their code into all sorts of strange and unpredictable
combinations, I don't want to try and search for a pattern when we're working on
initializing a boxmapgraph. Tell me again what's the problem this design is
meant to solve? When adding a box to the mapgraph, we need to know where to
start it. Otherwise the boxes would just sit in memory. My understanding is that
we can identify some boxes as always being able to run because of their wiring
dynamics - a box with a ring buffer is always pushed into, so that's not a valid
initializer. We can't initialize it without a value in it's ring buffer. So no
boxes with ring buffers can be enqueue'd during initialization. However, we have
two other types of input ports - statics and gatherers, statics are just a value
in the port - they're valid for initalization enqueue'ing. Gathered ports are
when there are upstream boxes wired into this particular port, and each of the
upstream boxes has only static inputs, or gathered inputs. Of course, we have to
traverse the entire tree - not just to one leaf node, but to ALL of the leaf
nodes - to determine whether a box is "input only", or if it's just the next
step in a chain - what if 3 boxes back there's a ring buffer? We need to
traverse the whole tree. If the box is "input-only", then it essentially can be
collapsed logically into a single box. Neat. But what that means is we can run
the boxes it feeds, if all their other inputs are either static or gathered.


I think... I think the main thing we're looking for, really the ONLY thing we're
looking for, is a ring buffer that is fed by boxes that are input-only. Is that
right? Any ring buffer that is fed by an input-only box can be a valid target
for initialization enqueue'ing. But, didn't we say that part of the definition
of a port as a ring buffer port was that it wasn't fed by input-only ports?
Meaning... it has to have a port above it that's ring buffer. GAH this is so
confusing, can you help illuminate?


--------------------------------------------------------------------------------

### Assistant Response 25

You found a real contradiction, and it's mine — I introduced the rule that
creates it, several turns ago, and you agreed with it because it sounded right.
Let me name it and then take it back out.


## Where the knot is


The rule was: **a wire out of an input-only box is always a pull, never a ring
buffer.** You put it more sharply than I did — *"an input-only box can never
be wired into a ring buffer, it just can't happen."*


Follow it through and it eats itself. A port is a ring only if what feeds it
isn't input-only. A box isn't input-only only if it has a ring somewhere
upstream. So every ring needs a ring above it, forever, and there is no first
one. That's why I went looking for cycles — I needed *something* to be a ring
without anything above it, and a loop was the only thing I could find that
forced one.


**You're right that cycle-hunting is the wrong answer.** It makes initialization
depend on finding a topological pattern in wiring that people will draw in
whatever shape they like, and it makes the engine's startup behaviour depend on
a property nobody can see by reading one line of a file.


## Drop that one rule and everything comes apart cleanly


The format already has a base case and it's been sitting there the whole time:
**a port is a ring buffer unless a line says otherwise.** Ring is the default; a
gather is the exception; only the exception is written. That's not a new term
— it's the current file format, unchanged.


Keep that, and there's nothing to derive about push versus pull. It's declared,
compactly, in the place where somebody is already drawing the graph.


## What stays derived, and it's the useful half


**Input-only stays computed, exactly as you described it.** A station is
input-only when every one of its inputs is a static, or is a gathered port whose
source station is itself input-only. You traverse the whole tree — every
branch to every leaf, not just the first one — because a ring buffer three
boxes back makes everything below it not input-only.


**And that computation has no circularity**, because it only ever follows
declared gather wires and stops at declared ring buffers. It starts at stations
with only statics or no inputs at all, and works forward. No fixed point, no
iteration, no pattern-hunting.


Your observation about what it means is the good part: an input-only station and
everything above it **collapses into a single box.** It's a pure expression with
constants at the leaves, and the engine could in principle evaluate the whole
subtree as one call. That's a real property worth having a name for.


## Your seeding rule is right, with one clarification about which box gets the task


I think you already meant this, but let me say it back to be sure, because the
phrasing can be read two ways.


**The station that gets enqueued is the input-only one.** It's the only kind
that *can* be enqueued — every one of its inputs is a static or something it
can go fetch, so a task can be built for it with nothing waiting on anybody. A
station with a ring buffer can never be the one you start, because you'd have to
invent a value to put in it.


**The ring buffer downstream is the reason to enqueue it.** An input-only
station that feeds a ring buffer will never run otherwise — nothing pushes it,
and nothing pulls it, because the thing it feeds expects a delivery. So it needs
a first task or it sits forever.


So: **seed every input-only station that feeds at least one ring buffer.**


That's a small improvement on what the engine does today, which is "seed every
station with no ring inputs that nobody gathers from." Yours is stated
positively rather than as a double negative, and it correctly declines to seed
an input-only station that feeds nothing at all — which today's rule would
start, pointlessly.


## And your pure-function case still works


Source with no inputs, wired into an output station. The output station's port
is a ring buffer, because that's the default and nobody said otherwise. So the
source is input-only and feeds a ring buffer, so it's seeded. It runs, pushes,
the output station runs, one answer comes out.


No cycles anywhere, nothing special-cased, same one rule.


## So, corrected


| | how it's decided |

|---|---|

| is this port a ring buffer or a gather? | **declared** — ring by default, gather written as an exception |

| is this station input-only? | **derived** — traverse upstream through gather wires, stop at any ring buffer |

| which stations get a first task? | **derived** — the input-only ones that feed a ring buffer |

| cycles | **not consulted at initialization at all** |


The only thing we lose against what I proposed is the idea that gatherers stop
being written down. They stay written — one short line, only where a port is
unusual. Which is what you said you wanted four turns ago when you didn't want
more terms: this adds none, it just keeps the one that's already there.


Does that match what you were reaching for?


--------------------------------------------------------------------------------

### User Request 26

alright I figured it out. A box is just a box. A box has a box function.
Functions have arguments and return values, boxes have inputs and outputs.


why don't we just require every box to have an input, and an output station?
What would we gain and lose?


--------------------------------------------------------------------------------

### Assistant Response 26

That's a much better idea than anything I've proposed in the last five turns,
and I think it deletes most of them.


## What it actually is


A composite box has **a set of input ports, a set of output ports, and a graph
in between.** The input station and output station aren't boxes that compute
anything — they're the names for those two sets. Neither runs a function.
Values arrive at the input station and leave from the output station, and the
graph between them is somebody's business and nobody else's.


Which makes a leaf box and a composite box the same thing seen from outside: one
has a C signature saying what goes in and what comes out, the other has two
stations saying it.


## What we gain


**Composition actually works.** Right now a map has a declared output and no
declared input, so a parent wiring *into* a sub-map has to reach inside and name
a station and a port by its internal name. That isn't composition, it's
trespassing. With both ends named, a parent wires to the door.


**Seeding stops being derived at all.** This is the big one. Starting a program
becomes: *deliver values to its input station.* For a sub-map, the values come
from the parent. For a top-level program, they come from whoever ran it —
arguments, or nothing at all. One mechanism, and it's the same mechanism in both
cases.


No traversal of the upstream tree. No cycle detection. No "input-only station
that feeds a ring buffer." The entire derivation we've been circling for six
turns was answering *what starts this*, and the answer is now written on the
drawing.


**The dead-station problem gets a real error.** Anything inside a map that can't
be reached from the input station by delivery, and isn't pulled by anything on
the way to the output station, can never run. Today that's an unnamed condition
we discover as "nothing to seed." It becomes a reachability check with a
sentence somebody can act on: *station X can never run — nothing delivers to
it and nothing pulls from it.*


**The two ends are symmetric**, which they should have been from the start. Your
pass-through insight about the output station applies identically to the input
station, and the moment both are pass-throughs there's nothing special about
either.


**Input-only stops being load-bearing.** It's still a useful derived property
— it's what makes the gatherable rule checkable, and it's how you know a
subtree collapses into one pure expression — but nothing about starting a
program depends on it any more. Fewer things resting on it means fewer ways it
can be wrong.


## What we lose


**Two stations that compute nothing, in every composite box.** Ceremony, and for
a small map it's two lines of file for zero behaviour.


**A source can no longer start itself.** A box that reads a file used to get
going because it was input-only and fed a ring buffer. Now it has to be
reachable from the door — wired from the input station, or pulled by something
downstream. That's one more arrow in a lot of maps.


I think that's a gain wearing a loss's clothes, though. "What kicks this off"
becomes something you can see instead of something the engine worked out.


**Several independent entry points have to share one door.** Two readers that
both drive parts of a program both hang off the input station. It can fan out,
so it's expressible — but it centralises something that used to be
distributed.


**One more required declaration**, symmetric with the one that's already
required.


**A new class of refusal.** Maps that used to run will now be rejected for
having unreachable stations. That's correct, and it will still be annoying the
first time.


## What I'd do


Take it. It's simpler than the derivation, it makes composition real rather than
aspirational, and it replaces a computed property with a drawn one — which is
this project's whole disposition.


It also kills, outright: the cycle-forcing-ring-buffers chain, the seeding
derivation in both its forms, and my "seed the station you can't pull" rule.
Cycles go back to being just a shape you can draw, and gather cycles stay
refused for the reason they always were.


## The one thing to decide before I write it


**What does a top-level program's input station receive?**


If it's genuinely the same mechanism as a sub-map, then somebody outside
delivers to it — and for a program started from a shell, that somebody is the
runner, and what it delivers is arguments, or nothing.


"Nothing" is the case that needs pinning down. A port that never receives a
value never becomes ready, so a program whose input station has ports nobody
fills would sit there. Options: the input station's ports are allowed to be
statics bound in the file (so the program starts with its own constants and runs
immediately), or an input station with no ports at all is trivially ready and
fires once, or the runner is required to supply something.


I lean toward the first — an input port with a static is already a solved
thing, and it means "run this program with these constants" and "run this
program with these arguments" are the same sentence. But it's your door.


--------------------------------------------------------------------------------

### User Request 27

if there's no input arguments, then the box should start itself as soon as it's
initialized. :)


  A new class of refusal. Maps that used to run will now be rejected for having
  unreachable stations. That's correct, and it will still be annoying the first
  time.


I don't think this should be an error? We might want to swap wiring along the
way. So unreachable stations might be reachable eventually.


  What does a top-level program's input station receive?


  If it's genuinely the same mechanism as a sub-map, then somebody outside
  delivers to it — and for a program started from a shell, that somebody is
  the runner, and what it delivers is arguments, or nothing.


  "Nothing" is the case that needs pinning down. A port that never receives a
  value never becomes ready, so a program whose input station has ports nobody
  fills would sit there. Options: the input station's ports are allowed to be
  statics bound in the file (so the program starts with

  its own constants and runs immediately), or an input station with no ports at
  all is trivially ready and fires once, or the runner is required to supply
  something.



how about arguments or nothing? CLI arguments if run from bash, regular
arguments if run from anywhere else.


--------------------------------------------------------------------------------

### Assistant Response 27

Taken, all three. Let me say what each one does to the design, and then there's
exactly one mechanical detail left.


## Arguments or nothing — and this reuses something we already have


A composite box's input ports are its parameter list. From a shell you get
`argv`, from a parent map or a C caller you get typed values directly.


**The shell case costs nothing to build**, which is the nice part. Command-line
arguments arrive as text, and the engine already has a text-to-bytes reader —
the one that turns a static's `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into
correctly-laid-out bytes by walking the generated field table. Point it at
`argv` instead of at a statics line and it's the same code, the same offsets
from the compiler, the same error messages naming the field that was wrong.
You'd get struct arguments on the command line for free, using the brace syntax
that already exists.


**And the input ports need no declared types.** If input port zero feeds a
station whose box takes an `int` there, then input port zero is an `int` —
derived from the registry, checked by the same wire check as everything else. A
port that fans out to two stations expecting different types is caught by that
check. Nothing new to write in the file, which is consistent with the format
never carrying types anywhere.


## Unreachable stations aren't an error — consistent, and here's the cost


That's the third time you've made the same call, and I think it's the right one
each time: an unwired station is ordinary, an unfed cycle is fine, and now an
unreachable one is too. All three rest on the same thing — you can rewire
while it runs, so *not reachable yet* is a different statement from *wrong*.


What it costs: a program can contain a station that will never run, and nothing
will say so unless somebody asks. That belongs in the completion pass as a
report — something you run when you think you're done, which tells you what
can't be reached — rather than as a refusal that stops you.


## The one detail left: what a zero-argument input station does when it fires


You said the box should start itself as soon as it's initialized, and I want to
nail down the mechanics, because "fires" is doing a lot of work there.


With arguments, it's clear: values arrive at the input ports, get delivered
onward into the graph, stations become ready, the program runs.


With no arguments there are no values, so the arrows out of the input station
carry nothing. Two ways to read that:


**Reading one — a value-less delivery is a trigger.** The arrow says *start
this*, with no payload. Mechanically it's coherent: delivery today writes a
value, then checks readiness, then builds a task. Drop the write and you get the
readiness check and the task build, which for a station that needs nothing
delivered is exactly a first task. So the arrow is `(station)` rather than
`(station, port)`, and firing it enqueues its destination.


The gain is that **which stations start becomes something you draw**, rather
than something the engine works out by traversing. That's the same disposition
as everything else here.


**Reading two — an empty input station means "start whatever can be
started."** No trigger arrows; initialization just enqueues every station that
needs nothing delivered to it. Simpler in the file, but it's the derived seeding
rule again in a smaller costume, and it means a program with no arguments has an
invisible entry point while a program with arguments has a visible one.


I'd take the first. It costs one narrow addition — an arrow that carries no
value — and it buys the property that a program's starting points are always
drawn, whether it takes arguments or not. Same picture either way; only the
payload differs.


The thing I'd want to check with you before writing it: a trigger arrow can only
point at a station that needs nothing delivered — one whose every port is
static or gathered. Point it at a station with a ring-buffer port and you'd be
asking it to run with an empty buffer. That's a refusal, and it's a good one,
because the message writes itself: *this station can't be started directly, it's
waiting on a delivery.*


## Where that leaves the pile


If you take reading one, this deletes a lot. Task one goes from "write up the
derivation" to "write up the door," and with it goes the
cycle-forcing-ring-buffers chain, both versions of the seeding derivation, and
my seed-the-station-you-can't-pull rule. Cycles go back to being a shape you can
draw. Input-only survives as a useful derived property — it's what makes the
gatherable rule checkable and what tells you a subtree collapses into one pure
expression — but nothing about starting a program leans on it any more.


Which is a good trade: the thing we couldn't derive turned out to be the thing
that should have been drawn.


--------------------------------------------------------------------------------

### User Request 28

  What it costs: a program can contain a station that will never run, and
  nothing will say so unless somebody asks. That belongs in the completion pass
  as a report — something you run when you think you're done, which tells you
  what can't be reached — rather than as a refusal that

  stops you.


alright what if there are peninsulas connected to the output port, and only one
of them connects to the input port? those other peninsulas would be stranded,
and would never run. What do we do about them? what if there are islands that do
important things, like maybe write to disk or send network packets or something?
impure functions...


if we could find a way to start tasks that are ready at any time, that would
help a lot.


--------------------------------------------------------------------------------

### Assistant Response 28

That's a real hole and the door alone doesn't fill it. An island that writes to
disk and feeds nothing is a completely legitimate program — half of what
people actually do with a dataflow engine is side effects — and under "the
input station is the only way in," it never runs and nothing about the picture
looks wrong.


## Your instinct is right, with one thing to add


"Start tasks that are ready at any time" almost works, and the thing that breaks
it is worth naming: **a station with no ring-buffer ports is *always* ready.**
Every one of its inputs is a static or something it can go fetch, so there's
never a moment when it isn't startable. Run that rule literally and such a
station gets a task, and another, and another, forever.


The fix is a latch. **Started once, not started again.**


So the rule becomes: *a station that can run without waiting for anyone, that
nothing pulls from, and that has not already been started, gets one task.* Three
clauses, each doing a job:


- **can run without waiting** — no ring-buffer ports, so a task can be built
  with nothing outstanding.

- **nothing pulls from it** — otherwise it runs on demand and starting it
  would be inventing an extra invocation nobody asked for.

- **not already started** — otherwise it fires forever.


## And it's checked whenever the graph changes, not once at startup


This is the part I think you were reaching for. Nothing except a wiring change
can alter whether a station satisfies those three clauses — a static changing
doesn't, because a static was always full; a value arriving doesn't, because
that's ordinary delivery. So the rule needs evaluating exactly when somebody
adds a station, draws a wire, or changes a port's source.


Which makes startup and runtime the same thing again, properly this time.
Initialization is the first evaluation. Adding a box to a running program is the
next one. Wiring up a stranded peninsula three hours in is another, and the
peninsula starts the moment it becomes startable, without anyone remembering to
ask.


It also quietly answers the re-seeding question from earlier. We said "seed only
what you added" to avoid starting old stations twice. With a latch you don't
have to scope it at all — you can evaluate the whole program every time and
the latch makes it idempotent.


## What this does to the input station


**It stops having a starting role entirely.** The input station is now just the
name for where arguments arrive. With arguments, values land on its ports and
flow onward like any delivery. With no arguments, it has no ports, and it
doesn't need to do anything — the program starts from whatever else is
always-ready, which is what would have happened anyway.


That's better than what I proposed last turn, because it deletes the trigger
arrow. I was inventing a value-less delivery to explain what a zero-argument
door does when it opens, and the answer is that it doesn't need to do anything.
The door is for arguments. Starting is a separate rule that applies everywhere.


## Your peninsulas and islands, walked through


**A peninsula** — feeds the output station, nothing feeds it. Its head station
has no ring-buffer ports, so it's always-ready. Nothing pulls it, since it
pushes downstream. Never started. So it gets a task, runs, and pushes down the
peninsula into the output. Works without anyone drawing anything special.


**An island that writes to disk** — same shape, no output at all.
Always-ready, unpulled, unstarted, so it starts. It runs once, does its side
effect, and produces nothing. That's a correct and complete program fragment,
and I like that nothing in the rule mentions the output station: the engine
doesn't care whether a station's purpose is to produce a value or to have an
effect.


**A station in the middle of the main path** — has ring-buffer ports, so it
isn't always-ready. Never started by this rule; it runs when something delivers
to it. Correct.


**A gatherer** — always-ready, but something pulls it. Excluded, so it runs on
demand instead of getting a spurious extra invocation. Correct.


## What it costs


**One bit of state per station**, the latch. Small, but it's memory that exists
purely so a rule can be idempotent, and it means a station's history now affects
its behaviour — which is the sort of thing this design has otherwise avoided.


**"Started once" is forever.** A station that ran at startup and finished never
runs again unless something delivers to it. If you rewire a peninsula's head to
give it new statics and want it to run again with the new values, the latch says
no. There'd need to be a way to say *start that again* deliberately — which is
fine, but it's a capability that has to exist rather than being implied.


**An island doing something expensive starts itself the moment it's wired.**
Draw the wire and it goes, immediately, before you've drawn the rest of what you
intended. That's the same property as your observation about batched wiring,
arriving at a different door.


If that's right, task one becomes a much smaller thing to write: one rule with
three clauses and a latch, plus the input station being a naming of ports and
nothing else. Shall I write it that way?


--------------------------------------------------------------------------------

### User Request 29

Hmmmmm I don't really like the idea of a latch either? Seems like fragile state
that's a little opaque. Okay. I want to try and remove the gatherer box
distinction. Can you help? Let's walk through the design and why it exists. I
think... I think we can "collapse" gatherer boxes into the boxes that feed them.
if they're input only. So like, let's say there's three boxes - the first one is
fed by two static values, the second is fed by the first box (making it an
input-only gatherer) and the third is fed by the second, but it has another
input from somewhere else that's a ring buffer. My question is... What if we
just, collapsed the first and second box together, and just made that slot an
"always ready" slot because the worker that picks up the task from the task
queue just has to prefix the task's function with the gather function for the
box? would that fix the issue? If the first and second box are "collapsed" they
could just be treated as one box on the graph. Maybe?


--------------------------------------------------------------------------------

### Assistant Response 29

Your collapse works, and it's worth seeing that it's already what the engine
will do at runtime — the pickup design has the worker run a prefix that walks
the upstream chain and fills the slot before calling the box. A and B evaluated
inline, one after the other, on the worker's stack. So "collapsed into one box"
is literally what happens; we just haven't been calling it that.


What you're adding is that it shouldn't be a *kind of port*. Let me follow that
through, because it buys more than it looks like.


## What removing the distinction actually gets you


Port kinds go from four to two. Not ring, gather, static, none — just **ring**
and **ready**, plus unconfigured.


A **ready** port has a value available without waiting. Whether that value is a
constant sitting there or a chain of boxes that gets evaluated on demand is the
port's own business, invisible from outside. Both answer "yes" to readiness
without looking. Both produce a value when asked. The difference is a copy
versus a call, which is one branch inside the port rather than a distinction the
rest of the engine has to know about.


And one load-time rule stops being a rule. *A gathered station may have no
ring-buffer inputs* becomes the **definition**: a port is ready exactly when
everything feeding it is input-only, and it's a ring buffer otherwise. There's
nothing to check, because there's no way to say the wrong thing.


That's the win you've been after for six turns — the declaration disappears
and no new word replaces it.


## And here's the wall, stated precisely


Work the definition down. A station is input-only when every input is a static
or comes from an input-only station. That bottoms out at stations with only
statics or no inputs, which *are* input-only.


So input-only is the only thing the recursion can produce. Everything is
input-only, so every port is ready, so there are no ring buffers, so the entire
program is one pure expression evaluated on demand. Which is coherent — it's
just a function — but it means nothing repeats and nothing is ever pushed.


**Nothing inside the graph can be the base case.** Every station's nature is
defined by its inputs, so the chain has to stop somewhere outside.


## And the outside is the door you just built


**The input station is not input-only, because its values come from outside the
graph.** That's not a declaration and it's not a new term — it's what the
input station already *is*. Arguments arrive from a parent map, or from a shell,
or from a C caller, and they can't be computed on demand because they aren't
computed at all.


Everything follows from that one fact:


- The input station is not input-only, so anything it feeds has a **ring
  buffer** port.

- Anything fed by *that* has a ring buffer, and so on downstream — push
  spreads out from the door.

- Anything whose whole upstream is statics is **input-only**, so ports fed by it
  are **ready** — evaluated on demand, collapsed into whoever consumes them.


**And it kills the latch**, which you didn't like and I wasn't happy with
either. There's no seeding rule left to be idempotent about. Starting a program
is one event: values arrive at the input station. A program with no arguments
has an input station with no ports, which is ready immediately and fires once as
part of initialization — exactly what you asked for two turns ago, and it
needs no mark on any station because it happens once, at a named moment, by
construction.


## Your peninsula works. Your island doesn't.


**Peninsula** — statics feeding a chain that reaches the output station. Every
station in it is input-only, so the output station's port from it is a *ready*
port. The peninsula gets **pulled** when the output station runs, and the whole
chain collapses into that one pull. It runs, and its values are as fresh as the
moment they were used. No special case, no seeding, nothing drawn that wasn't
already there.


**Island** — writes to disk, feeds nothing, fed by nothing but statics. It's
input-only, so it can only run by being pulled. Nothing pulls it. It never runs.


I don't think that's a bug to patch, and here's why: **a disconnected
side-effect box has no answer to "when."** Dataflow says a thing runs because a
value arrived or because a value was wanted. An island wired to nothing has
neither, and the only way to give it a moment is to invent a trigger — a
delivery with no value — which is the thing I proposed last turn and withdrew
because it exists solely to answer this question.


The honest fix is to connect it. If it writes to disk, something is telling it
*what* to write, and that something is on the main path — so it isn't an
island, it's the end of a peninsula pointed the other way. If it truly takes
nothing and produces nothing, then it's a side effect that should happen once at
startup, and the place to say that is a wire from the input station, where "the
program started" is a thing that has actually happened.


## Where that leaves us


Port kinds: two. Load-time rules: one fewer, and it's now unsayable rather than
checked. Seeding: gone. The latch: gone. Cycles: back to being a shape you can
draw, consulted by nothing. The input station: earns its place by being the one
thing in the drawing whose values come from somewhere else.


The cost is that a program's side effects have to hang off the flow rather than
float beside it — which I think is the thing you'd want anyway, but it's a
real constraint on how people draw.


Shall I write this one up? It's a smaller note than anything I've proposed so
far, and it replaces four of the thirteen tasks.


--------------------------------------------------------------------------------

### User Request 30

alright I got an idea. What if, when we're checking readiness, if we encounter
any slots marked as "gathered" we check a little list of which boxes we need to
enqueue in order to gather from that slot. Then we enqueue those, and they
propagate down. I worry about collapsing boxes into a meta-box because what if
there's more wires that go in and out? what if the wiring changes? seems like a
lot of shuffling things around. We should be able to dynamically compute the
list of boxes we need in order to gather from a slot by doing the tree-walk that
we had before. This could be cached, on the box station, as a list of pointers
or indexes or something. What do you think? The idea is, if we need to gather
from a box because there's no value in it's input buffer and it has a gather
list and it's not a static value, then we start up the tasks that generate that
value and then we're done - when they're finished, they get delivered into the
box, and the box runs as if it were any other deliverer. Is that better or
worse? I think we nixed this design earlier, but I can't remember why.


--------------------------------------------------------------------------------

### Assistant Response 30

You did nix it, and what you nixed isn't this. Worth separating, because the
difference is the whole thing.


**What we rejected** was a *gather task*: build the consumer's task with empty
slots, enqueue a second special task whose job is to fill them, have it push the
first one when done. That needed a reserved station index meaning "this is
machinery, not an invocation," a branch at the top of the delivery walk to
honour it, and two allocations per invocation. Your words at the time: *if we're
making another task just to gather data, why not gather it when we're going to
use it.*


**What you're describing now has none of that.** There's no gather task. The
upstream boxes are enqueued as *ordinary tasks*, they run as ordinary boxes, and
they deliver through the ordinary delivery walk. Nothing anywhere knows a pull
happened.


## And your worry about collapsing is aimed at something we weren't building


The "collapse" was my word for what happens at runtime, not a structural
rewrite. Nobody merges two stations into a meta-box; the worker just walks the
chain and calls the boxes in order. So extra wires in and out are fine, and
rewiring is fine.


But the worry is still useful, because your design makes the question disappear
rather than answering it.


## What your version buys, and it's a lot


**The gather port kind stops existing.** Not renamed — *gone*. A port is a
ring buffer or a static. A "gathered" port is just a ring buffer that happens to
be filled by somebody you poked.


**Every box runs from the pool.** The one exception in the entire engine — a
gathered box running inline on a thread that's mid-delivery — dissolves. That
exception is where the design has said surprises would come from since phase
four.


**Gathered boxes stop needing to be thread-safe on demand.** They're ordinary
tasks now, and two invocations of one station are already handled by the claim
under its own lock. The open-read-close discipline, the no-kept-handles rule —
all of it becomes the ordinary rule that applies to every box.


**No worker ever blocks in a chain.** Gather-at-pickup lets a worker disappear
into an arbitrarily deep chain running arbitrary user code. Here it enqueues and
moves on.


**And the gatherable rule may not be needed at all.** Today a station that's
gathered from may have no ring-buffer inputs, because an inline pull can't wait
for a delivery. Poke a station whose ring buffer is empty and nothing bad
happens — it simply doesn't become ready, and the value arrives whenever it
arrives. Nothing blocks, so nothing deadlocks, so there's nothing to forbid.


**It also throws out most of the gathering-at-pickup issue**: the outer task
struct, the generic shim, the per-slot value-or-thunk tag, the snapshot of the
gather list.


## You don't need the cached list


If port A of station S is empty and fed by U, you don't need to know the whole
chain — you just **poke U**. Poking runs U's readiness: either every one of
U's inputs is available and a task gets built, or U has its own empty pulled
port and the poke recurses one more level. One level at a time, and the chain
unwinds itself.


That's a graph walk, not a stack of box calls — no user code runs during it,
so it can't block and can't overflow on a slow disk. Your cached list is then an
optimisation that skips the intermediate pokes, and it can be added later or
never.


## What it costs, and it's exactly one thing


Walk the sequence:


1. A value arrives at port B of station S. Port A is empty, so S can't run.

2. U gets poked and enqueued.

3. U's task waits in the queue, runs, produces a value, delivers it into port A.

4. *That* delivery makes S ready, so S's task is built and pushed.

5. S's task waits in the queue, runs.


**The value produced at step 3 is used at step 5 — a full queue traversal
later.** That's the same staleness as gathering at enqueue, which is the timing
we replaced three turns ago on the grounds that a pulled value's entire purpose
is to be fresh *at the moment it is used*. Under a shallow queue it's
microseconds. Under a backlog it's the backlog.


So the trade is stark: **your design deletes the pull path's machinery and the
pull path's reason for existing at the same time.**


## Which is the actual question


If pulled values only ever need to be *newer than a pushed value would have
been* — a config file that changes occasionally, a threshold somebody turns
— then one queue traversal is nothing, and this design is better than what we
have on every axis I can find.


If a pulled value has to match the instant it's consumed — a clock, a sensor
read, a counter that must not be double-counted — then that staleness is the
failure, and gathering at pickup exists specifically to prevent it.


I don't think you can have both without keeping two mechanisms, and I don't
think the engine should have two. So: which of those two is the pull path for?


--------------------------------------------------------------------------------

### User Request 31

  If a pulled value has to match the instant it's consumed — a clock, a sensor
  read, a counter that must not be double-counted — then that staleness is the
  failure, and gathering at pickup exists specifically to prevent it.


if you need fresh data, you might need to either... gather and consume it in the
same box function, or wire the upstream into two boxes. The second box just
takes literally anything, it's just a signal - saying "please run this and fill
in the downstream" but I think that's a special case that users will wire for
themselves. I don't think this system should really be used for things that are
timing critical? the whole point is that things happen out of order. I guess...


anyway I think the design is good. Something I'm concerned about. If there's a
long chain of "gather" boxes, and we only enqueue them one at a time... that
could take a while. It's no longer step 5, but step 25. What if when we fill in
an input port we find the "gather" ports on the same box and if there's not a
value in the "gather" ring buffer (new design?) then we enqueue the "gather"
chain? Or... is that not really a concern?


--------------------------------------------------------------------------------

### Assistant Response 31

Good — and that's a real design statement worth writing down as such: **this
engine is not for timing-critical work.** Things happen out of order by
construction, and a value being one hop older than it could have been is inside
the tolerance the whole thing already asks for. If you need a value that matches
the instant it's used, you read it inside the box that uses it. That belongs in
the guarantees document as a non-goal, so nobody builds a sensor loop on it and
then discovers this by measuring.


## The depth concern is real, and it's narrower than it looks


You're right about the shape. Poking walks *down* the chain immediately —
that's just a graph traversal, no user code, no queue, so finding the deepest
ready station costs nothing. But the values have to come back *up* one delivery
at a time, and each of those is a queue traversal. A chain ten deep means ten
trips through the queue before the consumer runs.


**But it's latency, not throughput.** While invocation one's chain is unwinding,
invocation two's chain is already in flight behind it. A station pulling from a
deep chain a million times doesn't take a million × ten traversals — the
chains pipeline. What's depth-proportional is how long any *single* invocation
waits, not how many the engine gets through.


That matters because it moves the problem from "this design doesn't scale" to
"this design has a latency floor you can see."


## Your fix doesn't quite work, and the walk already does the right part


Enqueueing the whole chain at once isn't available: only the *leaf* is ready.
The one above it has an empty port until the leaf delivers, so building a task
for it would mean building a task with a hole in it. The walk down already finds
the deepest ready station and enqueues exactly that one, which is the most that
can be enqueued.


## And poking earlier breaks something worth keeping


The tempting version is to poke as soon as *any* port gets filled, so the pull
overlaps with waiting for the other pushed inputs. It does overlap, and it costs
the accounting.


Say a station has one pulled port and two pushed ones. Values arrive at the
first pushed port five times before the second gets anything. Poke on every
delivery and you've pulled five values into a port that will be consumed once
— four fresh values sitting in a buffer going stale, and the next invocation
reads the oldest of them.


**Poking exactly when a set can't be completed keeps the count right by
construction.** One would-be invocation, one pull. The values match up without
anyone tracking anything.


## So what to do about depth


**Measure it.** The engine already computes the deepest gather chain when the
wires are drawn, and keeps it on the map — it was put there because the cycle
walk pays for the traversal anyway. Report it beside the per-station gather
timings, so a deep chain shows up as a number rather than as a mystery.


**And there's a general fix available later that isn't gather-specific.** When a
worker's delivery makes exactly one station ready, it could run that station
itself instead of pushing it and going back to the queue. A chain would then
unwind on one worker in one continuous stretch — one queue traversal
regardless of depth. That helps every chain in the engine, pushed or pulled, and
it needs no gathering machinery at all. It's a change to how delivery and the
pool meet, which is the part of this engine I'd least like to touch casually, so
it belongs in its own issue rather than in this one.


**And your own advice is the third answer:** a chain of input-only boxes ten
deep is a pure expression written across ten stations, and collapsing it into
one box function is something the author can just do. The engine reporting the
depth is what tells them to.


---


Before I write any of this up: this design change invalidates a fair amount of
what's on disk. The gathering-at-pickup issue is now describing a mechanism
we're not building, the gather-timing note argues for it at length, and three of
the thirteen tasks are void. Want me to sweep those as part of writing this one
up, or keep them separate?


--------------------------------------------------------------------------------

### User Request 32

  Enqueueing the whole chain at once isn't available: only the leaf is ready.
  The one above it has an empty port until the leaf delivers, so building a task
  for it would mean building a task with a hole in it. The walk down already
  finds the deepest ready station and enqueues exactly

  that one, which is the most that can be enqueued.


what if there's multiple? We gotta find all of the leaf nodes, and enqueue all
of them. Also, what happens if we have 2 ring buffer inputs to a box, and 1
"gather port" - if we fill in ring buffer A with one value, then we enqueue the
gather task, then we fill in buffer B with another value, and enqueue the gather
task, we'll get one total invocation of the box, and one dangling "gather"
value. Not ideal...


  Poking exactly when a set can't be completed keeps the count right by
  construction. One would-be invocation, one pull. The values match up without
  anyone tracking anything.


--------------------------------------------------------------------------------

### User Request 33

Enqueueing the whole chain at once isn't available: only the leaf is ready. The
one above it has an empty port until the leaf delivers, so building a task for
it would mean building a task with a hole in it. The walk down already finds the
deepest ready station and enqueues exactly

  that one, which is the most that can be enqueued.


what if there's multiple? We gotta find all of the leaf nodes, and enqueue all
of them. Also, what happens if we have 2 ring buffer inputs to a box, and 1
"gather port" - if we fill in ring buffer A with one value, then we enqueue the
gather task, then we fill in buffer B with another value, and enqueue the gather
task, we'll get one total invocation of the box, and one dangling "gather"
value. Not ideal...


  Poking exactly when a set can't be completed keeps the count right by
  construction. One would-be invocation, one pull. The values match up without
  anyone tracking anything.


not quite, in the example above, say we fill in A with one value, B with one
value, and we figure out how to only enqueue the gather task once. Then, while
the gather task is gathering, we put a value in B - the system would see "hey we
got a value in A and B, we must need to enqueue C" so it does. This is fine.
But... what if two B values come in before A gets consumed? C would have three
total values, B would have 3, and A would have one. We'd need to first ask "are
there any stale values in the gather port?" which brings back the gather
definition. Not ideal. Could we like, keep a counter maybe, of how many values
are coming? That feels wrong. What do you think?


--------------------------------------------------------------------------------

### Assistant Response 33

Both of those are right and the second one is a real bug in what I said. Let me
be precise about it.


## Multiple leaves — yes


The walk down branches. A station with two pulled ports has two chains, and they
may bottom out at different depths. You enqueue every station the walk reaches
that is actually ready, not just one. The consumer becomes ready when all of
them have delivered.


## Your counting bug is real, and my "by construction" was wrong


I said one would-be invocation means one pull. It doesn't. **Readiness fails on
every delivery while the pulled port is empty**, not once per completable set.
So your first scenario is exactly right: a value into A pokes, a value into B
pokes again, two pulls land, one invocation happens, one pulled value is
stranded — and it's stale, and the next invocation will use it.


## Here's the structural reason, and it's worth seeing


There are two places a pulled value can come from, and they account differently:


**If the pulled value arrives by delivery**, it lands in a buffer. A buffer is a
*queue of values*, so its depth has to be kept in step with the other ports'
depths — and nothing does that automatically, because deliveries arrive
independently. That's your bug, and it isn't an implementation slip; it's what
happens when you make a pull look like a push.


**If the pulled value is produced as part of building or running the
invocation**, there is exactly one per invocation and nothing needs to be kept
in step, because the value and the invocation are created by the same act.


The kick-off design's whole appeal is that it turns pulls into ordinary
deliveries. This is the bill for that.


## The fix is one integer, and it's less fragile than it feels


You already have the shape: don't poke on every failed readiness, poke when the
station is short of pulls. Concretely, on the port: **how many pulls are
outstanding.** Poke only when the pushed ports could complete more invocations
than the pulled port has values plus pulls in flight.


Increment when you poke; decrement when a value lands. And the pairing is exact
rather than hand-maintained, because **every delivery into a pulled port is a
pull result** — nothing else ever delivers there. So it isn't a
synchronization protocol with two independent parties that can drift; it's one
arrival matching one request, in a port where no other kind of arrival exists.


The failure mode is honest too: if a pull never comes back, the counter never
decrements and the station stops pulling. That's the same hang as any value that
never arrives, not a new class of wrongness.


I don't think your instinct is wrong, though — it *is* state that the other
designs don't need at all, and "feels wrong" is a reasonable reaction to a count
that exists to paper over a mismatch.


## So the three designs differ on exactly this


| | pulls per invocation | machinery | value age at use | worker blocked in a chain |

|---|---|---|---|---|

| **pull inside the invocation** (pickup) | one, automatically | outer struct + shim | zero | yes |

| **pull as its own task, filling a built task** | one, automatically | reserved index + a branch in delivery | one traversal | no |

| **pull by poking, arriving as delivery** (yours) | needs a counter | none | depth × traversals | no |


Yours is still the only one with no machinery, and that's a big deal. It buys
that by paying one integer per pulled port and by taking depth-proportional
latency.


The middle row is the one we dismissed early, and it's worth a second look now
only because it gets the accounting for free *and* keeps every box running from
the pool — which were the two things you liked most about your design. Its
cost is the machinery: something in the queue that isn't an invocation.


My read: the counter is acceptable and your design still wins on simplicity,
because "one integer, incremented at request, decremented at arrival, in a port
where arrival means nothing else" is a much smaller thing to hold in your head
than a reserved station index and a special case in the delivery walk.


But you found the cost, so you should be the one to decide whether it's worth
it.


--------------------------------------------------------------------------------

### User Request 34

alright I got an idea. What if, whenever we change a static value, we enqueue
the box task it's attached to? If the other inputs are satisfied of course.
Then, instead of having gather boxes deliver to... where? we instead deliver
into a static slot. Gathered input PORTS have static input SLOTS. Then, we don't
pop the value from a static slot, we just peek. What do you think? Will that
solve the concern? "change a static value" might involve changing wiring too,
not sure yet.


--------------------------------------------------------------------------------

### Assistant Response 34

That solves it, and it solves considerably more than it set out to.


## Why the counting problem disappears


The bug existed because a pulled value arriving by delivery landed in a
**queue**, and a queue has a depth that has to be kept in step with the other
ports' depths. Nothing kept it in step, so you got stranded values.


A static slot has no depth. There's one value, writing replaces it, reading
doesn't consume it. There's nothing to keep in step with anything, so there's no
counter, no outstanding-pulls bookkeeping, and no stranded value — a second
write just overwrites the first.


## But look at what else goes


If a pulled value is delivered into a static slot, then **there is no pull path
at all.** Everything is a push. Which deletes, outright:


- the gatherer port kind

- the gather module and its inline chain walk

- the gather cycle check

- the rule that a gathered station may have no ring-buffer inputs

- the rule that a station can't be both pushed-to and gathered-from

- the one exception to "a box only runs when a worker picks it up"

- the requirement that a gathered box be safe on several threads at once

- gather timing entirely — the whole implementation note, and the pickup issue


Nine turns of argument about *when* a pulled value is gathered evaporate,
because nothing is ever pulled.


## And the model that's left is coherent on its own terms


Two kinds of input port:


| | read | write | affects readiness |

|---|---|---|---|

| **ring** | consumed — one value taken per invocation | queued behind whatever is already there | yes — empty means the station can't run |

| **static** | peeked — the same value read by every invocation | replaces what was there | no — always full |


And **an invocation happens when any input changes and every ring port has a
value.** A delivery into a ring port triggers, as it does today. A write into a
static port also triggers, which is your new rule.


That gives you two behaviours out of one mechanism. Ring ports are a **stream**
— values queue up and get consumed one per run. Static ports are a **cell**
— change one and everything downstream recalculates, like a spreadsheet. A
chain of stations wired through static ports is a recalculation graph: change
the value at the top and it propagates down, once, shared by everyone who reads
it.


That last part is strictly better than what we had. Under the pull design, two
consumers of the same upstream chain each pulled it and it ran twice. Here it
runs once and both read the result.


## Seeding falls out of it


The initial binding of a static from the file **is a write.** It happens while
the program is being built, and its effect lands at step three of your procedure
— enqueue everything whose inputs are now satisfied. So there's no separate
seeding rule, no latch, no derivation. Construction writes the statics, and the
writes trigger.


And your parenthetical is right: **changing a port's source is a change to what
it holds**, so it should trigger the same way. Rewiring a port to a different
upstream, or turning it from ring to static, is a write.


## The losses, plainly


**Fresh-at-use is gone.** Not deferred, not weakened — gone. No mechanism
produces a value at the instant it's consumed. A static holds whatever was last
written into it, and the same value is read by every invocation until something
writes again. If a box needs the current time, it calls for the time inside the
box. You've already said that's the right answer, and this makes it the *only*
answer.


**A value recomputes only when its inputs change.** For a spreadsheet that's
exactly right. For anything that should be re-read per use — a file that
changes underneath you, a counter, a socket — the drawing has to say what
makes it re-read, and the honest way to say that is a wire back from whatever
consumes it. That's a cycle, and it'll look like one.


**Static writes couple two rates.** A station with a ring port and a static port
now runs when *either* changes. Write the static ten times against a ring
holding five values and you get five invocations, five writes doing nothing, and
the last-written value waiting for the next ring arrival. That's defensible but
it's a coupling that doesn't exist today, and somebody will be surprised by it.


## The one thing I want you to confirm


Under this model, **a value is recomputed only when something upstream of it
changes** — never because somebody read it. That's the whole character of the
design and it's the opposite of what the pull path was for.


Is that what you want? Because if it is, this is a much smaller engine than the
one we've been describing, and I'd want to write it up as a design note that
replaces the gather documents rather than amending them.


--------------------------------------------------------------------------------

### User Request 35

  Static writes couple two rates. A station with a ring port and a static port
  now runs when either changes. Write the static ten times against a ring
  holding five values and you get five invocations, five writes doing nothing,
  and the last-written value waiting for the next ring

  arrival. That's defensible but it's a coupling that doesn't exist today, and
  somebody will be surprised by it.


Hmmm. That seems like a flaw. A station with a ring port and a static port... I
think it should only run when the ring port has a value written into it. Even if
the static value changes. The reason is, we would attempt the readiness check
and find the ring buffer empty (if it weren't empty, it already would have
enqueue'd itself when it was filled). So, we *can't* run it when the static
value changes, because the ring buffer is empty and we'd have to make something
up. Not ideal. So, I think we should only run the spreadsheet update when we
can. We can literally just add the readiness check to the system that changes
the static values and, I dunno, maybe sometimes it'll run? Probably not. If
there's any ring buffers, it'll always come back negative. If there's no ring
buffers, we definitely want it to run though.


  Under this model, a value is recomputed only when something upstream of it
  changes — never because somebody read it. That's the whole character of the
  design and it's the opposite of what the pull path was for.


  Is that what you want? Because if it is, this is a much smaller engine than
  the one we've been describing, and I'd want to write it up as a design note
  that replaces the gather documents rather than amending them.



I think... I think yeah we want this smaller design. Hooray for simplicity! ^_^


--------------------------------------------------------------------------------

### Assistant Response 35

That fix is right and it dissolves my objection: a static write just runs the
ordinary readiness check, and a station with an empty ring port comes back
negative because there's nothing to make up. No special coupling — the same
check, reached from a new direction.


The one observable that remains is small: if a ring port has a *backlog*, static
writes will consume it one per write. But that's real work being done on real
values, not a surprise. I overstated it.


Let me fix the task list, because most of it just changed.


Design note written. It replaces the gather-timing note entirely rather than
amending it — same file, same place in the reading order, completely different
subject.


**What it says:** what pulling was for (fresh at the moment of use), the three
timings and why each fell short, and then the counting problem as the thing that
actually settled it — that a pulled value arriving by delivery lands in a
queue whose depth nothing keeps in step with the other ports'. Then the
replacement: two port kinds, ring consumed and static peeked, with a write to
either running the ordinary readiness check. Then what it buys — shared
recalculation, seeding falling out of construction, and nine concepts ceasing to
exist — and what it costs, with fresh-at-use named as gone rather than
weakened and "this engine is not for timing-critical work" written down as a
stated non-goal.


## Task list, after the change


**Done:** the design note.


**Rewritten in scope:** the first task went from "write the push/pull
derivation" to "write the push-only model," which is what actually happened.


**New:** rewrite the gather datapath document as statics and recalculation,
keeping its slot in the reading order. Retire the gathering-at-pickup issue and
mark the two completed gather issues superseded. Cut the gather input line from
the map file format and the parser.


**Unchanged and still valid:** the lock-free claim, hints and paged ring
buffers, giving up arrival order plus auditing what leans on it, immutable
destination arrays with the scrapyard, the mutex staying in the station record,
the output station as a pass-through, runtime box compilation, the three smaller
decisions, and the stale-question sweep — which just got considerably bigger.


Fifteen open, and about a third of them are now smaller than they were, because
a design with no pull path has less to say about locking, about task shapes, and
about what a port can be.


## One thing I deliberately didn't do


The file is still called `056-gather-timing.md` and its title is now *Why there
is no pull path*. That mismatch should be fixed, but five documents point at it
and three of those are about to be rewritten anyway — so renaming now means
fixing the references twice. It goes in the sweep, once the documents around it
have settled.


## Where I'd go next


The port record, because it's the most actively misleading thing on disk right
now: it describes four port kinds when there are three, and a locking scheme we
replaced twice over. Everything else references it.


Then the gather datapath document and the map file format, which are the two
places a reader would go to find out how input works and would currently be told
something false.


Then the sweep, then commit — which still needs you to tell me what to do
about the other work sitting in the tree.


--------------------------------------------------------------------------------

### User Request 36

Great, you're on the right track. Can you update the todo list as well? Anything
that can be stricken off of it before we finish up that first task, and all the
related documentation changes?


--------------------------------------------------------------------------------

### Assistant Response 36

Let me strike what the new design answered or absorbed.


Now the documentation. Starting with the one a reader would actually go to.


Now the retirements.


Done. Here's where things stand.


## Task list — four struck, four done


**Struck, because the new design answered or absorbed them:**


- *Where a port-source change sits relative to the rewiring lock* — the
  question existed because turning a port into a gatherer had to run the cycle
  walk. There are no gather cycles, so there's no tension: it takes the rewiring
  lock like every other structural change.

- *Whether the reader runs the completion pass at end of file* — the
  completion pass is now purely a report, and a report is opt-in by nature.
  Nothing to decide.

- *Where the station mutex lives* — folded into the lock-free claim task,
  which is where the question actually belongs.

- *Auditing what depends on arrival order* — folded into the decision task;
  they're one piece of work, not two.


**Done:** the design note, the gather datapath document, the retirements, and
the map file format.


**Nine left**, and several are smaller than they were, because a design with no
pull path has less to say about locking, task shapes, and what a port can be.


## Thirteen files changed


**The design note** replaces the gather-timing note in place: what pulling was
for, the three timings weighed for it, the counting problem that ended the
search, and what replaced it — with fresh-at-use named as gone rather than
weakened.


**The gather datapath document** keeps its slot in the reading order and becomes
*Statics and recalculation*: two port kinds, ring consumed and static peeked, a
write as an event, and construction's writes as what starts a program.


**The map file format** loses the `in 0 reader` form and two loader checks that
can no longer be stated. The worked example is rewired so it doesn't demonstrate
a syntax that's gone.


**The guarantees document** got the biggest change and it's mostly good news:
two guarantees that were marked *being given up* are restored, because removing
the pull path keeps them outright — a task is self-contained, and a task's run
time is its box's run time. Four gather guarantees left rather than being
weakened. Three arrived: a static is never consumed, a write runs the ordinary
readiness check, and **every box runs on a worker that picked up a task for it**
— the one exception in the engine, gone.


**Delivery, loading, stations-and-slots, and packaging** all had passages
describing a path that no longer exists. Each now says what was there, why it
went, and where the reasoning lives, rather than quietly not mentioning it.


**The three gather issues** are marked rather than deleted. The pickup issue
keeps its design under a header saying it won't be built and what replaced it;
the two completed ones record that the capability they built is being removed
— along with the findings that outlived it, which in both cases turned out not
to be about gathering at all. The push-loop finding especially: *a push loop
needs a finite companion input to ever stop* is now the load-bearing half of
what phase four discovered.


**The roadmap and table of contents** call phase four *Configuration* now, since
the pull path was half its content.


## What I'd do next


**The port record issue.** It's the most actively misleading thing on disk —
four port kinds when there are three, and a locking scheme replaced twice over.
Everything references it.


**Then the stations-and-slots document**, which I've corrected but not
rewritten; it still describes the record as it is rather than as it's becoming.


Then the remaining decisions and the sweep. And the commit still needs you to
tell me what to do about the other work sitting in the tree.


--------------------------------------------------------------------------------

### User Request 37

Great! Keep working on the documentation.


--------------------------------------------------------------------------------

### Assistant Response 37

Now the arrival-order non-guarantee, which is the documentation half of that
decision.


Now the output station. Rewriting it surfaced something the original conflated.


Ten of the fourteen tasks are done. Here's the stretch.


## The port record, rewritten


It was the most misleading thing on disk and now it's the central record. Three
tags — ring, static, none — with the gatherer gone. Cells carrying their own
state, where the state *is* the lock: one atomic flip takes ownership, so no
mutex is involved and the exclusion is per cell rather than per port. The state
on the cell rather than in a parallel array, so a writer and a reader at
opposite ends touch different cache lines when the value is big enough for that
to matter. The claim walking ports in ascending order, claiming or rolling back,
with the fixed order explained as the thing that prevents two threads from
grabbing crosswise forever. Scanning from a bookmark instead of computing a
position, and the distinction that makes it work written down plainly: **a
position must be exact and is therefore computed from the capacity; a hint may
be wrong and therefore is not.** And growth by adding a page, with the ordering
hazard it used to have — duplicate if you copy first, empty buffer if you
publish first — noted as having stopped existing rather than being handled.


## The stations document caught up


Its slot-kind table now reads ring, static, none, with what each one does to
readiness. It says where the two changes are coming — a static's value moving
onto the port, and head and tail going away — rather than describing the
record as though it were settled.


## The output station, and something it had been hiding


Rewriting it surfaced a conflation in the original. It had the output station be
an ordinary station running an ordinary box, which tangled two different things:


**Doing something with a result** — writing a file, sending a datagram — is
an ordinary box on an ordinary station. It needs no designation and it was never
the missing piece.


**Saying what a program produces** is the missing piece, and it isn't a
computation at all. It's a name for a set of ports.


So the output station is a pass-through that runs no box, and its input ports
are the program's output ports. Three warnings in the old draft evaporate —
that an output box runs concurrently with itself, that it must issue one write
per invocation, that a file handle held as a static will never be closed. None
of that applies to something that runs no code. All of it still applies to a box
that writes to disk, stated where it belongs.


**And the input station now exists as its own issue**, symmetric: a pass-through
whose output ports are the program's input ports. Command-line arguments reuse
the statics text reader unchanged, so struct arguments in brace syntax come
free. Input port types are derived from what they feed, so nothing new goes in
the file.


## Two more issues written


**Destinations without a lock** — the output half of getting the mutex off the
hot path. An immutable array published by pointer swap, and the scrapyard:
per-worker counters, odd while walking, filed snapshots, swept by a later
rewiring. Retire-don't-free first, because it's three lines and enough for
everything that exists.


**Boxes compiled while the program runs** — with the honest constraint stated
up front: a signature gives you names, a compiler gives you sizes and offsets,
and there's no version that skips the compiler. Shape-based type comparison is
named as a hard prerequisite rather than a companion, because by-name comparison
makes this corrupt silently.


## What's left


**Two decisions for you:** whether an unconfigured port survives into a dump,
and whether a program has one output station with several ports or several
output stations. The second matters more than it looks — it decides whether
the multi-output-boxes issue is about boxes catching up to programs or about
both being the same shape from the start.


**One piece of code work:** retiring the test that asserts in-order delivery.
The decision is documented; the test still passes and shouldn't.


**And the sweep** — the stale questions across the issue files, plus renaming
the design note to match its subject now that the six documents pointing at it
have settled.


--------------------------------------------------------------------------------

### User Request 38

okay so the output stations: each output station has one or more output ports.
The output ports are kinda like a mini station - they must have their values
filled before they can pass values down the output port. An output station might
have let's say 5 different input ports, but two output ports, and the first one
needs the first 3 input ports, and the second one needs the last 2 input ports.
So, if the first three are filled, we can send data down the first output port,
but not the one below it.


Each port handles the data that passes through it. That's their whole job. With
an output port specifically, it has two options - the first, is that it's part
of a larger boxgraph, and so it should pass it's values along to the next box.
It writes them to an input port ring buffer as usual. The second case is when
it's NOT wired up to another box. In this case, there must be procedures written
about what to do - by default, it writes to a file in a tmpfs like /tmp/. The
destination should be configurable. Each output port might write to a separate
location. When it writes, it appends to the file, like a log, instead of
overwriting. Any questions about that?


--------------------------------------------------------------------------------

### Assistant Response 38

Yes, several — and one of them is a genuine fork rather than a detail. Let me
first say back what I understood, so you can catch me if I've got it wrong.


**An output station has input ports and output ports, and the output ports own
subsets of the inputs.** Five inputs, two outputs, first output owns inputs one
through three and second owns four and five. Fill one through three and the
first output fires; the second sits waiting on four and five. So readiness is
per output port, over its own group, not over the whole station.


**An output port has two behaviours depending on whether anything is wired to
it.** Wired: it delivers onward like any port. Unwired: it writes, appending, to
a configurable file that defaults to somewhere under a tmpfs, one destination
per output port.


## The questions


**1. What actually crosses when an output port with three inputs fires?** Three
possibilities and they're quite different:


- *Three values, one per wire* — the group being purely a barrier, so no
  partial set ever crosses the boundary. The parent sees three ports.

- *One value* — the three are packed into a record and the parent sees one
  port carrying a struct.

- *Three writes to the same place* — which is really the file case, where "one
  destination" means one file and the three values are one record in it.


I lean toward the first, because it makes the barrier the whole idea and doesn't
invent a bundling type. But your phrasing — *"it writes them to an input port
ring buffer"*, singular — reads like the second.


**2. How is the grouping written down?** Your example is contiguous — first
three, last two — which could just be a count per output port. Or it could be
an explicit list, which handles a program whose outputs interleave. Contiguous
is easier to read and easier to get wrong when someone reorders parameters.


**3. Are the group's input values consumed when it fires?** I'd assume yes —
they're ring ports, one value each per firing, exactly like a normal claim.
Worth saying, because the alternative would make them behave like statics.


**4. What does the file actually contain?** The engine knows every value's
layout, so it could write raw bytes or it could write text using the field
tables it already has for parsing statics. Text is readable and round-trips
through the same reader; bytes are smaller and exact. This also decides whether
one firing is one line.


**5. Where is the destination configured?** A static on the output port would
mean it uses machinery that already exists and is changeable at runtime through
the same call that turns any other knob. The box file is the other candidate.


**6. Does the default belong in `/tmp` or in the shared-memory tier?** Your own
convention puts executables in `/tmp/{project}` and artifacts and logs in
`/dev/shm/{project}`, reached through `tmp/shared-memory/`. Program output reads
like an artifact to me, so I'd default there — but you may mean something more
durable than a RAM disk, since a result that vanishes on reboot is a strange
default for "here is what the program computed."


## Two things I'd flag


**The per-output-port grouping is a new concept, and there may be a way to not
need it.** Every other station in the engine has one readiness check over all
its ports. This is the first thing with several checks over subsets. You could
get identical behaviour from **several output stations, each with its own inputs
and one output** — no subsets, no grouping syntax, no new readiness shape, and
the existing check does all of it.


What you'd lose is that a program would have several doors out instead of one.
If having exactly one named output is the point — and I think it might be,
since it's what makes "a program is a box" a clean sentence — then the
grouping is the price and it's worth paying. But it is a price, and it's paid in
a concept that appears nowhere else.


**An unwired output port writing a file is the first time the engine itself does
I/O with program data.** Everything the engine writes today is diagnostics —
buffer reports, statistics, dumps. Results have always been a box's business.
This makes the engine responsible for a file handle, for appending safely from
several threads at once, and for what happens when the write fails.


Two consequences worth deciding rather than discovering. **The same program
behaves differently depending on whether it's composed** — standalone it
writes a file, embedded in a parent it doesn't. That's clearly intended, but it
means testing a sub-program in isolation exercises a path its real deployment
never takes. And **the log's order is not the program's order**, since we just
gave up arrival ordering and several invocations can append concurrently. A
results file that looks sequential but isn't is the kind of thing somebody will
build on before noticing.


--------------------------------------------------------------------------------

### User Request 39

  3. Are the group's input values consumed when it fires? I'd assume yes —
     they're ring ports, one value each per firing, exactly like a normal claim.
     Worth saying, because the alternative would make them behave like statics.


yes they're consumed


  2. How is the grouping written down? Your example is contiguous — first
     three, last two — which could just be a count per output port. Or it
     could be an explicit list, which handles a program whose outputs
     interleave. Contiguous is easier to read and easier to get wrong when

  someone reorders parameters.


I think it doesn't matter, they should be specifically wired not assumed from
position.


  4. What does the file actually contain? The engine knows every value's layout,
     so it could write raw bytes or it could write text using the field tables
     it already has for parsing statics. Text is readable and round-trips
     through the same reader; bytes are smaller and exact. This

  also decides whether one firing is one line.


I think it should be definable in the output box how we want to handle it. But
in the case where it's not defined, I think we should write it as text, because
bytes can be wrong, even if interpreted by the same program. I think? Confirm?


  5. Where is the destination configured? A static on the output port would mean
     it uses machinery that already exists and is changeable at runtime through
     the same call that turns any other knob. The box file is the other
     candidate.


Sure. A static works.


  6. Does the default belong in /tmp or in the shared-memory tier? Your own
     convention puts executables in /tmp/{project} and artifacts and logs in
     /dev/shm/{project}, reached through tmp/shared-memory/. Program output
     reads like an artifact to me, so I'd default there — but you may

  mean something more durable than a RAM disk, since a result that vanishes on
  reboot is a strange default for "here is what the program computed."


good point. This is very platform dependent. I don't know if I have an answer,
because running on two systems with different OSs will have differing answers.


  The per-output-port grouping is a new concept, and there may be a way to not
  need it. Every other station in the engine has one readiness check over all
  its ports. This is the first thing with several checks over subsets. You could
  get identical behaviour from several output

  stations, each with its own inputs and one output — no subsets, no grouping
  syntax, no new readiness shape, and the existing check does all of it.


  What you'd lose is that a program would have several doors out instead of one.
  If having exactly one named output is the point — and I think it might be,
  since it's what makes "a program is a box" a clean sentence — then the
  grouping is the price and it's worth paying. But it is a

  price, and it's paid in a concept that appears nowhere else.



good point. Yeah your design is better. Let's do multiple output stations if we
need them. Then, there's no boxes that have more than one output port.


  An unwired output port writing a file is the first time the engine itself does
  I/O with program data. Everything the engine writes today is diagnostics —
  buffer reports, statistics, dumps. Results have always been a box's business.
  This makes the engine responsible for a file

  handle, for appending safely from several threads at once, and for what
  happens when the write fails.


  Two consequences worth deciding rather than discovering. The same program
  behaves differently depending on whether it's composed — standalone it
  writes a file, embedded in a parent it doesn't. That's clearly intended, but
  it means testing a sub-program in isolation exercises a path

  its real deployment never takes. And the log's order is not the program's
  order, since we just gave up arrival ordering and several invocations can
  append concurrently. A results file that looks sequential but isn't is the
  kind of thing somebody will build on before noticing.



good point. Maybe we should write to an output ring buffer and just, hold onto
the values until the output port has something wired from it.


--------------------------------------------------------------------------------

### Assistant Response 39

Your last line makes most of the rest unnecessary, so let me take the settled
things quickly and then get to it.


**Consumed on firing** — yes, ordinary ring ports, one value each per claim.


**Multiple output stations** — good, and that removes the subset-readiness
concept entirely. Nothing in the engine gains a new shape.


**Destination as a static** — it costs nothing, since statics already live on
ports and are already changeable from outside at runtime.


**Text by default, definable per station** — I'd confirm your reasoning, and
there's a sharper version of it. Raw bytes are exact only against the *exact*
build that wrote them; change a struct's layout and yesterday's file silently
means something else. Text goes back through the field-table reader, which
resolves layout at read time, so it survives a rebuild. It also round-trips —
a results file is readable as a statics section, which means a program's output
can be another program's input with no conversion. One thing to know: the reader
exists, the **writer doesn't**. Turning bytes into text by walking a field table
is the mirror of the parser we have, and nobody has needed it yet.


## One thing to check about "no boxes have more than one output port"


I read that as: **no box produces more than one value.** A comparator still has
three ports and an iterator many — but those carry the *same* value routed to
one of several places, which is a property of the station, not of the box. Ports
are a choice of destination, not a set of results.


If that's right, it retires the multi-output-boxes issue. Its whole argument was
that a program with several output stations, used as a box, *is* a box with
several outputs — so if maps and boxes are to be the same kind of thing, boxes
need what maps have. With several outputs being several *stations*, that parity
claim evaporates. What's left is only the efficiency case: a box computing two
related results from shared work has to be two boxes computing the shared part
twice. Worth keeping as a note, not worth a mechanism.


## The platform question — push it out of the engine


You're right that there's no answer, and I think that's the answer: **the engine
shouldn't have a default path at all.** Whoever runs the program supplies one. A
shell wrapper can default it to the project's shared-memory tier, which is a
project convention; an embedded C application defaults it to wherever that
application keeps things. The engine knowing about `/tmp` would be the engine
knowing about an operating system, which is the sort of thing the packaging note
spends its whole length trying to remove.


## Buffering instead of writing — yes, and it dissolves nearly everything above


This is the better design and I want to be clear about how much it takes with
it.


**The earlier objection doesn't apply here.** We scrapped output buffering
because an unwired port currently discards, and that's *deliberate* — an
unwired comparator branch is normal, and buffering it would grow forever. But an
**output station's** port is not that. Discarding a program's results means the
program did nothing. Holding them is the right answer for exactly this port and
no other, which is why the general rule and this one can both be true.


And look what stops being a problem:


- No file handle for the engine to own.

- No appending safely from several threads.

- No decision about what to do when a write fails.

- No platform-dependent path.

- No format question at all, at this layer.

- No "the log looks sequential but isn't," because a ring buffer never pretended
  to be a log.


The engine goes back to never touching the filesystem for program data, which is
where it was before this conversation and where it should probably stay.


**And it needs one thing to be complete: somebody drains it.**


For a **sub-program**, nobody has to — the parent's wires are attached and the
buffer never fills.


For a **standalone program**, the runner drains it. Which gives the interface a
shape I like: **outside the graph can write a static, and outside the graph can
read an output buffer.** Two operations, exact mirrors, and together they're the
program's whole surface to the world. Everything else — the file, the format,
the path, the platform — becomes the runner's business, written in ordinary C
by whoever is running the thing.


Draining at the end is easy: the pool declares completion, and the runner reads
what's there. Draining a program that never ends is the same call made
repeatedly, which is what any long-running consumer would do anyway.


**One thing this leaves open**, and it's the only cost I can find: an unwired
output buffer on a program nobody drains grows without bound. That's visible —
buffer growth reporting already exists and would shout about it — but it's now
the failure mode where the file was, and it fails by consuming memory rather
than by filling a disk. Whether that wants a cap, and what a cap would do when
reached, is a real question and I don't have a good answer to it yet.


--------------------------------------------------------------------------------

### User Request 40

  Text by default, definable per station — I'd confirm your reasoning, and
  there's a sharper version of it. Raw bytes are exact only against the exact
  build that wrote them; change a struct's layout and yesterday's file silently
  means something else. Text goes back through the

  field-table reader, which resolves layout at read time, so it survives a
  rebuild. It also round-trips — a results file is readable as a statics
  section, which means a program's output can be another program's input with no
  conversion. One thing to know: the reader exists, the

  writer doesn't. Turning bytes into text by walking a field table is the mirror
  of the parser we have, and nobody has needed it yet.



yeah let's do text by default. We'll need that writer. And if the user wants to
write their own output spec, we should allow them to - basically, ship a default
C function (like a box function!) that writes to a specific location (static
value on every output station?) and then let the user swap out that C function
for a different one. Easy peasy.


  One thing to check about "no boxes have more than one output port"


  I read that as: no box produces more than one value. A comparator still has
  three ports and an iterator many — but those carry the same value routed to
  one of several places, which is a property of the station, not of the box.
  Ports are a choice of destination, not a set of

  results.



yep you're right, my bad.


--------------------------------------------------------------------------------

### Assistant Response 40

Both taken. There's one tension between that and what we settled last turn, and
I think it resolves in a way you'll like better than either half.


## The tension


Last turn: an unwired output port **buffers**, and the engine never touches the
filesystem.


This turn: an unwired output port runs a **default C function that writes**,
swappable.


Those can't both be the unwired behaviour. But they compose if the writer isn't
special.


## The synthesis: the writer is just a box, and the runner wires it


**Unwired means buffer. Always. No exceptions, no fallback function.**


**Writing results to a file is a box** — one we ship, called something like
`write_text`, taking the value and getting its destination the way every other
box gets configuration: **a static on its own station.** Exactly how a read box
already gets its file path today. Nothing new.


**Running a program standalone means the runner wires that box to the program's
output**, unless told otherwise. So from where you're standing it still looks
like "run it and results appear in a file" — but the mechanism is a wire, and
swapping the writer is wiring a different box rather than replacing a function
pointer the engine holds.


What that buys over a fallback function on the output station:


- **The engine still never does I/O with program data.** No handle to own, no
  concurrent appends to get right, no decision about what a failed write means
  — those all become the writer box's problem, which is to say an ordinary
  box's problem, which is a solved category.

- **The output station stays a pure pass-through.** It runs no code. That was
  the thing that made three warnings disappear from its issue, and a fallback
  function would quietly bring them back.

- **The destination lives where every other piece of configuration lives.** Not
  a special static on the output station — a static on the writer's station,
  indistinguishable from a read box's path.

- **Swapping the writer needs no mechanism at all.** Write a different box, wire
  that one instead. There's nothing to "allow"; it's already allowed.

- **And running standalone becomes a special case of composition.** The runner
  is a tiny parent that wires a writer to a sub-program's output. Same act, same
  machinery.


The one cost: a program that nobody wires and nobody drains buffers forever.
That was already the cost last turn, and buffer growth reporting will shout
about it.


## The formatter is needed anyway, and sooner than you'd think


Turning bytes into text by walking a field table isn't only for output. **The
dump needs it the moment statics stop keeping their original text.** Right now a
statics entry holds both the parsed bytes and the text it came from, and the
dump writes the text back out. Once a static's value lives on the port that
reads it — with no table and no retained string — the dump has nothing to
print unless it can format bytes.


So it's one piece of work serving three callers: the shipped writer box, the
dump, and eventually anything that wants to show a value to a person.


## One thing left to confirm


Does the runner wire the writer **by default**, or only when asked? Default
means "run a program and results appear somewhere," which is friendly and
matches what you described. Not-by-default means a program that buffers is the
base case and getting output is a deliberate act, which is more consistent but
will surprise the first person who runs something and finds nothing.


I'd take default-on with a way to turn it off, and I'd have the runner say where
it wrote — the same way the seed sweep announces what it seeded, so nobody has
to guess.


Say which, and I'll write up this whole stretch: the output station as several
stations rather than grouped ports, the writer as a box, unwired-means-buffer,
the drain as the mirror of a static write, and the formatter as its own piece of
work. Plus retiring the multi-output-boxes issue, which your correction just
made unnecessary.


--------------------------------------------------------------------------------

### User Request 41

Hmmmmm okay what I think we should do is just buffer forever, and forget the
file writer. Maybe emit a warning every time the output buffer resizes.


--------------------------------------------------------------------------------

### Assistant Response 41

Simplest answer and it removes the most. Let me write it all down.


Written down.


**The output station** now says several stations rather than grouped ports, with
the reason kept: the subset-readiness version would have been the only thing in
the engine with several readiness checks over subsets of its ports, and several
stations gets identical behaviour from the check that already exists.


**Unwired means hold**, and the file writer is gone along with the handle, the
concurrent appends, the failed-write decision, the platform-dependent path, and
the format question. The rule and its exception are both written down as being
about different things — discarding is right for an unwired comparator branch
and wrong for a program's results.


**Growth warns from the first doubling**, framed as a third diagnosis beside the
two the phase 7 report already draws: a slot piling up means uneven inputs, the
task ring piling up means slow consumers, and an output buffer piling up means
nobody is collecting the results at all.


**Taking values out is the mirror of writing a static in** — those two calls
are a program's whole surface to the world, and everything a file would have
done is ordinary C in whoever runs it.


**The multi-output-boxes issue is retired**, with its argument recorded as
having dissolved rather than been rejected: a program's several outputs are
several stations, so there's nothing for a box to catch up to. What outlived it
is your correction — ports are a choice of destination, not a set of results
— which is the sentence that makes "a box produces one value" and "a station
has several ports" both true.


**And the formatter got recorded** where the need actually arises: the dump
keeps a static's original text today, and the moment a value moves onto its port
with no table, the dump has nothing to print. One piece of work, three callers.


## What's left


**One decision:** whether an unconfigured port survives into a dump.


**One piece of code work:** retiring the in-order delivery test, which still
passes and shouldn't.


**Two writing jobs:** the formatter as its own issue, and the sweep — the
stale questions across the issue files, plus renaming the design note now that
everything pointing at it has settled.


**And the commit is still waiting on you.** My changes are cleanly separable,
but the tree is still carrying somebody else's uncommitted work — seventy-odd
regenerated pages, rewritten demos, four new demo-engine files. Tell me whose
that is and whether to stage only mine, and I'll get this session's work into
history before it grows any further.


--------------------------------------------------------------------------------

### User Request 42

  One decision: whether an unconfigured port survives into a dump.


I think so?


  One piece of code work: retiring the in-order delivery test, which still
  passes and shouldn't.


it still passes because we haven't implemented the new design, right? or is
there another reason?


  Two writing jobs: the formatter as its own issue, and the sweep — the stale
  questions across the issue files, plus renaming the design note now that
  everything pointing at it has settled.


great let's work on that.


--------------------------------------------------------------------------------

### Assistant Response 42

Two quick things, then I'll work.


**On the test — you're right and I said it badly.** It passes because it's
*correct about the engine as it exists*: head and tail are still positions,
delivery is still FIFO, nothing has been implemented. It isn't a test that's
wrong now; it's a test that stops being right at the moment the lock-free claim
lands. So it isn't independent work — it's a note attached to that change.
Folding it in.


**On the dump — recording it as yes.** Let me put it where it belongs and pick
up the syntax it implies.


Now the sweep. Starting with the rename, since everything pointing at it has
settled.


The HTML is generated and regenerates; the markdown sources need fixing. Same
reasoning applies to the gather datapath document, so let me do both renames at
once.


Renames done — 39 links across 16 files. Now the stale questions.


The list is clear — every task is done.


## What the sweep found


**Answered questions still reading as open, in four issues.** Each is now marked
answered with its reasoning kept, because the reasoning is usually the more
valuable half:


- The gathering issue's three questions are all moot, but two of them were about
  the engine rather than about gathering, so their answers survive. A gather
  that fails halfway could only ever have been fatal — by the time a worker
  holds the task, the ring values are already out of their buffers and can't be
  put back, so abandoning loses data silently, and retrying isn't safe because a
  box that reaches out to the world may already have consumed what it read. And
  the sleeper-count question had a wrong premise, which is yours: a gathered box
  is a box, so there was never an engine-versus-your-code distinction to draw.

- The station table issue's hand-raising question is moot in the issue and alive
  in the strategem, which is where it belongs now.

- The construction issue lost three questions to the seed sweep's disappearance
  and kept one, with your out-of-memory answer sharpened: the distinction has to
  be visible to *code*, not just to a reader, or anything that retries on
  failure spins forever on the kind that can't succeed.


**And stale statements, which I wasn't looking for but couldn't leave.** The
construction issue still had a whole section arguing about *when* to seed — a
rule that no longer exists. Step three is now "let the construction writes take
effect," with the reason they land at the end rather than as they happen: a
static bound in step one would make its station ready before step two had drawn
any wires, so it would run and deliver into a program that wasn't connected yet.
Same reason the wiring is a batch.


The guarantees document claimed the seed sweep and the delivery walk share one
door; that's now stated as everything sharing one door — a delivery arriving,
a static being written, the statics bound during construction, all reaching the
same readiness check. And both it and the stations document said the station
count can't grow, which shelves change — kept as a guarantee rather than
traded away, since nothing already placed is copied.


## The renames


The design note is `056-no-pull-path.md` and the datapath document is
`004-datapath-statics.md`. Thirty-nine links across sixteen markdown files
retargeted by a script rather than by hand, which deliberately left `docs/HTML/`
alone — every generated page embeds the full sidebar, so they all carry the
old names, and they come right on the next generator run. Hand-editing a build
artifact would be the wrong kind of fix.


That script might be worth keeping in `scripts/` rather than the scratchpad.
Renaming a document and chasing its links is not a one-time problem, and it's
the kind of thing that gets done badly by hand.


## What's actually left


**The HTML set knows none of this.** Two renamed documents, eight new issues,
and substantial rewrites to nine more. That's one generator run — but the
generator is still mid-edit under the documentation issue, so I've left it alone
all session.


**And the commit.** Every piece of this session's work is uncommitted, sitting
alongside somebody else's: seventy-odd regenerated pages, six rewritten demo
sources, four new demo-engine files, and a re-opening of the statics issues that
pulled them out of `completed/` before I got here. Mine are cleanly separable.
Tell me whose the rest is and I'll stage only what I touched.


--------------------------------------------------------------------------------

### User Request 43

Okay let's keep working on the docs then :)


--------------------------------------------------------------------------------

### Assistant Response 43

Plenty left. Let me find what's actually stale rather than guess.


The core documents are clean. The completed issues aren't — several describe
built work that's being removed. Let me mark them the way the gather pair got
marked.


The phase 4 demo is mostly a demonstration of the pull path — that needs more
than a mark.


The pull path is now out of every document that described it. Here's the
stretch.


## Twelve completed issues marked


Each says what's changing, why, and — the part worth the writing — **what
outlived the mechanism.** A few that earned their keep:


**The seed sweep** is removed outright, not replaced. But it established that a
task must come into existence by exactly one path, and that instinct is now the
whole design: a delivery arriving, a static being written, and a program being
built all reach the same readiness check.


**Load-time validation** loses half its rules and keeps its shape — collect
failures and print them together, because somebody fixing a new program wants
the whole list rather than one error per run.


**Runtime rewiring** turned out to have been the foundation rather than a late
feature. It called itself *the feature the whole design had been quietly
preparing for*; it was preparing for something larger, because editing a running
program and building one stopped being two things.


**The map dump** forced station names to be kept when the design had proudly
discarded them — a thing that seems like pure overhead until something needs
to describe itself. And it's about to need the bytes-to-text formatter, since
it's been printing a static's *original string* and there won't be one.


**Hand-built maps** got the most useful correction: the reason a hand-placed
station can't bind a static was never a limitation of hand placement. It's an
argument nobody ever passed.


## Five phase progress files rewritten


Phase 4 shrank the most and now says so — it was *the pull path and
configuration* and is now just configuration, with the pull path's failure
recorded as a finding rather than hidden. Phase 5 is the only phase that got
*smaller*, its one open issue retired rather than built. Phase 2 grew a capstone
nobody planned: six new issues, ending in one construction surface. Phase 3
notes that its field tables were emitted and then unused for four phases, and
now answer two separate problems. Phase 7 lost the station-table issue to phase
2, because a late convenience turned out to be the mechanism underneath every
program's first moment.


## One thing I'd flag


The two backlog diagnoses this project drew in phase 2 are now three, and the
third behaves differently: a slot piling up means uneven inputs, the task ring
piling up means slow consumers — both performance signals, summarised at
teardown — and an **output buffer** piling up means nobody is collecting the
results at all. That one fires from the first doubling, because by the time it's
summarised it's too late to matter.


## What's left


**One task:** the demo scenes. Three demos show capabilities that are going, and
each issue now names what should replace it — phase 4 worst, with four of five
scenes gone.


**And the two things that have been waiting all session:** the HTML set knows
none of this, and nothing is committed.


--------------------------------------------------------------------------------

### User Request 44

Okay great :)


--------------------------------------------------------------------------------

### Assistant Response 44

Good session. The engine got meaningfully smaller — a whole path through it
removed, nine concepts with it, and the pool never changed once for any of it.


Three loose ends, all waiting on you rather than on me:


**The demo scenes** — one open task, with each affected demo now naming what
should replace what.


**The HTML set** — one generator run, held all session because that generator
is mid-edit under the documentation issue and running it would mix somebody's
unfinished work into the output.


**The commit** — everything from today is uncommitted, sitting alongside work
that isn't mine. Say whose it is and I'll stage only what I touched.


And a small one: the script that retargeted thirty-nine links across sixteen
files is still in the scratchpad. Renaming a document and chasing its links
isn't a one-time problem, so it might belong in `scripts/` — your call.


--------------------------------------------------------------------------------

### User Request 45

Okay let's work on the demo scenes, HTML, and then commit.


--------------------------------------------------------------------------------

### Assistant Response 45

Let me look at the demos before I touch them — they're runnable programs, and
something about the timing bothers me.


The demos call `map_slot_gather()` and `map_statics_alloc()` directly — the
phase 4 one is 32KB of C built around demonstrating gathering. **Rewriting them
now would produce programs that don't compile**, because the engine they run
against still has the pull path. The demos are the deliverable; they have to
run.


So the scene rewrite is downstream of the implementation, not parallel to it.
What isn't blocked is specifying it properly — right now that's scattered
across three completed issues' marks.


The generator's uncommitted change isn't half-finished — it's the complete
hand-written pass for implementation notes that the documentation issue already
describes and flags as the wrong *shape*. It works. That unblocks the HTML.


98 pages, one broken link — my retarget script's file list missed the
completed demo issues.


Five orphans — two from the renames, two from issues that moved back out of
`completed/`, one from a page that was open when it was generated and is now
finished.


98 pages, zero unresolved references.


Now — before committing, I worked out whose the other work is, and it changes
the picture.


The word-problems issue said in writing: *"This issue stays open until the set
is regenerated."* I just regenerated it. So that work is finished, and the thing
it was worried about — the generator being mid-edit — turned out to be a
complete eleven-line addition, not unfinished work.


Let me close it, since the regeneration earned that.


**[Asked the user]**


*Q1 — Commit split:* The tree holds two pieces of work: the
demos-as-word-problems rewrite from a previous session (now complete), and this
session's removal of the pull path. Many files carry both — the three demo
issues, the documentation issue, the guarantees, the port record, and all 98
regenerated pages. A perfectly clean split isn't available. How do you want it
committed?

- Two commits, boundary where it's clean (Recommended) — First: the
  word-problems work — six demo sources, seven launcher scripts, the four new
  presenter files, the four demo issues only that session touched, the
  generator's implementation-notes pass, the Makefile and file index. Second:
  the pull path removed, plus every entangled file and the regenerated site.
  Honest about where the seam is rather than pretending there isn't one.

- One commit for everything — Simplest, and defensible since the regeneration
  is what completed the first piece — the two are genuinely coupled at that
  point. Costs the ability to read either piece on its own later.

- Only my changes; leave theirs uncommitted — Stages nothing that isn't mine.
  But the entangled files would have to be committed anyway, so their work lands
  in pieces regardless — and the regenerated site can't be split at all.

→ **Answered:** huh? tell me, why can't you just split the files at the
correct lines and commit only what is part of each change to that commit?


*Q2 — Link script:* The script that retargeted 39 links across 16 files after
the two renames is still in the scratchpad. Renaming a document and chasing its
references isn't a one-time problem.

- Keep it in scripts/ (Recommended) — Give it the next file index and a
  comment saying what it's for. It already skips generated HTML deliberately,
  which is the part that's easy to get wrong by hand.

- Leave it in the scratchpad — It did its job. If another rename comes up,
  writing it again is twenty minutes and the second version would probably be
  better anyway.

→ **Answered:** Leave it in the scratchpad". Read the answers carefully —
they may request clarification, changes, or that you not proceed — and follow
what they actually say.


You're right and I was wrong — hunks split cleanly, and your own convention
says to use `git apply --cached` for exactly this. Let me look at the shared
files properly.


Cleanly separable — hunk one is mine, hunk two is theirs. Let me read the
rest.


The demo issues split cleanly. Let me check the other three before building the
patches.


Fair question, and the honest answer is in three parts.


**For most files you're right and I was wrong.** The three demo issues split
perfectly — my mark is one hunk at the top, their scene descriptions are a
separate hunk further down. Same for the purely-one-session files, which are
most of them. I should have said "mostly separable" rather than "not available."


**For four files the two sessions are interleaved inside single hunks, not just
in the same file.** The phase 7 progress table is the clearest case:


```

| 702 — station statistics | **extended** | ...          ← mine

| 707 — demos as word problems | complete | ...          ← theirs

| 709 — slideshow and transcripts | open | ...           ← mine

```


Adjacent lines, one hunk. And worse, some lines are *theirs edited by me* —
they added a table-of-contents row pointing at `056-gather-timing.md`, and I
later changed both its text and its target. There is no line-level boundary
there, because the line has two authors.


Splitting those means me hand-reconstructing their intermediate state — the
version that existed when I started, which was never committed. I can do it: I
read every one of those files this session, so the content is recoverable. But
it's hand-written reconstruction across four files with no mechanical check that
I got it right, and a subtle error would put words in somebody else's commit.


**And the 98 generated pages genuinely can't be split**, at any granularity.
Every page embeds the full sidebar listing every other page, so both sessions'
work appears in all 98 files. There's no correct line to cut. That's a property
of it being a build artifact.


## One thing that's already lost, whichever way we go


The original gather-timing note — the 427-line document that laid out three
gather timings with their costs — **was never committed, and I overwrote it.**
`docs/implementation-notes/` is untracked, so there's no version in history to
recover. What replaced it is good and it cites the reasoning, but the original
analysis exists now only in the transcripts.


That's a consequence of doing several sessions of work without committing, and
it's worth naming before we do another one.


## What I'd suggest


Do the split, and let me hand-reconstruct the four interleaved files — you'll
get two readable commits, and I'll show you each reconstruction before staging
it so you can catch me if I've mangled someone's sentence. The generated pages
go in the second commit, because there's nowhere else for them.


Or, if that reads as more ceremony than it's worth for work that was never
committed anyway: one commit, message saying plainly that it covers two pieces
of work and why they arrived together.


Which?


--------------------------------------------------------------------------------

### User Request 46

well, we probably don't have to commit build artifacts, right? or are they HTML
builds? if so, then that's fine we can just commit the latest HTML. Otherwise,
just do your best. We probably don't have to hand reconstruct things.


  Or, if that reads as more ceremony than it's worth for work that was never
  committed anyway: one commit, message saying plainly that it covers two pieces
  of work and why they arrived together.


... yeah it's just ceremony really. It's helpful to stay organized.


--------------------------------------------------------------------------------

### Assistant Response 46

Right on both counts — the HTML is a build artifact, so committing its latest
state is the whole point, and hand-reconstruction isn't worth it.


Here's the rule I'll use, stated so you can check it: **commit one gets every
file this session didn't touch at all, plus the cleanly separable hunks in the
three demo issues. Commit two gets everything else.** No reconstruction, nothing
invented.


Exactly right — `MM` means their hunk is staged and mine is still in the
working tree. Now the whole files.


That last transcript is this session's — it doesn't belong in the prior
session's commit.


Both committed, tree clean.


## What landed


**`Every scene opens with something you can hold`** — the previous session's
demo work, which was complete and waiting on exactly the regeneration I ran.
Thirty-five scenes, the two presenters, the four new files, and the four demo
issues only that session touched. Plus its own three findings, including the one
worth repeating: the overhead scene had been reporting the instrumented build as
*faster* than the plain one, because the two were timed as blocks and whichever
ran second inherited a warm machine. Best-of-five made the claim more confident
rather than less wrong.


**`Nothing is pulled any more`** — this session. The removal, what killed it
(accounting, not cost), what replaced it, the nine concepts that stopped
existing, and the second pass over everything the removal touched.


## How the split actually went


The rule held: commit one got only files this session never touched, plus one
cleanly separated hunk each in the three demo issues — their scene
descriptions staged, my marks left in the working tree for the second commit. A
small script filtered the diff by hunk index rather than me hand-editing
patches, so nothing was reconstructed and nothing invented.


Four files were genuinely interleaved line-by-line and went whole into the
second commit: the table of contents, the map file format, the documentation
issue, and the phase 7 record. Along with the packaging note and the
implementation-notes README, which the previous session wrote and I edited —
their work is in my commit, and that's the part of the split that isn't clean.
It's named here so the record isn't misleading even where the attribution is.


## One thing that's gone


The original gather-timing note — four hundred lines laying out three timings
with their costs — was never committed and I overwrote it. It exists only in
the transcripts now. That's the price of several sessions of work without a
commit, and it's the argument for committing earlier next time rather than for
anything else.


## Where things stand


Every task closed. The engine is documented as it will be, not as it is —
which is the right order, since none of it is built yet. The nearest work is the
port record and the statics move; the demos can't be rewritten until those land,
which their issue says explicitly.


--------------------------------------------------------------------------------
