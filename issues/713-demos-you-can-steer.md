# 713 — The demos you can steer

The seven phase demos are replaced rather than repaired. What they
demonstrate is largely right; the *shape* they demonstrate it in is
the opposite of what this engine is for.

A ceramic program is not a batch. It comes up, it stays up, and it
absorbs change without stopping — a value written from outside, a wire
cut and redrawn, a station added, a box compiled and placed while the
workers are busy. Every one of those is built and proven. And the way
a visitor currently meets them is by pressing space thirty-five times
while a program does all of it to itself.

**The reader should be the one turning the dial.**

## The note that started this

Kept verbatim, because the reasoning in it is the specification:

> Can we remove all the demo files? they kinda suck. Let's make a new
> issue to create new demos. Also, they should be more interactive
> than they currently are - ceramic is a system intended for
> persistent operations, so we should treat the demos as such and give
> the users some buttons and levers to tweak and push and such,
> updating the system while it's running, and demonstrating each of
> the implementation details of the phase that the demo belongs to.

## Current behavior

**Deleted.** The first implementation step below has been taken: the
seven demo programs, the two presenters, the four map files and the
seven runner scripts are out of the tree. They remain in git history,
which is the only copy anybody should need — the issue files that
describe them are untouched in `completed/` and remain the record of
what the first generation was and what it proved.

What stood there until now, so that this is not lost:

Seven demos, one per finished phase, discovered by the launcher in the
project root rather than listed by it. Thirty-five scenes across them,
every one following one shape set by
[707](completed/707-demos-as-word-problems.md): state the problem in
the engine's own vocabulary, offer an image to hold it by, tabulate
the correspondence between the two, justify every row of that table,
measure, and end with a finding. A hundred and forty-two justified
correspondences. Two presenters, one compiled and one shell, matched
column for column. Paging rather than printing, so a reader faces one
scene instead of a transcript. A report mirrored to the shared-memory
tier on every run. A non-terminal run that says it is one, skips the
paging and the colour, and plays the scripted version of anything
live.

Four of the thirty-five scenes were live panels the reader steered —
the pool's ring under fan-out, backpressure at a gated station, the
statics dial turned mid-run, and the map under load. **Those four are
the seed this issue grows from, and they were the exception.**

Three of the seven no longer compiled at all. Removing the pull path
([210a](completed/210a-the-pull-path-removed.md)) took the gatherer out
of the engine, and phase four still asked a port to become one, phase
six still switched on its tag, and phase seven still repointed one on
a running map. That is what
[710](710-demos-after-the-pull-path.md) was written to repair, and it
is superseded here: repairing three programs whose shape is being
replaced is work that would be thrown away twice.

## Why paging is the wrong shape

A paged demo is **a transcript of something that already happened.**
The program decides what to do, does it, measures it, and the reader
advances to the next thing that was decided for them. That shape is
honest for a measurement — you cannot let somebody steer a benchmark
and still report a number — and it is exactly wrong for a machine
whose distinguishing property is that it can be changed while it runs.

The engine's three most unusual capabilities are all *responses to an
outside act*:

- **A write is an event.** Writing a static runs the ordinary
  readiness check on the station holding it, which is what turns a
  chain of stations into a recalculation graph.
- **The shape is editable while it runs.** Stations can be added and
  removed, wires cut and redrawn, ports converted from one source to
  another, and the table grows without moving anything.
- **New code can arrive after the program started.** A box's source
  compiled, loaded, and placed into a running map, with the pool never
  stopping.

None of the three has a subject until somebody outside the program
acts. A demo that supplies its own acts is demonstrating the mechanism
with the interesting half removed.

## Intended behavior

**Each phase's demo is a control panel over a live map that does not
end until the reader quits.** It comes up, the map starts running, and
the screen stays. Nothing is a transcript; every figure on it is
current.

### The engine is a real process; the panel is a page

**Decided.** A panel is two halves. The lower half is an ordinary
ceramic program — real workers, a real station table, real mutexes,
built by the same `make` as everything else — running a map chosen for
the phase. The upper half is a page in a browser that draws that
program and sends the reader's lever movements down to it. They speak
over a local socket the program opens and the reader's browser
connects to.

