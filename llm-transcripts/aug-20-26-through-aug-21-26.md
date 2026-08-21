# Conversation Summary: 55f347f4-2832-4363-ab6b-50a0c7b42395

Generated on: 2026-08-21 07:45:48

--------------------------------------------------------------------------------

### User Request 1

hi, can you build me a roadmap for the issue files?


--------------------------------------------------------------------------------

### Assistant Response 1

I'll start by looking at what's actually in the issues directory and the
existing documentation, so the roadmap describes the real state of things.


There's already a roadmap document. Let me read it and the phase progress pages
before deciding whether to rewrite or extend.


Existing roadmap describes the phases in prose but doesn't order the open issue
files. Let me read the live status pages and the headers of every open issue.


Now let me extract the section structure and any stated dependencies across
every open issue.


Let me verify the two things that look actively broken before I assert them in
the roadmap.


Everything checks out — including that three demos genuinely fail to compile
(they call a gatherer conversion, a gatherer tag, and a live repoint, none of
which exist in the engine any more). Now writing the roadmap.


Let me verify the documentation generator accepts the new page (it reports
unresolved references, which is the check I want).


Built it at `issues/roadmap.md`, beside the seven phase progress pages. Here's
what's in it and what I found while building it.


**What the page does.** The existing roadmap in the docs directory explains why
the eight phases are shaped the way they are. It doesn't say what's left. This
new page reads across all seven progress pages at once and orders the
twenty-four open blueprints by what can actually start now versus what's waiting
on something specific — with every "this waits on that" traced back to the
sentence in an issue file that says so, rather than to my judgement.


**Two things I found that are worth knowing before any ordering:**


Three of the seven demos no longer compile. Removing the pull path took the
gatherer out of the engine, and the phase four demo still asks a port to become
a gatherer, the phase six demo still switches on a gatherer tag, and the phase
seven demo still repoints a gatherer on a running map. I checked all three names
against the engine header — none of them exist any more, so this is real and
not just the issue file predicting it. The launcher in your project root offers
seven entries and three stop at the compiler.


The blueprint called *values back into text* opens by saying the engine can't
turn bytes into text. It can — the mirror formatter was built alongside the
statics work, the map dump calls it, and floats already come back with enough
digits to read identical. What that issue still genuinely wants is different:
the formatter and reader emitted as generated code per struct instead of walking
a field table at runtime. Its current-behavior section is describing a gap that
closed underneath it.


**The ordering it lands on.** Four lines of work that mostly run in parallel: a
strictly sequential concurrency line (per-cell states → copies leaving the
lock → growth by appending, each step removing the reason the next was hard);
a construction line (changing what a port is → one way to build a station →
one way to build a program, with both doorways and the shutdown work hanging off
the end); a generator line (boxes addressed by file first, then placement
functions and embedded source in parallel, then the map becoming code); and the
legibility line. The longest chain is five deep and starts at *changing what a
port is*, which is one of the smallest issues left — that's the argument for
starting there rather than somewhere that looks more urgent.


Phase two is where almost everything else is waiting. Phase four is the one that
closes soonest — both its open issues can start today.


**Verified:** the documentation generator accepts the page with zero unresolved
references, so every link in it resolves. I added it to the tree and the
where-to-look table in the table of contents, which meant regenerating the site
— 124 HTML pages changed because the navigation sidebar appears on every one
of them. Nothing is committed.


**Six open questions came out of building it**, and by your rule this isn't
finished until we've worked through them. Starting with the first:


The page sits in the issues directory because it's a live status page like the
progress pages, and because the docs directory already has a roadmap that says
something different and should keep saying it. The alternative is an indexed
document in docs taking the next file index — that would put it in the
project's reading order, at the cost of having two documents named roadmap
sitting near each other. Which do you want?


--------------------------------------------------------------------------------

### User Request 2

whoa whoa whoa we don't want anything in issues/ besides the issue files and
progress files. We should just remove it and keep it in memory as we work
through the remaining issues.


Can we remove all the demo files? they kinda suck. Let's make a new issue to
create new demos. Also, they should be more interactive than they currently are
- ceramic is a system intended for persistent operations, so we should treat the
demos as such and give the users some buttons and levers to tweak and push and
such, updating the system while it's running, and demonstrating each of the
implementation details of the phase that the demo belongs to.


We have 6 new open questions? Wow! Let's work through them one-by-one.


--------------------------------------------------------------------------------

### Assistant Response 2

Three things, and the demo work needs an issue file before I touch anything. Let
me start with the roadmap removal, and check a couple of facts first.


"Ceramic" is the project's own name — the repo was renamed to
`ceramic-core-engine`. Let me read that context and then survey the demos before
deleting anything.


Before deleting: let me check what else references the demos, so the issue file
can name everything that moves.


Now the issue file — it has to exist before the deletion, since the deletion
is its first step.


Now the bookkeeping the deletion forces. First, the launcher currently tells a
falsehood with no demos present:


The count in that message is wrong — seven phases had demos, and two of those
phases are fully closed. Let me fix it and add the reason comment the house
rules ask for.


Now the phase 7 progress page, which is where the demo issues live:


Done with all three pieces of mechanical work. Tests pass, the site regenerates
with zero unresolved references.


**The roadmap is gone** — file removed, table-of-contents edits reverted,
generated page deleted. I'm holding the ordering in context: four lines of work,
the longest chain is five deep starting at *changing what a port is*, and phase
two is what almost everything else waits on.


**The demos are deleted** — all twenty-five files: seven programs, both
presenters, the four map fixtures, the seven runner scripts. They're in git
history. I wrote the issue file first, as the house rules ask, so the deletion
is recorded as its first step rather than discovered later.


**The new issue is *the demos you can steer*** (phase 7, number 713). Its
argument: a paged demo is a transcript of something that already happened, and
this engine's three most unusual capabilities — a write being an event, a
shape editable while it runs, and code arriving after the program started —
have no subject until somebody outside the program acts. A demo that supplies
its own acts demonstrates each of them with the interesting half removed. What
replaces them is one live control panel per phase, three regions on screen (the
machine, the levers, a log of what you did and what the engine did about it),
staying up until you quit. Each phase gets a lever table — phase two fills one
port at a time so you watch readiness fire on the last slot and not before;
phase three lets you type a C function into a field and compile it into the
running program.


Two things I changed as fallout, both flagged: the launcher's empty-state
message said *"Nothing has been finished yet"*, which became a falsehood at the
front door the moment the demos went, so it now names what's actually absent.
And *the demos after the pull path* is marked superseded — the three programs
it repairs no longer exist — but kept in full, because it's the only written
record of which scenes depended on pulling and what each was for.


On the questions: the first of the six is answered by your instruction —
nothing in the issues directory but issue files and progress files. That leaves
five, and writing the demo issue added five more, so the queue is ten. I want to
jump one to the front, because it decides whether the issue file I just wrote is
right:


**[Asked the user]**


*Q1 — Panel surface:* The new demo panels — terminal programs, or browser
pages?

- Terminal panels — A C program per phase, drawing three regions to the screen
  and reading keys. It links against the real engine — the station table, the
  pool, the mutexes — so what you steer is the actual machine, and a lever
  that lies would show up as a wrong number. Costs: keyboard levers rather than
  clickable ones, and no shared look with the documentation site.

- Browser panels — HTML pages beside the documentation set and the workbench,
  sharing the ceramic palette, with real buttons and sliders. The engine is C
  and threads, so the page can only reach it by compiling to WebAssembly —
  which has one thread by default, meaning the panel would demonstrate a
  single-threaded port of an engine whose whole point is that it runs on every
  core.

- Terminal now, browser later — Build the seven terminal panels against the
  real engine. The browser version becomes part of phase 8, beside the
  workbench, where the WebAssembly build is already being paid for — and where
  a simulated map is already the plan, so a simulated panel would be honest
  there rather than a lie.

- Terminal engine, browser front — The panel runs as a real C process; a local
  page talks to it over a socket and draws the buttons. Real threads, real
  levers, ceramic aesthetic. Costs a wire protocol and a server, which is the
  one thing issue 801 says explicitly must never exist for the workbench.

→ **Selected:** Terminal engine, browser front


Your choice makes me correct something I said: I overstated the conflict with
the workbench. Let me check what that issue actually promises, because it may
already anticipate this.


That changes both issues. The workbench already plans a browser canvas that
draws stations and wires — but backed by a *simulator* that carries tokens
rather than values, explicitly "a picture of the traffic, not of the cargo."
Your choice gives that same canvas a real process to talk to. Let me fold the
decision in.


Now the implementation steps and the open questions, both of which the decision
changes:


Let me verify the page count, since I deleted files the generator is known not
to clean up.


Decision folded in, and it produced the most useful finding in the issue so far.


**The protocol needs no new engine surface.** Everything the page would show
going one way already exists: the whole shape is the map dump, which already
round-trips as a map file, and the motion is the statistics and growth
instruments. Everything the page would send going the other way is the
construction surface — add a station, configure a port, draw a wire — plus
writing a static and compiling a box. So the panel is a second caller of
surfaces that already have one, which is also the test of whether a lever is
legitimate: a lever that can't be expressed as a call somebody could already
make is demonstrating something the engine can't do.


That fixes the issue's place in the order. **A complete panel waits on *one way
to build a program***, because half its levers are that surface. Phase one's and
phase two's panels can be built before it against what exists today.


I also wrote in the piece that keeps the split honest: a panel process started
with nobody connected plays a recorded sequence of lever movements against
itself and writes a report. That's how `make test` catches a broken panel, and
it forces every lever to be reachable without a mouse — the recording is the
specification of the protocol, and the page is a second way to send what it
already sends.


One correction to what I told you: I said a browser front conflicts with the
workbench's "nothing on the server" rule. It doesn't. That rule is about work
leaving your machine — no account, no upload — not about two programs on one
machine talking over loopback. The real cost is smaller and different: a panel
is no longer one file you can open, because something has to be running first.


Next question, and it's the one your answer created:


**[Asked the user]**


*Q1 — Panel vs canvas:* The demo panel and the browser workbench draw the same
picture. Do they become one page?

- One page, two backings (Recommended) — One canvas that draws stations,
  wires, kinds and static values. Behind it is either the token simulator, when
  nothing has been compiled, or a live process, when there is a real map
  running. This is the project's own habit — one surface with several callers,
  the way loading a map file became the first caller of the construction surface
  rather than a mechanism of its own. The workbench gains a run that is real
  rather than modelled; the panel gets its drawing layer for nothing. Costs:
  phase 8 ends up owning a piece of phase 7's deliverable, and no panel ships
  until the canvas does.

- Two pages, shared drawing layer — Separate pages with separate purposes —
  one composes a map you download, one drives a map that is running — sharing
  the code that draws a station and a wire. Each ships on its own schedule and
  phase 7 keeps its own deliverable. Costs: the shared layer answers to two
  masters and drifts, and a visitor meets two pages that look alike and behave
  differently.

- Two pages, fully separate — No shared code. Fastest to start, and neither
  page can break the other. Costs: the hardest part of both — laying out a
  graph and drawing wires that stay readable when stations move — gets built
  twice, and the two will disagree about what a comparator looks like.

- One page, live only — The canvas always talks to a real process; the token
  simulator is never built. Simplest story and nothing is ever a model. Costs:
  you cannot watch a map you have not compiled yet, which was the workbench's
  whole appeal for someone with no toolchain, and the slideshow was promised
  that simulator to draw its screens with.

→ **Answered:** each demo should have a different page. It shouldn't re-use
any of the workbench stuff. The demos are for showing very specific machinery,
so we should display them with more of a "dynamic documentation" approach. Feel
free to use the HTML UI design skill for this part.". Read the answers carefully
— they may request clarification, changes, or that you not proceed — and
follow what they actually say.


