# Conversation Summary: bc2f1355-a646-454d-a731-9aab2bb3e23c

Generated on: 2026-08-22 01:03:57

--------------------------------------------------------------------------------

### User Request 1

rc


--------------------------------------------------------------------------------

### Assistant Response 1

"rc" isn't a term used anywhere in this project — I grepped the docs, notes,
issues, and the whole transcript archive, and it appears nowhere, so I don't
have a stored meaning to fall back on. The `input/` directory, which by your
convention is the first place a program looks for its startup instructions, is
empty too.


So I'd be guessing. Rather than guess wrong and touch files:


**[Asked the user]**


*Q1 — rc:* What does "rc" mean here?

- Review changes — Look at the working tree — right now the only
  modification is the growing transcript file for Aug 20–21 — and tell you
  what changed and whether it's ready to commit.

- Run and check — Build and run the test binaries (make test) and report what
  passes and what fails.

- Resume context — Read the tail of the current transcript and the phase
  progress pages, then pick up whatever thread was left unfinished last session.

→ **Answered:** Don't worry about it. Can you add the next 5 issue files to
your to-do list and keep working until they're done?". Read the answers
carefully — they may request clarification, changes, or that you not proceed
— and follow what they actually say.


I've read the open issues and the phase progress pages. Baseline: `make test`
passes clean.


**The five, in dependency order** (they're also the five lowest-numbered open
ones):


| # | Issue | What's left |

|---|---|---|

| 1 | 210g — one way to build a station | the loader and runtime editing become callers of the one port-configuration surface |

| 2 | 210 — what an input port is | parent; closes when 210g does |

| 3 | 212 — one way to build a program | loader reduced to a reader; composing two programs into one table; the add-to-a-running-program test |

| 4 | 209 — the output station | the load-time check on what's wired into a result, and a program used as a box |

| 5 | 106 — stopping on purpose | three signals, and refusals that end the program instead of returning a number |


Starting on the first. Reading what the loader and the rewiring path do today.


Now I'll make the change. First, the shared refusal wording in the station
layer:


Now the port-range refusal in the configuration surface, so the loader loses
nothing by dropping its own copy:


Now the header declaration, and the destination-port refusal in the wiring path:


Now the wiring path's destination-port refusal, so the loader can drop its copy:


Now the loader itself — the part that actually makes it a caller rather than a
builder:


Now the second pass — dropping the loader's private width check:


Now `map_load_file` itself:


Found a real defect the unification exposed — the wiring path's width check
was guarded on the *source* station having input ports, so any station with no
inputs (a constant source) skipped it entirely:


Both 210 and 210g have all their open questions answered. Updating the
blueprints to match what now stands:


Now the station-header companion, which was stale about the port surface:


The dry run exposed two ordering faults in my own tool — sibling links getting
re-broken, and completed-directory files being rewritten in the wrong direction.
Fixing before applying:


Now let me verify no markdown link in the tree points at nothing:


105 of those dead links predate this session — they're issues moved into
`completed/` over the project's history whose outbound links were never
repaired. The completed record is the project's stated blueprint, so it should
be walkable. Let me check what a repair would have to resolve:


Every markdown basename in the project is unique, and every referenced target
exists somewhere — so a repair can resolve by name without guessing. Writing
that as its own tool:


Now the phase-2 progress page, then the commit:


Tests pass. Splitting this into two commits — the tooling and the project-wide
link repair are separable from the issue work:


Two of the five are done. Moving to 212 — first checking what its remaining
steps actually still need, since 210g's work covered some of them:


The engine behaved correctly and my assertion was wrong — values from the
first batch were still in flight when the new wire was drawn, so they went down
it too. That's the documented behaviour; the test needs to account for it:


Step 9 is done. Now 212's real remaining work — composing. Its blueprint says
the table's design decisions must be re-asked *before* composing is built, so
let me get the facts first:


The parser splits arrow destinations on the *last* dot and already refuses
duplicate names within a file — both of which matter for composing. Let me
first take the one table question that has an answer rather than a decision:


Three genuine design decisions block the last piece of 212, and 212's own
blueprint says to ask them before building rather than after:


**[Asked the user]**


*Q1 — Composing:* All ten of 212's implementation steps are now done.
Composing — bringing one program inside another so they become one station
table — is described in its intended behaviour but was never one of those
steps, and it's the one part that needs real building rather than a field. Where
should it live?

- Its own issue (Recommended) — 212 closes now with its ten steps done and its
  table questions answered. Composing becomes a new phase-2 issue carrying the
  merge design. Keeps the capstone from growing a third time, and lets the five
  you asked for actually close.

- Inside 212 — 212 stays open until composing is built and tested. Honest to
  the issue as written — it does say a map is a box — but it means the
  capstone absorbs a feature the size of its own remaining work.

→ **Selected:** Its own issue (Recommended)


*Q2 — The child:* A wire is a pair of indices valid only inside one station
table, so merging renumbers every wire in the incoming program — the first
time this engine ever rewrites a wire. What happens to the program that was
brought in?

- Consumed by the merge (Recommended) — Its stations move into the parent, its
  table is emptied, and its handle stops being valid. Anybody still holding one
  of its station numbers is holding a stale number — this says so at the
  moment it becomes true instead of leaving it to be discovered.

- Copied, both survive — The parent gets its own copy and the original keeps
  running separately. Nothing is invalidated, but there are now two programs
  that look identical and will drift the moment either is edited.

- Consumed, and the handle is poisoned — As recommended, but the emptied
  program keeps a mark saying it was merged, so a later call on it refuses by
  name rather than acting on an empty table. Costs one field and one check per
  operation.

→ **Answered:** We're not taking a program that's already running and merging
it. We're adding new boxes, which is like importing a library. So, when we add a
box, instead of "renumbering" wires, which we technically are doing, think of it
more like we're translating from a template into a newly constructed and wired
piece of machinery. So if it says wire 0 goes here and wire 1 goes there, and we
have 55 stations currently, after importing we'd essentially say wire 0 goes
here + 55 and wire 1 goes there + 55.


*Q3 — Names:* Two programs may each have a station called `gate`. Names exist
so a program can be written back out as a file that reads in again — a merged
program with two `gate`s cannot be. What should a merge do about them?

- Prefix at the seam (Recommended) — The incoming program's names gain a
  prefix, so `gate` becomes `sub.gate`. The merged program still round-trips,
  and the format needs no change: the arrow parser already splits a destination
  on its LAST dot, so `sub.gate.0` reads as station `sub.gate`, port 0.

- Refuse a colliding merge — Simple and loud, but it makes composing a program
  with a second copy of itself impossible — which is the most obvious thing
  anyone would try.

- Leave names alone — Cheapest. The merged program runs perfectly and cannot
  be written down and read back, which breaks a property the dump tests
  currently rely on.

→ **Answered:** There might be another issue file that works on this, try and
find that. But my recollection was that we're going to include the path in the
function name. So, math.c:rand_int() becomes math_dot_c_colon_rand_int() - but I
think what you're asking about is station names not box function names, right?
Two box stations can be named the same thing. The name is an arbitrary text
label. We shouldn't use them for any mechanical purposes.". Read the answers
carefully — they may request clarification, changes, or that you not proceed
— and follow what they actually say.


The user's reframing changes the design substantially — composing is
instantiating a template, not merging a live program. Let me check the issue
they recalled:


Fixing a scoping question my own change introduced — with names as
within-description labels, arrow resolution must not reach past the stations a
read created:


Now the new issue for composing, carrying your reframing:


I mangled a heading there. Let me look and repair:


Three of five done. Now 209 — the output station. Its remaining check has a
tension with 212 that I need to look at before building:


The blueprint answers this one already, so I'll build it. First checking whether
a map that builds a map could even satisfy the new requirement:


The requirement exposed a real gap: a map that builds a map had no way to
declare the built program's doors. Adding those operations as boxes:


The new requirement forced the first program to gain a station, which
invalidates this scene's premise — and the honest replacement is a stronger
claim than the one it had:


Four of five done. Now 106 — the largest of them. Let me understand the pool's
surface first:


Noted — and that lands on something the engine already has. Let me capture it
properly before continuing, because it gives an existing invariant a hardware
reason.


Now 106. Checking what the reports can read:


Now the pool additions — the finish signal, the stop, and the per-worker
station:


Now I'll add the three pool capabilities 106 needs:


Pool additions build clean. Now the stopping layer itself:


Now step 7 — refusals stop the program instead of returning an ignorable
number. Let me see who relies on the current shape:


Now removing the ignorable return from the rewiring path:


Now the construction boxes stop instead of returning an ignorable zero:


Now the test that proves all of it — the signals, the reports, and that an
ignored refusal can't leave a half-built program:


The test hangs — the polite path can't complete while a standing promise is
held. That's the real shape of the mechanism, so the test should model it
properly:


Two real problems there — a race where the pool can finish before anyone asks
to be told, and a polite scene that wasn't actually exercising the polite path:


The child released the pool before the feeder registered its promise, so the
pool declared itself finished in that window — and the polite scene had been
passing for that wrong reason:


All five scenes pass. But step 5's escape can't work as built — with signals
blocked, a second interrupt during a blocked gather just sits pending. Fixing
that, then proving it:


All eight scenes pass. Let me check the one refusal 210g explicitly deferred to
this issue:


38 call sites in a recursive parser — so I'll route the stop rather than
refactor, and record precisely what accumulates and what can't:


Everything passes. Now the companion files and documentation for 106:


Now updating issue 106 itself and the phase page:


All five are done and the suite passes. Let me check the guarantees page, since
this touched concurrency:


All five are done, tests pass, and the tree is clean apart from one file I
deliberately didn't touch.


## What's finished


| Issue | What it took |

|---|---|

| **210g** — one way to build a station | The loader gave up its own port check, width check, and name table and became a caller of the configuration surface |

| **210** — what an input port is | Parent; closed when 210g did |

| **212** — one way to build a program | Last two steps built; four table questions answered before building; composing split out |

| **209** — the output station | Every program must declare where its results come from |

| **106** — stopping on purpose | Three signals, two reports, and refusals that end the program |


## Three things worth your attention


**Giving up a duplicate check uncovered a real hole.** The loader kept its own
wire-width check beside the wiring operation's. Removing it exposed that the
surviving one asked the width question only when *both* stations had input ports
— and a box that takes nothing and returns a value has none. Every program
here starts with one of those, so every one could be wired into a port of any
width at all. No program read from a file could ever have shown it, because the
loader's copy ran first. The second check wasn't redundancy; it was a curtain in
front of a gap only the newer path could reach.


**A guarantee was nominal.** 106 promised that a second ctrl+C always works. It
couldn't have: with every signal blocked, a second interrupt arriving during a
report sits pending behind a report that may never finish — so the escape
opened only when nobody needed it. It works now, and it took the only signal
handler in the engine.


**Three test scenes were passing for the wrong reason.** The child released the
workers before anything held a standing promise, so the last-sleeper rule
correctly declared the program over before it began. The scenes were asserting
things about a program that had already finished. The rule was working exactly
as written.


## Two things I need from you


**The transcript file.** `llm-transcripts/aug-20-26-through-aug-21-26.md` was
already modified when I started and documents the *previous* session's work (the
guarantees-page commit), not mine. I left it alone and committed around it —
but that means it's still uncommitted. Do you want it folded into a commit, or
is someone still writing it?


**Shared station tables.** I wrote your locality note up as
`090-one-table-per-processor.md`, and the engine turns out to already model it
— a wire cannot leave a table, a door doesn't care where its caller is. But
one reading is unresolved and the two ask for different engines: *one process
across sockets* costs nothing and is what everything currently assumes; *several
processes sharing a mapping* would put the mutexes, port arrays and slot pages
inside it as offsets rather than addresses, and would still leave a box being a
function pointer that means nothing in another process. Which did you mean?


--------------------------------------------------------------------------------

### User Request 2

commit the llm-transcript files with the relevant work. They should just ride
along.


Can you add the next 5 issues to the todo list and start work on them, in the
same way you just did the previous 5? Good job by the way you're a marvel of
engineering.


--------------------------------------------------------------------------------

### Assistant Response 2

Thank you — and noted on the transcripts; I'll treat them as riding along from
here.


Both belong: one is the previous session's paperwork, one is this session's.


Clean tree. Now the next five — reading them before I plan the order:


Here's my read of the next five. Three of the literal next five stand on
**311d**, which 212's completion just unblocked — so the completable order
pulls it forward:


| # | Issue | What's left |

|---|---|---|

| 1 | 217 — a program inside another | instantiating a description into an existing table; unblocks the last steps of 209 and 213 |

| 2 | 213 — the input station | port types derived from what they feed, the command-line path, a program used as a box |

| 3 | 311c — source rides in the binary | each included box source emitted a second time, as text |

| 4 | 311d — the map becomes code | the generator turns a map into construction calls, so no name survives into a running program |

| 5 | 311b — placement instead of records | the record, the table and the type-name strings deleted, which waits on 311d |


That leaves **311a** and **311** (the parent) as the immediate next pair.
Starting on 217.


Confirmed — `map_add_station` reuses freed places, so N consecutive calls need
not be contiguous. The offset story is the common case; the general mechanism is
a translation table. Implementing it that way:


Now the instantiation operation itself:


Now the test that proves it — including the two steps 209 and 213 have been
waiting on:


The station named `in` collides with the `in` keyword — a real constraint on
names that isn't written down. Fixing the test and recording it:


Now 217's last step — the operation as a box:


All four scenes pass. But 217's open question just became a live bug —
instantiating twice gives two stations named `way_in`, and the dump can't write
that back:


Now a scene proving a doubly-instantiated program can still be written down and
read back:


Now the documentation and the issue, then 217 closes:


Now 213. Its remaining steps are 3 and 5 — step 6 was just done by 217. Let me
read what step 3 actually asks:


Step 3 turns on something step 2 already decided. Let me check the text-to-bytes
reader that step 5 needs:


Step 5 needs the statics text reader pointed at arguments instead of at a file.
Extracting the parse so both callers share it:


My edits duplicated the header block. Let me look and clean it up:


Refactor is clean. Now the command-line path:


The same window 106 found — the pool finishes between release and the first
delivery. This trap has now bitten twice, so let me make it announce itself:


Now updating 213 and its documentation:


Two done. Now 311c — small and self-contained. Let me see what the generator
has at emit time:


Now the emission of source text itself:


Now a test that the carried text is byte-for-byte the source that was compiled:


311c's steps 1, 4 and 5 are done. Step 2's second half assumes something that
isn't true — let me record that before I ask about it:


Two open forks now block finishing 311c and 217, and both are yours to call:


**[Asked the user]**


*Q1 — Reading names:* 311c carries every box source in the binary, byte for
byte. Reading a *name* back out of that text — an argument's name, for a debug
report — needs a declaration parser at run time. The issue assumed it had one:
making the generator a C program made it callable as a **process**, not as a
library, and the parser has never been linked into the engine.

- Link the parser in (Recommended) — Two of the generator's four files — the
  text utilities and the parser, not the emitter — become part of every
  program. A program reads its own carried source with no subprocess, and
  runtime compilation could check a box before shelling out to compile it. Costs
  every program an arena allocator and about a thousand lines of what is
  currently build tooling.

- Ask the generator binary — It already answers --describe, and the engine
  already runs it to compile a box mid-run, so nothing new is depended on. Costs
  a process per question, and a program shipped without the generator beside it
  loses the feature — which is most of what carrying the source was for.

- Emit the names as data too — The generator knows every parameter name
  already. By far the cheapest, and it runs against 311b, which is deleting name
  strings from the engine. Counter-argument: these are read by reports, not by
  the engine, and a table of names beside a blob of text that already contains
  them is a strange thing to carry.

→ **Answered:** need more background on this question.


*Q2 — Handles:* 217's part-placing box says "put this part between here and
there" — one call, no handle escapes. It can't reach a part with several
entrances or several ways out, because that needs returning a handle carrying
two station numbers, and a box returns one value. I declined the struct on the
grounds that unpacking it downstream is "a function written to fit the engine"
— which is what 209 refused. But `program` is already a one-field struct
travelling on a wire.

- Accept the struct (Recommended) — 209 refused faking *several returns* as a
  struct. An instance handle is a genuine value you hold and ask questions of,
  the way a program handle is. Two small unpacking boxes stop being scaffolding
  and become ordinary accessors.

- Keep it as it is — One entrance and one way out covers the common case, and
  a part with several doors waits until somebody actually has one. Nothing is
  built on a guess about what that would look like.

- Wire list as text — One call and one value, with the wiring given as a
  string the engine parses — the way a constant is given. Keeps the handle out
  of the graph entirely, at the price of a second little language inside a
  string.

→ **Answered:** I have no idea - more background please? Try and make it
concise, focusing only on the technical details, but no code, just
datastructures and how they connect.". Read the answers carefully — they may
request clarification, changes, or that you not proceed — and follow what they
actually say.


Fair — let me lay out the machinery first.


## Question 1: where the ability to parse a declaration should live


**What the generator is.** A build-time program that reads each box source file
and produces one generated C file. From each function it extracts a name, an
ordered parameter list (each with a type name), and a return type. It holds that
as a *description* — boxes, each holding parameters — allocated in an arena.


**What ends up in the built program.** Per box: a shim (the call site), a record
(name, parameter type names and sizes, return type name and size), a placement
function (every size a folded constant), and — as of now — the source file's
text as a string.


**Why names are in question at all.** The engine never reads a type name. A wire
is legal when both ends count the same bytes, so names exist purely for
messages: *"box returns int (4 bytes), port takes double (8 bytes)"*. 311b is
deleting those strings on exactly that ground. 311c carries the source text so a
message can *recover* a name instead of the engine *storing* one.


**The gap.** Recovering `parameter 2 of mix is called factor` means parsing a C
declaration at run time. The only thing that knows how is the generator's parser
— and the engine reaches the generator by **running it as a process** (that's
how a box compiled mid-run gets compiled). The parser has never been linked in
as a library.


Nothing on the running path wants this. Only reports and error messages do.


## Question 2: what a parent gets back after putting a part inside itself


**The pieces.** A box takes arguments by value and returns exactly one value. A
station is one placement of a box; a wire joins one station's output port to
another's input port, by index. Construction operations exist as boxes so a map
can build a map — and to say *which* program, a box takes a value called
`program`, which is a one-field struct holding the program's address, travelling
on a wire like any other value.


**The problem.** Instantiating a description creates new stations. To wire to
what it just placed, a parent needs **two** numbers: where the entrance landed
and where the way out landed. A box returns one value.


**What's built** avoids the issue by doing the wiring itself: it takes the
program, the description, and the two endpoints, and returns success. Nothing
escapes — and it hard-codes *one* entrance and *one* way out.


**The alternative** returns a small record of two station numbers, and something
downstream reads each field — which means two boxes whose only job is to pull
a field out of a struct.


**The objection** is 209's: a station with several output ports was refused
because faking several returns as a struct means writing a function purely to
fit the engine, and this design says a box should be a function somebody was
going to write anyway. **The counter** is that `program` is already a one-field
struct on a wire and nobody called it scaffolding — a handle is a value you
hold, not a fake tuple.


**[Asked the user]**


*Q1 — Parser:* Given that only reports and error messages want parameter names
— nothing the engine runs — where should the ability to parse a declaration
live?

- Emit the names as data — The generator already knows every parameter name.
  It writes them out beside everything else. Cheapest by far; a program is
  self-contained. Costs: strings carried that the sibling issue is otherwise
  deleting, and a name table sitting beside a text blob that already contains
  those names.

- Link the parser into the engine — The generator's text utilities and parser
  (not its emitter) become part of every program. A program reads its own
  carried source with no subprocess, and could check a box before compiling it.
  Costs about a thousand lines and an arena allocator in every program.

- Ask the generator binary — Run it as a process and read its answer — the
  same dependency a mid-run compile already has. Costs a process per question,
  and a program shipped without the generator beside it loses the feature.

- Don't build it — Carrying the source stands on its own; recovering names
  from it waits until something concretely needs them. The issue closes short
  one step, recorded as declined rather than pending.

→ **Answered:** we should make the error messages and reports refer to
arguments by index, and size, on both ends of the wire, to highlight the
incompatibility, rather than attaching names. Maybe we build the parser in if we
supply a --debug or similar flag while the system is being built?


*Q2 — Handles:* And what should a parent get back when it puts a part inside
itself?

- A handle, with accessor boxes — The operation returns a record of two
  station numbers; small boxes read each field. Reaches parts with several
  doors. Treats a handle as a value you hold — the same status the program
  value already has.

- Keep it as it is — One call that instantiates and wires, covering one
  entrance and one way out. Nothing escapes and nothing is built on a guess
  about parts with several doors.

- One box per door — Separate operations that instantiate and hand back the
  entrance, or the way out. Avoids the struct, but each call would build its own
  copy unless the engine remembers instances — which is state it does not
  currently keep.

→ **Answered:** Okay so a map is adding a box to itself. This box has a
box_function() and it runs it as normal. This is the default case. Let's say
that the running map wants to add a box that is itself a map of boxes. All it
does is import that map as boxes to itself, copies the wiring, and connects the
input and outputs as requested. The imported map shares a thread pool and
station table with the running map. Adding a box and adding a map should be the
same operation, but adding a map is just recursive through a list of boxes (and
their wiring, connected after instantiating the boxes) while adding a box is
just a list of one.". Read the answers carefully — they may request
clarification, changes, or that you not proceed — and follow what they
actually say.


That reframing is much better than what I built. Let me restate it to be sure I
have it:


**Adding a box and adding a map are one operation.** A map is a list of boxes
plus wiring; a box is a list of one. What you get back is the same kind of thing
either way — and for a single box, its entrance and its way out are the same
station. So a handle is never unpacked, because everything that consumes one
takes it whole.


Building that:


Now the box-level surface, so a map can do the same thing:


The old boxes are gone; updating the two tests that used them:


Now the other answer — refusals that name both ends by position and size
rather than by type name:


Everything passes. Recording both answers in their issues:


Three of five done. Now **311d** — the keystone. Let me read the rest of it:


311d is the largest item in the backlog — 9 steps including moving the map
parser out of the engine entirely. Let me check what its core needs:


The parser only needs three enums from the engine, so the generator can read
maps. Let me see what a placement function looks like, since the generated map
should call those rather than resolve names:


The placement functions are `static`, so a generated build function must live in
the same file. Let me see how the generator is invoked:


Now adding map compilation to the generator:


A map is now compiled into the calls it describes. Now the test that proves it
builds the same program as reading the text:


The dumps match. Fixing an index collision — the map and the test both took
095:


Let me check whether 311b has anything completable now — its step 5 may
already be done:


311b's step 5 turns out to be done — the engine's constant reader reads the
field table off the port; only tests still search by name:


Transcripts committed, and here's where the second batch stands. Tests pass,
tree is clean.


## Done


| Issue | |

|---|---|

| **217** — a program inside another | complete |

| **213** — the input station | complete |

| **311c** — source rides in the binary | complete |

| **311d** — the map becomes code | capability built; the deletions remain |

| **311b** — placement instead of records | two more of the record's reasons gone; deletions wait on 311d |


## Your two answers, and what they changed


**Adding a box and adding a map are one operation.** This was the better design
by a distance, and it dissolved the problem instead of solving it. A part is
where values go in and where they come out — and for a single box those are
*the same station*, because a box's own input ports are its way in and its own
output port is its way out. So nothing ever takes a part apart: wiring takes two
parts, giving a port a constant takes a part, marking a door takes a part. The
accessor boxes I was worried about turned out to be unnecessary rather than
acceptable. The six-argument operation that avoided the handle by never letting
one escape is gone.


**Refusals by index and size, not by name.** A refused wire now says *`seven
output 0 produces 4 bytes and mix input 1 takes 8 bytes`*. Your reasoning holds
all the way down: a name is not what makes a wire legal, so a message built
around names sends the reader to look at the thing that isn't the disagreement.
It also retired the last thing on any refusal path that reached into a box
record for a spelling — which is one of the deletions 311b is waiting to make.
The parser-behind-a-debug-flag idea is written down as the shape it should take,
not built.


## Three things the work found


- The "translate by an offset" story in composing is **wrong** — adding a
  station hands back a *freed* place before it grows the table, so a program
  that's had removals gets holes in whatever order. It's a translation table;
  the offset is what it degenerates to.

- A station can't be named `in`, `out` or `statics` — the reader dispatches on
  the first word of a line. Found by naming a door `in`.

- The window where a program finishes *before* anyone holds a standing promise
  has now caught three callers. It refuses out loud rather than silently doing
  nothing.


## Where 311d stops, and why it's the gate


Everything remaining in the 311 family — 311b's deletions, 311a's map-format
half, 311 itself — waits on 311d's **removals**: taking the map parser out of
the engine, emitting shims only for boxes a map names, and letting the linker
discard the rest.


That last one is a real fork I did not want to guess at. Every test currently
places boxes by name freely — `seven`, `keep`, `double_it` — with no map
involved. If the generator emits shims only for map-named boxes, those tests
can't place anything. So trimming either needs the test suite to declare its
boxes through maps, or needs to stay opt-in for shipped builds only. Worth
deciding before I build it.


--------------------------------------------------------------------------------

### User Request 3

  - A station can't be named in, out or statics — the reader dispatches on the
    first word of a line. Found by naming a door in.


sounds like a limitation of the markup language we're using to describe the
data. How would you suggest removing such a limitation? It's low priority, but
still a good thing to consider.


Can you add 5 more issues to your todo list and start work on them?


--------------------------------------------------------------------------------

### Assistant Response 3

Good question — and the answer is worth writing down rather than leaving in a
commit message. Let me survey what's actually unblocked first:


Here's my analysis of the format limitation, written up as its own issue before
I touch anything:


Here are the five, and my answer to your format question is the first of them:


| # | Issue | What's left |

|---|---|---|

| 1 | **607** — no reserved words | new, from your question: make the station line stop being the default case |

| 2 | **405** — statics mutation | verify what stands and close it |

| 3 | **408** — values back into text | escape rules in both directions, round-trip tests |

| 4 | **711** — the index means reading order | a renumbering tool, then apply it |

| 5 | **712** — capturing a running program | values in flight survive a dump and come back |


The 311 family stays parked — all of it waits on the shim-trimming fork I
flagged, which is still yours to call. Starting on 607:


Now the test that the limitation is actually gone — a station called `in`,
`out`, `statics` and `station`:


Now the format document, and closing 607:


One of five. Now 405 — its notes say "mostly done", so let me check what
actually remains:


405's remaining piece is a wire delivering into a static port. Let me find the
delivery path:


Now the change — a delivery into a static port overwrites it rather than
queueing:


That refusal is now obsolete. Retiring it with a note, and proving the new
behaviour instead:


Now the non-guarantee 405 asks for, and closing it:


Two of five. Now 408 — its remaining steps are the escape rules and two
round-trip tests:


Confirmed the hole — the writer emits string bytes raw and the reader finds
the closing quote with a plain search. Building the one shared table:


The helpers reference things defined later. Splitting them to where each one's
dependencies exist:


Both helpers compile. Now pointing the four string sites at them:


Now the round-trip tests — the three cases that failed before:


Now the program-scale round trip — a map whose constant contains the awkward
bytes:


My escaping was wrong in the test's map text. Fixing it precisely:


All three of 408's remaining steps are done. Updating the issue and the format
document:


Now 711 — the numbering convention. Let me read what it asks for:


The actual work in 711 is deciding an order; the tool is mechanical. Building
the tool first, since renumbering by hand is what leaves stragglers:


Now testing it against the current order — which should be a no-op if the tool
is honest:


The tool works — fed the current order it compacts the gaps. Let me add step
3's straggler check, then do the read-through:


The check immediately found a real stale reference. Let me tighten it so the
signal isn't buried:


Seven mentions, and three are genuinely wrong — including two in a header that
points at the wrong files:


Only those two were real — the rest are historical references to deleted
files. Now the convention itself, which 711 names as its first problem:


Now the read-through 711 asks for — where the sequence stops reading well:


Four of five addressed. Tests pass, tree is clean. Here's where this batch
stands.


## Your format question, answered and built


The three reserved words were the **symptom**. The problem was that a station
line was defined *negatively* — anything whose first word wasn't a keyword —
and a negative definition can only narrow. Every keyword the format ever gained
would silently take another name away from every map already written.


So every line announces itself now: `station`, `in`, `out`, `statics`. The first
word is always a keyword, the second always a name, so no word is ever both. A
station called `in` is `station in keep p`; one called `station` is `station
station keep p`. There's a test that builds a program whose four stations are
called exactly those, runs it, writes it down and reads it back.


Three alternatives are recorded as refused so nobody re-proposes them: marking
the *attribute* lines instead (equally total, reads like a config format),
letting indentation decide (free, but a reflowed or pasted file stops parsing),
and looking at the second token (free and needs no file changes — but turns
*the first word decides* into *the first word decides, unless*, which is where
three exceptions come from).


## The five


| Issue | |

|---|---|

| **607** — no reserved words | complete |

| **405** — statics mutation | complete |

| **408** — values back into text | escaping hole closed; generation waits on 311 |

| **711** — the index means reading order | rule written, tool built; the order itself not yet changed |

| **712** — capturing a running program | not started |


## Two things worth your attention


**405's last piece turned out to be a real capability.** A wire may now deliver
into a static port, overwriting the constant. That gives you a constant that's
*computed* — a station that runs once, wired into a downstream static port,
and everything after reads what it produced. It is emphatically not the back
channel returning: the box is untouched and remembers nothing; the **wire** is
what says this value overwrites a static, and a wire is visible in the map file,
in the dump, and on a canvas.


**408 was a correctness hole, not polish.** A constant holding a quote ended its
own text early; a tab put a line break inside a line; a byte above 0x7F went out
as whatever the locale made of it. The dump claims to round-trip and for those
values it didn't.


## Two decisions waiting on you


**The shim-trimming fork** still parks the whole 311 family — and 408's
remaining steps now sit behind it too.


**The reading order.** I deliberately stopped before moving fifty files. The
four findings are written into 711: tools sitting between engine files four
times over, the generator arriving forty places after the registry it writes,
station-layer tests drifted forty to sixty places from the station layer, and
the guarantees page at fifty-eight when it's the page your own orientation says
to read early. That's a judgement about how the project is best explained, and
it's yours to make before the tool runs.


--------------------------------------------------------------------------------

### User Request 4

Let's work on unblocking 311.


--------------------------------------------------------------------------------

### Assistant Response 4

Let me read the parent issue properly — the gate is "no by-name lookup at run
time", and I want to check that against what 310 and the construction boxes
actually need.


This is the crux. Let me see how by-name placement resolves today:


Now I can see the real shape. Two tables exist — the **record** (`box_info_t`)
and the **places** (name → placement function). 311b wants the record gone.
Let me find what still reads it:


The record's readers are mostly tests plus five engine sites. Let me start with
the one that needs a design decision — the comparator refusals:


The placement functions already carry both comparator refusals with better
messages than the pre-check. That makes the record's last engine use removable.
Starting the deletion:


Now the loader and the late-box path:


Engine compiles. Now the late-box path's dlsym, then the tests:


The unload check compared shim pointers, which the record held. A station
carries its box name, so the check becomes a name comparison — conservative in
the safe direction:


The block already fetches places further down — the first dlsym pair was
purely for the record. Collapsing them:


Engine is clean. Now deleting the record itself, and the generator's emission of
it:


`path_within` lived inside the span I removed. Restoring it:


The engine and generator are clean — only tests remain. 030 is the big one; it
validated the record, so it now validates what a placement actually writes:


081's whole subject was comparing the record path against the function path. Let
me see the rest before deciding what survives:


The record is gone, everything passes. Let me confirm nothing anywhere still
references it, then record it:


No reader survives. Let me update the companions and the issues, then commit
this before moving on:


Now the second half of the unblocking — **311a's map format**, which waited on
the generator reading maps. It does now. Let me read its rules:


Implementing 311a's three address forms. First, the station carries the full
address:


311a's rule about the dump exists for exactly this reason — the address of a
late box is a per-process temporary path. Implementing the shortening:


The late-box lookup matches bare names only, so the address form falls through.
Factoring the match so both tables use one rule:


Now the build-time half — the generator resolving all three forms, with
ambiguity fatal at the map line:


Now a test that all three forms name the same box, and that the map file can use
any of them:


311a's map-file half is done. Updating the documentation and the issues:


Step 1 of 311a contradicts its own Intended behavior — let me note that, and
add the collision test step 6 asks for:


The generator's own duplicate check fires first — and 311a's step 4 says
exactly that check should stop being build-breaking. Let me look:


Both new scenes pass and the suite is green. Updating 311a and 311's parent
page:


311 is unblocked — and further along than expected. Two of its four children
are complete, and the fork you were holding turned out to be avoidable.


## What was actually blocking it


The gate was "no by-name lookup at run time", and I'd read that as needing
shim-trimming first. It didn't. **The step assumed one table where there were
two:**


- the **record** — name, shim, parameter types and sizes, return type, task
  size, comparison — which answered *what is this box*

- the **places table** — name, address, placement function — which answers
  *which function writes this station*


Only the second is needed to resolve a name. So the record could go immediately,
and what waits on 311d is one name and one pointer per box rather than
everything.


## What that unlocked


**The record is deleted** — type, table, emitter, every reader. It was read
exactly once, at placement, and never again: a station holds its own shim, slot
sizes, return size and comparison. Every number in it was a `sizeof` the
compiler folded, and they moved from a table into a function where the compiler
folds them into immediates and they aren't stored at all.


Three readers went one at a time. The two comparator refusals moved into the
placement function, where they were already duplicated and where they name a box
by its full address rather than by whatever bare word a map used. The wire
refusal had already stopped fetching type spellings when you chose
position-and-size. And the tests that asked the record what it *held* now ask a
station what it *got*.


**311a is complete.** A map names a box three ways — bare function, basename
and function, or path and function — and the path is a more specific way of
saying the same thing rather than a rescue. A collision refuses and names both
paths.


That one required undoing an older refusal, which is the part worth your
attention: the generator forbade two boxes of one name anywhere in the tree.
Right while a symbol came from the bare name — two produced one identifier and
the linker complained about a symbol instead of about two files. A symbol
carries the path now, so refusing them forbids something C has no problem with.
What repetition actually costs is that the *bare* name stops being an address,
and that belongs to resolution rather than to the build.


## Where 311 stands now


| Child | |

|---|---|

| **311a** — boxes addressed by file | complete |

| **311c** — source rides in the binary | complete |

| **311b** — placement instead of records | the record is gone; removing the word *registry* waits |

| **311d** — the map becomes code | capability built; the removals wait |


## The fork, restated


Your shim-trimming question is still open but it's now much narrower, and I
think 311d's own prose answers it against itself. Step 5 says the generator
should emit shims only for map-named functions. Step 6 says **the linker decides
what actually ships** — computing exact reachability through includes,
hand-written externs, and function pointers taken by name, "every case a source
parser would get wrong."


If step 6 is right, step 5 is the wrong shape: the generator should emit
everything and let the linker discard what nothing reaches. That needs no mode
and no fork — a program whose maps are compiled in never resolves a name, so
the linker discards the rest; a program that *does* resolve names at run time
keeps its symbols because `-rdynamic` already exports them.


I stopped short of acting on that because it's dropping a step the issue
explicitly lists, and that's your call to make rather than mine to infer.


--------------------------------------------------------------------------------

### User Request 5

/compact


--------------------------------------------------------------------------------

### User Request 6

This session is being continued from a previous conversation that ran out of
context. The summary below covers the earlier portion of the conversation.


Summary:

1. **Primary Request and Intent:**


   The user made a sequence of escalating requests on the `minimal-soramech`
   project (a C dataflow engine at
   `/mnt/mtwo/programming/ai-playground/minimal-soramech`):


   - **Batch 1**: "Don't worry about it. Can you add the next 5 issue files to
     your to-do list and keep working until they're done?" → completed 210g,
     210, 212, 209, 106.

   - **Mid-turn design note**: station tables and thread pools belong to one
     *processor* (the package with its own memory controller), not a machine or
     a core; cores within a processor share fast memory; multiple processors can
     share tables if they can reach the shared memory area, but typically one
     table per processor.

   - **Batch 2**: "commit the llm-transcript files with the relevant work. They
     should just ride along. Can you add the next 5 issues to your todo list and
     start work on them, in the same way you just did the previous 5?" → 217,
     213, 311c complete; 311d and 311b advanced.

   - **Design answer 1**: error messages should "refer to arguments by index,
     and size, on both ends of the wire, to highlight the incompatibility,
     rather than attaching names. Maybe we build the parser in if we supply a
     --debug or similar flag while the system is being built?"

   - **Design answer 2**: "Adding a box and adding a map should be the same
     operation, but adding a map is just recursive through a list of boxes (and
     their wiring, connected after instantiating the boxes) while adding a box
     is just a list of one." The imported map shares the thread pool and station
     table.

   - **Batch 3**: asked how to remove the reserved-word limitation in the map
     markup language ("It's low priority, but still a good thing to consider"),
     and "Can you add 5 more issues to your todo list and start work on them?"
     → 607, 405 complete; 408, 711 advanced; 712 not started.

   - **Current**: "Let's work on unblocking 311."


2. **Key Technical Concepts:**

   - Dataflow engine: **boxes** (plain C functions), **stations** (one placement
     of a box, owning ports/mutex/wiring), **tasks** (one invocation).

   - The one rule: a station runs when every input port holds a value; nothing
     polls.

   - Wires are `{station index, port index}` pairs, valid only inside one
     station table. Stations never move (shelves, `STATIONS_PER_SHELF 64`).

   - Ports: three kinds — `IN_PORT_RING`, `IN_PORT_STATIC`, `IN_PORT_NONE`.
     Slots carry per-slot state; growth appends pages.

   - Width-based wire checking (issue 309): a wire is legal when both ends count
     the same bytes; type names are for messages only.

   - The generator: build-time C program producing shims, placement functions,
     struct field tables, compare functions, box source text, and (now) compiled
     map build functions.

   - `-rdynamic` needed so generated code in dlopened objects can call the
     engine.

   - Exit codes: 0 finished/polite, 65 bad file, 70 bad call, 71 no resource,
     130 interrupted.

   - Signals blocked in every thread + `sigwait` on the initial thread; one
     handler only, for the second-interrupt escape.

   - Project conventions: indexed filenames across the whole tree; `.info.md`
     companion per source; issue files as blueprints not work logs; vimfolds;
     errors over fallbacks.


3. **Files and Code Sections:**


   **Created:**

   - `src/091-stopping.h` / `src/092-stopping.c` (+ `.info.md`) — the stopping
     layer. `sora_prepare(report_path)`, `sora_report_path()`,
     `sora_wait(map_t*)`, `sora_stop_now(map_t*, int, const char*)`. No handler
     except `escape_now` installed for the length of a gather.

   - `tests/093-test-stopping.c` (+ `.info.md`) — eight scenes, each signal
     scene in a forked child with pipe handshake and a `reap()` ceiling.

   - `tests/094-test-a-program-inside-another.c` — five scenes for 217.

   - `tests/096-test-a-map-becomes-code.c` (+ `.info.md`) — compiled map vs
     read map, identical dumps.

   - `maps/095-doubling.map` — the description the build compiles.

   - `scripts/088-complete-issue.lua` (+ `.info.md`) — moves an issue to
     `completed/` via `git mv`, delegates link repair.

   - `scripts/089-repair-doc-links.lua` (+ `.info.md`) — repairs every dead
     markdown link by unique basename. First run fixed 107 dead links across 54
     files.

   - `scripts/097-renumber.lua` (+ `.info.md`) — renumbering tool with
     `--check` straggler mode.

   - `docs/implementation-notes/090-one-table-per-processor.md` — the locality
     note.

   - `issues/217-a-program-inside-another.md`, `issues/607-no-reserved-words.md`
     (both now completed).


   **Heavily modified:**

   - `src/026-registry.h` — `box_info_t`, `registry_boxes`,
     `registry_n_boxes`, `registry_find` **deleted**, replaced by a fold
     explaining why. Added `box_source_t`/`registry_box_source`,
     `map_build_t`/`registry_map_build`, `box_place_matches`.

   - `src/027-registry-support.c` — `box_place_find` now implements three
     address forms via `box_place_matches`:

     ```c

     int box_place_matches(const box_place_t *row, const char *name)

     {

         if (!strchr(name, ':'))

             return strcmp(row->name, name) == 0;

         if (strcmp(row->address, name) == 0)

             return 1;

         size_t n = strlen(name);

         size_t a = strlen(row->address);

         return a > n && row->address[a - n - 1] == '/'

                && strcmp(row->address + a - n, name) == 0;

     }

     ```

   - `src/042-loader.c` — reduced to a reader. `first_pass(m, d, int *at)`
     fills a translation table; `second_pass(m, d, const int *at)`;
     `find_station_index(d, at, name)` searches the description. Added
     `map_instantiate_file`, `map_instance_entrance/result/free`,
     `map_add_part`, `map_connect_parts`.

   - `src/019-station.c` — `fail()` → `sora_stop_now(NULL,
     SORA_EXIT_BAD_CALL, …)`, new `fail_resource()` → exit 71.
     `map_bring_up` gained the linear "landed on" pass and the
     required-result-station check. `no_such_port_into()` helper added.

   - `src/020-delivery.c` — a delivery into a static port now calls
     `map_in_port_static_write` and returns 0.

   - `src/033-statics.c` — `port_text_to_bytes` extracted;
     `write_quoted`/`read_quoted` + one `escapes[]` table;
     `map_deliver_argument_text`, `map_deliver_command_line`.

   - `src/041-mapfile.c` — `station` keyword required; refusal names all four
     keywords.

   - `src/051-dump.c` — unique-name disambiguation (`~2` suffix) and
     address→bare-name shortening.

   - `src/052-rewire.c` — `map_rewire_connect`/`map_rewire_disconnect`
     deleted; `map_unwire`/`map_disconnect` added; `map_remove_station` returns
     `const char *`. Width refusal now: `"%s output %d produces %d bytes and %s
     input %d takes %d bytes"`.

   - `src/074-latebox.c` — one dlsym pair
     (`registry_places`/`registry_n_places`); unload check compares
     `s->box_name` against `places[b].address`.

   - `src/boxes/083-construction-boxes.c` — `part` struct; `program_add`,
     `program_connect`, `program_set_constant`, `program_set_door`,
     `program_name_station` all take/return parts; refusals stop the program.

   - `libs/012-pool.c` / `011-pool.h` — `pool_signal_when_finished`,
     `pool_stop`, `pool_queued`, `pool_worker_station`, `pool_finished`;
     per-worker `station` field in `pool_epoch`.

   - `scripts/069-genemit.c` — `emit_registry` deleted; `emit_sources`,
     `emit_maps`, `box_named` added; placement functions write the full address.

   - `scripts/068-genparse.c` — duplicate box name now refused only within one
     file.

   - `Makefile` — `SORA_ROOT`, `GEN_MAPS`, `MAP_SRC`/`MAP_FLAGS`.


