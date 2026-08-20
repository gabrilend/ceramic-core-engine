# 801 — The workbench in the browser

First issue of phase 8. Everything before it makes programs run; this
is the first thing that helps somebody *write* one.

## Current behavior

Writing a map means writing a text file by hand.

The format is deliberately friendly to that — names rather than
numbers, only the exceptional input lines written, no capacities, no
types ([008](../docs/008-map-file-format.md)). A person can and does
author one in an editor. But everything that tells you whether the map
is any good happens later, somewhere else:

- **Whether a wire is legal** is decided by the loader, at startup, by
  comparing the source's return type against the destination
  parameter's type through the registry. The author finds out after
  writing the whole file and running it.
- **What boxes exist at all** is decided by whichever C files the build
  designated as box sources. There is no list to consult while writing
  except the C.
- **What shape the map is** exists only in the author's head. A map
  file is a list of lines; the graph it describes is not visible in it.
  The dump ([703](completed/703-map-dump.md)) writes a running map back
  out as another list of lines, which is the same problem in the other
  direction.

And using the engine at all means having its source, its build, and —
until [308](completed/308-generator-in-c.md) lands — LuaJIT.
[057](../docs/implementation-notes/057-packaging.md) is about that
distance: every inherited dependency is a reason somebody else's build
fails on a machine we never saw.

So the engine is approachable in principle and has no front door in
practice.

**This issue solves the third of those three and part of the first.**
The shape of a map becomes a picture, and the rules that are
properties of the picture get enforced while it is drawn. The other
two are untouched on purpose: the page never sees C, so it cannot
tell you which boxes exist, and it cannot compare one function's
return type against another's parameter. Naming that here rather than
letting a reader assume the workbench closed all three gaps is the
point of writing it down.

## Intended behavior

**A page you open in a browser, where you draw a map and leave with the
files needed to run it.**

Boxes are placed on a canvas. Each one is given a name, a kind — plain,
comparator, or iterator — and the box function it places, **written as
a name rather than chosen from anything the page holds**. Wires are
drawn from an output port to an input slot. Static values are typed
into the slots that take them. When it looks right, one button hands
back the map file.

**The workbench is for layout, and only for layout.** It does not run
maps, does not hold C, does not read C, and does not hand any back.
What it produces is one file: the map. The C functions the map names
live wherever the author keeps their C, and the workbench never sees
them.

This is the scope decision the whole phase rests on. A canvas that ran
maps would need the pool, the delivery walk and the loader in the
browser, and would be a second engine that could disagree with the
first. A canvas that read C would need a parser, and a parser in the
page is a second opinion about what a box is. **A canvas that only
arranges names has neither problem**, because there is nothing in it
capable of disagreeing with anything.

The simulation is an application of the same rule rather than an
exception to it: nothing executes, no value exists, and a box is a
duration rather than a function. See "the simulated run" below.

### Boxes are names you write

A box is placed and then told what it is by typing it. Where a
function's home matters, the file name is typed beside it, and both go
into the map the download assembles. The page holds no library, offers
no menu of available functions, and cannot tell you whether the name
you typed exists — because the thing that would answer that question
is a compiler, and there is not one here.

**Nothing is uploaded and nothing is bundled.** The earlier shape of
this issue had a plus button reading C files into the page and a
drawer of functions shipped alongside, with the build's own generator
compiled to WebAssembly so the page's parse matched the build's. All
of that is gone. What survives from that reasoning is the conclusion
it was protecting: the page must never be a second authority on what a
box is. Holding no C at all satisfies that more completely than
sharing a parser ever could.

**The assembler is still WebAssembly.** Turning a drawn graph into the
map file's exact text is the one place the page could get something
subtly wrong, and it is the one place the project already has correct
code — the map format's writer. Compiling that rather than
reimplementing it in JavaScript is what keeps a downloaded map loadable
by the real loader.

### What can and cannot be checked while drawing

The appeal of a canvas is that a mistake can be refused at the moment
it is made rather than at startup, and this scope divides the loader's
rules ([604](completed/604-load-time-validation.md)) sharply in two.

**Checkable, because they are properties of the drawing:**

- a box fanning out to both a gatherer slot and a ring-buffer slot
- a cycle among gather wires — the one that most repays being visible,
  since it is a property of the whole graph, invisible in any single
  line of the file, and shown on a canvas simply by refusing to
  complete the drag
- port limits by station kind
- an arrow to a slot number the station does not have

**Not checkable, because they are properties of C the page never
sees:**

- a wire whose source return type and destination parameter type
  differ
- an arrow to a slot the *function* does not have, as opposed to one
  the station does not have
- a comparator whose box returns a type with no compare function