One page per phase, purpose-built, no workbench reuse. That also answers the
panel-versus-selector question, and it connects to something already in the
project: the documentation set already carries three interactive pieces riding
the pages they explain — a ring buffer with a capacity slider, a readiness
check that fires on the click filling the last slot, an iterator dealing under
an adjustable slow consumer. The panels are that idea at full size.


Folded in, references all resolve. Nine questions left in the queue. Next one,
and it's timed rather than abstract — it affects code that's about to be moved
by four other issues:


**[Asked the user]**


*Q1 — Rename timing:* The source and the documents disagree about the two
commonest nouns in the engine. When does the rename happen?

- Before the port family — The documents say a port is the standing interface
  for one input of one station, and a slot is one place a value sits. The source
  says the opposite: a station's input ports are called slots, and the thing
  that is actually a slot is called a cell. Renaming first means the concurrency
  work and the construction surface get written in the right words and never
  have to be re-read. Costs: it renames code that four open issues are actively
  moving, so their diffs get noisier and any half-finished branch conflicts.

- After the port family lands — One deliberate pass over code that has stopped
  moving, which is what the rename issue itself asks for. Costs: four issues get
  built in the wrong vocabulary, and since their own blueprints already use the
  right vocabulary, whoever implements them reads one word in the issue file and
  types a different one into the source — which is exactly how the debt was
  contracted the first time.

- Build the rewriting tool first — The rename reaches the station header,
  delivery, statics, the loader, the dump, and a document's filename. The
  renumbering issue needs the same thing: a tool that changes a name and
  rewrites every reference to it across the tree. Build that once, then the
  rename is a command rather than a careful afternoon, and it stays cheap enough
  to run again whenever the next drift appears.

- Header and file format only — Rename just the names that leak outward —
  the station header's fields and anything appearing in the map file or the dump
  — and leave the internal spellings until later. Smallest surface, and it
  fixes the places where a reader meets the wrong word. Costs: the source then
  uses both vocabularies at once, which is worse than consistently wrong.

→ **Selected:** Before the port family


215 moves to the front, ahead of the concurrency and construction work. Let me
record that in the issue itself, since its own open questions section is where
the timing was left undecided.


Recorded in three places. Eight questions left. Three of them — the launcher's
interface, how much old demo prose comes back, and the honesty rule for
modelled-versus-live pieces — I'd rather answer while building the thing
rather than now, since they're all "decide when you can see it" calls. Three
more are small bookkeeping I can batch with recommendations. That leaves two
that change what gets built next; here's the first:


**[Asked the user]**


*Q1 — Generator order:* Does turning a map into generated code really have to
wait for the construction surface?

- Emit against today's calls, retarget later — The generator writes
  construction calls against the placement and wiring functions that exist right
  now, and the emitter is changed later to write the new ones. This is cheap in
  a way it would not be for hand-written code: the output is generated, so
  nobody maintains it, and retargeting means editing the strings one emitter
  prints and regenerating. The generator line then runs fully in parallel with
  the construction line instead of queueing behind the longest chain in the
  project.

- Hard dependency, wait for it — The generator is written once, against the
  interface that will still be there afterwards. Nothing is built twice and
  there is no moment where generated code calls something that is about to be
  deleted. Costs: the four issues that dissolve the registry all sit behind the
  construction surface, which is the largest single issue open, so two of the
  project's four lines of work stall on one.

- Emit through a named seam — The generator emits calls to a small fixed set
  of names — place this box here, point this port at that, draw this wire —
  which today forward to the existing placement and wiring functions and later
  forward to the construction surface. The emitter is written once and never
  retargeted; only what the names forward to changes. Costs: one layer of
  indirection that exists purely because two things landed in an awkward order.

- Pull the construction surface forward — Stop treating it as phase 2's
  capstone and build it next, ahead of the concurrency line, so everything
  waiting on it stops waiting. Costs: it is the biggest issue open and it stands
  on the port conversion, which stands on the rename we just moved to the front
  — so this is a longer detour than it looks.

→ **Selected:** Hard dependency, wait for it


Recorded in the child, the parent, and the phase page. Now the three bookkeeping
calls, batched as I said — all small, all with a recommendation:


**[Asked the user]**


*Q1 — Stale blueprint:* The blueprint for turning values back into text
describes a gap that has already been filled. What happens to it?

- Correct it and re-scope (Recommended) — Its current-behavior section says
  the engine cannot turn bytes into text. It can — a formatter walking the
  field table was built alongside statics moving onto ports, the map dump calls
  it, and floats already round-trip. Correct that section, and state what
  genuinely remains: emitting the formatter and the reader as generated code per
  struct with the escape rules written once. That remainder belongs beside the
  registry work, since that is what stops field tables being searched by name.

- Correct the section only — Fix the factual error and leave the scope exactly
  as written. Smaller edit, and it keeps the issue's shape intact for whoever
  picks it up. Costs: the steps list still reads as though nothing has been
  done, so the first hour of the work is spent rediscovering which of the seven
  steps already landed.

- Leave it alone — The house rule permits editing the current-behavior section
  of a partially complete issue, but it does not require it. Costs: the
  blueprint states something false about the engine, and it is the kind of false
  thing that survives because it looks like it was written on purpose.

→ **Selected:** Correct it and re-scope (Recommended)


*Q2 — Phase 8 pages:* Phase eight has no progress page and no demo issue,
unlike every other phase. Create them now?

- Progress page only (Recommended) — Every other phase has one and they are
  the live status pages the project reads from, so the gap is real. It would
  carry one row — the workbench — and say plainly that the phase has not
  started and what it waits on. The demo issue can wait, because the demos issue
  we just wrote already says phase 8 will want a panel and that it is the one
  whose demo is not a terminal program.

- Create both now — Full symmetry with the other seven phases immediately, and
  the phase's shape is stated before anybody starts rather than discovered while
  building. Costs: a demo issue written before the thing it demonstrates exists
  tends to describe the demo somebody imagined rather than the one that makes
  sense later.

- Neither until the phase starts — Nothing is blocked by their absence and a
  page that says nothing for a long time is a page people learn to skip. Costs:
  the asymmetry keeps being rediscovered, which is what just happened.

→ **Selected:** Progress page only (Recommended)


*Q3 — Demo history:* Seven completed demo issues now describe programs that no
longer exist. Mark them?

- One-line banner on each (Recommended) — The house rule says the project
  should be rebuildable by re-completing every issue in the completed directory.
  Left silent, somebody doing that builds seven paged demos and then deletes
  them again. A single line at the top of each — replaced by the panels, see
  the new issue — costs nothing and keeps the rebuild honest, while leaving
  every word of the original intact as the record of the first generation.

- Leave them untouched — They are accurate accounts of work that was genuinely
  completed, and a completed issue is a historical record rather than a live
  document. Costs: the rebuild-from-issues property quietly stops holding, and
  nothing says so.

- Banner on the two phase-7 ones only — Mark the demo-storytelling issue and
  the phase 7 demo, since those are where the demo practice was defined, and
  leave the five per-phase ones alone. Costs: an inconsistency that needs its
  own explanation, which is more words than the banner would have been.

→ **Selected:** One-line banner on each (Recommended)


All three accepted. Let me verify the actual state of the text formatter before
rewriting that issue's current-behavior section — I want the correction to be
accurate about which steps landed.


No escape handling on either side — so a string constant containing a quote
can't round-trip in either direction. That's a correctness hole, not just
restructuring. Rewriting the section accordingly:


Now the banners on the completed demo issues:


All eight decisions are recorded in the files they affect. Tests pass, the site
regenerates with zero unresolved references.


**What the answers changed, beyond the issue that prompted them:**


The rename moving first is written into three places — the rename issue itself
now carries the argument (the blueprints for the concurrency and construction
work already use the correct vocabulary, so building them against the old source
means translating every noun by hand, which is precisely how the debt was
contracted), the port family's parent says its children are implemented against
it, and phase two's status page marks it as going first.


The hard dependency on the construction surface is recorded in the child, the
parent, and phase three's page — including the alternatives that were refused
and why, so nobody re-litigates it: the emitter would have been cheap to
retarget, but the project's most-read generated file would have spent that
period demonstrating an interface the project had decided against.


**The stale blueprint turned out to be hiding a real bug.** Correcting it meant
reading the formatter, and there are no escape rules on either side — the
writer emits a string constant as its characters between two quotes, and the
reader has no notion of a backslash. A string holding a quote, a tab, or a high
byte does not round-trip in either direction, and nothing says so. That's not
the restructuring the issue was about; it's a value the engine will silently
corrupt. I split the issue accordingly: escaping and its round-trip tests now,
the move to generated code later alongside the registry work, and the three
steps that already landed marked so nobody does them twice.


Phase eight has a progress page. The eight completed demo issues carry a
one-line banner — seven saying the program was deleted and nothing there needs
rebuilding, and the storytelling one saying its contract was replaced rather
than withdrawn, since its finding survives per lever instead of per scene. I
wrote that as a small tool rather than eight hand edits, because eight hand
edits is eight chances to word it differently.


**One question left**, and it's the one the roadmap opened with in a different
form:


**[Asked the user]**


*Q1 — What's next:* Given all of that, what gets built first?

- The escaping hole — Small, self-contained, and the only thing on the list
  that is currently wrong rather than merely absent — a string constant
  containing a quote is corrupted on the way out and misread on the way back, in
  silence. Fixing it is one pair of routines sharing one table, plus the two
  round-trip tests that would have caught it. It touches nothing the other lines
  of work are about to move.

- The rename — Already decided to precede the port family, and everything in
  phase two queues behind it. It reaches the station header, delivery, statics,
  the loader, the dump, every error message, and a document's filename.
  Mechanical but wide, and the longer it waits the more code there is to rename
  — which is the argument that put it first.

- The panels, headless half — The lever vocabulary, the recorded sequences,
  and the runner that plays one and writes a report — no socket and no page.
  It gets the front door working again, and it is the step that keeps the design
  honest, since every lever must be reachable without a mouse. Phase 1 and phase
  2 panels can be built against today's engine; the rest wait on the
  construction surface.

- Boxes addressed by file — Starts the generator line, which then runs in
  parallel with everything in phase two for three of its four children. It
  decides how a box is named and how its symbol is spelled, and the other three
  are written against that, so it is the cheapest thing to do early and the most
  expensive to do late.

→ **Selected:** The rename


The rename it is. Before touching anything I need the full extent of it — this
is the kind of change where a missed reference compiles fine and means the wrong
thing.


Survey done, and the scope is worth stating before I start: **26 source and test
files**, and the rename has three distinct pieces that must not be confused with
each other.


Three distinct pieces, and conflating them is how a rename like this goes wrong:


| What the source says now | What it actually is | What it becomes |

|---|---|---|

| a **cell** in a ring buffer | one place one value sits | a **slot** |

| `slot_t *slots` on a station | its **input ports** | ports, named as input |

| `port_t *ports` on a station | its **output ports** | ports, named as output |


That's 493 occurrences of *slot* and 172 of *cell* across 26 source and test
files, 10 interface documents, 14 project documents, and a document filename
with every link into it. The middle row is the dangerous one: it renames a type
to a name that currently means something else, so a half-finished pass compiles
cleanly and lies.


One thing blocks me from starting, and it's yours to decide:


**[Asked the user]**


*Q1 — Commit first:* The demo work is sitting uncommitted — 45 files,
including 25 deletions. Commit it before the rename starts?

- Commit it first (Recommended) — One commit for the demo generation ending:
  the deleted programs, the new panel issue, the superseded markings, the
  launcher's corrected message, and the decisions recorded across the other
  issue files. Then the rename lands on a clean tree as its own commit. The two
  records stay readable, which matters more than usual here because a
  665-identifier mechanical diff mixed with 25 deletions is a commit nobody will
  ever read twice.

- Two commits first — Split the demo work further — one commit for deleting
  the old demos and writing their replacement issue, another for the decisions
  recorded across the port family, the registry family, and the text formatter.
  Those are genuinely separate concerns that happened to be decided in one
  sitting. Costs: one more commit boundary to draw, and the decisions are hard
  to describe without referring to the demo work anyway.