The alternative was a page carrying the engine compiled to
WebAssembly, and it fails on the one property this engine exists for:
WebAssembly runs one thread unless a great deal of extra machinery is
arranged, so the panel would demonstrate a single-threaded port of a
machine whose entire claim is that a shape executes itself across every
core. **A demo that quietly removes the concurrency is worse than no
demo**, because it teaches something false and looks authoritative
doing it.

**This does not break the rule that nothing lives on a server.** That
rule ([801](801-browser-workbench.md)) says no account, no upload,
nothing sent anywhere — it is about work leaving the author's machine,
not about two programs on one machine talking to each other. The panel
sends nothing anywhere: the process is started by the reader, listens
on the loopback interface only, and dies when they quit. What it does
cost is that a panel is no longer one file you can open — something
has to be running before the page has anything to draw — and that is a
packaging question, recorded in
[057](../docs/implementation-notes/057-packaging.md)'s terms rather
than waved at.

### What crosses the wire, and why it needs no new engine surface

The most useful thing found while designing this: **both directions of
the protocol already exist as engine features.**

| Direction | What it is | Already built as |
|---|---|---|
| Engine → page | The whole shape: stations, kinds, ports, sources, wires, static values | The map dump, which already writes the live table as a map file and already round-trips ([703](completed/703-map-dump.md)) |
| Engine → page | The motion: per-port occupancy, ring depth, worker states, run counts, time spent, growth events | The statistics and reporting instruments ([701](completed/701-buffer-growth-reporting.md), [702](completed/702-station-statistics.md)) |
| Page → engine | Every lever: add a station, configure a port, draw or cut a wire, convert a port's source | The construction surface ([212](completed/212-one-way-to-build-a-program.md)), which is legal at any moment by design |
| Page → engine | Turn a dial | Writing a static ([405](405-statics-mutation.md)), which is an event and runs the readiness check |
| Page → engine | Type a function and place it | Boxes compiled while the program runs ([310](completed/310-boxes-compiled-at-runtime.md)) |

So the protocol is a transport, not a new capability, and the panel is
**a second caller of surfaces that already have one.** That is the
argument for building it at all: a lever that cannot be expressed as a
call somebody could already make is a lever demonstrating something
the engine cannot do.

It also fixes this issue's position in the order. **A complete panel
waits on the construction surface**, because half its levers are that
surface. Phase 1's and phase 2's panels can be built before it against
what exists today; the rest arrive with it.

### Seven pages, not one page seven times

**Decided: each phase's panel is its own page, drawn for its own
machinery, sharing nothing with the workbench.** No general-purpose
canvas, no station-and-wire editor with the phase selected from a
menu. A generic graph view is the right answer when the subject is *a
graph*; here the subject is a specific mechanism, and a picture built
for that mechanism shows it in a way a general one cannot.

Phase 1's page is a ring of task slots, a row of workers each awake or
asleep, and a sleeper count — there is no graph on it at all, because
the pool does not know what a station is and the page should not
either. Phase 5's page is one comparator with three exits and a
threshold you drag, drawn large. Phase 2's page is two stations and
the ports between them, close enough to see individual slots fill.
**The page is a drawing of the thing being explained, not a viewer
that happens to be pointed at it.**

This is also the reason not to reuse the workbench: that canvas exists
to let somebody lay out a map they are composing, so it must treat
every station alike and every wire alike. A page that treats every
station alike cannot make the one station this phase is about bigger
than the others.

### Dynamic documentation, which the project has already started

The governing idea is not *dashboard* and not *demo program*. It is
**a document that moves and answers back** — the form
[705](705-html-documentation.md) already began, where three
interactive pieces ride the pages that explain them: the ring buffer
with a capacity slider showing wrap and unwrap, the readiness check
firing on the click that fills the last slot, and the iterator dealing
even counts under an adjustable slow consumer.

Those three are the seed. What separates a panel from them is that a
widget models the mechanism in the page's own script, while a panel is
**wired to a real process** and shows what actually happened. The form
is the same and the honesty is different, which the pages must say
plainly wherever both appear.

