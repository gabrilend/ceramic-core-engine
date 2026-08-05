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
until [308](308-generator-in-c.md) lands — LuaJIT.
[057](../docs/implementation-notes/057-packaging.md) is about that
distance: every inherited dependency is a reason somebody else's build
fails on a machine we never saw.

So the engine is approachable in principle and has no front door in
practice.

## Intended behavior

**A page you open in a browser, where you draw a map and leave with the
files needed to run it.**

Boxes are placed on a canvas. Each one is given a name, a kind — plain,
comparator, or iterator — and the box function it places. Wires are
drawn from an output port to an input slot. Static values are typed
into the slots that take them. When it looks right, one button hands
back the map file and the C source for every box function the map used.

**The canvas does not run maps.** It composes them. What it produces is
a schematic and the code that schematic refers to, which is exactly the
two halves a program has already been split into since phase 6 — a
directory of C functions and a text file. The workbench is a better way
to write the text file, not a second implementation of the engine.

This matters for scope. A canvas that ran maps would need the pool, the
delivery walk, and the loader in the browser, and would then be a
second engine that could disagree with the first. A canvas that only
composes has one job and one failure mode.

### Bringing your own boxes

A plus button reads a C file into the page. Its function declarations
become boxes available to place, exactly as if they had been designated
box sources in a build. Nothing is uploaded anywhere — the file is read
by the page, in the browser, and the page has no server to send it to.

Beside that, a drawer of functions the project bundles: common
operations, and interfaces to software and hardware worth having ready
to hand. They are there to be read as much as used — someone who wants
their own version can open one and see what a box looks like.

**The parser that reads those files must be the build's parser, not a
second one.** This is the strongest argument in the issue and it
depends on [308](308-generator-in-c.md). Once the generator is a
standalone C program that reads C source as text and writes C source as
text, with no dependency on the engine, it compiles to WebAssembly
without ceremony. The page then gets exactly the parse the build gets,
including the describe mode's account of what it found and the same
refusal on a malformed source. A hand-written JavaScript parser would
be a second opinion about what a box is, and the two would eventually
disagree — at which point the workbench would be handing people maps
that do not load.

### Checking while drawing

The value of a canvas over an editor is that the rules can be applied
at the moment the mistake is made rather than at startup. Every rule
the loader enforces ([604](completed/604-load-time-validation.md)) is a
rule the canvas can apply as a wire is dragged:

- a wire whose source return type and destination parameter type differ
- an arrow to a slot the function does not have
- a comparator whose box returns a type with no compare function
- a box fanning out to both a gatherer slot and a ring-buffer slot
- a cycle among gather wires
- port limits by station kind

The cycle walk in particular is the one that repays being visible: it
is a property of the whole graph, invisible in any single line of the
file, and trivially shown on a canvas by refusing to complete the
drag.

**These are the same rules, not a similar list.** However they are
implemented, the canvas and the loader agreeing is a thing to be
arranged deliberately, and the honest ways to arrange it are to compile
the loader's rule checks to WebAssembly alongside the generator, or to
have one written description that both are generated from. Two
hand-maintained copies is the option that quietly stops being true.

### Nothing on the server

No account, no storage, no upload. The page is static; the work happens
in the browser; the only things that leave are the files the author
downloads. A map is theirs, and the honest way to mean that is to have
nowhere to keep it.

The bundled functions ship with the page and are readable.

### On compiling C in the browser

The note asks whether a C compiler can be run in JavaScript, and
whether the page could therefore hand back a compiled binary rather
than source. The answer is worth writing down properly because it
shapes what phase 8 can promise.

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

**Producing WebAssembly that runs in the same page is tractable.** The
target is one the compiler already has, and no sysroot has to be
guessed at. The catch is that the engine is a thread pool: real threads
in WebAssembly need shared memory, which browsers only grant a page
that is cross-origin isolated, which is two response headers — a static
hosting concern rather than a server, but not nothing. A single-worker
build of the engine would run without any of that, and would be a
demonstration rather than the engine.

So: source download is the deliverable, a wasm build in the page is the
interesting stretch, and native binaries in the browser is a promise
worth not making.

### Why this is a phase rather than a feature

The canvas is an authoring surface, and authoring surfaces accumulate:
a validator that reports on a map without running it, a shareable
encoding of a map that fits in a link, a diff between two maps, the
dump read *back* into the canvas so a running map can be looked at as a
picture. Each is small beside the engine and none of them belongs in
the phase about seeing inside a running program. Phase 8 is where tools
that stand outside the engine live.

## Suggested implementation steps

1. The canvas alone: place, name, choose kind, wire, with a fixed
   hard-coded set of boxes and no validation. The interaction has to
   feel right before anything is built on it, and it is the part with
   no prior art in this project.
2. Emission: canvas to map file, matching
   [008](../docs/008-map-file-format.md) exactly, checked by loading
   the result with the real loader.
3. The generator compiled to WebAssembly (needs
   [308](308-generator-in-c.md)), so uploaded C files are parsed by the
   build's own parser and the drawer's functions are described the same
   way.
4. Validation as wires are drawn, from the rule list in
   [604](completed/604-load-time-validation.md), with the
   arrangement that keeps the two copies from diverging decided before
   the first rule is written rather than after the fifth.
5. Statics: typed into slots, written into the statics section — noting
   that [401](401-static-slots.md) and [405](405-statics-mutation.md)
   are retiring the shared table in favour of a value owned by the port
   that reads it, so this should be built against where the format is
   going rather than where it is.
6. Upload, and the bundled drawer.
7. Download: the map file plus the C for every function the map used.
8. Read a map file back into the canvas, which makes the dump viewable
   as a picture and makes the round trip testable.

## Open questions

- Does the workbench live inside the documentation site — same
  aesthetic, same sidebar, one more page — or is it its own thing that
  the site links to? They want to look alike either way; the question
  is whether it is documentation.
- Reading a map file back gives a graph with no positions. Does the
  canvas lay it out automatically, and if so does saving a map from the
  canvas write positions into the file as comments — which the format
  can carry, since the dump already uses comments for derived facts —
  or is layout deliberately not the map's business?
- What is in the bundled drawer? The note says functions that interface
  with software and hardware the author has worked with. That is a list
  only you can write, and its size decides whether phase 8 is one issue
  or several.
- Is the wasm build of the engine in scope for phase 8 at all, or is it
  its own thing? It is the difference between a page that writes
  programs and a page that also runs them.
- Nothing is stored on the server, so a map in progress dies with the
  tab unless the page keeps it in browser storage. Storing it locally
  is still "nothing on the server" — is that within the spirit of the
  note, or does the author keep their work by downloading it?

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
- [308 — The generator, in C](308-generator-in-c.md), which is what
  makes one parser servable to both the build and the browser
- [604 — Load-time validation](completed/604-load-time-validation.md),
  the rules the canvas applies while drawing instead of at startup
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