- Don't commit, keep going — Carry on and leave everything for one commit
  later. Costs: the rename touches files the demo work already modified, so the
  two become hard to separate afterwards, and reverting either one alone stops
  being possible.

→ **Selected:** Commit it first (Recommended)


That untracked transcript carries this session's own ID, so by the project's
rule it's this session's paperwork and goes in with the work. The ten *modified*
transcripts predate this session — I'm leaving those alone.


Clean commit, tree is clear apart from the ten transcripts that were already
modified when I started. Now the rename. Let me read what the issue specifies
for the new names before inventing any.


Let me enumerate every identifier the rename touches, so the mapping is decided
once rather than discovered file by file.


The list has traps: `report`, `support`, `proportion`, `exported` all contain
"port" as a substring, and `port0_sink` is a box function name in test fixtures
rather than engine vocabulary. Let me check the ambiguous identifiers before
deciding the mapping.


The collision is confirmed — the input-port word would land on a name the
output port already uses in the same file. That rules out sequential
substitution. I'll build the rename as a tool with two-pass placeholder
substitution, which the renumbering issue also needs later.


The tool needs to run on code and prose separately — renaming the station's
`ports` field must not turn the phrase "input ports" in a comment into nonsense.
Let me add root selection.


Now the mapping for code identifiers, with the input and output sides kept
strictly apart:


960 replacements, every mapping matched except one capitalized form which I'd
expect. Before applying, let me measure the prose damage — renaming the field
will mangle comments that use the word in a sentence.


The majority are prose. Renaming the bare plurals would mangle 116 comment lines
I'd then repair by hand — better to rename the field accesses directly and
leave the English alone.


Now the struct tags, which the identifier pass deliberately left alone, and then
a build to find what the rename broke:


Two structs now share the tag `struct port` — the input side landed on the
output side's tag, exactly the collision the two-pass scheme was designed for at
the identifier level but which struct tags sit outside of. Fixing both:


A struct member kept the old name while its access site was renamed. Let me find
every remaining declaration of the two fields:


Only the one struct is a real error — the pool's ring genuinely holds task
pointers in slots, so that name is now correct rather than wrong. Fixing the
rewire struct:


The compiler caught the collision I expected — an input-port parameter and an
output-port local both landed on `port`:


Caught rather than silently mis-delivered. The local is the *output* port chosen
for the task; the parameter is the input port being written. Renaming the local:


The pool's ring genuinely holds one task per slot, so that field was already
correct and my field-access pass over-reached. Reverting it there:


That error is a deliberate one — a test feeding the runtime compiler a box
that references an undefined function. Let me see the actual result:


All tests pass. Now let me check for shadowing the compiler wouldn't flag, and
whether the generator emits any of the renamed names — it lives outside the
roots I renamed.


The generator's only use is a local naming one element position in an array —
which is a slot under the new vocabulary, so it's now correct rather than wrong.
Now the shadowing check:


Code rename is mechanically clean. Now the prose inside comments — the word
"slots" meaning input ports is now actively wrong, since a slot is the thing a
value sits in.


Most of these are now correct — they were "cells" and became "slots" properly.
A few genuinely mean input ports. Let me check the ambiguous ones:


A few genuinely mean input ports — and one of them, the static port reporting
what "its slots were carrying," is now correct English where it wasn't before.
Fixing the four that are wrong:


Now the interface documents. Let me check whether the earlier pass already
reached them — they live under the same roots:


The pool is the risk here — its ring slots were correctly named all along, so
the vocabulary shift shouldn't touch them. Let me inspect what the tool did
there:


One wrong out of four — the task ring's size described in slots, which the
pass turned into "port". Fixing:


Let me review the header's diff, since it carries the most prose and is where a
wrong rename would do the most damage:


The vocabulary reads correctly throughout. Let me check the longer names haven't
pushed lines past the project's width:


Let me check whether those were already long before the rename, rather than
assuming I caused them:


Code side is complete and green. Now the documents. The rename issue says the
documents are right and the source was wrong — let me verify that before
touching them:


The documents don't match that claim — doc 002 uses "slot" for an input port
throughout its body, not just in its filename. The issue only anticipated
renaming the file. Let me run the pass on the documents with prose-appropriate
plurals:


The tool rewrote the filename inside every link as a side effect, which is
convenient but means the file must now actually move. First let me check one
line that looks suspicious:


The doc was already mixed — that line used "port" correctly before. But two
field rows now collide, both reading `ports`:


Now the file rename itself, tracked so both names exist in history:


Rename recorded as a rename, so both names are in history. Now every remaining
reference to the old filename — the documents pass didn't reach issues or
notes:


The blanket substitution damaged the rename issue's own sentence — it now says
the file "becomes" its own new name. Fixing:


The generator never deletes, so the renamed document's old page is still sitting
there with a live URL — exactly the fault the documentation issue describes:


Now the interface documents — the pass reached them, so let me verify it
didn't do to them what it did to the pool:


That dropped a phrase across the line break. Restoring it:


Now let me sweep every interface document for field names the pass left stale,
and check the error messages the issue calls the most visible surface:


Only two are wrong — the rest are now correct. And no user-facing message
still says "slot", so those renamed cleanly:


Green. Now the tool needs its interface document and an index, per the house
rules:


Now recording what actually happened in the issue, including the two places its
own premise was wrong:


The rename is done and committed. Two commits, clean tree.


**What it took:** 787 identifier replacements across 40 source and test files,
115 more across 14 documents, the station document renamed with every link and
every piece of link text followed, and the state-machine test renamed with it.
Build clean, all tests pass, and the word *cell* no longer appears anywhere in
the project. The sentence "a port has many slots" is now sayable — under the
old naming it couldn't be, because a slot had many cells and the word had come
to mean the thing that contains slots.


**Your issue got two things wrong about itself**, and both are recorded where
they were claimed:


*"The documents are right and the source is wrong"* was half true. Issue 210
states the vocabulary correctly and the newer documents follow it — but
document 002, which is the authority on what a station is made of, called an
input port a slot throughout its body. The step list only anticipated moving the
file. A document can drift from the vocabulary it defines, and the drift is
invisible precisely because everyone trusts it.


**The steps could not be followed in their order.** They rename the cell first,
then the two records separately. Done sequentially, the input port's new name
lands on the word the output port hasn't vacated yet — and the map file parser
holds one variable of each kind in the same file. All three renames had to be
one substitution. That's why this arrived with a tool rather than an afternoon
of editing: it substitutes every old name for a placeholder before writing any
new one, and matches whole identifiers only, so `report` and `support` didn't
quietly become something else. The renumbering issue wants the same tool.


**The compiler caught the one thing the tool couldn't** — a delivery function
where the input-port parameter and an output-port local both landed on `port`.
It failed to build rather than shadowing silently, which is the argument for
renaming onto a name already in use rather than onto a fresh one: collisions
surface at build time instead of becoming a variable that quietly means
something else. A shadow-warning pass afterwards found nothing more.


Three names were deliberately left alone because the change made them correct
rather than wrong: the pool's ring, which has held one task per slot all along;
the generator's array helper; and the ring-buffer test, whose title was wrong
under the old vocabulary.


**One question is open, so I haven't moved the issue to completed.** The code
and documents now agree — the blueprints don't:


**[Asked the user]**


*Q1 — Blueprints:* The issue files still speak the old vocabulary. The three
open children of the port family are titled and written around cells. What
happens to them?

- Rewrite the open ones only — The three open children — a state on every
  cell, the copies leaving the lock, growth adding a page — are instructions
  for work not yet done, and somebody following them will read "cell" and type
  "slot", which is the translation-by-hand this whole rename existed to abolish,
  running backwards. Rewrite their bodies and rename their files with the tool.
  Completed issues stay untouched as the record of what was built and when.

- Rewrite open and completed alike — One vocabulary everywhere, including
  history. Anyone reading the completed blueprints to understand how the engine
  was built gets the same words the code uses. Costs: it edits the record of
  what was actually written at the time, and the house rule treats completed
  issues as a buildable history rather than a live document.

- Bodies yes, filenames no — Correct the prose in the open issues but leave
  their filenames alone, so no links move and nothing has to be chased. Costs:
  an issue called "a state on every cell" whose body never says cell is its own
  small lie, and the filename is what people refer to it by.

- Leave them all alone — The issues are dated documents and their vocabulary
  is part of their date. Anyone building from them now has correct source to
  check against, which settles any confusion in one grep. Costs: the
  disagreement this rename closed reopens immediately, in the opposite
  direction.

→ **Selected:** Rewrite open and completed alike


One vocabulary everywhere. There's a trap in that, though: some issue files
deliberately quote the *old* names as history — the rename issue's own table
documents what the source used to say. Let me size it first.


480 replacements. Let me protect the rename issue's own historical table, then
apply:


The pass rewrote link targets inside filenames too, so the files must now move
to match — and two of them shouldn't have moved. Let me find the damage:


My check was too crude — let me use the generator, which resolves references
properly. First the file renames the pass implied:


Double-application damage — files I'd already corrected got renamed a second
time. Let me assess:


The blanket pass corrupted the vocabulary definition itself — turning "a slot
is one place a value sits" into "a port is one place a value sits". The issue
corpus isn't uniformly old-vocabulary: newer files already use the correct
words, so renaming them a second time destroys them. Reverting:


Now I need to classify, because the corpus is mixed. Let me see which issue
files use which vocabulary:


I hit a real complication and should state it before spending more time: the
issue corpus is written in **two** vocabularies, not one. Files written after
the port record established the correct words already say "port" for an input
and "slot" for a place — so a single pass renames those a second time and
inverts them. That's exactly what corrupted the vocabulary definition a moment
ago. Let me classify properly:


Even per-file classification isn't clean — three files use *both* vocabularies
internally. This can't be done in one mechanical pass without corrupting
history, which is what just happened. Let me extend the tool to work on an
explicit file list so I can treat the two groups differently:


Now the safe half — the files already written in the correct vocabulary, which
only need the ring position renamed:


The safe pass is correct. Now the parent issue, which is one of the mixed files
and has to be done by hand — its definition uses both words in one sentence:


Two sentences that deliberately quoted the old word got renamed with everything
else. Fixing them and then the file rename they now point at:


The safe half is done and every reference resolves. Now the old-vocabulary half,
which needs the combined pass. Two files need special handling, so let me take
the unambiguous ones first:


Now the two internally-mixed files, which have to be read rather than
substituted:


407's old-vocabulary uses are all inside a verbatim quoted note, so they stay.
But the project's own instruction file states the one rule in the old vocabulary
— that has to change:


Now the file renames the passes implied, and a full link sweep:


That one's correct — it's the rename issue itself. Final verification sweep:


Two things outside the roots I swept. The generator's remaining uses are HTML
table cells, which are legitimately cells — but a strategem document still
uses the old word:


Now let me record what this second half taught, since two of the rename issue's
premises turned out wrong:


That progress row is the double-renamed one I spotted earlier. Rewriting it, and
marking the issue complete:


That's mine and accurate, though it reads awkwardly. Polishing:


Done. Three commits, clean tree, all tests pass, every documentation reference
resolves.


**The rename is complete and the issue is closed.** One vocabulary across
source, documents, interface files, issue files, the strategem notes, and the
project's own statement of its single rule — which had still been describing
an arrangement the source abandoned earlier in the session. The word *cell*
survives in exactly three places, all deliberate: two historical sentences that
say what the source *used to* call things, and the HTML generator, where the
remaining uses are `<td>` table cells and belong to HTML rather than to this
project.