Those three are found when the map is loaded, exactly as they are
today for a hand-written file. **This is the price of the narrow
scope and it is stated on the page rather than discovered**: one
sentence beside the download button saying the page checks the graph's
shape and the loader checks the types, so a map that draws cleanly is
well-formed and not necessarily well-typed. It is said once, in the
place where it becomes true, rather than marked on every wire — a mark
that appears on everything carries nothing.

### Nothing on the server

No account, no upload, nothing sent anywhere. The page is static and
the work happens in the browser. What the page does keep, it keeps on
the author's own machine — see below.

### Keeping what you drew

**The page remembers, on your machine, without being asked.** The map
being drawn — its boxes, their kinds, the names typed into them, the
wires, the statics, and the positions — is written into the browser's
own storage as the work happens, and is there again when the browser
is next opened. A tab closed by accident, a machine restarted, a
browser that crashed: none of them cost the drawing. Nothing is sent
anywhere, so "nothing on the server" holds exactly as written.

**And there is more than one of them.** A shelf of maps, not a single
remembered document: start a new one, return to an older one, rename,
discard. The map being drawn is one of the shelf's entries rather than
a special case beside it, so switching away is not a decision about
whether the current work survives.

**Downloading stays the way work leaves.** The shelf is where a map
lives while it is being made; the downloaded file is the map itself,
and it is the only artifact the engine ever sees. The two do not
compete — one is a workbench, the other is a delivery.

### On compiling C in the browser

The note asks whether a C compiler can be run in JavaScript, and
whether the page could therefore hand back a compiled binary rather
than source. **The scope above makes the question moot** — the page
holds no C to compile — but the reasoning is kept, because it is what
established that the promise could not have been made anyway.

**A C compiler in the browser is real.** Clang and its libraries have
been compiled to WebAssembly, and a page can host one. It is not small
— a usable toolchain is tens of megabytes of wasm — but it works, and
it is a download rather than an install.

**Producing a binary that runs on the author's machine is a different
problem.** A compiler needs to know what it is compiling *for*: the
target's instruction set, its calling convention, its C library
headers, and something to link against. Handing someone a native
executable means shipping a full sysroot per platform and picking the
right one, and this engine links against pthreads, so the threading
library for their platform too. That is a distribution problem wearing
a compiler's clothes.

**Producing WebAssembly that runs in the same page is tractable**, and
is not being done. The target is one the compiler already has and no
sysroot has to be guessed at, but the engine is a thread pool: real
threads in WebAssembly need shared memory, which browsers only grant a
page that is cross-origin isolated, which is two response headers — a
static hosting concern rather than a server, but enough to stop the
page being a file you can open. A single-worker build would avoid all
of that and would be unable to demonstrate the one property the engine
exists for.

So: the map file is the deliverable, native binaries in the browser is
a promise worth not making, and the engine in the browser is not
happening at all — see the simulation below, which is what the page
does instead.

### The simulated run

**No box function ever executes in the browser.** The page does not
carry the engine, does not carry a compiler's output of the engine,
and does not carry a reimplementation of it. What it carries is a
model of the *scheduling*, which is the part worth watching.

**A box has a duration you can set, and that is all it has.** Rather
than calling anything, a station that becomes ready starts a timer set
to whatever number the author dialled in, counts it down, and on
reaching zero hands its output along the wires leaving it. Turning
that dial is the whole of box behaviour on this page.

**What travels is a token that means "something is here."** Not a
value — there are no values, because nothing computed one. Each
delivery is a mark saying data of the right type arrived, which is
exactly enough to make readiness, backlog, fan-out, and a full
destination visible, and is honest about carrying nothing.

**What this can and cannot show.** It shows the shape of a running
program: which stations wait on which, where work piles up, what
happens when a slow box sits upstream of a fast one, how a value
reaching a fan-out becomes several arrivals. It cannot show a result,
cannot show a comparator choosing an exit on the basis of a value it
does not have, and cannot be used to check that a program computes the
right thing. It is a picture of the traffic, not of the cargo.

**And it is the same machinery the slideshow needs.**
[709](709-slideshow-and-transcripts.md) chose screens drawn live in
the browser rather than pre-rendered frames, and every one of those
screens is a small graph with values moving through it on a clock.
That is this simulator with a fixed graph and a script driving it. The
two should be one thing built once, which also means the honesty rule
709 has to keep by hand gets easier to keep: a screen driven by the
simulator can only show behaviour the simulator has.

### Why this is a phase rather than a feature

The canvas is an authoring surface, and authoring surfaces accumulate:
a validator that reports on a map without running it, a shareable
encoding of a map that fits in a link, a diff between two maps, the
dump read *back* into the canvas so a running map can be looked at as a
picture. Each is small beside the engine and none of them belongs in
the phase about seeing inside a running program. Phase 8 is where tools
that stand outside the engine live.

## Suggested implementation steps