So a panel page reads as prose with instruments set into it: the
explanation of a mechanism, and inside that explanation the mechanism
itself, running, with the controls in the paragraph that describes
them rather than gathered into a control bar at the edge. **A reader
scrolls a panel the way they read a page**, and the thing they are
reading about is alive next to the sentence about it.

### What every page must carry, however it is laid out

Three things, present on every panel; where they sit is decided per
page rather than by a template.

| Must carry | Holds |
|---|---|
| **The machine** | What is actually there right now — whatever this phase's mechanism consists of, drawn to fit. Per-port occupancy, ring depth, worker states, run counts, timings. It redraws on a fixed cadence, not on the reader's action, because a picture that only moves when poked implies the machine only moves when poked. |
| **The levers** | Every control this phase offers, each showing its current setting rather than only its name. A dial reads as a dial with a value on it. A lever that is unavailable right now says why it is unavailable. |
| **The log** | What the reader did and what the engine did about it, newest last. *You raised the threshold to 40 → traffic moved from the greater branch to the lesser within two deliveries.* This is what keeps a panel a demonstration rather than a dashboard: a dashboard shows state, and a demonstration shows consequence. |

### The phase's issue list is the panel's checklist

**Every mechanism the phase built has at least one lever that
exercises it or one readout that shows it**, and the panel can say
which mechanism each of its controls belongs to. This is the contract
that replaces 707's per-scene story contract, and it is checkable in
the same way that one was: a phase's completed issues are a list, and
a panel that covers six of nine is a panel with three gaps that can be
named.

Where a mechanism cannot be given a lever — because it is internal,
like the last sleeper's re-scan — it gets a readout and a way for the
reader to *provoke* the condition, which for that example means making
work arrive at exactly the wrong moment on purpose.

### Every lever still says what it stands for

707's finding survives the change of shape and is the reason this is
not simply a dashboard: **a control that does not say what it means
teaches nothing.** A reader who moves a slider and watches a bar move
has learned that the two are connected, which they already assumed.

So each lever carries the same three-part sentence a scene's
correspondence table carried — the engine term, the thing it stands
for, and *why it stands for it* — shown when the lever is first
touched and reachable afterwards. The interface should have no way to
add a lever without saying what it does to the machine, exactly as the
old one had no way to name a correspondence without justifying it.

### What each phase's panel offers

| Phase | Levers | What the reader watches happen |
|---|---|---|
| 1 — the pool | Submit a burst; change the arrival rate; hold a worker so it finishes slowly; force the arrival that races the last sleeper; ask for the polite stop | The ring doubling under a burst, the sleeper count climbing to full, the re-scan catching the work that arrived during the last sleeper's final look, and the difference between the three ways a program ends |
| 2 — stations and the push path | Fill one port at a time by hand; set a port's depth; draw and cut wires; add a station; remove one and add another to watch its place reused | Readiness firing on the *last* slot filled and not before; the two backlog kinds building for their two different reasons; fan-out costing one copy per destination; a station's place coming back |
| 3 — the build path | Type a C function into the panel and compile it into the running program; place it; wire it to something whose width disagrees | New code entering a program that never stopped, and a refusal that names both stations, the port, both type names and both widths |
| 4 — configuration | Turn a dial wired to a static; type a struct constant as brace text; convert a port from one source to another | One write cascading through a chain of stations as a recalculation, and the value dumped back out as the text that produced it |
| 5 — routing | Move a comparator's threshold; unwire one of its three outcomes; change how many ports an iterator deals to | Traffic moving between branches as the threshold crosses it; an unwired outcome discarding, on purpose; the deal staying exactly even under any crowd |
| 6 — the map file | Edit the map text in a pane and reload it; introduce errors on purpose; dump the running map beside what you typed | Every refusal collected and printed together rather than one per run, and a dump that reads back as a map file |
| 7 — seeing inside it | Rewire under load; relieve a bottleneck mid-run by deepening a buffer or adding a parallel station; turn timing instrumentation on and off | The three backlog diagnoses telling three different stories, and the cost of measuring, measured |

Phase 8 will want one too, and it is the phase whose demo is not a
terminal program. That is left to that phase.

### The run that nobody connects to