**The part worth telling you about is the mistake.** My first pass over the
issue files was wrong and I reverted it. The corpus is written in two
vocabularies, and a few files in both — everything predating the port record
uses the old words, everything after already uses the new ones. One mapping over
all of them renamed the second group a second time, and turned *"a slot is one
place a value sits"* into *"a port is one place a value sits"*. It destroyed the
single definition the whole rename existed to make true, in the very issue that
states it.


That failure mode is worse than a compile error and I want to be plain about
why: a bad rename in source stops building. A bad rename in prose produces
well-formed sentences that read naturally and say the wrong thing, and nothing
catches it but a person reading. I only caught it because I went looking at the
definition specifically. The tool gained a mode that works from an explicit file
list, and the interface document now says why — when a corpus speaks more than
one language, the substitution that's right for half is silent damage to the
other half.


**Two categories I deliberately left in the old words.** Verbatim quoted notes
— the gathering-at-pickup issue's argument survives only in the note that
prompted it, and a note edited to agree with a later decision stops being
evidence of what you actually asked for. And the rename issue itself, which
keeps both vocabularies, because an account of a change that speaks only the new
words can't say what changed.


**Your issue was wrong about two of its own premises**, both now recorded where
they were claimed: the documents were not all correct — the station document's
body used the old words throughout, not just its filename — and the
ring-buffer issue's title was not "already correct", it read *Ring-buffer input
slots* and was renamed with the rest.


The next thing on the list is the concurrency line — the copies leaving the
lock, which closes the state-machine issue with it, and which is now written in
the same words as the source it will modify.


--------------------------------------------------------------------------------

### User Request 3

okay sure let's keep working


--------------------------------------------------------------------------------

### Assistant Response 3

Next on the line is *the copies leave the lock*, which closes the state-machine
issue with it. Let me read the blueprint before touching anything.


Steps 1–3 are done; 4–8 remain. Let me read the current claim path.


Before I write code — the claim copy moving outside the lock reads a pointer
into the port's storage, and growth still reallocates that storage. Let me check
the paging issue, because I think the order here is wrong.


Confirmed, and it changes the order. The paging issue says plainly that the
existing growth **stops being correct** once the copies leave the lock, and that
the two "cannot be separated by very long." But the dependency actually runs the
other way round from how the family is sequenced:


- the scan (already built) is what makes paging possible

- paging is what makes the copies leaving the lock *safe*


Building the copies-out first would knowingly introduce a real race — a
claimer copying outside the lock while a grower reallocates the storage
underneath it. So I'm taking the paging first. Let me read its steps.


Now I have the full picture. Let me find everything that touches the storage or
capacity.


Implementing the paging. First the header:


Now the slot addressing, which becomes a page walk:


Now the growth path in delivery, and the scan that must walk pages rather than
index arithmetic:


Dead code behind `#if 0` isn't this project's style — the comment already
records what the old growth was and why it went. Removing it:


Now the scan, which must follow page links rather than do index arithmetic —
this is where paging's cost lands:


That still re-walks the pages inside the state move, defeating the purpose. Let
me split the state transition so it can work from an address:


Now the scan can work from addresses, and the teardown paths need updating:


All green with paging. Now the buffer report, whose numbers changed meaning:


The shout threshold was calibrated for doubling — under paging the same
backlog produces far more growth events, so the number means something
different:


Now the test the issue asks for — growth deep while readers and writers are
both working the port, which is exactly the failure the old ordering produced:


Now the test. Let me check how a test defines its own box and does
multi-threaded delivery:


Now I have everything. Writing the test:


Passing. But it emits a shutdown warning that a reader would mistake for a
failure — the test deliberately builds a huge backlog. Let me announce it:


Two output artifacts: the header is printed twice (the Makefile already prints
it), and stdout buffering makes the warning appear before the line announcing
it.


The apparatus sets a starting depth large enough that the port never grows, so
paging isn't on that measurement's path — worth stating rather than implying.
Let me check the strategem note, which anticipated this pattern:


Now the interface documents and the stations document:


Now the issue itself — recording what was built, and the ordering finding that
made me build it before its sibling:


Now correcting the parent's account of the order, since building it is what
revealed the dependency runs between halves of one issue:


The parent's "current behavior" still describes the fields as they were two
issues ago. Correcting:


One of the phase's established statements is now false — nothing is
reallocated any more:


That transcript carries a different session ID — it's another session's, not
mine, so I'm leaving it. Let me check my own and commit:


Committed and clean. **Growth by paging is done** — that's the fifth child of
the port family closed.


**What changed in the engine:** a port's slots now live in a list of equal-sized
pages, and growth appends one. Nothing is copied and no slot that already exists
moves. The scan resolves a page once and then follows links, so a sweep costs
one pointer hop per page boundary rather than a page walk per candidate —
which meant the state transition gained a form taking an address rather than an
ordinal, since the scan already holds the address.


**The reason I built this before its sibling is the finding worth reporting.**
The family's stated order is states → copies-leave-the-lock → paging. The
first link holds. The second is backwards, and it took trying to build it to see
why:


- the *scan* that the copies-issue introduced is what makes paging possible

- but the *copies leaving the lock* are only safe once paging has landed


A worker copying a value out of a slot it has claimed holds no lock — that's
the whole point — and reallocating that slot underneath it is precisely the
thing ownership doesn't protect. Built in the written order, the engine would
have carried a real race for however long the two were separated. The paging
issue already said the old growth "stops being correct" and that the two "cannot
be separated by very long"; they don't need separating at all. **The dependency
was never between two issues — it was between two halves of one of them.**
That's now recorded in the parent, the phase page, and both children.


**Two numbers changed meaning**, and I moved one rather than leaving it: the
buffer report counts pages instead of doublings, and its shout threshold went
from four to sixteen — the same backlog restated, since four doublings meant
sixteen times the starting depth. Left at four it would have fired whenever a
consumer was briefly slow, and a warning that cries wolf costs more than it's
worth.


**The new test drives the failure the old growth actually produced** — 900
distinct values piling onto one side of a station until it's 129 pages deep,
then draining while the other side opens; each must come out exactly once, which
says "nothing lost" and "nothing doubled" in one assertion. A second scene
checks the addressing directly, because a page boundary is where that breaks and
a concurrency test would only catch it by luck.


Two things I should flag rather than let pass:


**The fan-in measurement is unchanged, and that is not evidence.** The apparatus
sets a starting depth large enough that its ports never grow, so paging isn't on
the path it measures. What paging costs is a page walk on a port deep enough to
have several, and that apparatus deliberately has none. I've written that into
the issue rather than let the flat number imply "no cost".


**A transcript appeared that isn't mine** —
`llm-transcripts/aug-20-26_agent-1.md`, carrying a different session ID. I left
it untracked rather than sweeping it into my commit.


Next on the line is the other half of the copies issue: check-all-then-flip-all
under one hold, with the claim's copies moving outside the lock — which is now
safe to build.


--------------------------------------------------------------------------------

### User Request 4

okay let's keep going. Also you can sweep that transcript file into your next
commit.


--------------------------------------------------------------------------------

### Assistant Response 4

Next is the other half of that issue. Let me read the two call sites that will
have to change.


Now implementing the claim's copies leaving the lock. The scan already holds the
slot's address, so let me have it hand that back rather than re-walking:


Write path is updated. Now the claim side — replacing the pop-and-release with
a take that only flips, leaving the copy for outside the lock:


Now the dispatch table and the walk, which must record what it took rather than
copy it:


Now the two call sites:


Claim copies are out of the lock and green. Now the write side, which needs the
page links to be safe for a reader while another thread appends:


The header's fold comment now sits above the wrong struct, and its field list is
two issues stale. Let me fix both:


Now the scan snapshots the capacity once, which is what bounds the sweep, and
the writer stops taking the lock:


All green with a lock-free writer. Now the static-write path, which the issue
says must be one hold rather than two acquisitions:


Now the per-candidate compare-and-swap, which is what the measurement blamed.
Under the lock a claimer is the only thing that can touch a ready slot, so it
doesn't need one:


Now wiring the scan to use it, with the exclusive path for claims:


Substantial: large values roughly halved (≈1180 → ≈680 ns), small values
improved too. Let me get a better sample and check what the test says about
itself:


Now correcting the header's claim that every transition is a compare-and-swap,
which is no longer true:


The guarantees page already describes the target state — it was written ahead
of the code. One clarification is worth adding, since a reader could think the
copies are inside the hold too:


Now the interface documents, then the issues themselves:


Now the two issues this closes. First recording what was built and measured:


All three transcripts staged — yours to sweep, mine, and one spanning today
that was already modified. Committing:


Committed, tree completely clean. **The concurrency line is finished** — three
of the port family's children closed in this stretch.


**What the engine does now:** a station's mutex covers the slot states and
nothing else. The write path takes no lock at all — reserving a slot is one
compare-and-swap and the copy that follows goes into bytes the writer owns. The
locked section is a search and one state mark per port, with no `memcpy` inside
it. The copies out happen after the lock drops, in parallel across every worker
doing the same thing to the same station.


**Measured, six runs either side:** large values (200-byte structs, four ports)
fell from ≈1180 ns per delivery to ≈620. The small-value control fell from
≈470 to ≈400 — that was the number worth watching, because the scan had
pushed it *above* its original figure and the question was whether this brought
it back under rather than merely back to it. The ratio between the two runs fell
from 2.69 to ≈1.5, which is the copy's share of a delivery shrinking because
it now runs alongside rather than in turn.


**Three findings worth your attention:**


**The design had an exception it hadn't noticed.** The plan said both copies
leave the lock. A *static's* copy can't — and it fails for exactly the reason
the others succeed. Ownership is a claim about *slots*: reserved and claimed
mean one worker owns this and nobody else may touch it. A static is peeked
rather than taken, so nothing owns it, and the mutex is the only thing between a
claim reading that constant and somebody writing it. Moving it out would have
reintroduced precisely the torn read that putting the value on the port was
meant to make impossible — the one the statics test hammers four thousand
times. So that copy stays inside, and the ownership argument is now stated as
what it is rather than as a blanket.


**The phase's own account of itself was wrong.** It said copying the values out
before releasing the lock was what let two invocations of one station run at
once. It wasn't — the exclusive claim was. The copying was doing none of the
work. Once that was visible the copy could leave and the guarantee didn't move,
because it had never rested on it. Corrected in the phase page and the delivery
document.


**Only one of the four slot transitions needs a compare-and-swap.**
Empty→reserved is where two writers genuinely race. The other three have
exactly one possible mover — an owner moving its own slot, or a claimer under
the mutex taking a ready one, which nothing else can touch. That matters because
the search asks its question of *every candidate it walks past*, so a
read-modify-write per candidate was measurable where a load isn't. That was most
of the small-value regression.


The transcripts went in, including the one from the other session, and I noted
that in the message.


What's left in this family is the two that branch off the port record rather
than off the concurrency work: converting what a port is, and the construction
surface — which is phase two's capstone and the thing three other issues are
waiting on.


--------------------------------------------------------------------------------

### User Request 5

okay let's keep going


--------------------------------------------------------------------------------

### Assistant Response 5

Next is converting what a port is — the small one that unblocks the
construction surface. Let me read it.


Most of this is already built; what's owed is two tests and a note. But first
— the lock-free writer I just built may have opened a race with conversion.
Let me check the delivery guard:


The check was already outside the lock before my change, so this race pre-dates
it — but `kind` is a plain byte read while conversion writes it, which is a
formal data race. Let me see how widely it's read:


Only two of those reads actually race — the guards in the delivery entry
point, which run with no lock. The narrow fix is to make the field atomic; on
the hot path a byte load stays a byte load. Let me do that and then measure to
confirm:


No regression — same band. Now the two tests the issue still owes. Let me find
where the existing conversion test lives:


Now the two owed tests:


The binding call takes text and needs a registry type name, so hand placement
can't do it — the tests must place by name. Let me rewrite them:


Both pass, but the cycling test emits a growth warning and its assertion is weak
— it should also prove the station actually ran rather than sitting wedged:


Now the note the issue owes the guarantees page — a second reason for a
non-guarantee it already carries:


That note is already there — step 5 was done. But it cites a roll-back path
that no longer exists:


Now closing the issue and updating the phase page:


Committed, clean. **The port family is down to one child.**


That issue was mostly built already — what it owed was two tests and a note,
plus one thing that turned up while looking.


**The tests.** A port takes six values, becomes a constant, becomes a buffer
again, and every value is still served. That's the case a tidy-looking
implementation gets wrong: freeing the waiting values when a port stops being a
buffer looks like cleaning up, and is actually discarding work a producer
already handed over with nobody ever told. The second cycles a port through all
three states six thousand times while a feeder floods its sibling — asserting
both that no run was conjured from values nobody delivered *and* that the
station actually ran, since a station wedged by the cycling would satisfy the
first check by doing nothing, which is how a test comes to pass by testing
nothing.


**The thing that turned up.** A delivery reads a port's tag in two guard checks
*before taking any lock*, while conversion writes that tag under the station's
mutex. Plain read against plain write is a data race regardless of the values. I
made the tag atomic — no measurable cost, confirmed against the fan-in
figures, which stayed in the same band.


Worth being precise about blame: **this predates the concurrency work rather
than being caused by it.** Those guards were always outside the lock; the
lock-free writer didn't move them. It only becomes visible once you go looking
at what a delivery does while holding nothing.


And the reason nothing more is needed: **losing that race is harmless.** A value
written into a port that just became a constant lands in storage that exists
whatever the tag says, and waits until the port is a buffer again — the same
promise conversion already makes about values it finds. Refusing the delivery
would be wrong for the same reason destroying the values would be.


**One correction while nearby:** the guarantees page blamed out-of-order
delivery partly on "a claim that rolls back." There's no such path any more —
the claim checks every port before taking from any, so a partial claim can't
exist. The scan is the reason on its own, and a conversion is the second.


What's left in the family is **one way to build a station** — the
configuration surface — and that's what phase two's capstone stands on, which
in turn has three other issues waiting behind it.


--------------------------------------------------------------------------------

### User Request 6

Okay let's keep going


--------------------------------------------------------------------------------

### Assistant Response 6

Next is the last child — one way to build a station. Let me read it.


This one is gated, and I want to state that before I pick something else. Its
first step — hand placement becoming the primitive — is explicitly *"built
in 311b; this issue is its caller rather than its author"*, and that sits in the
generator line, unbuilt. Let me confirm the two placement paths really do differ
as described:


Confirmed — the two contracts differ exactly as described, and unifying them
is the generator's job. Let me record that this is gated, then take the head of
the generator line instead.


This one has the same shape of tangle — worth checking before I start. Its
step 1 puts map parsing "in the generator, which is now its only home", but the
generator doesn't read maps until the *fourth* child, which itself waits on the
construction surface. Let me see what the generator knows about box files today:


Found a genuine gap in this one, and it changes what gets built. The escaping
table transcribes `.` and `_` so that `math.c` and `math_c` can't collapse to
one symbol — but it has **no rule for the path separator**, while the design
relies on paths to settle ties between two files sharing a basename. Those two
can't both be true: if a symbol derives from the basename alone, two `math.c`
files in different directories produce the same symbol and the linker rejects
them, so a path never settles anything.


**[Asked the user]**


*Q1 — Symbol source:* A box's generated symbol has to be unique. What is it
derived from?

- The full path, with a rule for the slash (Recommended) — The symbol comes
  from the file's path relative to the project root, canonicalised first so that
  a map naming a box briefly and a map naming it by path produce the same symbol
  rather than two. The escape table gains one row for the separator, alongside
  the dot and the underscore. Two files sharing a basename can then both exist,
  addressed by path, which is what the issue's own paragraph about brief
  addressing assumes. Costs: longer symbols in the generated file, which nobody
  reads by hand.

- The basename, and basenames must be unique — The symbol stays short and
  comes from the basename, as the table shows. Two box sources sharing a
  basename become a build error anywhere in the tree, whether or not a map
  mentions them. Simple and easy to explain. Costs: the "a path settles ties"
  half of the design goes away entirely, because there are no ties left to
  settle — the issue would lose one of its stated behaviours rather than
  gaining a rule.

- The basename, plus a disambiguating suffix — Short symbols normally, and
  when two files do share a basename the generator appends something that
  distinguishes them — a counter, or a hash of the path. Keeps both halves of
  the design working with the shortest symbols. Costs: the same file can get a
  different symbol depending on what else happens to be in the tree, so adding
  an unrelated box source can change an existing box's symbol, which is the kind
  of action-at-a-distance this project has avoided elsewhere.

→ **Selected:** The full path, with a rule for the slash (Recommended)


Full path with a slash rule. That surfaces two more characters this project
actually uses — the box sources are named like `029-demo-boxes.c`, so hyphens
and a leading digit both appear. Let me look at where the generator keeps its
text utilities:


Now the escaping scheme. It has to be total — this project's own box sources
contain hyphens and start with digits:


Builds clean. Now the round-trip test the issue asks for, with the awkward names
it names plus the ones this project actually contains:


The new checks ran but print nothing — the file's idiom is one summary line
per group. Let me match it:


The escaping is built and proven — 48 awkward names, 48 distinct legal
identifiers. That's the piece the rest of the family needs. Let me record
honestly what remains and why:


Committed, clean tree, all tests pass. This stretch was mostly **finding that
two issues were gated in ways their plans didn't say**, and building the piece
that ungates them.


**The construction surface can't go next.** Its first step — hand placement
becoming the primitive — is explicitly *"built in 311b; this issue is its
caller rather than its author."* Two placement paths exist today: one takes
element sizes and no type names, the other draws everything from the registry,
and the first therefore can't bind a constant. Unifying them by hand would mean
writing a third contract the generator is about to replace. So the order for
that corner is: addressing → placement functions → the surface → the
capstone. Recorded in both the issue and the parent.


**The addressing issue had a real gap.** Its escape table transcribes `.` and
`_` so `math.c` and `math_c` can't collapse into one symbol — but it has no
rule for the path separator, while the design leans on paths to settle two files
sharing a basename. Both can't hold: if a symbol comes from the basename alone,
two `math.c` files produce one symbol and the linker rejects the build, so a
path settles nothing. You chose the full path, canonicalised so that naming a
box briefly and naming it in full produce the *same* symbol rather than two
definitions of one function.


**Two more characters turned up, and this project's own sources are why.** The
box sources are named like `029-demo-boxes.c` — a hyphen and a leading digit.
So the table gained a hyphen row, and every symbol carries a fixed prefix, which
handles the leading digit and keeps generated names clear of anything a box
author writes for one decision. Plus a catch-all hex row, so the scheme is
**total**: a character nobody anticipated gets transcribed rather than dropped,
and dropping is precisely how two files quietly become one symbol.


**Tested by property, not by spelling** — because a bug here doesn't surface
here. It surfaces as a duplicate-symbol error hundreds of lines away in
generated code, naming a mangled identifier instead of the two files that should
have been distinguished. Forty-eight awkward names, every pair chosen to collide
under a naive mangling, give forty-eight distinct legal identifiers.


**The map-file half is deliberately not built.** A station line carrying
`file:function` is a rule applied when the generator reads a map, and the
generator doesn't read maps until the fourth child of that family — which
waits on the capstone, which waits on the surface, which waits on placement
functions. Building it now would put it in the runtime loader those steps
delete.


Next in the chain is **placement instead of records** — the generator emitting
a placement function per box, which is what the surface is waiting on and what
now has its naming scheme.


--------------------------------------------------------------------------------

### User Request 7

Okay let's keep going


--------------------------------------------------------------------------------

### Assistant Response 7

Now let me look at the emitter to see where a record is written, so the function
can be emitted beside it:


Now I have everything. Emitting a placement function per box, alongside the
record so both can be compared:


Two lines came out malformed — let me fix the return size and the array
terminator:


The generator didn't rerun — no box source changed. Forcing it:


`make` alone runs `describe`, not a build — that's why nothing regenerated.
Using the real target:


Two real problems. The symbols contain the **absolute build path** — the
design says canonicalised, and I let the generator use what the Makefile handed
it. And nothing references the functions yet, which `-Werror` rejects. Let me
check how the runtime compiler invokes the generator before changing its
arguments:


Let me add the root so paths canonicalise, and emit the table that both makes
the functions reachable and is what by-name placement will call:


My substitution landed in the wrong function. Let me look:


The table block matched an earlier function's tail. Let me revert that file and
redo it in one carefully-anchored pass:


The runtime-compiled box now calls into the engine, and a dlopened object can't
see the host's symbols unless the host exports them:


The binary wasn't relinked — make doesn't track the Makefile as a dependency:


All tests pass. Let me look at what the generator now emits:


Exactly the shape the issue sketched, every size a `sizeof`. Now the test that
proves the translation faithful — cheap while both paths exist:


16 boxes identical field for field, 12 as comparators too. Now routing placement
through the function — but a box compiled at runtime also has to be placeable,
so let me see how those register:


A late block will need to carry its placement rows too. Let me see how the
existing lookup reaches them:


Now routing placement through the functions, with late boxes carrying theirs the
same way:


The file declares the late lookup locally rather than including that header —
let me match its idiom:


Now routing by-name placement through the function, so the record stops being
read:


Steps one through three are done and green. Let me record where this stands and
commit — the remaining steps delete the record, and that can't happen until
the generator reads maps.


Committed, clean, all tests pass.


**The generator now places boxes rather than describing them.** A box's record
was read exactly once — when a station was built from it — and never again,
since the station keeps its own copy of everything. So the record existed only
to be read at the one moment generated code could do the writing instead. Now a
generated function per box builds the station directly, every number a `sizeof`
the compiler folds into an immediate and stores nowhere.


**That settles the question the port work left open.** Stating a station's shape
by hand isn't a second, weaker door into the engine — a placement function
*is* hand placement, written by the generator instead of a person, and naming a
box was only ever a way of finding which one to call. That's what the
construction surface was waiting to hear.


**Proven before trusted:** every box placed both ways and compared field for
field, as a plain station and as a comparator wherever a box can be one. Sixteen
boxes, twelve of them comparators, identical. That comparison is only cheap
while both sides exist, which is why it happened now.


**Two things nobody foresaw, both found by building:**


Generated code that builds stations needs the engine's symbols **exported from
the executable**. A box compiled at runtime arrives as a loadable object, and
such an object can't see a symbol the host never published. This never came up
while generated code held only call sites — a call site calls the box, and the
box is in the object beside it. It appeared the instant generated code started
building stations. Anyone linking a program with this engine inherits that
linker flag, and the failure it produces without it points at a function in the
engine rather than at anything about the box, so I wrote it into the packaging
note.


A symbol built from a file's location has to use a location that **means the
same thing everywhere**. The build hands the generator absolute paths, so the
first symbols carried the entire directory layout of the build machine —
enormous, and different per machine, which makes two generated files impossible
to compare. The generator now takes the project root and shortens against it.


**One correction to the plan:** step 1 said emit the functions with nothing
routing through them yet. That can't compile under warnings-as-errors — a
function nobody calls is an error. So the lookup table that by-name placement
needs anyway arrived in the same step.


The deletions this was heading toward — the record type, the table, the
type-name strings — wait on the step that stops a running program resolving
names at all. While something still looks a name up, something has to be there
to look in. That's recorded in the issue rather than left to be rediscovered.


--------------------------------------------------------------------------------

### User Request 8

Ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 8

Next is the name literal on the station, which removes the dump's backwards
lookup. Let me see how the dump handles a shim it can't name today:


The rewire's wire check is the last user, and it reaches through the backwards
lookup for two things the station nearly has already — its return size, which
it does have, and its return type name, which it doesn't:


Now the backwards lookup has no users left — deleting it and its late-box
half:


The backwards lookup is entirely gone. Next is the by-name struct search — let
me see where it happens:


Step 4 is complete. Let me update the documents that describe the deleted
lookup, then commit:


Committed, clean, all tests pass.


**A station is now told its own name rather than being searched for.** Writing a
program back out needs each station's box named, and that worked by taking the
station's call site and scanning every box the build knew about for a matching
one — the description table read backwards, a linear search answering a
question the station could simply have been told. The generated placement
function now writes the name on as it builds the station, and the dump reads a
field. One pointer per station, of text the compiler was going to emit anyway.


**The backwards search had a second caller nobody had listed.** It only surfaced
when I removed the known one and the function refused to disappear: the check
that refuses a wire between mismatched types on a *running* program was reaching
through it to a box description for two things — how many bytes the box
returns, and what that type is called. The station already knew the first. The
second is now a literal beside the name, so a refused wire still reads *"box
returns int (4 bytes), port takes double (8 bytes)"* rather than four bytes
against eight, which names no fix. Both halves of the search are gone — the
one over compiled-in boxes and the one over boxes compiled while the program
ran.


**A station built by hand has no name to write, and the dump says so** rather
than inventing a spelling. That's the honest answer: nothing on disk describes
such a program either. The marker it writes is deliberately not a legal box
name, so a dump carrying one can't be read back in silence.


Where that issue stands: four of its seven steps are done. **Step 5** —
pointing ports at their struct field tables instead of searching for them by
type name — is buildable now and is the last one that doesn't wait on
anything. **Steps 6 and 7**, deleting the record type and the table outright,
wait on the generator reading maps, because while a running program still
resolves a name there has to be something to look in.


--------------------------------------------------------------------------------

### User Request 9

Um we explicitely chose to not store type names. If they require the same number
of bytes, they should be compatible. The reason for this is because separate
boxes might name types differently, but be referring to the same kind of data.


--------------------------------------------------------------------------------

### Assistant Response 9

You're right, and that's a real regression against a decision the design states
outright: *type and argument names are not carried; the engine runs on widths,
and a message reads names out of the box source the binary carries.* I added a
type name to the station, which is exactly the thing being removed — and for
the weakest possible reason, a string in an error message.


The comparison itself is fine: it's `from->out_size != dest->elem_size`, widths
only. Only the spelling carried alongside it was wrong. Let me take it back out.


Now the spelling is fetched only when a wire is actually refused, rather than
carried by every station:


Message intact, and the test that proves differently-named types of equal width
still connect passes. Now correcting the documents where I wrote the wrong thing
down:


One type name is still carried, and it's worth distinguishing from the one I
just removed — it's used for a value's *shape*, not for comparing wires:


Fixed and committed. Clean tree, tests pass, and the width-wiring test still
shows *"identical layouts, identical bytes, different names"* connecting — the
property I'd have undermined.


**What I got wrong:** I put the return type's spelling on every station so a
refused wire could name both sides. That's carrying a type name in the engine,
which the design removes precisely because two boxes written by two people will
spell one shape differently and mean the same data — comparing names would
refuse a sound connection. And I paid for it to improve a sentence that only
appears when somebody has *already* made a mistake, which is the worst version
of that trade.


**What was right and is unchanged:** the check itself compares `from->out_size`
against the destination port's `elem_size` — widths only. Only the spelling
carried alongside was wrong.


**The fix:** spellings are fetched when a wire is actually refused and nowhere
else. The message still reads *"box returns int (4 bytes), port takes double (8
bytes)"* rather than four bytes against eight, which names no fix. A refusal is
rare enough to pay for its own message, and when the box records go this reads
the name out of the box source the binary carries — the exact text that was
compiled.


**One type name genuinely remains**, and I've written the distinction beside it
since I just proved it's easy to miss: the port's `type_name` is what a
written-out constant is read through. Turning `{ 5, 2.0, "hey" }` into bytes
needs to know which field sits at which offset — that's a question about
*layout*, not a comparison against anything. Nothing is ever matched against it.
And it's on its way out too: that's step 5 of the same issue, where a placement
function hands the port its field table directly instead of the port searching
by name.


That step is the next unblocked piece of work, and it now has a sharper reason
than "tidying" — it's the last name-based search on the placement path.


--------------------------------------------------------------------------------

### User Request 10

Ok let's keep going.


--------------------------------------------------------------------------------

### Assistant Response 10

Step 5 — pointing ports at their field tables instead of searching by name.
Let me see how the structs are emitted and who does the searching:


Both callers have the port in hand. Let me see everything `type_name` is used
for, since classifying by name is the same shape of problem:


Precise scope: the *search* goes, not the name. Let me check the ports are
zeroed so an unset pointer is null:


Ports are `calloc`'d so an unset pointer is null, and the emitter already has a
struct-index helper. Let me check its shape:


Now the reader follows that pointer instead of searching:


The placement function sets it correctly, so the failure is elsewhere. Let me
find which scene:


Output was buffered and lost on abort, so the failing scene isn't the first. Let
me look at the struct one:


The test hand-places a station and grants it a type *name*, relying on the
search I just removed. That's the honest consequence — hand placement has no
field table — so it must now grant the layout too:


Now recording the step and the limit it made visible, then committing:


Committed, clean, all tests pass.


**A struct port is now handed its field table rather than searching for one.**
Reading a written-out constant needs to know which field sits at which offset
— a layout question, not a compatibility one. It used to take the port's type
name and search every layout the build had emitted for a matching name. The
placement function knows the type concretely, so it writes the address and the
reader follows it. One fewer search on a path with no business searching: the
answer was known when the station was built and was being recomputed later from
a string.


**It brought an old limit into view rather than creating one.** A station whose
shape is stated by hand is never told any types. Granting it one used to mean
setting a spelling and letting the search do the rest; granting one now means
two things — the spelling, which messages and the dump read, and the layout,
which is what reading a constant actually needs. A hand-placed port given only a
name is a port whose constant can't be read.


That was always true — hand placement has never been able to bind a
written-out constant, for exactly this reason. What changed is *where it becomes
apparent*: at the port, rather than being quietly covered by a search that
happened to find something. One test relied on the old behaviour; it now grants
both and says why in the place somebody would look.


**A note on scope, since it's the same distinction I got wrong earlier:** the
search that went is the one on the *reading* path. `struct_find` itself stays a
little longer for callers holding only a name — it goes with the box
descriptions, when nothing in a running program resolves a name at all.


That issue is now five of seven. **Steps 6 and 7** — deleting the record type,
the table, the parameter arrays and the type-name strings, then removing the
word *registry* from the source — all wait on the generator emitting
construction calls, because while a running program still resolves a name there
has to be something to look in. And that step waits on the construction surface,
which is now unblocked by the placement functions.


So the next genuinely available piece is **the construction surface itself** —
the last child of the port family, which three other issues sit behind.


--------------------------------------------------------------------------------

### User Request 11

Okay let's keep going


--------------------------------------------------------------------------------

### Assistant Response 11

Now the construction surface — its first step landed with the placement
functions. Let me read what configuring a port looks like today:


Now the one configuration operation. Let me see how the existing live-editing
surface returns a refusal, so the shapes match:


All green. Now the configuration-time check the issue asks for — every
parameter needs a source, and the complaint should arrive while somebody is
still looking:


The complaint says `?` because a program built by calling the surface has no
station names — those come from a map file. It should fall back to the index
rather than printing a question mark:


Committed, clean, all tests pass. **The port family's last child is unblocked
and half built.**


**One operation now says where a port's values come from.** Giving a port a
constant, taking its source away, and handing it back to the arrows were three
calls with three shapes — some stopping the program on refusal, some returning
a code — and what separated them was history rather than meaning. Whether text
is supplied distinguishes the two ways of becoming a constant: given some, the
port takes that value; given none, it returns to the value it held before, which
works because a constant survives conversion exactly as waiting values do.


The point isn't tidiness. It's that there's now **one description of what it
means to give a port a source, and it's executable** — which is what lets
reading a program from a file become a sequence of ordinary operations rather
than a privileged path that quietly does more than anything else can.


Toward that, it hands back a refusal instead of stopping, so a caller reading a
file can gather every mistake and show them together. **One thing still stops
the program and I've named it rather than glossed it:** text that won't parse
dies inside the reader. Moving that onto the same path belongs with the rest of
the refusal policy rather than being half-done here.


**Also landed: the completeness check.** A port with no source is the ordinary
state of a station nobody has finished wiring, so it's not an error during
assembly — it's an error the moment somebody calls the program finished.
Asking then lets the complaint name the station and the port while whoever
mis-wired them is still looking; left unasked, that station simply never runs,
and a program quietly doing less than it was asked is a miserable way to
discover a typo. Every missing one is named and counted, and there are no
exemptions — with a refused proposal on record explaining why there won't be
one.


One thing the test caught: the complaint printed `?` for a program built by
calling the surface, because station names come from map files. It now falls
back to the index, since a complaint saying "?" is one nobody can act on.


**What's left is the callers** — the loader becoming the first and runtime
editing the second, plus the test that a program read from a file and one built
by hand dump identically. That's the capstone's work, not this issue's, and it
can't be written until the loader is a caller. So the next piece is **one way to
build a program** itself, which is what three other issues have been waiting on.


--------------------------------------------------------------------------------

### User Request 12

Ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 12

Now the capstone. Let me read what it actually asks for.


Several steps are already done — the table growing, port configuration,
placement functions. The heart of what's left is the loader becoming an ordinary
caller. Let me look at what it does today that nothing else can:


The whole-program pass is the piece that makes "still loading" stop being
special. Let me build it as something a caller invokes repeatedly:


Now the loader becomes a caller of it rather than doing its own:


The loader sets the station names *after* validating, so the pass would print
indices. Let me reorder and route it through:


A test map deliberately contains a port with no source — that's the half-built
program the map format was taught to round-trip. Let me see what it expects:


Found a genuine contradiction between two issues, and it's worth resolving
rather than patching. Let me check what the capstone itself says:


The capstone governs: a port may hold no source indefinitely. So the check is a
*report*, not a refusal — let me correct it:


The old sweep refused a file where nothing could ever start. That policy belongs
to the caller, not to a repeatable pass — bringing up a grown program
legitimately seeds nothing:


All tests pass. Let me verify the loader still reports what it used to, then
record the contradiction I had to resolve:


Committed, clean, all tests pass. **The capstone is underway** — this is the
step that ends *still loading* as a state only one mechanism could be in.


Checking a whole program and starting the parts that can run without waiting
used to happen inside reading a file, in a phase nothing else could enter. Now a
caller assembles a program by whatever route and then says it's finished: the
checks run, everything startable starts, and a complaint naming every fault
comes back if anything's wrong — with **nothing started in that case**,
because a program running half of what it was asked to is worse than one that
refuses. Reading a file is now one caller of that, with no privileges.


**It's repeatable, and that's the point rather than a convenience.** A station
added to a running program is checked and started by the next call; one already
started isn't started twice. That needs a mark per station — without it,
bringing a grown program up again would hand every input-less station another
run nobody asked for, which is the kind of thing that reads as a scheduling bug
for a week.


**Two issues disagreed and I had to settle it rather than code around it.** The
station issue wanted a parameter with nowhere to get a value to be *fatal*. But
the port record later taught the file format to *spell* such a port so a
half-built program round-trips — there's a test that reloads one — and the
capstone says plainly that a station may hold that state indefinitely, because
that's what makes "add a station now, wire it in a moment" an ordinary sequence
rather than a window of invalidity. **The later two win.** Refusing it would
make the thing this surface exists for impossible to express, and nothing
breaks: such a station just never becomes ready, the same outcome as a buffered
input nothing feeds, which was already a warning. So it's reported loudly
instead. Recorded in both issues, including that the *unqualified* half survives
exactly — no exemptions, every one reported.