1. The canvas alone: place, name, choose kind, type a function name,
   wire, with no validation at all. The interaction has to feel right
   before anything is built on it, and it is the part with no prior
   art in this project.
2. Emission: canvas to map file, matching
   [008](../docs/008-map-file-format.md) exactly, checked by loading
   the result with the real loader. The writer is the project's own,
   compiled to WebAssembly, rather than a JavaScript reimplementation.
3. The shelf: the drawing kept in browser storage as it is made,
   several maps side by side, restored when the browser reopens.
   Early rather than late, because every session after this one is
   spent testing on a drawing worth not losing.
4. Validation as wires are drawn — the drawing-shaped rules only, from
   the divided list above, with the page saying plainly which checks
   it does not perform rather than leaving a reader to assume a clean
   canvas means a loadable map.
5. Statics: typed into slots, written into the statics section — noting
   that [401](completed/401-static-slots.md) and [405](405-statics-mutation.md)
   are retiring the shared table in favour of a value owned by the port
   that reads it, so this should be built against where the format is
   going rather than where it is.
6. Download: the map file, and the companion layout file beside it.
7. Read a map file back into the canvas, which makes the dump viewable
   as a picture and makes the round trip testable — laying out from
   the companion file when one is offered alongside, and arranging the
   graph itself when one is not, since a map arriving alone must still
   open.
8. The simulator: a duration per box, a clock, and tokens moving down
   wires. Built as its own piece rather than as part of the canvas,
   because [709](709-slideshow-and-transcripts.md)'s slideshow screens
   are the same thing with a fixed graph and a script, and building it
   twice would give the site two accounts of how the engine behaves.

## Open questions

**Answered:**

- *Does the workbench live inside the documentation site, or beside
  it?* **Inside it, as a third front door** — a peer of the slideshow
  rather than an entry in the sidebar's numbered reading order. The
  entrance offers *watch, read, build*: the slideshow from
  [709](709-slideshow-and-transcripts.md), the reference from
  [705](705-html-documentation.md), and this. It shares the generator,
  the stylesheet and the aesthetic, so it is unmistakably the same
  site.

  The question "is it documentation?" resolves by being refused rather
  than answered. A numbered place in the reading order would make a
  canvas into a chapter, and a reader working down that order would
  meet a tool where a document was promised. A separate build sharing
  a stylesheet by hand would drift, which is what stylesheets shared
  by hand do. Being a peer keeps one look and one generator without
  claiming the workbench is a thing to be read.

  **The cost is that the workbench has to deserve equal billing.** A
  front page offering three ways in is a promise that all three are
  worth taking, and a canvas that is not yet good enough to hand a
  stranger should not be behind that door until it is.

- *Where does a re-read map's layout come from?* **A companion file
  beside the map.** The map file keeps meaning exactly what it meant,
  and the drawing lives in its own file that the canvas writes and
  reads.

  Positions were the wrong thing to put in comments, and the reason is
  worth keeping. Every comment the dump writes today — resolved types,
  element sizes, station indices — is a *derived* fact, which is why
  the loader can ignore comments and why stripping them costs nothing.
  A position is authored: nothing can recompute where somebody dragged
  a box. Putting one in a comment would make the comment channel
  lossy for the first time, so a tool that strips comments would
  silently destroy work.

  A companion file also stops the layout from being cramped by the map
  format's vocabulary. It can hold colours, groupings, notes, and
  anything else the canvas grows a use for without the map format
  learning a word for any of it. **The cost is the familiar one for
  sidecars**: two files that must travel together, and the one that
  looks unimportant is the one that gets lost. That is accepted — the
  map alone still opens, still loads, and still runs; what a lost
  layout file costs is an arrangement, not a program.

- *Is a WebAssembly build of the engine in scope?* **No, and not
  later either. The engine will never run in the browser.** Instead
  the page simulates: box durations that tick down, and a token in
  place of a value. Written up under "the simulated run" above,
  including what it can and cannot show, and the fact that it is the
  same machinery the slideshow needs.

- *What is in the bundled drawer?* **There is no drawer, and no
  upload either.** The question dissolved when the phase narrowed:
  **the workbench is for layout.** A box is a name you type; where a
  function's home matters, the file name is typed beside it; the
  download assembles a correct map file out of those names. No C is
  bundled with the page, read into the page, or handed back by it.

  This retires the issue's own strongest argument — that the page's
  parser must be the build's parser, compiled to WebAssembly — by
  removing the parser rather than sharing it. The conclusion that
  argument protected still holds and holds harder: **the page is never
  a second authority on what a box is**, because it holds nothing that
  could form an opinion. What stays compiled to WebAssembly is the map
  file's writer, which is the one thing the page genuinely could get
  subtly wrong.

  It also decides the phase's size. The drawer was what threatened to
  make phase 8 several issues; without it, the canvas, the shelf, the
  download and the simulator are the whole of it.