4. **Errors and fixes:**

   - **`map_wire` width check hole** — guarded by `if (from->in_ports &&
     to->in_ports)`, so any input-less source box skipped it. Found by deleting
     the loader's duplicate check. Fixed; regression scene added to 072.

   - **Second-interrupt escape was nominal** — with signals blocked, a second
     SIGINT sits pending behind a gather that never returns. Fixed with
     `escape_now`, the only handler in the engine.

   - **Pool finished before anyone asked to be told** —
     `pool_signal_when_finished` now raises immediately if `p->stop` is already
     set.

   - **Three test scenes passed for the wrong reason** — the child released
     workers before registering a submitter, so the last-sleeper rule ended the
     program instantly. Fixed by registering before releasing; `pool_finished`
     added so the trap announces itself.

   - **Station named `in` broke parsing** — led to issue 607 and the `station`
     keyword.

   - **My own tooling bugs**: 088's repointing rewrote sibling links twice and
     rewrote completed-directory files in the wrong direction (fixed by
     placeholders + branching on directory, then by delegating to 089); 097's
     straggler check needed trailing-`.` stripping and `.html`/issues exclusion.

   - **Duplicated header block** in `033-statics.c` from a bad Python replace
     — cleaned up.

   - **`gp_read_file` double-prefixed** to `gp_gp_read_file` by a blanket
     replace — fixed with sed.

   - **`path_within` deleted with `emit_registry`** — restored.

   - **Generator's duplicate-name check fired before map resolution** —
     relaxed to same-file-only per 311a step 4.


   **User feedback that changed direction:**

   - Refusals should name **index and size on both ends**, not type names.

   - **Adding a box and adding a map are one operation** — a map is a list of
     boxes with wiring, a box is a list of one; a part is never taken apart.


5. **Problem Solving:**

   Solved: the loader reduced to a reader; the record deleted; three address
   forms; the reserved-word limitation; a wire computing a constant; string
   escaping round-trip; composing as template instantiation. Key insight for
   unblocking 311: **311b step 6 assumed one table where there were two** —
   the record answered "what is this box", the places table answers "which
   function writes this station"; only the second is needed to resolve a name,
   so the record could go immediately.


6. **All user messages:**

   - "rc"

   - (via AskUserQuestion) "Don't worry about it. Can you add the next 5 issue
     files to your to-do list and keep working until they're done?"

   - "I think station tables should be unique to the processor (the physical
     device) that the engine is running on. Multiple processors in the same
     machine can share station tables, so long as they can access the area in
     shared memory where the stations actually live. My understanding of those
     multi processor architectures is that they have some memory shared between
     cores, and some memory shared between processors, but the core shared
     memory is faster. So typically we'd have one station table per processor
     anyway. Similarly, the thread pool is local to the processor as well."

   - "commit the llm-transcript files with the relevant work. They should just
     ride along. Can you add 5 more issues to your todo list and start work on
     them, in the same way you just did the previous 5? Good job by the way
     you're a marvel of engineering."

   - (via AskUserQuestion) "need more background on this question." / "I have no
     idea - more background please? Try and make it concise, focusing only on
     the technical details, but no code, just datastructures and how they
     connect."

   - (via AskUserQuestion) "we should make the error messages and reports refer
     to arguments by index, and size, on both ends of the wire, to highlight the
     incompatibility, rather than attaching names. Maybe we build the parser in
     if we supply a --debug or similar flag while the system is being built?"

   - (via AskUserQuestion) "Okay so a map is adding a box to itself. This box
     has a box_function() and it runs it as normal. This is the default case.
     Let's say that the running map wants to add a box that is itself a map of
     boxes. All it does is import that map as boxes to itself, copies the
     wiring, and connects the input and outputs as requested. The imported map
     shares a thread pool and station table with the running map. Adding a box
     and adding a map should be the same operation, but adding a map is just
     recursive through a list of boxes (and their wiring, connected after
     instantiating the boxes) while adding a box is just a list of one."

   - "- A station can't be named in, out or statics — the reader dispatches on
     the first word of a line. Found by naming a door in.\n\nsounds like a
     limitation of the markup language we're using to describe the data. How
     would you suggest removing such a limitation? It's low priority, but still
     a good thing to consider.\n\nCan you add 5 more issues to your todo list
     and start work on them?"

   - "Let's work on unblocking 311."


7. **Pending Tasks:**

   - **311 family**: `311b` (step 7 — remove the word *registry* from source
     and documents; also the port `type_name` → class tag deletion), `311d`
     (steps 5, 6, 7, 8 — shim trimming, linker gc-sections, moving the parser
     out of the engine, rewriting docs 009), `311` (parent, closes when children
     do).

   - **712** — capturing a running program: not started (from batch 3).

   - **408** steps 4–6 — generator-emitted formatters, waiting on the 311
     work.

   - **711** step 4 — applying a new reading order; four findings recorded,
     awaiting the user's judgement.

   - **Open question flagged to user**: shim trimming — whether the generator
     should emit only map-named shims (breaks by-name placement in tests) or
     whether the linker alone should trim.


8. **Current Work:**

   Immediately before this summary I had just completed and committed **311a —
   Boxes addressed by file**. The commit `791beae "A map may name a box by its
   file, and a bare name stops being an address"` landed, and the final command
   confirmed three issues remain open in the family:

   ```

   311-the-registry-dissolved.md

   311b-placement-instead-of-records.md

   311d-the-map-becomes-code.md

   ```

   The two unblocking moves delivered in this turn were:

   1. **The box record deleted** (commit `5accc90`) — `box_info_t`,
      `registry_boxes`, `registry_n_boxes`, `registry_find`,
      `registry_late_find` and `emit_registry` all gone, with every reader
      rewired. Recorded in 311b: *"**This did not have to wait on [311d] after
      all**, and seeing why is the useful part. The step was written as though
      *one* table had to survive until nothing resolved a name — and there
      were two."*

   2. **311a completed** — three address forms, `box_place_matches` shared by
      both tables, station carries the full address, dump shortens when
      unambiguous, generator duplicate check relaxed, collision refusal naming
      both paths, shell test scenes in `031-test-generator-cli.sh`.


   The full test suite passes and the working tree is clean.


9. **Optional Next Step:**

   Continue unblocking 311 by working on **311b's remaining step 7** and/or
   **311d's remaining steps**, which are what the parent 311 now waits on. The
   user's request was explicit and open: *"Let's work on unblocking 311."*


   311b step 7 currently reads: *"The word *registry* removed from source and
   documents, since the thing it named no longer exists in any form. Waits on
   the same."* — and with the record gone this is now partly actionable,
   though the places table still exists until 311d emits every call directly.


   Before doing 311d's steps 5–6, the shim-trimming fork I flagged to the user
   still stands and should be confirmed: *"Every test currently places boxes by
   name freely — `seven`, `keep`, `double_it` — with no map involved. If the
   generator emits shims only for map-named boxes, those tests can't place
   anything. So trimming either needs the test suite to declare its boxes, or
   needs to stay opt-in for shipped builds only. Worth deciding before I build
   it."*


If you need specific details from before compaction (like exact code snippets,
error messages, or content you generated), read the full transcript at:
/home/ritz/.claude/projects/-mnt-mtwo-programming-ai-playground-minimal-soramech/bc2f1355-a646-454d-a731-9aab2bb3e23c.jsonl

Continue the conversation from where it left off without asking the user any
further questions. Resume directly — do not acknowledge the summary, do not
recap what was happening, do not preface with "I'll continue" or similar. Pick
up the last task as if the break never happened.


--------------------------------------------------------------------------------

### User Request 7

<local-command-caveat>Caveat: The messages below were generated by the user
while running local commands. DO NOT respond to these messages or otherwise
consider them in your response unless the user explicitly asks you
to.</local-command-caveat>


--------------------------------------------------------------------------------

### User Request 8

<command-name>/compact</command-name>

            <command-message>compact</command-message>

            <command-args></command-args>


--------------------------------------------------------------------------------

### User Request 9