**Moving a policy also showed who it belonged to.** The old sweep refused a
program where nothing could start. That's right for a *file somebody asked to
run* — it would do nothing at all — and wrong for a repeatable pass, where
starting nothing is the ordinary result of bringing up an already-running
program. So the pass reports what it started, and the loader is what decides
zero means refuse.


What's left in the capstone: the wire operations joining the same surface, box
wrappers so a map can perform these operations on another map, and the proof
that a program read from a file and one built by calling the surface produce
identical dumps.


--------------------------------------------------------------------------------

### User Request 13

Ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 13

Next: the proof that there's one construction path rather than two that agree by
coincidence. That needs a surface-built program to be nameable — let me check
what adding a station offers:


Naming needs to be part of the surface, or a surface-built program can't be
written back out. Let me add it:


The teardown loops over the station count, but the name array is sized
separately now — that would read past it:


The dump reads a name unguarded — that would crash on a surface-built program.
Let me see what it already refuses:


Now the proof — a program built by calling the surface and one read from a
file, dumped and compared:


The map syntax for an `in` line doesn't name an upstream — arrows are declared
separately. Let me look at a real map:


The `x64 -` form sets a depth *and* declares no source. Let me read what the
dump writes, since that's the canonical spelling:


Found a real bug. The dump writes **the same line** for two different things —
a buffer with a non-default depth and a port with no source both come out as `in
N x64 -`. Let me confirm what the parser makes of it:


Confirmed — a genuine round-trip bug. The dump writes a deepened buffer as `in
N x64 -`, and a bare dash always parses as *no source*. So a program with a
deepened buffer, dumped and reloaded, comes back with that port unwired — and
any arrow into it is then refused. Let me see how an input line is represented:


My own test map still uses the old spelling. Fixing it to say "a buffer this
deep":


Now a regression test for the bug itself — a deepened buffer must survive
being written out and read back:


Let me tidy the test's output and then document the new form:


Committed, clean, all tests pass.


**The two construction paths are now proven to be one.** A program is written to
disk and read back; the same program is built by calling the surface; both are
dumped and compared byte for byte. Not equivalent — *the same*. Comparing
dumps is the cheapest proof available, because the dump walks the live station
table and writes what's actually there, so two identical dumps are two identical
tables including every detail a hand-written comparison would forget to check.


**Naming a station had to become an operation** for that test to be writable at
all. It was something only the loader did, in one sweep at the end once its own
lookup table had served — which works exactly as long as reading a file is the
only way to build a program. Built any other way, a program dumped as a row of
indices and couldn't be compared to anything.


**The proof earned its keep on its first run, which is the point of writing
one.** I found a genuine round-trip bug: the dump wrote a buffer told to start
deeper as `x64 -`, and a bare dash already meant *a port with no source at all*.
Two different things spelled the same way. So a program containing a deepened
buffer could be written out and **could not be read back** — it returned with
that port unwired, and any arrow pointing at it was then refused. The refusal
was at least loud, but what it named was the wrong thing entirely; somebody
would have spent an afternoon on the arrow.


The format gained the form it was missing — a depth with nothing after it,
saying only how deep, since a port's source is arrows unless the line says
otherwise. The format document now carries that, including why it exists. And a
test asserts the two stay different things across a round trip.


Worth noting what kind of bug that was: not a crash, not a race. **Two distinct
states sharing one spelling**, which no amount of testing either state alone
would catch — only round-tripping both through the same channel exposes it.


What's left in the capstone: the wire operations joining the same surface, and
box wrappers so a map can perform these operations on another map — which is
what makes a program able to build a program.


--------------------------------------------------------------------------------

### User Request 14

Ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 14

Last of the five operations: wires. Currently there are two ways to draw one —
construction-time and runtime — with different rules and different refusal
styles. Let me compare them:


Rewiring's checks are a strict superset except one. Let me make it the single
implementation with three faces, differing only in what a caller wants done with
a refusal:


A test expects the whole-map wording, but the refusal now happens earlier at the
wire itself — which is better placed, and should name the edge at least as
well:


All tests pass with one wiring implementation. Let me verify the refusals still
read well and that the weaker path really was weaker:


Committed, clean, all tests pass.


**Drawing a wire no longer depends on when you draw it.** There were two
implementations — one for assembling a program, one for editing a running one
— that were supposed to apply the same rules.


**They didn't, and the construction one was weaker by exactly two rules.** It
never asked whether the destination could actually hold a value, and it never
checked that both ends counted the same number of bytes. So a program built by
hand could contain a wire that the *same program*, written to a file and read
back, would have been refused.


That's worth dwelling on because it's the characteristic damage of having two
paths: not a crash, which somebody notices, but **a quiet difference in what
each path is capable of that nobody wrote down**. The kind of thing you only
find by deleting one of them.


The rules were never about *when* — a box returning nothing has nothing to
send whether or not the workers have started, and a destination that can't hold
a value has nowhere to put one either way. So there's one implementation now,
taking the rewiring lock at any moment, with three faces differing only in what
the caller wants done with a refusal: hand it back, print it and return a code,
or stop the program. Which of those you want is a property of the caller, and
it's the only thing that ever legitimately varied.


**One consequence I had to chase:** a refusal that used to arrive when the whole
program was checked now arrives as the wire is drawn — earlier and better
placed. But it had to learn to name the station and port on the way, because the
whole-map version named them and the per-edge version said *"the destination
port"*, which names nothing anybody can go and look at. A test caught that
immediately.


The capstone now has four of its five operations unified — create, add, name,
configure, wire — with removal keeping its own shape for now since it needs
the quiescence sweep the others don't. What remains is **box wrappers**: plain C
functions for these operations, so a map can perform them on another map. That's
what makes a program able to build a program, and it's the last thing standing
between here and the generator emitting construction calls.


--------------------------------------------------------------------------------

### User Request 15

Please continue


--------------------------------------------------------------------------------

### Assistant Response 15

Last piece of the capstone: box wrappers, so a map can build a map. That needs a
box to name a map, and a box takes only values. Let me check whether that's been
decided anywhere:


This is the moment a hazard the project already wrote down falls due, and it
says explicitly that it *"should be a decision somebody makes rather than a
discovery somebody has."* So I'm stopping to put it to you.


**The situation.** A wire is legal when both sides count the same bytes —
that's what lets two boxes spell one shape differently and still connect, which
you reminded me of earlier. It's a mild price while values are *data*: a wrong
wire gives a wrong number, which is visible and eventually noticed. Box wrappers
change that. Their inputs are **handles** — a map, a placement function, a
compiled box — and on a 64-bit machine every one of those is eight bytes. So
is a `double`, a `long`, and a pointer to something else entirely. A wire
delivering any eight-byte value into a box expecting a map handle is legal by
the current rule, and the failure isn't a wrong number: it's a call through
whatever was in those bytes.


**[Asked the user]**


*Q1 — Handles:* Box wrappers make handles into wire values. What happens about
the eight-byte problem?

- Build them; record the hazard — The wrappers land and the risk is written
  down as a stated non-guarantee in its own words: wiring anything eight bytes
  wide into an operation expecting a handle is accepted by the engine, and what
  follows is a call through those bytes rather than a wrong answer. Cheapest,
  and honest about what it costs. It also means the first person to mis-wire a
  self-building program gets a crash with no useful location, since the engine
  did exactly what it was told.

- Take shape comparison first — The design for comparing types by layout
  rather than by width was written in full during the width work and
  deliberately not taken — width won for being one integer against another.
  Its field tables have been emitted since phase 3 and have never been
  consulted. Reviving it would catch this honestly: a handle and a double have
  different layouts even at the same size. Costs: it is a real piece of work
  before the wrappers, and it partly re-opens a decision that was closed on good
  grounds.

- Keep handles off the wires entirely — Don't give the operations box
  wrappers. A program that builds another program does it through generated
  construction calls instead — which is where the map-becomes-code work is
  heading anyway, since the generator emits the calls and no name survives into
  the running program. Handles then never become values a wire can carry, and
  the hazard does not arise. Costs: a map cannot build a map at runtime from
  values it computed, only from what was compiled in.

- Check handles by name, data by width — One targeted exception: a wire
  carrying a handle type is checked by identity, everything else by width.
  Narrow, and it puts the check exactly where the danger is. Costs: an exception
  in a rule whose whole appeal was having none, and something has to decide
  which types count as handles — which is a list somebody maintains, and lists
  somebody maintains go stale.

→ **Answered:** I need more background context. What is a map handle? We're
talking about composing boxes right?". Read the answers carefully — they may
request clarification, changes, or that you not proceed — and follow what they
actually say.


Fair — let me lay out the mechanism, because the answer turns on it.


**What a map is.** One running program. Concretely it's a heap-allocated struct
holding the station table, the names, the rewiring lock, and a pointer to the
worker pool. Every operation that changes a program takes it as the first
argument — `map_add_station(m)`, `map_wire(m, …)` — because "add a
station" is meaningless without *to which program*.


**Why a box can't just reach one.** A box is a plain C function that takes its
arguments by value, returns one value, and is not permitted to remember or reach
anything. That's not decoration: earlier work deleted the last global pointer to
a map precisely so two programs could run in one process without seeing each
other. A box has no way to name a map except by being handed one.


**What step 7 asks for.** The capstone wants the construction operations to
exist *as boxes* — a station whose box is "add a station", wired to a station
whose box is "draw a wire" — so a program can build another program using
nothing but the engine it already has. This isn't about composing ordinary data
boxes; those are unaffected. It's specifically about boxes whose job is to
modify a program.


**Where the handle comes in.** For such a box to act on a map, the map has to
arrive as one of its input *values*. The only way to say "this particular map"
as a value is its address — an 8-byte number on a 64-bit machine. That number
is the handle.


**And the hazard.** The engine decides a wire is legal by comparing byte counts,
which is what you pointed out is right for data — two boxes may spell one
shape differently and mean the same thing. But a `double` is 8 bytes. So is a
`long`, a file offset, and a pointer to something else entirely. So the engine
would happily accept a wire feeding a computed `double` into the "which map"
input, and the box would then treat that bit pattern as an address and write a
station table into it. Not a wrong number — a write through arbitrary memory.


**[Asked the user]**


*Q1 — Handles:* Given that — should a map's address ever travel down a wire
as a value?

- No — keep addresses off the wires — Don't give the construction operations
  box wrappers. A program that builds another program does it through generated
  construction calls instead, which is where the map-becomes-code work already
  heads: the generator turns a description into the calls it names, so nothing
  has to carry an address as a value. The hazard never arises because the
  dangerous value never exists. Costs: a running program cannot build a program
  out of values it computed while running — only out of what was compiled in.

- Yes, and write down what it costs — Build the wrappers; record plainly that
  wiring any eight-byte value into an operation expecting a map is accepted by
  the engine, and that what follows is a write through those bytes rather than a
  wrong answer. Cheapest, and the engine keeps one rule with no exceptions.
  Costs: the first person to mis-wire a self-building program gets a crash with
  no useful location, because the engine did exactly what it was told.

- Yes, but check layouts first — Before the wrappers, revive comparing types
  by their layout rather than their size — designed in full during the width
  work, deliberately not taken, and its tables emitted since phase 3 and never
  consulted. A map's address and a double have different layouts even at the
  same size, so this catches it honestly rather than by exception. Costs: real
  work before the wrappers, and it reopens a decision closed on good grounds.

- Yes, but names for addresses only — One targeted exception — a wire
  carrying an address is checked by which type it is, everything else by size.
  Narrow, and it puts the check exactly where the danger is. Costs: an exception
  in a rule whose appeal was having none, and somebody has to maintain the list
  of which types count as addresses.

→ **Selected:** Yes, and write down what it costs


Decided: the wrappers land and the cost gets written down. Let me look at what a
box source may contain, since the generator refuses anything it doesn't
recognise:


Registered as ordinary boxes. Now the test that a map actually builds a map:


A map builds a map, and the program it built comes up and runs. Now writing down
what that costs:


Committed, clean, all tests pass. **A program can now build a program.**