- *Does the page keep a map in progress, or is downloading the only
  way work survives?* **It keeps it, on the author's machine, and
  keeps several.** The drawing goes into browser storage as it is
  made and is there when the browser reopens, and the page carries a
  shelf of maps rather than one remembered document — start a new one,
  return to an old one, discard one. Nothing is sent anywhere, so
  "nothing on the server" is untouched. Written up under "keeping what
  you drew" above.

- *How does the page admit it cannot check a wire's types?* **One
  plain sentence, beside the download button.** The page says that it
  checks the shape of the graph and not the types of the values, and
  that the loader checks those the first time the map is run — placed
  at the moment the author is about to leave with the file, which is
  when the limitation starts mattering.

  Marking every wire as unverified was the alternative and is worse
  for a reason worth keeping: **a mark that appears on everything is
  not a mark.** When no wire on the canvas has ever been confirmed,
  drawing them all as unconfirmed adds texture rather than
  information, and the reader stops seeing it by the third wire.
  Letting the author declare types by hand was the other alternative,
  and it checks intent against itself rather than against the C, which
  is a check that passes when you were wrong about your own function —
  the one case where it was needed.

## The note that started this

Kept verbatim, because the reasoning in it is the specification:

> Hi, okay, so for the HTML documentation we need to update it a bit.
> Can you load the design tool and work on it again? It should be more
> artistic!
>
> I like the color scheme though. Very seraphic. Did I say angelic? I
> meant ceramic.
>
> anyway here's the update to the HTML. We should be able to run the
> tools and utilities in the browser. You'll be presented with a canvas
> that you can place and modify box stations on, and draw their wiring
> to connect. You should be able to fill in static values. If you click
> a button, it downloads the map as a file and also the source-code
> required to compile the functions that you used in the map. Oh by the
> way, you can push a + button to upload a file to your browser memory
> meaning you can use it in the map. The canvas we're creating doesn't
> run the map files, it simply helps generate them and the source-code
> that can be compiled to run the things that you need it to. By the
> way, we'll also have some of our own functions in a drawer to the
> side that you can make box stations out of. These functions are
> pretty common ones that interface with some of the software that I've
> made / hardware that I've worked with. Just, things that I think are
> especially common or important in concern. Anyway when you click
> download the javascript will compile and generate a file for you. Can
> we run a C compiler in javascript? If so, then yay! we can just
> provide the compiled binary and it's map file, which could be pretty
> cool.
>
> anyway you could download it and if you have a C runtime which
> everyone does then you can run the program. Yay!
>
> so, we can make boxes, choose their type and name, their wiring
> connections, which box function they call, and... anything else?
>
> We should not store anything on the server. We should not require
> users to download anything except the files they create. We will
> bundle some of our own stuff that they can see and read if they
> choose to use it - they can just use their own.
>
> I think this should be able to fully democratize computer
> programming, at least as much as any other system like this could.
> Like, a text editor and a compiler.
>
> new paradigm, new possibilities. Some people prefer it, some people
> don't.
>
> you can also write soramech in a text editor.

The note's first three lines are a different request from the rest of
it — the documentation set should be more artistic, keeping its colour
scheme. That belongs to [705](705-html-documentation.md)'s second pass
and is recorded there rather than here.

The last line is the constraint the whole phase is held to: *you can
also write soramech in a text editor.* The canvas is an alternative to
writing the file by hand, never a prerequisite for it, and a map
authored in a text editor must remain a first-class map. The moment the
canvas can express something the format cannot, the format is what
needs fixing.

The question in the middle — *anything else?* — is answered by the
station line and what may follow it: name, box function, and kind on
the line itself; per-slot exceptions saying a slot is static or
gathered; per-port destinations. Everything else about a station is
derived from the registry rather than authored, which is why the canvas
can validate without asking the author to declare types.

## Related

- [008 — Map file format](../docs/008-map-file-format.md), what the
  canvas emits, exactly
- [308 — The generator, in C](completed/308-generator-in-c.md), which was this
  issue's dependency while the page still read C, and is now only
  relevant to the packaging distance named above
- [604 — Load-time validation](completed/604-load-time-validation.md),
  whose rules divide into the ones the canvas can apply while drawing
  and the ones only the loader can reach
- [404 — Gather chains and cycles](completed/404-gather-chains-and-cycles.md),
  the check that most repays being shown rather than reported
- [703 — The map dump](completed/703-map-dump.md), the other direction
  of the same round trip
- [705 — The HTML documentation set](705-html-documentation.md) and
  [709 — The slideshow and the transcript library](709-slideshow-and-transcripts.md),
  the other two things that live on a page and share the aesthetic
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where the distance between the engine and someone else's machine was
  first measured