**A panel process started with nobody watching plays a recorded
sequence of lever movements against itself and writes what each one
did.** No socket is opened, no browser is wanted, and the report that
comes out is the artifact — which is how the panels stay testable,
how `make test` catches one that has stopped working, and how the
project keeps a written account of what each phase can do for somebody
reading rather than driving.

This is the half that keeps the split honest. Every lever must be
reachable by the recording as well as by a mouse, which means no lever
can exist that is only a gesture on a page: it has to be a call the
process can make to itself. **The recording is the specification of
the protocol**, written first, and the page is a second way to send
what it already sends. The old demos solved the same problem by
printing every scene at once and saying in the banner that they were
doing so, which is the precedent for saying plainly which mode is
running.

The recorded sequence is also the answer to *what should I press
first* — a reader who wants to be shown rather than to explore can ask
the panel to play the recording on screen, at reading speed, with the
levers moving by themselves and the log narrating.

## What is being deleted, and why replacing beats repairing

| Gone | Was |
|---|---|
| Seven demo programs | One per phase, thirty-five scenes |
| Two presenters, compiled and shell | The layout, paging, colour and report mirroring |
| Seven runner scripts | The per-phase build-and-run front |
| Four map files | Fixtures for the phase 6 demo |

Repairing was considered because most of the prose in those files is
good and every measurement in them is honest. It loses on three
counts: three of the seven do not compile at all, and their repair is
a rewrite of the scenes that made them interesting; the presenters are
built around paging, which is the thing being removed, so they would
be rewritten rather than reused; and the correspondence-and-measure
scene shape is a *narrative* structure, while what replaces it is a
*spatial* one, three regions living at once. Carrying prose across is
cheap and can be done from git history one paragraph at a time.

**The completed issue files stay exactly as they are.** They describe
the first generation and remain buildable as written, which the house
rule about recreating the project from its issue files requires. This
one says what replaced them.

## What must not be lost

- **The word-problem instinct.** A reader who does not already know
  what a station is must be able to learn it here. Every lever
  justifies itself.
- **Honest mechanics.** A figure on screen is measured, not animated
  to look plausible. Where the panel simulates rather than measures,
  it says so on the panel.
- **A run that is not a terminal says it is not a terminal**, rather
  than pretending, and still produces a report.
- **The launcher stays the front door**, and stays discovering rather
  than listing, so a new panel becomes available by being dropped in.
- **One meaning per colour**, consistently across every panel, as the
  old demos held to.

## Suggested implementation steps

1. **Delete the old demos, the presenters, the map fixtures, and the
   runner scripts.** *(Done.)* Git history is the copy. The launcher's
   two empty-state messages were reworded in the same breath: they read
   *"Nothing has been finished yet"*, which stopped being true seven
   phases ago and became a falsehood at the project's front door the
   moment the demos went. It now names what is absent — the panel, not
   the engine — and points here.
2. **The lever vocabulary and the recording that drives it**, before
   any socket or any page exists. A declared set of lever kinds, each
   one a call the process can make to itself, each carrying the
   sentence that justifies it; a recorded sequence in a text file; and
   a runner that plays a recording and writes a report. At the end of
   this step a panel is a headless program that demonstrates a phase
   and produces a document, with no browser involved — which is the
   fallback-free version of every step that follows, because nothing
   later can add a capability that this cannot already reach.
3. **The state feed**, as a serializer over the dump and the
   statistics rather than as anything new: the whole shape plus the
   motion, in one form a page can draw. Proven by feeding it back
   through the map reader, which the dump already round-trips.
4. **The socket and the protocol**: loopback only, one connection at a
   time, state out and lever movements in, and a refusal that names
   itself when a lever arrives that this phase's panel does not offer.
5. **The pages**, one per phase, each drawn for its own machinery and
   sharing nothing with the workbench canvas. What they *do* share is
   the ceramic aesthetic and the documentation set's page furniture,
   since a panel is a document that moves rather than an application.
   The interface design for these is its own piece of work and should
   use the project's HTML design tooling rather than being assembled
   from whatever the previous page did.