The operations that create structure — add a station, draw a wire, write a
constant, name a station — exist as ordinary box functions in a box source the
engine has no idea is special. A test assembles a builder, points it at an empty
program, runs it, and then **brings up and runs what it made**. Both halves
matter: a station can look correct and still be unrunnable, and the way to find
out is to run it.


**On your question about the handle** — the answer is now in the code and the
documents, but the short version: a box takes its arguments by value and may not
reach anything ambient, because the last global pointer to a running program was
deleted on purpose so two programs could share a process without seeing each
other. So a box acting on a program must be *handed* one, and its address is the
only way to say "that particular program" as a value.


**And that's where the accepted risk became real.** The ticket that chose
comparison-by-size wrote this hazard down when it made the choice — including
the sentence that it should be *"a decision somebody makes rather than a
discovery somebody has"*. It fell due exactly here. You decided to accept it and
write down what it costs, so the guarantees page now records it in its own words
rather than folded into the comparison-by-size entry: a mis-wire yielding a
wrong number and a mis-wire yielding a write through arbitrary memory are
different facts about the same rule.


What's actually defended: a null, checked before anything is touched, because a
port never given a value delivers zero and that's the likeliest mistake by a
wide margin. Past null nothing distinguishes a real address from any other eight
bytes, and the box source says so rather than implying otherwise.


The capstone is now down to one thing: **starting a second program beside this
one** rather than building into it — separate station table, no wires across,
reached only by delivering into its input station. That waits on the input
station itself, which isn't built.


So the next genuinely available work is **the input station** — where a
program's arguments arrive — and its sibling, the output station, which is
startable now and is what the input one stands on.


--------------------------------------------------------------------------------

### User Request 16

Great, let's keep working.


--------------------------------------------------------------------------------

### Assistant Response 16

Next: the output station — where a program's results come from. It's what the
input station stands on. Let me read it.


Large issue; let me take the mechanism first — the designation, holding rather
than discarding, and the calls that drain. Here's where a value is discarded
today:


The station has no index field — let me pass what the message needs:


Now the three operations — designating a station, asking how many results
wait, and taking one:


Now the test — including that an *un*designated station still discards, since
that's the rule this designation is an exception to:


Committed, clean, all tests pass. **A program can now say where its results come
from.**


The gap wasn't mechanical — a box could always print or append to a file. What
was missing was a **name**: no station was designated as the place results come
from, so a program meant to sit inside another had nowhere for a parent to wire
from, and somebody reading a program couldn't tell which station was the point
of it.


A designated station stays entirely ordinary — same shape, same readiness
check, running whatever box it was given. **The designation adds exactly one
rule**: when its output port is wired nowhere, values are held instead of
dropped. Two calls take them out — how many wait, and take the oldest — and
both exist because a caller asked to drain results can't write that loop with
only one of them.


**Oldest first, and that's worth noting given earlier work:** this is the one
ordering the engine can still honestly promise. A single station produced them
all in sequence, unlike values leaving a port, where order stopped meaning
anything when the scan replaced the head index.


**A station whose box returns nothing is refused as an output** rather than
accepted and useless — it has no output port to wire from and nothing to hold,
so whoever asked has almost certainly named the wrong station.


**The pile-up is shouted from the first doubling**, and that's deliberate. There
are now three kinds of backlog and they're three different diagnoses: an input
backing up means the sides feed at different rates; the task queue backing up
means the workers are behind; both are performance signals that can wait for the
teardown report. Results backing up means *nobody is collecting them at all* —
a program computing into somewhere nobody is looking — and waiting until
shutdown to mention that wastes the entire run.


**The test asserts both directions, and the second earns its place:** an
undesignated station runs, produces, and drops what it made. That's the rule
this designation is an exception to, and a test checking only the holding half
wouldn't have shown the exception to be narrow.


What remains on it: the map file syntax for the designation and the dump
carrying it, the load-time check that what's wired in type-checks, and a program
used as a box inside another — that last one needs **the input station**, the
same design pointed the other way, which is now the next available piece.


--------------------------------------------------------------------------------

### User Request 17

okay can we keep going?


--------------------------------------------------------------------------------

### Assistant Response 17

Next: the input station — the same design pointed the other way. Let me read
it.


Step 2 says the mark is shared — "a station carries a mark saying it is a door
and which way it faces." I built it as an output-only flag; let me generalise it
properly rather than add a second one:


A stray non-English word got into that message. Fixing:


Now the test that matters: a parent using a program as a box without naming
anything inside it.


Two findings. The result order isn't the argument order — with two workers
those three values are in flight concurrently, so asserting the sequence would
be asserting a schedule. And the warning about unfed buffered inputs now fires
on a declared entrance, which is exactly the station something outside *does*
deliver to:


Committed, clean, all tests pass. **A program now has a surface.**


Before, a value got into a running program exactly one way: somebody holding it
named a station *inside* it. That works, and it means every caller knows the
program's insides — rename an interior station and every caller breaks. That
isn't encapsulation; the program had no surface, only internals that happened to
be reachable.


**The refusal is the whole of it.** Underneath, an argument arriving is the
ordinary delivery down an ordinary wire — nothing was added to the delivery
path. What changed is *who may use it*. The test proves the refusal as carefully
as the delivery, because a surface nobody has to respect isn't a surface.


**One mark, two directions**, shared with the output door — they're one design
seen from either end. Being both is refused: a program whose entrance is its
exit is somebody having named the wrong station.


Two things worth calling out:


**A declared entrance stopped setting off a false alarm.** The whole-program
check warns about a station whose buffered inputs nothing feeds, and that
warning has always ended *"unless something outside delivers into it."* A
declared entrance is exactly such a station — so the sentence's own escape
clause finally became something the engine could check. Warning about it would
be telling somebody that the thing they just declared might not happen.


**And one claim the test deliberately doesn't make.** My first version asserted
results come out in the order the arguments went in, and it failed —
correctly. Three values fed to a program with two workers are in flight
together, and which finishes first is a schedule. Results are held oldest-first,
so they emerge in the order they *arrived* at the output station, which is a
different thing. The test checks the set and says why, because asserting the
sequence would be asserting the scheduler — the thing this engine most
deliberately refuses to promise.


Also recorded: the "station that runs no box" both door issues called for turned
out **not to be needed**. A door that shapes nothing is a station running an
identity box — an ordinary function somebody was going to write anyway, rather
than a new kind of station. That keeps the count of things this engine has at
what it was.


What remains on both doors: the map file syntax for the declarations and the
dump carrying them, and then one program used as a box inside another — which
is now the only thing standing between here and the capstone being finished.


--------------------------------------------------------------------------------

### User Request 18

Ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 18

The map file needs to spell the two declarations. Let me look at how a station
line is read, since `in` and `out` already mean something on the lines beneath
it:


A fourth word on the station line. Deliberately not `in`/`out` — those already
mean a port on the lines beneath, and two things spelled the same way is the
exact trap I fixed in the depth form earlier.


Now the round trip — a program's doors must survive being written down, or it
can't be composed after reloading:


Same shape as the warning: a program with a declared entrance is fed from
outside, so "nothing to seed" stops being a fault for it. That refusal needs the
same escape clause:


One doc edit didn't apply — let me check what 209 actually says now:


My bookkeeping failed there — the code landed but that issue's record didn't,
because a script asserted before writing and lost every edit in the batch. Let
me redo it one at a time so a single miss can't discard the rest:


Committed, clean, all tests pass. Two things happened here, and one was my
error.


**The work:** a station line gained a fourth word saying that station is where
the outside delivers, or where results come from. The reader takes it, the dump
writes it back, and a test reloads a written-out program to prove both survived
— a program whose declarations were lost on the way to disk couldn't be
composed afterwards, which is most of what naming them was for.


**The words are `entry` and `result`, not `in` and `out`** — and the obvious
choice was the wrong one. Those already name a *port* on the indented lines
beneath a station. A file where one word means a port in one place and an entire
station in another reads perfectly well and round-trips wrong. That's the second
time this session: the depth-followed-by-a-dash bug was the same shape. Worth
naming the pattern — **the danger isn't a word that's ambiguous to the parser,
it's one that's unambiguous to the parser and ambiguous to the reader.**


**Two refusals gained an escape clause they'd always described.** The
whole-program check warns about a station whose buffered inputs nothing feeds,
ending *"unless something outside delivers into it."* And reading a file refuses
a program where nothing can start. Both sentences were written when a program
had no way to *say* it expected to be fed, so both assumed the worst — and a
declared entrance is precisely the case they were assuming away. Neither
exemption was foreseen; both fell out of the declaration existing.


**My error:** the output door's issue file didn't record any of its work. A
script making several edits and saving once hit a mismatch on the last one and
discarded the whole batch, so the ticket kept saying the work was still to do
while the tests proved otherwise. I've redone it one edit at a time, and the fix
is committed separately so the record shows the correction rather than hiding
it.


The thing most worth having recovered is what was *not* built: both door issues
asked for a station that runs no box. It turned out unnecessary — such a door
is a station running an identity function, an ordinary box somebody was going to
write anyway rather than a new kind of station. The engine's inventory of
distinct concepts stayed where it was. That's a decision that only exists in a
ticket, because the code has nothing in it to point at.


Both doors now exist with map-file syntax. What's left is the demonstration that
ties them together: **one program used as a box inside another** — wired to
its entry, read from its result, with the parent never naming anything inside
it.


--------------------------------------------------------------------------------

### User Request 19

Ok let's keep going


--------------------------------------------------------------------------------

### Assistant Response 19

The remaining demonstration is a program *using* another program. The capstone
distinguishes two ways — composing merges the tables, starting keeps them
separate and shares only the pool. Let me check whether a second map can share a
pool:


The capstone says the pool is shared "because nothing about finishing requires
knowing which program a task came from." Let me check whether that's true as
built:


Confirmed, and the claim is false as built: a station index means nothing
without a map, so the finish hook has to come from the pool's context — which
ties one pool to one map. The fix is small and the shape is already there. Note
what `task_build` does with the map it's handed:


Now a program can be started beside another, sharing only the workers:


Passing. One line of noise to account for — the final delivery lands after the
workers have gone home, so its task never runs. Let me say what that actually
proves:


Committed, clean, all tests pass. **A program can now set another going beside
it.**


Own station table, own editing lock, own everything — sharing only the
workers. **You cannot wire to it, and that's the point rather than a
shortcoming.** A wire is a station number and a port number; a number means
something only inside one table, and a table belongs to one program. So two
stations can be joined exactly when they sit in the same program.


**The test proves the negative the honest way.** Both programs have a station
numbered zero, and they're different stations. Asking to wire from one into the
other's number isn't refused on grounds of *programs* — it's simply a wire
inside the first program, to a station the first program doesn't have. Nothing
had to be added to forbid it. Numbers just don't cross.


**And the issue's own explanation of why the pool could be shared was wrong.**
It said sharing was fine "because nothing about finishing requires knowing which
program a task came from." Finishing requires precisely that — it resolves a
station index, and an index means nothing without the table it indexes. So the
map came from the pool's own context, which bound one pool to one program. The
claim would have read as settled forever if nobody tried to build on it.


**The repair was a shape the pool already had.** A task carries a station number
and an exit number that the workers ferry without ever looking at them —
that's what keeps the pool ignorant of programs. The owning program becomes one
more such field. And the map was *already* being handed to task construction and
explicitly discarded, so carrying it cost nothing. Either a happy accident or an
earlier decision paying out late.


That's step 10 of the capstone done, and with it the last claim the capstone
made that was checkable without new mechanism.


**What remains is composing** — merging two station tables into one, joined at
the doors that mark the seam. That's genuinely different from everything so far:
it means copying stations between tables and offsetting every wire index, on
records that hold their own mutexes. It's the one piece of that issue that needs
real work rather than a field, and it's where I'd start next.


--------------------------------------------------------------------------------