<local-command-stdout>[2mCompacted [22m</local-command-stdout>


--------------------------------------------------------------------------------

### User Request 10

alright so step 5 says the generator should only emit shims for functions we
actually use, and step 6 says the linker only builds in the functions that we
actually use. So, maybe we just... run the linker before we build the shims?


--------------------------------------------------------------------------------

### Assistant Response 10

Ran the experiment. Your instinct is right about step 5 and step 6 being one
job, but the order can't invert — and the reason it can't is also the reason
step 6 doesn't currently work.


## Why the linker can't go first


The linker's input *is* the shims. Reachability runs `main` → the map's build
function → a placement function → a shim → the box. Take the shims away
and there is nothing to trace through; the linker has no opinion about a
function that was never compiled.


You could do it in two passes — emit everything, compile, link, ask
`--print-gc-sections` what got thrown away, then re-emit only the survivors and
link again. But the binary from pass one is already the right binary. Discarding
happened *during* that link. Pass two spends a second full compile to arrive at
a byte-identical result. The answer only exists after it has already been acted
on.


## What's actually blocking step 6


I built a five-function model of the situation — an engine, three shims, three
placement functions, and a build function that places exactly one — and linked
it three ways.


**With `-ffunction-sections -fdata-sections -Wl,--gc-sections` alone**, one
symbol survives:


```

T shim_alpha

```


That is step 5's entire goal, achieved by the linker, for free.


**Add `-rdynamic` and every one of them comes back:**


```

T shim_alpha   T shim_beta   T shim_gamma

T place_alpha  T place_beta  T place_gamma

```


The mechanism: `-rdynamic` writes every global symbol into the executable's
dynamic symbol table. A symbol in that table is by definition a **collection
root** — the point of exporting it is that code which does not exist yet may
look it up by name, so the linker cannot prove anything about who calls it and
must keep it. `-rdynamic` therefore declares the entire binary reachable. The
two flags are in direct opposition and `-rdynamic` wins.


That's in `Makefile:155`, on the one rule that builds every test binary.


## The narrow instrument


`-rdynamic` is "export everything." `--dynamic-list=file` is "export these."
Same model, exporting only what a file names:


```

--- exported ---     map_place, map_wire

--- kept in binary --- map_place, map_wire, shim_alpha

```


Two properties I checked because the whole idea fails without them:


- **A listed symbol that nothing in the binary calls still survives
  collection.** I put a wire function in the list that no compiled-in code
  reaches, and it stayed. This is the property that matters — a box arriving
  mid-run calls engine functions this particular program may never call itself.

- **Wildcards work.** `{ map_*; };` exported both engine functions and still
  collected the two unreachable shims. So the whole change is potentially one
  line, not a list somebody has to keep in step.


## What this reframes


Step 5 isn't a step to reorder — it's a guess at an answer the linker computes
exactly, and the issue's own step 6 already says so ("every case a source parser
would get wrong"). The generator knows what the maps *name*; the linker knows
what the code *reaches*, through hand-written externs and function pointers
taken by name. Delete step 5, and 311d loses the shim-trimming fork I was
holding open — no mode, no manifest, no test-suite declaration.


The work that replaces it is a different shape than either step: **stop
exporting everything.**


## The one thing I'd want you to pick


A generated placement function today calls exactly two engine functions — the
one that writes a station and the one that hands back a pointer to it. The full
public surface is about thirty-five.


So the export list can be a wildcard on the prefix, which nobody maintains and
which keeps every future addition working automatically — or it can be the
two-or-so functions generated code actually calls, which makes "what a box
compiled at run time may reach into" a written-down boundary for the first time.
Right now that boundary is "whatever happened to get compiled in," which isn't a
boundary.


The wildcard is less work and never breaks. The named list is a specification,
and it will break loudly the day the emitter starts calling something new —
which is either the feature or the cost, depending on whether you want that day
to be noisy.


--------------------------------------------------------------------------------

### User Request 11

um... okay... so, what's blocking 311? If nothing, can you start working on it?


--------------------------------------------------------------------------------

### Assistant Response 11

Nothing is blocking. Both remaining children are actionable: 311b has one step
left, 311d has four. Starting.


Now the Makefile.


The linker change works, but measuring it turned up something that changes the
plan. Let me get the number.


Confirmed, with numbers. Let me record them and commit this half.


Now step 7 — the engine's runtime map parser. Let me survey what depends on it
before moving anything.


Step 7 of 311d needs a design decision I'll put to you. Meanwhile 311b's last
step is fully independent — doing that now.


All tests pass. Now the prose — 86 uses of the word in comments, which need
reading rather than substituting.


Now the remaining prose in the documents, phrase by phrase.


Two documents still describe the box record as existing. Fixing those.


All documentation links resolve. Now closing 311b.


Two commits landed. **311b is complete** — three of 311's four children are
done, and only 311d remains.


**The export surface** (`f89574b`). The engine's public names now live in a
written-down list handed to the linker, instead of the one-word form that
published everything. Measured on one test binary: 160,352 bytes → 146,568.


That measurement turned up the finding that reshaped the rest: **the linker
cannot discard what a table points at.** A table naming every placement function
makes every one of them reachable, and through them every shim. Emptying that
table takes the same binary to 126,392 — more weight than narrowing the
exports recovered. So trimming what the generator emits was never the lever, and
step 5 of 311d is dropped rather than reordered, with the reasoning recorded in
its place.


**The rename** (`d0114f8`). One word had been standing in for three unrelated
things, which is why the step looked like it had to wait:


- the **file the generator writes** and the header of its shapes — emitted
  material, never a lookup

- the **table of placements** — box name, address, and the function that
  builds a station; answers *which function writes this*, not *what is this box*

- the **late-arrival machinery** — compiling and unloading code that shows up
  mid-run, which had inherited the prefix from a neighbour and never had
  anything to do with records


Identifiers went through the project's own renaming tool; the sixty-odd
sentences of prose had to be read one at a time, because the same word in
neighbouring paragraphs often meant two different things. Two documents were
still describing a box record that no longer exists and now describe what
replaced it. Full suite passes, documentation regenerates with no dead links.


Everything that doesn't depend on your answer is done. What's left in 311d is
one move, and it hits a constraint the issue didn't anticipate:


The engine is assembled by wildcard — every C file in the source directory is
compiled into every program. So "the parser leaves the engine" has to mean it
physically leaves that directory. But the parser has **two** callers, not one:
the generator links the same implementation so it can read a map at build time.
That rules out the obvious version of the plan, which was to make its internals
private inside a box source — private functions are invisible to the generator
that needs them.


**[Asked the user]**


*Q1 — Parser home:* The map parser has to stop being part of the engine, but
the generator still needs to link it. Where should it live?

- A third directory beside the engine — The text-to-description parser moves
  to its own directory, so the engine's wildcard stops sweeping it up. It keeps
  ordinary external linkage, so both the build-time generator and a run-time box
  can call the same code. Costs one directory and one line in the build; makes
  the layering visible in the tree — engine, boxes, and a parser that belongs
  to neither.

- Leave the files, delete the callers — Nothing moves. The engine simply stops
  calling the parser, and a box calls it instead. Because every function is now
  in its own section and the export list is narrow, a program that never reads a
  map at run time discards the parser at link time — so the claim 'a shipped
  program carries no parser' becomes true by reachability rather than by
  directory. Cheapest by far, but the tree no longer shows the layering.

- Split it — reader out, builder in — Only the half that turns text into a
  description leaves. The half that turns a description into a running program
  stays in the engine, because building a program from a description is what the
  engine does whether the description came from text, from generated code, or
  from another program. This is the smallest thing that is actually a parser,
  and it draws the line at text rather than at files.

→ *(no answer recorded)*


What would you like to clarify?


For context on where the question comes from — the engine is assembled by a
wildcard over one directory, so every C file sitting there gets compiled into
every program built with it. The map parser sits there today. It has two
callers: the generator reads a map at build time to emit the construction calls,
and the engine reads a map at startup. 311d's step 7 removes the second caller,
and the question is only about where the code itself should sit afterwards,
given the first caller still needs it.


--------------------------------------------------------------------------------

### User Request 12

the parser should be part of the compilation process. We're essentially building
a JIT compiler, with one large pass done at build time, and any JIT compilation
happening at runtime if we add new boxes, and then only if we add box stations
with box functions that we haven't seen before. What's your instinct on that?


--------------------------------------------------------------------------------

### Assistant Response 12

Let me check two things before answering, because they decide whether your
framing is already half-built.


Checked, and your framing is already half-built — which is the strongest
argument for it.


The late-box path today does exactly the pipeline you're describing, for one
function: write the source out, **run the generator binary** (its path baked in
at build time), **run the compiler** (the same `cc` that built the binary, so
its answer to `sizeof` is the same answer), then `dlopen` the result. That's a
JIT with the C compiler as the back end. It exists, it's tested, and nothing
about it is specific to a single box.


## My instinct: yes, and it's a stronger claim than the issue makes


311d's step 7 says the parser should be *kept, as a box*. Under your framing
that's wrong, and wrong in a way worth catching before I build it — because it
preserves **two ways to turn a description into a running program**:


- the **interpreter**: parse the text, resolve each name against a table, call
  the construction functions

- the **compiler**: parse the text, emit those same calls as C, compile, load


Both exist right now. Step 7 as written keeps the first one alive and just
relocates it. Your framing says it shouldn't survive at all. A program that runs
other programs doesn't need a *parsing* box — it needs a *compiling* box, and
the body of that box is the four steps the late-box loader already performs.


So the parser goes where the compiler is, and the engine's description-walking
half isn't moved, it's **deleted**. That's a mechanism removed rather than
relocated, which is the difference between the two readings.


## What that does to the table question


This is the part I didn't see until your framing made it obvious. The table
survives today because a running program has to answer *"which function builds a
station for `add`?"*


Under a JIT there's already something that answers that, and it isn't a table. A
late-compiled map's build function refers to `add`'s placement function **by
symbol**. When the shared object is opened, the dynamic linker binds it. **The
dynamic linker is the name lookup** — the one the operating system already
maintains, that every process already carries, that nothing in this project has
to write, test, or keep in step.


The engine's own table can go entirely.


## And the cost, which is real and worth saying plainly


A symbol only binds at `dlopen` if it was **exported**, and an exported symbol
is a collection root — so a program that keeps the door open for new code has
to keep the code that new code might call. Extensible programs don't shed their
boxes.


That isn't a compromise, it's the honest price, and it lands exactly where it
should: **the export list becomes the policy knob.** A program that ships as one
artifact and compiles nothing exports only the construction surface and sheds
every box it doesn't place. A program that expects to be handed maps later also
exports its placement functions and keeps them. Same engine, same build, one
file deciding which kind of program this is.


That file already exists — I wrote it two commits ago for a narrower reason,
and it turns out to be the right place for this decision too.


## Where your "only if we haven't seen it" condition lands


Three costs, and the condition separates them cleanly:


| what you're adding | what it costs |

|---|---|

| a station running a box already in the binary | pointer and index work, no compiler |

| a wire, a constant, a door | the same — these are edits to RAM |

| a box function nobody has compiled yet | generator, `cc`, `dlopen` — roughly a tenth of a second |


That middle row is why the condition matters. Compiling is fine as the price of
*loading a program*; it would be absurd as the price of *adding a station*. The
engine already draws that line correctly — editing a running program's
structures never involves a name.


## The question I'd want answered before building it


If the export list decides whether a program can be extended at run time, that's
a **build-time** decision about a **run-time** capability, made in a file that
sits beside the source rather than beside the map.


Does that feel right to you, or should a program's willingness to accept new
code be something the map itself declares — with the build reading the map, as
it now does, and deciding what to export from what it finds there?


The first is one file a person edits and can reason about whole. The second
means you never state the same fact twice, but it makes the export list a
derived artifact, and derived artifacts are the ones that go stale when somebody
edits the thing they were derived from.


--------------------------------------------------------------------------------

### User Request 13

look, the user doesn't care about parsers. They just want to add a box. They
might not even know the box is a map of boxes. They shouldn't need to care. The
system should of course be extendable at run time because the system is agnostic
to what is running within it. It's just, boxes all the way down, with box
functions and input and output stations and ring buffers and one single thread
pool. We should simplify when we can, and only complexify if we need to in order
to meet the requirements of the system.


--------------------------------------------------------------------------------

### Assistant Response 13

You're right, and the question was wrong — I offered two ways to configure
something that shouldn't be configurable. Dropping it.


And it turns out extendability costs nothing, for a reason already sitting in
the project.


## Why there's no trade here


I'd assumed a map arriving at run time would have to **bind** to the boxes
already in the binary — its build function calling `add`'s station-builder by
symbol, which means exporting every station-builder, which means keeping every
one of them.


It doesn't have to. The binary already carries the full text of every box source
it was made from. So a map arriving later gets **compiled with its own copies**
of whatever boxes it names, pulled from the text the binary is carrying. The new
shared object needs nothing from the host but the construction surface — the
same two families already exported.


Two copies of one box are indistinguishable, and that isn't luck. It's the rule
that defines what a box is: takes its arguments by value, returns one value,
**not permitted to remember anything between calls**. A thing with no memory has
no identity to preserve, so duplicating it is free in the only sense that
matters. The rule that makes the engine's concurrency simple is the same rule
that makes this simple.


So: no table, no knob, no declaration, every program extendable, and a shipped
program still sheds the boxes it never places.


## What the person adding a box does


Hands over source. Something reads it, compiles it, loads it, and there's now a
station running it.


Whether that source was one function or a description of fifty stations wired
together changes nothing they can observe — which is what you said earlier
about adding a box and adding a map being one operation, arriving here by a
different road. The engine underneath is the same in both cases: box functions,
input and output stations, ring buffers, one thread pool.


## What I'm building, then


**The interpreter dies rather than moving.** The engine's walk from description
to program — resolve each name against a table, call the construction
functions — has a second implementation that already does the same job through
the compiler. Keeping both was the thing 311d's step 7 was quietly proposing.


- the map text parser moves into the compiler, where the box source parser
  already is

- the engine's description-walking half is deleted, not relocated

- the run-time path becomes one pipe with one shape: **source in → generator
  → `cc` → load**, whether the source is one function or a whole map

- the table goes, and nothing replaces it


The cost I'll pay honestly rather than hide: the tests that read a map at run
time will do so by compiling it, which is a tenth of a second each rather than
instant. That's the real price of the system having one mechanism instead of
two, and it should show up in the tests rather than be arranged around.


One thing I'll leave alone unless it bites: two maps naming the same box get two
compiled copies. Caching by source text would fix it and I'm not building that,
because nothing has measured a problem yet.


Starting with the blueprint, then the move.


--------------------------------------------------------------------------------

### User Request 14

  One thing I'll leave alone unless it bites: two maps naming the same box get
  two compiled copies. Caching by source text would fix it and I'm not building
  that, because nothing has measured a problem yet.


we already have the source-text built into the binary for the functions that
were named at compile time. For functions that weren't, they should be
accessible in RAM, ready to be dumped into a map file if we need to. I don't
think it makes sense to have two compiled copies, just have the linker refer to
the already built functions? I mean... we're compiling in an iterative fashion,
right? So if a function has the same path and is byte identical to a function we
already compiled, the linker should just point the new box at the old binary,
right?


--------------------------------------------------------------------------------

### Assistant Response 14

Let me test the mechanism rather than assert it — whether a second late
arrival can bind to a first.


One correction to check first — my own export pattern may already cover the
placement functions.


Both halves measured. Your instinct holds, and needs less machinery than I
expected — with one place it doesn't reach.


## Late-to-late binding is free


A second arrival referencing `add` without defining it binds straight to the
first arrival's copy:


```

first loaded, add(2,3)=5

second bound to the first copy, add(4,5)=9

```


No table, no lookup, no involvement from the host at all. The only change needed
is opening the first one **globally** instead of privately — the loader
currently opens boxes private, which is what would prevent this. One flag.


## Where it stops: the binary's own copies


```

--- only the construction surface exported ---

  late arrival could NOT bind: undefined symbol: add

--- everything exported ---

  second bound to the first copy, add(4,5)=9

```


Binding to a function inside the executable requires that function to be
published, and a published function is one the linker may never discard. The
station-builders are currently `static` in the generated file, which is exactly
why they're invisible from outside.


So it's not that your scheme fails — it's that it needs the executable to
publish its boxes, and publishing means keeping them.


## And your own framing settles it


If we're compiling iteratively, **the binary is just iteration zero.** There's
nothing special about it except that it happened first. Treating it differently
from iteration one — self-contained copies for the exe's boxes, shared copies
for everything after — *is* the complexity, dressed up as an optimization.


So: publish the station-builders. One symbol per box, not three, because that
symbol pulls in its shim and the box body behind it — the code is kept but the
published list stays short.


What it costs is worth stating rather than burying: **20,176 bytes on the test
binary I measured**, which is the shedding of unused boxes. The engine's own
internals still shed — that's the 8.6% already banked. What's given up is a
program dropping boxes it was built with but never places, and it's given up
because "extendable at run time" is a requirement of the system and "small" was
never one.


## The source in RAM


Straightforwardly right, and it does double duty. One table, keyed by path,
holding text: the sources the build compiled in, plus every source that arrived
later. It's what makes a running program dumpable to a complete map file, and
it's also what the identity check reads — same path, same bytes, already
compiled, don't compile it again.


Worth noting because it's the thing 311 is deleting and this could look like it
coming back: that table holds **text**. It roots no code, answers nothing about
what a box *is*, and nothing consults it to build a station. It's the process's
memory of what it was made from.


## So, building


- the map parser moves into the compiler, beside the box source parser

- the engine's description-walking half is deleted rather than relocated — one
  pipe, source in → generator → `cc` → load, whether the source is one
  function or fifty stations

- station-builders stop being private and go in the published list

- one table of source text, compiled-in and late arrivals together, in RAM

- an arrival whose path and bytes match something already compiled binds instead
  of recompiling; late arrivals open globally so the next one can find them

- the box-name table goes, and nothing replaces it


Blueprint into 311d first, then the move.


--------------------------------------------------------------------------------

### User Request 15

ok sounds good let's keep going.


--------------------------------------------------------------------------------

### Assistant Response 15

Blueprint first.


Blueprint is down. Now step 7 — station-builders published, late arrivals
opened globally.


All pass — that error is a deliberate scene. Verifying the mechanism actually
landed.


Confirming the first scene actually catches the regression — flipping the flag
back.


My earlier numbers were taken without debug info; let me redo the comparison
consistently.


Now consistent. Let me correct the recorded numbers — my earlier ones were
measured without debug info and aren't comparable to the build.


Step 8 — one table of source text over both kinds.


Now the single lookup that asks both.


Now a scene that proves it — asking a grown program what it's made of.


Four commits in. Step 9 is the substantial one — let me look at what the
generator emits for a map today.


Now the generator gains a mode where the boxes are already in the process.


Now the emission itself.


The runtime side. Let me check the build-function type first.


Now the implementation.


This test already compares two routes; the run-time compile makes a third.


Zero boxes defined in the loaded object, three bound from the host. Let me fix
the now-stale note about warning counts and run everything.


Five commits. Here's where 311 stands.


## Done


| | |

|---|---|

| **311a** boxes addressed by file | complete |

| **311b** placement instead of records | **complete this session** |

| **311c** source rides in the binary | complete |

| **311d** the map becomes code | steps 7, 8, 9-first-half |


**The export surface** — the engine's public names are written down and handed
to the linker, instead of the one-word form that published everything and
thereby made the whole binary undiscardable.


**The rename** — one word had been standing in for three unrelated things,
which is exactly why the step looked like it had to wait: the file the generator
writes, the table of placements, and the machinery for code arriving mid-run,
which had only inherited the prefix from a neighbour.


**Binding** — station-builders stop being private and code arriving later
opens globally, so a second arrival binds to the first with no lookup and no
table. The system's own loader is the name lookup.


**One question about source** — what the build compiled in and what has
arrived since, answered together. It needed no new storage: a loaded library
already carries its own text, so the arrival brings its paperwork with it.


**A map compiles at run time.** The measurement that says it's the right shape:
the loaded library defines *zero* box code — no call wrappers, no
station-builders — and asks the program for the three it needs. Seventeen
kilobytes for a whole program's structure.


Three routes to one program now — read as text, compiled at build, compiled
while running — dumped and compared byte for byte. The two compiled forms are
compared against *each other*, since that's the comparison that catches them
drifting while both still loosely match the text.


## What's left, and one thing worth your eye


Steps 10–12: the parser moves into the compiler, the box-name table goes, and
the loading document gets rewritten around what replaced it. Plus the deletion
half of step 9 — the engine's own reader.


The deletion is where I'd want a word from you first, because it isn't free.
Nine test files read a map at run time, and several exist *to test map reading*.
Under one mechanism they'd each spend a compiler invocation — about a tenth of
a second — where they currently spend nothing.


I think that's correct and should show in the tests rather than be arranged
around: it's the honest price of the system having one way to do this instead of
two, and hiding it would mean keeping the second way alive just for the test
suite. But it does turn a fast suite into a slower one, and that's a cost you'll
feel every time you run it, not me.


--------------------------------------------------------------------------------

### User Request 16

um ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 16

Instantiating a description needs to know where its stations landed, so the
built function reports it.


Now the deletion — the engine's own walk replaced by compiling.


I made an error there — the built function always builds, so it can't be asked
for a count first. The count belongs on the row.


Several refusals moved into the compiler and got better. Let me see which.


The exit code changed — and thinking about why says the old one was wrong.


The fresh-process reload needs the box recovered before the description
compiles. The compiler has to say what a description references.


Now the runtime brings in what it doesn't have, before compiling.


All green with the walk deleted. Let me measure what it cost, since I said it
would.


The engine no longer calls the parser at all. Let me check whether the linker
has already noticed.


Step 10 — the file still sits in the engine's directory even though nothing
there uses it.


Step 11 — the table. Let me size what depends on it.


Before I delete the table, let me check what it still costs — because the
ground moved under that step.


1.6 KB, and it no longer roots anything. Let me finish step 12 first, then put
that finding to you.


Eight commits. **311d is one step from done** — and that last step is one I
want to stop on, because the ground moved under it.


## Since the last report


**The engine stopped reading descriptions.** There were two ways to turn a
description into a program — walk it and make the calls, or write those calls
out and compile them. Only the second survives.


The linker noticed before anything else did: the parser is absent from every
test binary, discarded the moment nothing reached it. **No program built with
this engine carries a parser**, and that became true by deleting callers, not by
moving files.


Three things changed shape, each an improvement rather than a consequence to
absorb:


- **Two refusals moved into the compiler and gained a line number** — a box
  name answering to nothing, an arrow pointing at a station nobody declared.
  Caught while the description is still text, so the complaint can say *which
  line*. The engine never could; by the time it looked there were no lines left.

- **A refusal from a built description is now a malformed input, not a bad
  call.** It had been saying bad-call while the hand-written reader said
  bad-file for the identical fault — one description, two exit codes,
  depending on which route read it.

- **Fetching a box the program doesn't hold became ordinary.** A program that
  grew and wrote itself down names boxes that arrived after it started. The
  compiler is asked what a description references — reading text being its job
  now — and anything missing is compiled first.


**The parser moved in with the compiler**, and the build got simpler: it used to
be named separately because it had two callers; beside the generator it's just
another generator source.


**The loading document lost its subject for the second time**, and now describes
four movements with only the last belonging to the engine.


The price, measured: two compiler invocations per description read at run time.
The ten-map test takes 1.7s, the whole suite 7.4s. A program built from its own
descriptions pays nothing.


## The last step, and why I stopped


Step 11 deletes the table mapping box names to station-builders. Two things I
found while doing the rest make me want your call first.


**It costs almost nothing now.** 528 bytes of rows and 1,134 of strings — and
critically, **it no longer holds any code alive**, because the station-builders
are published anyway for extendability.


**And its premise expired.** 311's parent says *"a running program holds no
name, no lookup."* But you chose extendability, which requires publishing names
— this binary publishes 22 of them, and the dynamic symbol table is a lookup.
The claim was already spent before I got to this step.


What the table actually does now is map a **short name to an address** — it's
the namespace that 311a said existed but nobody had written down. The dynamic
linker can't do that job; it needs an exact symbol.


Deleting it means either 117 call sites across 17 test files spelling out
`src/boxes/029-demo-boxes.c:seven`, or putting the name-mangling rule into the
engine — a second copy of a rule that must never drift, or dragging the
generator's buffer library into every program.


My read is that step 11 should be **dropped and the reason recorded**, the same
way step 5 was: it was written to serve a premise that later work replaced. But
that's deleting a step you're owed, so it's yours to say.


--------------------------------------------------------------------------------

### User Request 17

don't know, don't care


--------------------------------------------------------------------------------

### Assistant Response 17

Making the call then: dropping it, with the reason recorded.


**311 is done.** All four children complete, nine commits, full suite passing.


## What it came to


A description becomes a running program one way now: hand it to the generator,
which turns it into the calls it describes; compile those with the same compiler
that built the binary; load and call. Same pipe whether it's one function or
fifty stations, at build time or mid-run. No program built with this engine
carries a parser.


The record of what every box *is* is gone entirely — every number in it was a
`sizeof` the compiler had already folded, so they moved from a table into a
function and stopped being stored at all.


## The thing worth keeping from it


The original draft said a running program would hold **no name and no lookup**.
That died, and it died on something you said rather than on a technical
obstacle: the engine is agnostic about what runs inside it, so a program must be
extendable while it runs. A program that can be handed a description next year
has to let it reach a box compiled today — which means publishing names.


So names didn't disappear. They moved into the operating system's symbol table,
which every process already carries and nothing here maintains. That's the
lookup.


Two of the twelve steps in the last child got **dropped rather than done**, both
because work below them moved the ground:


- trimming what the generator emits was a guess at an answer the linker computes
  exactly, and runs anyway

- deleting the last small table was written to serve the premise above; once
  that was spent, what the table does is map a short name to an address — 1.6
  KB, holding no code alive, answering something nothing else answers


Both are recorded with the reasoning, because a step nobody can find the reason
for is a step somebody does later.


## What it cost, stated plainly


A description read while a program runs spends two compiler invocations, roughly
a tenth of a second each. The ten-map test went to 1.7s, the suite to 7.4s. A
program built from its own descriptions pays nothing.


And a program keeps the boxes it was built with — 21,658 bytes on the binary I
measured — because a published name can never be discarded. That's the price
of extendability, and it's the trade you picked.


## Still open in the tree


`607` (reserved words), `408` steps 4–6 (generator-emitted formatters, which
were waiting on this work and are now unblocked), `711` step 4, `712` (capturing
a running program), and the phase 7–8 issues.


--------------------------------------------------------------------------------

### User Request 18

okay let's add those to the todo list and get started


--------------------------------------------------------------------------------

### Assistant Response 18

Let me set the order and start. First, checking whether 607 is actually
finished.


Now **408** — the runtime walks that were waiting on 311. Let me see what they
do today.


**607 filed.** Now 408 — and reading the runtime walk changes what I think the
right shape is.


This wants building. Let me read the pieces the generated code would call.


Now the helper surface generated code will call.


Now the exported helpers themselves.


Now the generator emits a reader and a writer per struct, reaching fields by
name.


Now the statics reader stops walking and starts calling.


The placement functions still hand over the old table. Let me point them at the
pair.


That's the shape I wanted — every field by name, no offsets anywhere. Let me
check what it did to the binary.


**607 and 408 closed.** Next: **712 — capturing a running program**, which the
311 work makes far more reachable than it was.


This is a seven-step feature. Let me look at what the pool already offers for
the drain.


The drain's open question may dissolve rather than need answering. Let me check
what the stopping path already reports.


That answers 712's open question — the reasoning is already written down next
door. Let me record it and build the format.


The engine explicitly promises nothing about order within a port, so a capture
shouldn't either. Let me build on that.


Now the dump writes them, and the format learns the form.


Now the reader side — the parser learns the bracket form.


Now the engine side — putting values back into a buffer from text.


Now the operation that puts a captured queue back.


Now the generator emits the call, so a captured description rebuilds its queues.


Now the round trip — a program with work in flight, put down and picked up.


The program needs an entrance to be startable. Let me restructure the test's
maps.


The buffer grows rather than refusing, so both my check and the test's were
wrong.


Scene 1 passes. Two problems — one is my test, the other may be the engine.
Let me check the static-binding path.


That's a real gap, and a revival is exactly what surfaces it. Let me look at the
readiness restart.


All three scenes pass. Let me check nothing else broke.


Three issues down this stretch. Here's where things stand.


## Closed


**607 — no reserved words.** Was already built; filed into the buildable
history with its lesson: the three unusable words were the symptom, and the
problem was that a station line had been defined as *whatever was left over*. A
negative definition can only narrow, so every keyword added later would have
taken another name away.


**408 — values back into text.** Each struct now gets a reader and a writer of
its own, reaching **every field by name** — `v.pos` rather than `bytes +
fields[1].offset`, with each width a `sizeof` at the point of use. No offset is
stored or computed anywhere, which is a stronger form of the
compiler-computes-everything promise than a table of correct offsets: there's no
number left that could be wrong.


One step got completed *by not doing it*, and the reason generalises. The plan
was to emit the escape rules and number handling into every generated type so
the halves couldn't drift. But none of that varies by type — braces, commas,
escapes, widths are the same for every struct — so emitting them per type
would have been one grammar written many times, which is the exact failure the
step existed to prevent. The split runs the other way: **what varies by type is
generated, what doesn't is written once and called.**


Two things came free: a struct's padding is now deterministic, so two values
read from the same text compare equal all the way through; and the old field
table is discarded by the linker from every binary that doesn't explicitly ask.


## 712 — capture, three steps of seven


A dump can now say what a program is *holding*, not just what it's shaped like.
Buffers write their waiting values in brackets; reading them back goes through
the ordinary delivery, so the readiness check runs and the tasks that form are
the tasks that would have formed.


Proven three ways — work survives the round trip, the revived program finishes
the work it was carrying and gives the answers the first would have, and a queue
of struct values survives with its inner commas intact.


**Two findings worth flagging:**


A **real engine gap surfaced** doing it. A station whose buffer filled while
another port sat empty started exactly one task when that port finally filled,
and stranded the rest. The one rule says a station runs whenever every port
holds a value; asking once is only enough when values arrive one at a time. Now
it's asked until the answer is no. That would have silently lost captured work.


And an **open question dissolved** rather than needing an answer. The drain
needs no bound of its own — the reasoning was already written beside the
shutdown path: whoever sent the signal already has a timer, and theirs is the
only clock that knows how long is too long for this deployment.


## Remaining on 712


The drain itself, iterator cursors, the binary-producing path for a program that
grew boxes, and the report. Then 711's reading order and the phase 7–8 issues.


--------------------------------------------------------------------------------

### User Request 19

oh good point. I'm picturing a situation where we have 30 values sitting in a
ring buffer on input port 0, and input port 1 is a static with no value. Then,
when the static value is filled in, we'd only enqueue one single task. When
updating static values, we should recursively check the readiness over and over
until the system is sufficiently drained such that it can no longer enqueue any
new tasks. If a box has only static inputs, we should enqueue it once, and do so
once again if the values are ever modified.


--------------------------------------------------------------------------------

### Assistant Response 19

Let me verify the all-static half actually behaves that way — I asserted it
but didn't test it.


The write path has the same gap — the loop only went into the bind path. Let
me check the seed guard too.


Now a test for exactly the three cases you described.


All six pass. Let me confirm the new scene actually catches the regression.


The tests catch it. Let me record this where the design lives.


You caught a real one — and your refinement about the all-static case was the
half I'd gotten wrong.


## What you were right about that I'd missed


I'd put the drain only on the **bind** path. Your scenario named the **write**
path too — a constant that already exists and gets changed while thirty values
sit waiting — and that was still stranding twenty-nine. Both doors are covered
now, plus the third one (a revived program putting its captured queue back),
since all three are the same shape: a port filling all at once.


The write path keeps doing its write and its first readiness check inside a
single lock hold, which is deliberate — it's what closes the gap between the
value changing and the question being asked. What changed is that the asking
continues from there.


## And your rule for the all-static case is exactly the constraint


"Enqueue it once, and once again if the values are ever modified." That isn't
just a preference — it's forced. A constant is never consumed, so a station
made only of constants is ready *forever*, and a drain on it would spin without
end. So the drain declines to touch such a station outright rather than relying
on a count to stop it.


That's now three scenes:


| | |

|---|---|

| thirty waiting, constant bound | thirty runs, not one |

| constant written while running | nothing stranded, nothing started twice |

| only constants | once at completion, once per change |


Removing the drain fails four checks across two of them, so it stays caught.


## Where it's written down


This became **guarantee T6** on the guarantees page — *a station that is ready
several times over starts that many tasks* — and a correction section on the
ticket that built constants-on-ports. That ticket's rule was right and
incompletely built: it said writing a constant runs the ordinary readiness
check, making a chain of stations into a recalculation graph. It did run the
check. It ran it once.


Worth noting where this came from: the bug was invisible until a program was
revived from a capture, because in ordinary running, values arrive one at a time
and each arrival asks again. The feature didn't cause the bug — it was the
first thing that could see it.


--------------------------------------------------------------------------------

### User Request 20

oh also if we "change" a static to the same value that it currently is, that
counts, and we should enqueue if there's only other statics, and drain if
there's a ring buffer with values inside it. Only if all the values are present
of course.


--------------------------------------------------------------------------------

### Assistant Response 20

No comparison anywhere — a write always copies and always asks. That's already
the behaviour you want, but nothing pins it, so someone could add "skip if
unchanged" as an optimisation later.


All eight pass. Let me make sure the identical-write scene would actually catch
an optimisation someone adds later.


The test didn't catch it — because skipping the *copy* doesn't skip the
*check*. Let me try the placement that would actually break it.


--------------------------------------------------------------------------------

### User Request 21


--------------------------------------------------------------------------------

### Assistant Response 21

No response requested.


--------------------------------------------------------------------------------

### User Request 22

hi something broke so if you were running some tests or whatever, maybe don't
run them again. otherwise, just continue, it might have been a different program
on the computer.


--------------------------------------------------------------------------------

### Assistant Response 22

Nothing lost — the last commit landed and the tree is clean. Picking up
**712's** remaining pieces; the iterator cursor is the other half of "what a
program holds".


The station line can carry it. Let me widen what follows the kind.


Now the engine side and the emission.


Now the dump writes it.


Nine scenes. Let me check the rest of the suite once, then document.


My earlier edit to the step list silently failed to match and never landed. Let
me redo it properly.


Nothing was lost — the last commit had landed and the tree was clean, so
whatever broke was outside this work.


## Since then


**712 step 3 is complete.** The other half of "what a program holds": an
iterator's place in its exits, which the station header has always called *the
one memory a station keeps* and which nothing wrote down.


It rides on the station line as `@N`, either order with the door word. Written
only when it says something — zero is where an iterator starts, and no other
kind of station has a position to be in. Setting one on a non-iterator is
refused, since there's nothing for it to mean.


The reason it matters is the failure mode: a capture that reset it produces a
file of exactly the **right shape** whose next value goes to an exit it was
never going to. The wiring is all there and only the behaviour is off, so
nothing about the file looks wrong.


**Also repaired:** the ticket's own step list. An earlier edit of mine had
silently failed to match — I'd written the replacement against text that
didn't quite exist, without an assertion to catch it — so the ticket still
described finished work as unstarted. Found by reading the file rather than by
anything failing.


## 712 now stands at


| step | |

|---|---|

| 1 — the drain | not built |

| 2 — the waiting-value form | done |

| 3 — buffer contents and cursors | done |

| 4 — revival | done for the ordinary case; refusing an incomplete artifact waits on step 1 |

| 5 — round trip | done for values in flight, not for a running pool |

| 6 — the binary-producing path | not built |

| 7 — the report | not built |


Nine scenes in the capture test, and the suite passes.


Next is step 1, the drain — which is where the "incomplete capture" header
comes from, and therefore what step 4's remaining half needs.


--------------------------------------------------------------------------------

### User Request 23

ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 23

Step 1 — the drain. Let me see how the stopping layer already reports wedged
workers, so I compose rather than duplicate.


Now the drain and the incompleteness header.


Now the three pieces: the drain, the immediate capture, and refusing to revive a
lossy artifact.


Now the reading side refuses a lossy artifact unless asked for salvage.


Now the scenes — including one with a worker genuinely wedged.


All twelve scenes pass, including the wedged-worker capture. Full suite check.


**712 is at five of seven steps.** Twelve scenes in the capture test, full suite
green.


## The drain


Two doors, because there are two situations and collapsing them would make the
more important one lie.


**The polite one** shuts the entrance — the only way anything outside pushes
work in — lets everything in flight finish and deliver, waits for the workers
to go home, then writes. Complete by construction: nothing was running when it
was written.


**The honest one** writes immediately, whatever is happening, and states what it
lost — a header naming every station a worker is still inside. A program that
cannot drain is exactly when a capture is worth most, so the writing has to
happen anyway.


Neither invents a bound, which is the open question **answered by not needing an
answer**. Quiet is decidable exactly; the only case that never returns is a box
that never returns, and nothing inside a process can tell that from a box that's
merely slow. Whoever asked already has a clock.


## The refusal


Reading a lossy artifact through the ordinary door is fatal. There's a second
door for salvaging one, with a different name, so whoever uses it has said out
loud that they know what's missing.


The check asks the *file*, not the description — the marker is a comment, and
a comment is by definition not part of what a file says about a program. That
distinction is why it lives in the loader rather than the parser.


## Proof


The wedged-worker scene is the one I'd point at: a worker goes into the box that
never returns, in a forked child since it stays there for the life of the
process. The child waits for the worker to actually be *inside* the wedge by
asking the pool what it's doing, rather than guessing — nothing there invents
a clock either.


## Left on 712


Step 6, the binary-producing path for a program that grew boxes — its source
compiled in like any other box, so the artifact stands alone without a
toolchain. Step 7, the report.


Step 6 is largely reachable now: a program already carries the text of every
source it's made of, including late arrivals, and the compile-at-runtime pipe
exists. What's missing is emitting a *new* binary rather than a shared object.


--------------------------------------------------------------------------------

### User Request 24

ok um, keep working I guess?


--------------------------------------------------------------------------------

### Assistant Response 24

Step 6. The pieces mostly exist — a program already carries every source it's
made of. Let me expose the spill.


Now a capture that stands alone: description plus every source it names.


All thirteen scenes pass. Let me verify the captured directory is actually what
it claims — buildable.


The test verifies the addresses resolve to spilled files on every run, which is
the claim. Full suite, then documentation.


Step 7 — the report. Let me see what's already gathered.


All seven steps done. Let me work through 712's remaining open questions before
closing it.


**712 is complete** — all seven steps, all open questions answered, fourteen
scenes, suite green.


## The last two pieces


**Step 6 turned out not to want a binary.** It asked for a program that grew
boxes to capture into a new executable. What it actually needed was one step
short: a **directory** holding the description plus every source the program is
made of, at the paths the description addresses them by. Having the running
program link a fresh binary would mean carrying the engine's own source in every
binary so it could be linked again. Building a captured program needs the engine
— which is what building anything with this engine needs, not a new dependency
— and the binary that comes out needs no toolchain, which is what was actually
wanted.


**Step 7 is a report beside the description**, not inside it: the two are read
by different readers. Nothing in it is measured for it — run counts, buffer
depths, growths and late arrivals were all already kept. Its most useful line is
a buffer that had to grow, which names where one input side outran another, in
slots rather than in a judgement.


## The three open questions answered themselves


- **Counters reset, history in the report** — a description says what a
  program *is*, and how many times something ran is not that. Exactly the shape
  the question hoped for, reached by building rather than deciding.

- **A revived program keeps no identity because there is none to keep** —
  nothing here has ever given a program a name that outlives its process. Not a
  property lost in the capture; a property that never existed.

- **An incomplete artifact does nothing to the round-trip promise**, because the
  ordinary door refuses to read one. Refusing was cheaper than qualifying the
  guarantee, and it says the same thing to the person holding the file rather
  than to the person reading the guarantees page.


## Standing back


The thing worth keeping from this ticket is what proving it exposed: a
**readiness gap the engine had carried since constants moved onto ports**. A
station whose buffer filled while another port sat empty started one task and
stranded the rest. Invisible in ordinary running, because values arrive one at a
time and each arrival asks again — and the capture was the first thing that
could see it.


## What's left in the tree


`711` (reading order), `705` (HTML interactive pieces), `709` (slideshow),
`710`/`713` (demos), `801` (browser workbench).


--------------------------------------------------------------------------------

### User Request 25

can we add a cli flag that is basically "this compiled system is viewable" which
adds a signal that is emitted at each important event that can be picked up by
arbitrary processes? for debug purposes, or just for watching a program running
dashboard style. Then, we should make a separate workbench system that only
views a system that's running, and doesn't do anything else. This could be a web
app, or a native viewer, or even a CLI program, but for our purposes let's build
it for the browser. We should not use the same machinery as the workbench but it
can go in phase 8 as well. Once that issue file is built, let's work on 801


--------------------------------------------------------------------------------

### Assistant Response 25

Two issues then — the emitting half and the viewer are built by different
hands and only one of them is a browser. Let me see what phase 8 holds.


801's stated blockers are both complete now. Let me check the observe layer
before writing the new issues.


Now the two issues.


Both issues are down. Now **801** — let me read the rest of it before
building.


The generator deletes anything in the output it didn't produce, so the workbench
must be copied in rather than authored there. Let me check the site's look
first.


Good — dark, with wire-blue and box-amber as the two accents. Building the
canvas.


Now the site generator has to carry it in rather than sweep it away.


Node is broken on this machine — that's likely what you hit. Let me review
carefully instead; I found two real bugs reading it back.


Now the front door, so the page is reachable.


Let me make a self-contained copy so you can actually try the interaction.


Two issues written and the first step of 801 built. The preview above is live
— try it.


## The two new issues


**802 — a program you can watch.** A build flag makes the engine leave a trail
as it runs, in a ring in shared memory any number of processes can read. Without
the flag the emitting isn't compiled at all, which is the shape timing already
chose: *a measurement apparatus whose cost is unmeasured is a rumour, and one
that cannot be removed is a tax.*


The constraint everything follows from: **watching must never change what is
watched.** No lock a reader can hold, no buffer a reader can fill, no handshake.
A slow reader loses events and is told how many — losing them is correct,
because the alternative is a reader that can slow the program by being slow, and
being told is what stops a view quietly missing things somebody would trust it
about.


A ring in shared memory was chosen for what it *doesn't* need: no socket, no
port, no protocol, no dependency, no second thread. A reader opens a file and
maps it.


**803 — the viewer.** A page that watches and cannot touch. It shares nothing
with the workbench, deliberately: a canvas composing a map treats every station
alike and exists to let somebody change things; a view of a running program
cares which station is busy right now and can change nothing. A layer serving
both serves neither.


Its first open question is the awkward one, and I put it up front rather than
buried: **a page cannot map shared memory**, so something has to stand between.
That choice decides whether phase 8 gains a long-running program, which nothing
else in this project has.


## 801, step one


Also: phase 8's stated blocker is gone — both format changes it waited on have
landed.


**What building it settled**, now written into the ticket: a station on the page
carries a fact the map file deliberately doesn't — how many input ports it
has. That comes from the C, and the format has never said it, because a map
carrying an arity would be a second source of truth able to disagree with the
compiler. So the author says instead, which makes it **layout rather than
program** — it belongs in the companion file, never in the map. Same for an
iterator's exits, and pointedly not for a comparator's, since having three is
what being one *means*.


One note: **node is broken on this machine** (`undefined symbol:
sqlite3session_attach`), so I couldn't syntax-check the script with a real
engine. I reviewed it by hand instead and found two bugs that way — the first
station named itself `station2`, and a hover style promised something that
couldn't happen on a layer that ignores the mouse. Both fixed, but the page
hasn't been run by anything but a browser you're about to point at it.


--------------------------------------------------------------------------------

### User Request 26

can you write a license file that'll show up on github that basically says "you
can use this software if you want but if you're a large corporation then you
have to pay me first"?


also, we probably have enough to write a readme.md file. Can you do so?


then, let's add all of phase 8 to the todo list and work on that.


--------------------------------------------------------------------------------

### Assistant Response 26

Now the README. Let me pull the current numbers rather than assert any.


Both are down. Before phase 8, let me check something 801 step 2 assumes.


Clang can target wasm. Let me check it can link one.


No `wasm-ld` here — and checking that turned up something bigger about step 2.
Let me look at what it assumes exists.


Now the round trip it makes testable.


Round trip passes on every form. Full suite, then commit.


Three deliverables. Two caveats on the license, then where phase 8 stands.


## LICENSE.md


Plain-language summary first, then terms. **Large is a hundred people or a
million dollars a year** — borrowed from the PolyForm Small Business license
rather than invented, so a company that's already decided how it feels about
those numbers has already decided about yours. Both tests rather than one: a
ten-person company making forty million isn't small, and a three-hundred-person
co-op making eight hundred thousand isn't large in the way this cares about.


The refusing clause is written as an invitation — *"the answer is not going to
be no"* — plus a twelve-month grace period if you grow past the line while
already using it.


**Two things you should know:**


- **I'm not a lawyer and this isn't legal advice.** It's modelled on licenses
  that are widely used, but if enforceability against a real company ever
  matters, have someone qualified read it.

- **GitHub won't give it a named badge.** Custom licenses show as "View license"
  rather than "MIT" or similar. That's the cost of a custom one; using PolyForm
  Small Business verbatim would get recognized, at the price of it not sounding
  like you.


I used your **GitHub handle, not your email**, as the contact — publishing a
personal address in a repo felt like your call rather than mine. Easy to change.


## README.md


Leads with the one rule, since everything else is a consequence and a reader who
has it can predict most of the rest. Both example maps were **run through the
real parser** rather than written by eye, and the directory table was checked
against the directories. It names no counts — a number in a readme is a number
that goes stale — and points at the command instead.


## Phase 8


Building step 2 turned up that **the thing it said to compile didn't exist.**
The plan was to reuse the project's own map writer rather than reimplement it in
the page. But the only writer was the dump, which walks a *live station table*
— and someone drawing a map has a drawing, not a table. Using it would have
meant the whole engine in the browser, which the ticket spends three sections
refusing.


So the missing half now exists: **a description written back out as the text it
came from**, which closes a round trip the format has been without since it was
written. Two writers on purpose — the dump says what's *running*, this says
what was *written down*.


Tested by comparing descriptions rather than texts (files differing only in
blank lines describe one program), plus the stronger claim that the writer is a
**fixed point**, so a file can't grow each time it's opened.


**One blocker, on tooling not design:** this machine has no `wasm-ld`, so the
writer can't reach the page yet. I recorded it rather than working around it,
because a JavaScript reimplementation is exactly the second authority the ticket
exists to avoid.


Next in phase 8: the shelf (801 step 3, which the issue says should come early),
then 802's shared-memory ring.


--------------------------------------------------------------------------------

### User Request 27

let's do agpl as the license actually so we get the neat badge. Make sure it's
for both branches in the project.


let's keep going?


--------------------------------------------------------------------------------

### Assistant Response 27

Real AGPL-3.0, Affero clause intact. Installing it on this branch first.


Now the `original` branch. Using a worktree so this one isn't disturbed.


Both branches carry it, byte for byte. Neither is pushed — you didn't ask.


Now phase 8, step 3: the shelf. The issue wants it early, before sessions get
spent on drawings worth not losing.


Reviewing it back, two real bugs. Let me fix them.


A README change that isn't mine got swept into that commit. Let me split it back
out.


The license is on both branches, and the shelf is built. Three things to flag.


## The license


Verbatim AGPL-3.0 as `LICENSE` — 34,523 bytes, Affero clause intact — so
GitHub's detector will give you the **AGPL-3.0 badge**. Same file byte-for-byte
on `original`, which had no license at all before.


Neither branch is pushed; you didn't ask.


Worth knowing about the trade you made: **AGPL doesn't contain the "large
companies pay me" clause.** It has a different lever — anyone who runs a
modified version as a service owes their users the source. For an engine other
programs are built with, section 13 is the clause that matters, since without it
a modified engine could power a service forever with nobody ever seeing the
changes.


You still hold the copyright, so **dual licensing is available**: AGPL for
everyone, plus paid terms for companies who don't want the copyleft. That's
exactly how the "large corporations pay me" outcome usually gets reached in
practice, and it keeps the badge. Say the word and I'll add it to the README.


## Someone edited the README while I was working


`README.md` changed on disk under me — `# minimal soramech` → `# ceramic
core engine`, and `An engine` → `An system`. I swept it into my commit with
`git add -A`, which was my mistake; I've split it back out and **left it
uncommitted** for you.


Two things about it: the rename would want to reach `CLAUDE.md`, `vision`, the
site title, and the docs, not just the README — and `An system` should
probably be `A system`. Tell me if it's a real rename and I'll carry it through
properly.


## The shelf


Drawings save to browser storage on every edit — not on a timer, because a
timer is a window in which work isn't saved yet, and that's the window a browser
crashes in.


**Two bugs found by reading it back**, since this machine has no working JS
engine: discarding a map to the *left* of the current one shifted every later
index, so clamping quietly moved you onto a different map — the one thing a
shelf must never do. And an older saved shelf missing a field is now filled in
rather than refused, because somebody's saved work isn't the place to be strict
about a schema.


Next in phase 8: validation as wires are drawn (step 4), then statics and
download.


--------------------------------------------------------------------------------

### User Request 28

okay so the README.md file kinda sucks, can we rewrite it? First give me a
report about why it sucks, assuming it structurally is just kinda bad, and then
tell me what shape it should have instead.


--------------------------------------------------------------------------------

### Assistant Response 28

Let me read it back cold rather than from memory.


Read it back cold. Here's the report.


## Why it's bad, structurally


**1. It has no reader in mind, so it serves none.** Four different people open a
README — someone deciding in twenty seconds whether to keep reading, someone
who wants to run it, someone who wants to understand the idea, someone about to
contribute. This interleaves all four. "Building it" sits between "What the rule
buys" and "Writing a program", then more theory follows. The reader ping-pongs
between registers.


**2. It explains, then shows. It should show, then explain.** The order is
textbook — definitions, theorem, corollaries. But nobody has agreed to study
yet. Vocabulary before payoff is memorization with no hook.


**3. The opening example is four unexplained syntaxes at once.** At line 9 the
reader doesn't know what a station is, what `p` means, what `entry` means, or
how to read `out 0 - parse.0`. It's meant as an intriguing hook and lands as
noise. It also names functions that don't exist in the repo, so it can't be
tried.


**4. Nothing in it can be run — and one instruction is simply false.** I told
people to run `./run-demo` without checking. It prints:


```

No demos directory at .../issues/completed/demos

The seven paged demos were deleted; their replacements are not built yet

```


For a project whose entire claim is *"the shape executes itself across every
core"*, there is **no path from clone to seeing that happen.** That's the single
biggest miss, and the false instruction is worse than the absence.


**5. "The parts worth knowing about" is a junk drawer**, and the heading admits
it. Five sections whose only shared property is that I found them interesting.
Two are build facts, two are runtime capabilities, one is operations.


**6. It summarizes the docs instead of routing to them.** This project has an
unusually good documentation set. The README duplicates chunks of it, which
guarantees drift and wastes the reader's twenty seconds on material that exists
better elsewhere.


**7. It repeats itself.** The one rule appears in the opening paragraph, its own
section, "What the rule buys", and "What this is not". Sizes-from-the-compiler
appears twice.


**8. Uniform emphasis is no emphasis.** Every section is three paragraphs with a
bolded lead. Nothing stands out because everything does.


**9. Wrong register.** It's written in the voice of the commit messages and
issue files — which is right for a blueprint read by someone already invested,
and reads as unearned confidence to a stranger. Many sentences assert a decision
was correct before the reader knows what it was for.


**10. No picture.** The central idea is *a graph of boxes with values moving
along wires*, and there's no diagram. The one thing that would make it instant
is missing.


**11. Scope limits are at the bottom**, where they read as apology. They belong
early, as a service — they let a wrong-fit reader leave happy in twenty
seconds.


**12. Two live errors from the rename**: line 3 says "An system", and the body
still calls the project "SoraMech" under a title that now says "ceramic core
engine".


## The shape it should have


The principle: **a README is a front door, not a summary of the house.** Show,
then explain. Route, don't duplicate.


| | section | job | reader leaves knowing |

|---|---|---|---|

| 1 | One line + a diagram | 15-second decision | what kind of thing this is |

| 2 | What's strange about it | the hook | why it isn't another task queue |

| 3 | **See it run** | earn trust | it works, on their machine |

| 4 | The smallest real program | first success | how to make one |

| 5 | The one rule | the idea, once | why it's shaped this way |

| 6 | Good fit / bad fit | a service | whether to stay |

| 7 | Where to go next | routing | which door is theirs |

| 8 | License | two lines | the terms |


Roughly ninety lines instead of two hundred and forty, and every section has a
different job and a different reader-state.


**Three things this fixes that aren't just trimming:**


- **Section 3 has to actually exist**, which means building something runnable
  first. Right now there is nothing to point at. Either a small runnable example
  lands in the repo, or `make test` becomes the honest answer with a note about
  what you'll see.

- **The diagram carries the three nouns**, so they don't need a list. A picture
  of two boxes, a wire, and a buffer between them teaches station-and-port
  faster than four sentences.

- **Sections 5 and 7 replace all of "the parts worth knowing about."** Each of
  those five topics has a real document behind it; the README should say *that
  they exist and where*, not restate them.


## One decision before I write it


Section 3 needs something to run. Options, best first:


1. **A tiny example in the repo** — one map, two or three boxes, `make
   example`. Twenty lines of C, and the README's example becomes real and
   testable.

2. **Point at `make test`** — honest, zero new code, but "run the tests" is a
   weak first impression for an engine.

3. **Restore a phase demo** — 713's work, much bigger, and it's a real issue
   already.


I'd do (1) and note it as serving 713 later. Want me to build the example first,
or write the README against `make test` and add the example after?


--------------------------------------------------------------------------------

### User Request 29

ok sounds good let's do it


--------------------------------------------------------------------------------

### Assistant Response 29

Building the example first, so the README has something real to point at.


It runs — and the numbers are wrong in a way that's actually the engine
telling the truth. Let me look at why.


The example landed on the project's subtlest documented property — a rejoining
station pairs whatever each port hands it. Correct, but a bad first impression.
Let me make it teach that rather than trip on it.


Correct, and it teaches the thing it nearly tripped on. Full suite check before
I commit it.


Now the README, to the shape we agreed.


Done — and building it turned up that the worst problem wasn't fixable by
writing.


## The report, condensed


The structural faults were: **it explained before it showed**; it had **no
reader in mind**, so it ping-ponged between the person deciding whether to care
and the person trying to build; the opening example was **four unexplained
syntaxes at once**; a section literally titled *"the parts worth knowing about"*
was a junk drawer; and it **summarized the docs instead of routing to them**,
which guarantees drift.


But the worst one was factual. **Nothing in it could be run.** The one
instruction promising a first look — `./run-demo` — printed an apology,
because the demos were deleted a while back. I'd written that line without
checking it.


## So the first fix wasn't the README


`make example` now exists. Four stations: a number goes in, two branches take it
at once because nothing connects them, a fourth adds their results and runs only
once both arrived. About a minute from cloning.


**It nearly told a lie on its first run, and that's the part worth your
attention.** Written to send five values through, it printed a table of answers
belonging to no input. The branches race, and the station where they meet pairs
whatever each port hands it — there is no batch, no round, and nothing could
implement one without a tag riding on every value. Your guarantees page has said
exactly this all along; the example had to be *run* to notice.


So it sends one value, gets the right answer, then spends its closing lines
explaining what happens if you send more and why that isn't a defect. The
reader's next instinct is to try exactly that, and finding out by surprise reads
like a bug rather than like the meaning of independence.


## The README


150 lines instead of 240. Picture in the first ten, because the central idea is
a graph and a graph is a thing you look at. Then what's unusual → a command
that runs it → how to write one → the rule, delivered to someone who has
already watched it work.


Scope limits moved **up** out of the basement, where they read as apology;
placed early they're a service. The reference section is links, not prose —
every path checked against the tree rather than typed from memory, which is how
the dead demo line got in last time.


## One thing you should decide


I kept your title, **ceramic core engine**, and fixed the "An system" grammar.
But the rename is only in the README — `CLAUDE.md`, `vision`, the docs, the
site title, and the directory itself all still say SoraMech. The README now says
both, which reads as an inconsistency rather than a distinction.


Say the word and I'll carry the rename through properly, or revert the title if
it was a scratch thought.


--------------------------------------------------------------------------------

### User Request 30

I want to emphasize that we use multithreading for everything. That this system
forces it, and does so in the most efficient way in terms of scheduling. Perhaps
not in terms of real-world performance of course, because there's a bit of
overhead for every task that isn't present if you do it by hand. But the novel
construction method enables multithreading for systems that are not naturally
designed for it, and it lowers the barrier of entry for people who want to
design things either visually, or with a markup language.


--------------------------------------------------------------------------------

### Assistant Response 30

Let me get the per-task cost right before I claim anything about it.


Two claims to state precisely: the scheduling one, and its honest cost.
Rewriting the top of the README around them.


211 lines — I've grown back most of what I trimmed, and the delivery-check
explanation now appears twice. Let me consolidate.


Reframed around the thesis. Three things worth flagging about how I stated it.


## The scheduling claim, made precise enough to defend


I avoided "fastest" and wrote what's actually true and checkable:


> **Finding ready work costs nothing, because nothing finds it.** No ready-queue
> to scan, no work-stealing search, no thread waking to look around. The write
> that fills a station's last empty port *is* the write that starts it — on
> the thread already standing there, looking at one station and nothing else.


That's the strongest honest form: the decision happens at **the only instant its
answer can have changed**, on the thread that changed it. It's not a claim about
wall-clock speed, which is good, because that's not the claim you wanted to make
either.


## The cost, in the same breath rather than a footnote


Verified against the code before writing it: every invocation is a `malloc`, a
copy of each input into the task, a push onto the pool's ring, sometimes a
worker wake, then a `free`. So:


> You stop deciding what runs where, and you pay a fixed cost per step to stop
> deciding.


That framing puts the overhead where it belongs — as what the scheduling
costs, not as an embarrassment. And it gave the "whether it fits" section the
case it was missing: **steps too small to pay for themselves.** A box that adds
two integers loses to a function call and no amount of wiring fixes it.


## The barrier-to-entry point needed a "why", not just a claim


"Lowers the barrier" is the kind of thing every project says. What makes it true
here is specific, so I said that instead:


> Somebody who could not write correct threaded C can still build a program that
> saturates a machine, **because the part that is hard to get right is not the
> part they are writing.**


And the sharper version of "enables multithreading for systems not designed for
it": making an existing pile of functions concurrent isn't rewriting them, it's
placing them and drawing arrows — and the argument about which lock is held
where doesn't get *won*, it stops existing, because nothing is shared.


## Net


198 lines, down thirteen despite the additions. Stating the rule once at the top
let a later section stop restating it, and a list of "unusual properties" turned
out to be three sentences already said better above it.


The rename question still stands: `CLAUDE.md`, `vision`, the docs, and the
directory all still say SoraMech, and the README now says both.


--------------------------------------------------------------------------------