6. **Phase 2's panel first**, not phase 1's. Stations and the push
   path are what somebody needs to understand before anything else
   makes sense, and filling one port at a time to watch readiness fire
   on the last one is the single clearest thing this engine does.
7. **Phase 1's panel**, whose hardest part is provoking the
   last-sleeper race on demand rather than waiting to be lucky.
8. **Phases 4, 5, 6 and 7**, in that order — each is a straightforward
   application of the vocabulary once it exists, and each is checked
   against its phase's issue list for coverage before it is called
   done. These arrive with the construction surface, since that is
   what most of their levers are.
9. **Phase 3's panel last**, because typing a C function into a field
   and compiling it into the running program is the most demanding
   piece of interface here and benefits from every lesson the others
   teach.
10. **The build runs every recording**, so a panel that has stopped
    working is caught by `make test` rather than by somebody opening
    it.
11. **Rewrite the launcher's own text** for a front door that starts a
    process and tells the reader where to point a browser, rather than
    one that runs a program and pages through it.

## Open questions

1. ~~Terminal or browser?~~ **Answered: a real C process with a
   browser front, talking over a local socket.** Written into the
   intended behavior above.
2. ~~Does this page and the workbench become one page?~~ **Answered:
   no, and nothing is shared with it.** A canvas for composing a map
   must treat every station alike; a page explaining one mechanism
   must not. Written into the intended behavior above.
3. ~~One panel per phase, or one with a phase selector?~~ **Answered:
   one page per phase**, each drawn for its own machinery.
4. **What is the honesty rule when a modelled widget and a live panel
   sit on the same site?** The documentation set's three interactive
   pieces model their mechanism in the page's own script; a panel is
   wired to a process and reports what happened. Both will be on the
   same site in the same aesthetic, and a reader cannot tell which is
   which by looking. The old demos had this problem in one program and
   solved it with a rule — say on the panel where a figure came from —
   but a site-wide version of that rule does not exist yet.
5. **Does the launcher script keep its numbered-choice interface**, or
   does a front door offering live panels want to name them rather
   than number them?
6. **Should the completed demo issue files be marked as superseded**,
   or left untouched? Leaving them silent means somebody rebuilding
   the project from `completed/` builds seven paged demos and then
   deletes them again.
7. **How much of the old prose comes back?** A hundred and forty-two
   justified correspondences exist in git history and most of them are
   still true.

## Related

- [707 — The demos told as word problems](completed/707-demos-as-word-problems.md),
  whose contract this replaces with a spatial one and whose finding
  about justification it keeps
- [710 — The demos after the pull path](710-demos-after-the-pull-path.md),
  **superseded** — the three demos it repairs are deleted here
- [105](completed/105-phase-1-demo.md),
  [208](completed/208-phase-2-demo.md),
  [307](completed/307-phase-3-demo.md),
  [406](completed/406-phase-4-demo.md),
  [505](completed/505-phase-5-demo.md),
  [606](completed/606-phase-6-demo.md) and
  [706](completed/706-phase-7-demo.md), the seven this replaces
- [704 — Rewiring while it runs](completed/704-runtime-rewiring.md),
  [405 — Changing a static while it runs](405-statics-mutation.md) and
  [310 — Boxes compiled while the program runs](completed/310-boxes-compiled-at-runtime.md),
  the three capabilities that need an outside actor to have a subject
- [705 — The HTML documentation set](705-html-documentation.md),
  whose three interactive pieces are the seed of this and whose
  aesthetic and page furniture these share — the one thing a panel
  does that a widget there does not is report a real process
- [801 — The workbench in the browser](801-browser-workbench.md),
  deliberately **not** shared with. Its canvas composes a map and must
  treat every station alike; a panel explains one mechanism and must
  not. The two look similar from a distance and answer to opposite
  requirements
- [709 — The slideshow and the transcript library](709-slideshow-and-transcripts.md),
  the third face this project shows on a page, and the one whose
  honesty rule about drawn-versus-measured this inherits at site scale
- [703 — The map dump](completed/703-map-dump.md),
  [702 — Station statistics](completed/702-station-statistics.md) and
  [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md),
  which together are the state feed, already built
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where a demo that needs a process running before it can be looked at
  is a cost that belongs
