# Conversation Summary: bc2f1355-a646-454d-a731-9aab2bb3e23c

Generated on: 2026-08-21 19:13:54

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
