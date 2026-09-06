# Conversation Summary: e1dd3730-6b88-4b9f-a4bb-32130c092d39

Generated on: 2026-09-06 00:23:11

--------------------------------------------------------------------------------

### User Request 1

hi can you tell me about this project, it's status, and a critical path to
utilization? I'd also like to know (briefly) about it's side projects - any
implementations (ceramic core engine or soramech is what you should grep for)
and any UI's that are built for it, and each of the project's rough
implementation stage. I know how the system works, I want to know what parts of
it are and aren't built.


--------------------------------------------------------------------------------

### Assistant Response 1

I'll dig into the project and its side implementations.


I read the docs, the phase progress pages, the open issues, and ran the test
suite. Here's the state.


## The engine — built, green, and honest about its edges


`make test` passes right now (I ran it, ~7 seconds, "all tests passed"). Nothing
in the core is broken.


| Layer | What it is | State |

|---|---|---|

| **The pool** | Worker threads, a ring of task pointers that doubles when full, workers that sleep rather than spin, shutdown decided by the last worker to fall asleep, signal handling | **done** |

| **Stations and delivery** | The station table, ring-buffer input ports, the readiness check at the tail of a write, fan-out on the way out, one construction surface anybody can call at any moment | **done, and extended well past the original plan** — a whole program can now be placed as a station inside another program |

| **The generator** | Reads your C, pulls out declarations and struct layouts, emits a wrapper per function plus every size as a `sizeof` the compiler evaluates | **done, and rewritten in C** — building the engine now needs a C compiler and nothing else |

| **Statics** | Constants parked on a port; writing one is an event that runs the readiness check, so a chain wired through constants behaves like a recalculation graph | **done**. The pull path that used to live here was deliberately removed |

| **Routing kinds** | Comparator, iterator, the three-way dispatch on the way out | **done** |

| **The map file** | Line-oriented parser, type checking every wire against emitted sizes, seed sweep | **done** — and then mostly dissolved: loading a map is now just a caller of the ordinary construction calls, so "still loading" is no longer a state anything can be in |

| **Seeing inside it** | Growth reporting, per-station counts and timings, dumping a live map back out as a map file, rewiring at runtime, **writing a running program to disk and picking it up again** | **done**, except the documentation site and the demos |


Two design tickets sit open on the pool — several queues rather than one, and
saying which queue a station's work goes to (which is where cross-machine and
per-socket placement would come from). Both are marked *nothing exists in code,
confer before building*, and both have already had a central decision reversed
once. They are conversations, not tasks.


## The three things that actually stand between here and using it


**1. There is no front door that runs.** `./run-demo` does not work — the
seven phase demos were deleted on purpose (they demonstrated a batch program,
which is the opposite of what this engine is for) and their replacements aren't
built. The script prints an explanation instead of failing silently, which is
right, but a visitor's first command still produces nothing. `make example` does
work and is currently the only runnable thing.


**2. The engine cannot leave the repository.** The packaging survey is thorough
and *nothing in it is built*. Concretely: about forty engine functions escape
with common English names like `map_create` and `pool_push`, so any host program
with its own notion of a map fails to link; one process-wide global means one
map per process, silently; every error calls `abort()` and takes the host down;
and the entry point is a file called `018-station.h`. The recommended fix —
concatenate the engine into one `.c` and one `.h`, which makes every internal
`static` for free — is three steps of work and would kill most of that list at
once.


**3. Nothing helps you compose a map except a text editor.** The browser canvas
draws stations, names them, gives them a kind and a box, wires them, and keeps
several drawings in the browser's storage. It has no validation, no simulation,
and **no way to produce a file** — it is a drawing surface, not a door.


## Critical path, depending on what "use" means


**To build something yourself, in-tree, today:** it already works. Write C
functions into the boxes directory, write a map file, build. This is exactly
what the train game did.


**To hand it to another project:** exclude the demo boxes from what ships (they
export `add`, which is the most collidable symbol in C), derive a single public
header from the sources rather than writing one, then the amalgamation script
that uses that header to decide what stays visible. Those three change the shape
of the thing. Prefixing, the error handler, and an out-of-tree build test are
finishing work after.


**To let somebody else draw a program:** the canvas needs the writer that turns
a drawing into map text. That writer exists — it was written in C during phase
8, after discovering the existing one only works on a *running* station table
and a canvas has no such thing — but it is not in the page.


**To watch a program run:** a build flag that makes a program leave a trail in
shared memory, then a page that reads it. Neither started.


## Three places the documents and the code disagree


- The phase 8 status page says the WebAssembly module "is a build artifact
  committed beside the page." There is no WebAssembly anywhere — not in the
  repository, not in the Makefile. The linker package was found to be available;
  the module was never built.

- The train game's roadmap links its phase 4 tickets to a `completed/` directory
  that does not exist, and to a datapath document that does not exist.

- The train game's build points at the old Lua generator, which was replaced by
  the C one. **It does not build against the current engine** — I tried; it
  stops immediately on the missing file.


## The side projects


**ceramic-trainstation** — the only real consumer of the engine, and the only
evidence it works for someone else. A train simulation: stations consume
resources at different rates, the player wires the routing, trains carry goods
between them. About 4,200 lines — a C driver, a wire protocol, a small server
joining the C process to a browser with no second runtime anywhere, and a
1,200-line page with a grid, a camera, dragging, an inspector drawer and a
palette. It builds the engine by copying its source in, which is the packaging
option the survey calls the worst one and supports only as a fallback. **It is
not under version control at all** — no git repository. Phases 1–3 were
assembled in one sitting with no tickets written, which the roadmap names as
"the debt." Phase 4 was done the right way round and the code looks present, but
nothing was filed. Last touched July 29, and stale against the engine as
described above.


**ceramic-any-language-plugin** — a vision file, 823 bytes, no code. It would
add back the multi-language bridge the minimal engine deliberately dropped, as a
patch kept in sync with upstream rather than a fork.


**ceramic-institutions** — a vision file, 215 bytes, no code. Represent a
place's institutions and economy as a map.


**The playground's own idea** — a vision and readme, no code. Use the engine
to run parallel LLM calls, one box per issue file, each asking "can you improve
this?" — issues racing each other to completion in parallel.


**The `original` branch of this same repository** — full SoraMech, and the
largest thing here: 592 files, 101 completed tickets, 179 test maps. It has the
real editor — eight browser modules covering the canvas, the box drawer,
wires, the inspector, a file browser and a source view, backed by a small Lua
server. Boxes in Lua, C, or Bash. Open tickets there head toward a llama.cpp
client and a hardware compilation target. It is where all of this came from; the
minimal branch is the distillation that dropped the bridge to focus on the
runtime underneath.


**UI inventory, since you asked specifically:** the original branch's editor is
the most complete and the only one that can actually run a map you drew. The
train game's view is second — live, playable, but tied to that one game. The
minimal engine's canvas is 750 lines and draws only. The generated documentation
site is 150 cross-linked pages with three of five planned interactive pieces
working — the ring buffer with a capacity slider, the readiness check firing
on the click that fills the last slot, and the iterator dealing under an
adjustable slow consumer.


Want me to write this up as a page you can keep, or start on any of the three
blockers?


--------------------------------------------------------------------------------

### User Request 2

  2. The engine cannot leave the repository. The packaging survey is thorough
     and nothing in it is built. Concretely: about forty engine functions escape
     with common English names like map_create and pool_push, so any host
     program with its own notion of a map fails to link; one

  process-wide global means one map per process, silently; every error calls
  abort() and takes the host down; and the entry point is a file called
  018-station.h. The recommended fix — concatenate the engine into one .c and
  one .h, which makes every internal static for free — is three

  steps of work and would kill most of that list at once.



can you update the issue files for the work that is needed for this? You can
edit them to reflect the correct design. Most of the issue files will be
completed/ issues that we need to edit. We can leave them in completed/ since
we're going to implement that now. Having one .c and one .h file is perfect,
exactly what I want. If an issue file says something like "and then in
src/directory/file.c we put this-and-this" then we should delete that line
because the assumption is that it goes into the core .c file. We would instead
say "this issue builds function abc() and it's signature in the .h file is int
abc()" or similar. Then, we should build a NEW file that is the main.c and
main.h file that we'll need. Well, don't call it main.c, call it cera.c and
cera.h instead. Once it passes all the same tests (updated only to point at the
new file in addition to the old file) and the output is byte identical (the
output of the tests I mean) then we can remove the old, deprecated src files.


  3. Nothing helps you compose a map except a text editor. The browser canvas
     draws stations, names them, gives them a kind and a box, wires them, and
     keeps several drawings in the browser's storage. It has no validation, no
     simulation, and no way to produce a file — it is a drawing

  surface, not a door.


this is fine. We have separate projects for building web UIs.


My memory is that we had plans for a separate web-based UI, two of them, one
which lets you edit maps and one which lets you view them in operation. Where
are we at with that? Let's focus on the refactor, but I want a report on where
this is at.


--------------------------------------------------------------------------------

### Assistant Response 2

I'll gather the ground truth before editing anything — the current file
layout, what the completed issues say about placement, and the two web UI
tickets.


Let me read the current public surface and headers so the blueprints are
accurate.


Now the header surface, so the blueprints name real signatures.


Tests reach into internals, and the generated file is a separate translation
unit. Let me measure what that forces to stay public.


Before writing blueprints I'll verify the amalgamation is actually possible —
static name collisions across the eleven engine files are the thing that would
sink it.


Collisions are all forward-declaration pairs inside single files — no
cross-file clashes. The amalgamation is mechanically safe. Writing the
blueprints now.


Blueprints are in place. Now implementing 901 — first the baseline capture,
since "byte-identical" needs a *before*.


Four tests report counts produced by concurrent scheduling — nondeterministic
by construction. I'll compare those by shape and the rest byte-for-byte.


Baseline is reproducible. Now let me check for anything else that breaks when
eleven files become one.


No cross-file type or macro collisions, except that the build-time facts are
used by two sections — so those must not be undefined. Writing the
amalgamation tool.


cera.c compiles clean under `-Wall -Wextra -Werror` on the first try. Now wiring
the build to it.


All tests pass from the one-file build. Now the actual proof — byte-for-byte
comparison against the baseline.


One difference, and it is the intended one: the emitted file's include block
lost exactly four lines. Verifying the rest of the build.


Everything builds and runs. Recording the state in the issue files now.


Everything is green: all tests pass, the example runs, 158 doc pages with zero
unresolved references. Let me check the transcript file and stage only my own
changes.


Committed cleanly, with the other session's files left untouched. Now the report
you asked for.


Done and committed. Here's where everything stands.


## The two web UIs — where they actually are


Your memory is right: there are two, and they were deliberately split apart
rather than drifted apart.


**The editor (ticket 801, "the workbench") — drawing works, the door does
not.** 750 lines across three files. You can place stations, name them, give
each a kind and the name of a box function, add and remove ports, and drag wires
from an exit to an input. It keeps several drawings side by side in the
browser's own storage, so closing the tab doesn't lose them. What it cannot do
is produce a file — no validation, no simulated run, no download. It is a
drawing surface, not a door.


Two scope decisions were made since you last looked, and both shrank it:


- **It holds no C at all.** The earlier design had a plus button that read your
  C files into the page and a bundled drawer of functions, with the build's own
  parser compiled to WebAssembly so the page and the build couldn't disagree
  about what a box is. All of that is gone. You type a box's name; the page
  cannot tell you whether it exists, because the thing that would answer is a
  compiler and there isn't one in a browser. Holding no C satisfies "the page
  must never be a second authority" more completely than sharing a parser would
  have.

- **The one piece that stays WebAssembly is the writer** — turning a drawn
  graph into the map file's exact text. That's the one place the page could be
  subtly wrong and the one place the project already has correct code.


That writer now exists, in C, and it was written *because* of a discovery: 801
planned to compile the existing map-writer for the browser, and found there was
nothing to compile. The existing one walks a **live station table** and says
what the engine is running; a canvas has a drawing, not a table. So there are
deliberately two writers answering different questions — what is running, and
what was written down. The new one is done and tested. **It is not in the page
yet**, and that is the single thing standing between the canvas and being
usable.


One correction to what the status page claims: it says the WebAssembly module
"is a build artifact committed beside the page." There is no WebAssembly
anywhere — not in the repo, not in the Makefile. What actually happened is
that the missing linker turned out to ship as a package in the same LLVM
version, so the *blocker* is gone. The module was never built.


**The viewer (tickets 802 and 803) — not started, and it's two tickets because
it's two problems.**


- **802 is the trail.** A build flag makes a program leave a record in shared
  memory as it runs — every station that ran, every value that moved, every
  buffer that grew — in a ring any number of processes can read. Without the
  flag the emitting isn't compiled at all: not a branch that's usually false, an
  absence. A slow reader loses events and is told how many rather than holding
  the program up, because watching must never change what is watched.

- **803 is the page that reads it**, and it **cannot touch what it looks at**.
  No button starts a station, no field writes a constant. A viewer that could
  reach in would be a debugger, and a debugger is allowed to change what it's
  looking at — which is the thing 802 refuses. Its one genuinely awkward part
  is admitted up front in the ticket: a browser page cannot map shared memory,
  so something has to stand between the ring and the page, and what that
  something is decides how much of the work is plumbing rather than picture.


**They share nothing, on purpose.** The workbench is a door and the viewer is a
window. One draws a picture where every station is alike and the point is what
you're about to connect; the other draws a picture where the point is which
station is busy right now. A drawing layer serving both serves neither. The only
thing they may share is an understanding of the map file format, because there
is one format and two readers of it would be two things that must agree.


## The refactor — 901 is done


`src/cera.c` (8,478 lines) and `src/cera.h` (2,935 lines) exist, the whole build
runs from them, and every test passes. It compiled clean under `-Wall -Wextra
-Werror` on the first attempt, with **no edit to any code that moved** —
nothing collided, no name and no type.


I proved it by comparison rather than by the tests passing. Every test's output
was captured from the old build and the new one and diffed byte for byte.
**Thirty-three of thirty-four match exactly.** The thirty-fourth quotes a
compiler's own complaint about a line number inside generated code, and that
file's opening lost precisely the four lines that five `#include`s collapsing
into one costs — 17 became 13.


Building the measuring instrument found something worth keeping: four tests race
threads and then report what happened, so their numbers differ on every run of
an unchanged binary. That's those tests working. They're compared by shape now,
and they're **named** in the capture script rather than detected, so a fifth is
somebody's decision instead of a silence. I checked the instrument reads zero by
diffing two runs of the unchanged build against each other before using it to
measure anything.


Three sections take their private macros with them on the way out. One set —
which compiler built the binary, where the generator is, the two RAM tiers —
deliberately does *not*, because two sections read them and the second would
have lost them.


**On your instruction about the completed issues:** I grepped all 74 and found
almost nothing to delete — they were already written abstractly, and only one
names a source path. The bigger half of what you asked for is the *positive*
statement, "this issue builds `abc()` and its signature is `int abc()`", and I
deliberately deferred that to ticket 902 rather than doing it now. Writing a
signature against a header that still holds every internal joint means writing
it twice. 902 narrows the header first, then writes the settled signatures back
into the completed issues — which turns them, read in order, into the
derivation of `cera.h`.


**The old files are still on disk, compiled by nothing.** That's ticket 904 and
it's deliberate — the duplicate is what made the comparison possible. Current
numbers to beat: a linked test binary publishes 102 symbols and defines 289.
Ticket 903 is what moves those.


Two things you should know that I didn't act on: there's uncommitted work in the
tree from another session (the multi-queue design tickets and some doc
corrections) which I left alone, so the generated HTML mirror is also
uncommitted — regenerating it folds their markdown edits in with mine and I
didn't want to bake that into my commit. And the train game in the sibling
directory doesn't build against the current engine at all; it names the Lua
generator that was replaced by the C one. Nothing noticed because nothing here
runs it. That's why ticket 907 exists.


**[Asked the user]**


*Q1 — Prefix:* Which prefix should every public engine name carry? This blocks
ticket 905 and applying the second one costs what the first did.

- cera_ — Matches the two filenames and what the readme calls the thing —
  the ceramic core engine. Costs renaming the ~15 symbols already on sora_, plus
  every include guard.

- sora_ — Already on stopping, the text conversions, and every generated
  station-builder. Renames fewer symbols. But the project's front door never
  says "sora" — the name is inherited from the larger project this was
  distilled out of.

- Something else — A shorter or different prefix — e.g. cc_, ce_, or a name
  you'd rather the engine answered to.

→ *(no answer recorded)*


That dissolves the question rather than answering it. Let me look at what you've
pointed at:


```

soramech

ceramic

```


Same consonants in the same order — S · R · M · K. The vowels are all
reduced to nothing much, and the trailing `h` is silent. Positions 3, 4, 5 and 7
are literally the same letters. These were never two candidate names for the
engine; they are one name spelled twice, and the distillation onto the `minimal`
branch changed the spelling without changing the word.


Let me record that, because it's the actual content of the ticket now.


Recorded and committed. The ticket now says what it's actually about.


**What changed in the blueprint:** ticket 905 no longer asks which prefix. It
states that the engine was never renamed when it came onto this branch — it
was *respelled*, the respelling stopped halfway, and nobody noticed, because a
half-finished respelling doesn't feel like a collision when there was only ever
one name to collide with. That's why both spellings have coexisted in the source
for months without anything seeming wrong.


What survives as a decision is only which spelling a consumer meets, and the
files settle that without preference entering into it: somebody holding `cera.h`
and writing `#include "cera.h"` should not then call something spelled
`sora_map_create`. The include and the call agree, or their first line hands
them a puzzle about whether they've got two libraries. So `cera_`, and the cost
— the fifteen-odd symbols already spelled the other way, plus the guards —
was never the price of choosing. It's the price of the respelling having been
half-done since the branch was cut.


The phase-9 status page carries the same observation, so somebody arriving at
the status list rather than at the ticket gets told too.


**Where the refactor stands:** 901 is complete and committed — one file, one
header, whole build running from them, thirty-three of thirty-four test outputs
byte-identical and the thirty-fourth explained down to the exact four lines. The
old numbered sources are still on disk, compiled by nothing, which is what makes
904's comparison repeatable.


Next in dependency order is 902 (narrow the header to what generated code binds
to and what a consumer calls) and then 903 (everything else becomes `static`,
which is the step the whole phase exists for — it takes the exported symbol
count down from the 102 a linked test binary publishes today). 902 is also where
the completed issues get their "this issue builds `abc()`, and its signature is
`int abc()`" sections, written once against the settled header rather than
twice.


905 can now run whenever you want it, and it's independent of 902 and 903 —
though doing it last means renaming a shorter list.


Two things still sitting in the tree that aren't mine and I haven't touched: the
other session's multi-queue tickets and doc corrections, and the regenerated
HTML mirror, which I left out of both commits because rebuilding it folds their
markdown edits in with mine.


--------------------------------------------------------------------------------

### User Request 3

great let's work on those. don't worry about other project's work in the
monorepo. Do however, worry about your llm-transcripts, and include them in any
relevant commits.


--------------------------------------------------------------------------------

### Assistant Response 3

Good — then let me take 902 through 905 properly. First I need the exact
current surface of the header.


Now let me see how these declarations sit in the header, so the move preserves
their documentation.


The internal joints cluster neatly at the tail of the station section. Let me
find which tests reach into them, since those have to move inside before the
header can narrow.


Seven test files reach inside. Moving them into the translation unit first —
that's what unblocks the header.


All green. Now narrowing the header — moving the internal joints, with their
documentation, into the body.


13 of 19 moved cleanly; 3 share a documentation block with a public call. Let me
place the moved block into the body and handle those three by hand.


Zero — because the forward declarations satisfy it. The real enforcement has
to compare exported symbols against the header. Let me derive the list that way.


23, including two the derivation caught that I hadn't listed. Applying `static`
to each.


Two joints are reached only from the white-box tests, never from inside the
engine — so compiling the engine alone, the compiler is right that they're
unused. Marking that honestly rather than deleting engine capability
mid-refactor.


123 → 100. Now the test that makes private-by-default a property that can't
decay.


A test that can't fail is worthless. Let me verify it catches a leak.


Every existing test output is byte-identical; the only new files are the new
test and the example. Updating the tickets.


Now 905 — the prefix. There's already a rename tool in the project; let me
check it before writing another.


A purpose-built one-pass rename tool with whole-identifier matching already
exists. Let me enumerate the full public surface — types and macros collide as
surely as functions do.


Struct tags I'll leave alone — renaming the tag `map` would rewrite the
English word throughout the documentation. Functions, typedefs and constants are
all safe. Building the mapping.


A regex artifact slipped in. Let me filter properly and review the whole list
before applying anything.


Three enum counts and one environment variable were missed. The env var matters
— it's the one rename a person types rather than a compiler reads.


4157 replacements. Before applying, let me do 904 first — deleting the dead
sources means the rename doesn't touch code nobody compiles.


The `ls` error is just the now-empty `libs/` being gone. Now collapsing the two
include paths into one.


Exactly the straggler 904 exists to catch. The generator's map parser still
includes a deleted header.


All green with the numbered sources gone. Cleaning the remaining references.


All illustrative examples pointing at files a reader would now go looking for
and not find. Updating them to live files, and marking the packaging survey as
executed.


Let me verify 904 changed no behaviour, then apply the prefix.


Byte-identical. Now the prefix — but first, the rename tool must not touch the
transcripts, which are a historical record.


Transcripts are excluded by default. Applying the respelling first, then the
prefix.


The `\b` didn't match inside `-DSORA_CC` — `D` and `S` are both word
characters, so there's no boundary there. Fixing the compile-line definitions.


All tests pass (that error line is a deliberate one — the late-box test
compiles a broken box to check the refusal). Now collapsing the export list,
which the prefix makes a single pattern.


Zero output differences across the whole rename. Let me measure the result and
check the example.


The moved and deleted paths weren't in the commit's path list. Folding them into
the same commit.


The build is clean and committed. One thing though — 902's blueprint promised
the completed issues would state their signatures, and I marked it complete
without that. Let me finish it rather than leave a ticket lying about its own
state.


Only six — because the house style deliberately keeps function names out of
prose. That changes the shape of what's left.


Those tables list calls the tickets merely mention, not calls they build —
misleading. Let me back them out and put the complete surface where it belongs,
in the interface document.


Done. Five commits, all tests green from a clean build, and the tree holds
nothing of mine.


## What the engine is now


`src/cera.c` and `src/cera.h`. Nothing else. The eighteen numbered sources,
their eighteen companion documents, and the `libs/` directory are deleted. A
consumer takes two files and adds one include path — or none, if they sit
beside their own source.


**It exports 100 symbols and every one begins `cera_`.** It exported 123 before,
all in ordinary English words. The twenty-three that went are `static` — not
renamed, not behind a convention, but genuinely not linker symbols, so a host
program with its own notion of a map or a pool has nothing to collide with.


**The list was derived, not written.** Compile the engine alone, ask the object
file what it publishes, subtract what the header declares. That's why it caught
two functions nobody knew were escaping — neither had ever been declared in
any header. Nobody reading would have found those, because reading finds what's
written down and their whole problem was that nothing was.


**And the same subtraction is now a test.** Write a function, forget the word
`static`, and the build names it and says the fix is either that word or a
deliberate line in the header — and that those mean opposite things, so it
declines to guess. I proved it by making it fail before believing it.


## What each step cost, and what it found


**The rename** was 3,008 replacements plus 706 respellings across 52 files,
reaching the header, the body, the generator that writes engine calls into
everything it emits, the tests, the example, the linker's export list, and the
prose in the documents. **Not one byte of test output changed.** The export list
collapsed to a single line — it only named two families because the engine was
spelling its own name two ways — and stayed a file anyway, because what won't
fit in a link command is the thirty lines around the pattern explaining what the
sweeping alternative costs.


One thing cost a build, and it's worth your knowing because the tool was right
and my expectation was wrong: **a word boundary sits between the dash and the
letter, not between the two letters after it.** A whole-identifier rename finds
`CERA_ROOT` standing alone and does not find it in `-DCERA_ROOT` on a compile
line. It surfaced far from the cause, as a test unable to see a macro the
Makefile was plainly defining. Uses and definitions aren't written the same way.


**Seven tests moved inside the engine** — they examine slots, pages,
destination sets, the constants a port holds, and a private function can't be
reached from outside no matter what's declared. That exposed something: **two
joints have no caller in the engine at all.** Only tests want them. I marked
them as expected-to-be-unused rather than deleting, because whether the ordinal
form of a slot move should exist when the engine only ever uses the address form
is a real question about the engine's shape, and a refactor is the wrong place
to answer it quietly. That one's yours.


## Where I stopped short of what you asked


You asked that completed issues say "this issue builds `abc()` and its signature
is `int abc()`". I tried to derive that and **six issues out of seventy-four
name a call at all** — three of those being phase 9's own, naming calls only
as examples of what got moved.


That isn't the blueprints failing. It's your house style working: describe a
function in English, because a name isn't a thing a reader can reason about. The
tickets say *a station added to a running program is checked and started by the
ordinary readiness check*, which is the right sentence, and it contains no
symbol. So the mapping can't be extracted — it has to be decided one blueprint
at a time by somebody holding both the blueprint and the header.


What I did instead: all 101 calls with their signatures, generated from the
header into `cera.h.info.md`, grouped into the seven sections. Derived, so it
can't drift. The per-blueprint pass is filed as its own ticket rather than left
sitting inside a "complete" one — a day's reading, not an afternoon's
scripting.


## What's left in phase 9


Three tickets, none about shape. A host still can't hear *why* the engine
stopped, only that it did. One process-wide variable still means one map,
silently. And **nothing has yet compiled this engine from a directory that
cannot see this repository** — so every claim above is inference from reading
rather than from doing. That last one is the one that matters, because it's the
only one that can prove the rest, and it exists because the train game in the
sibling directory doesn't build against the current engine and nothing noticed
for weeks.


--------------------------------------------------------------------------------

### User Request 4

I think the vimfolds in cera.c are a little off, at least the top one. The page
opens to a single closed vimfold that says this:


+----/* {{{ out_port_dests() / dest_set_build() / dest_set_retire() - issue 214


it's probably missing a closing bracket or there's an open bracket in a comment
somewhere. The c files should read like header files, and opening a vimfold
should be like "expanding" the implementation to display just there.


also, can we remove all the justification text for why we did things from the
comments? Just describe how it is, not how it got there.


  Three tickets, none about shape. A host still can't hear why the engine
  stopped, only that it did. One process-wide variable still means one map,
  silently. And nothing has yet compiled this engine

  from a directory that cannot see this repository — so every claim above is
  inference from reading rather than from doing. That last one is the one that
  matters, because it's the only one that can

  prove the rest, and it exists because the train game in the sibling directory
  doesn't build against the current engine and nothing noticed for weeks.



let's work on these three.


--------------------------------------------------------------------------------

### Assistant Response 4

Let me diagnose the fold breakage first — that's concrete.


Three unclosed folds — so the first one swallows the whole file. Let me find
exactly where the depth goes wrong.


Now I see it — nested folds, and my chunk-mover took an outer fold's opener
with an inner chunk. Let me check which of these I broke versus which were
already broken.


Two of these were already broken before the merge (the loader and the station
header); my chunk-mover added a third. Let me load the project's own guidance
for the comment pass, since it directly covers this.


--------------------------------------------------------------------------------

### User Request 5


--------------------------------------------------------------------------------

### Assistant Response 5

The skill's gate: my work is committed, nothing is staged, exempt directories
are known, phases 1–9 are discoverable. The only uncommitted files are the
other session's, which you've released me from. Proceeding.


Let me measure the comment mass before touching it.


Nearly half of `cera.c` is comment. Let me fix the folds mechanically first —
the two in `cera.h` by hand, since they're a stray closer and one missing.


Now the `cera.c` rebuild. Let me write it as a tool and dry-run it first.


Balanced, and nothing but markers changed. Let me see what the collapsed file
actually reads like.


Three folds came out unnamed. Let me look at what they are.


Type definitions — my namer only looked for functions and variables. Let me
teach it types and re-run.


Every fold named. Verifying the build is unaffected.


That's a growth report from one of the racy tests — present in one run, absent
in the other. Let me confirm it varies run-to-run on the same binary rather than
being caused by the refold.


Comments can't change behaviour, but let me prove it rather than argue it —
compile both versions and compare the objects.


The filenames differ, which shifts string tables. Let me redo with identical
filenames in separate directories.


Proven identical object code — the test difference was the known race under
build load. That's a real limitation of my output-comparison instrument worth
recording.


Now the comment pass. Let me extract just the comment blocks with their line
ranges, so I review 4,000 lines of prose rather than 8,800 of file.


This shows the real damage from my chunk move — a comment describing the
public wiring call is stranded here. Let me check whether the header lost it.


Confirmed — the header's wiring call lost both its fold and its comment to my
move. Let me restore it and clear the stray.


The header's folds are nested from the merge, so the wiring call ended up inside
a neighbour's fold. Let me restore it and start the comment pass, staging
genuine history for the progress files.


Now the strip itself. Batch one — the joints block and the pool.


Compiles-check, then a mechanical pass for the provenance references — those
are safe to remove wholesale.


37 remain, embedded in prose. Let me pull their context and rewrite each.


Zero references left. Now the history and justification prose — let me find
what still describes states the system isn't in.


84 statements to work through. Let me pull the enclosing blocks so I rewrite
whole thoughts, not fragments.


Rich blocks — current fact welded to history. Let me cut at the weld,
harvesting the complete transitions.


The write never happened, so the header is untouched. Let me find the exact
wording that didn't match.


Those two are ordinary present-tense prose. The header's clean. Now the body's
61.


Line-spanning references keep tripping the matches. Let me handle those
generally first.


Let me make the applier report failures instead of aborting on the first.


Now the second-range rewrites, with a tolerant applier.


Most are false positives — present-tense prose. Let me check the four that
aren't.


Proven comments-only. Now filing the harvested history into the phase progress
pages, where the development journey lives.


My commit swept in the other session's progress-page edits, which link to two
tickets still untracked — a dangling reference I created. Let me complete it
rather than leave it broken.


Tree's clean. Now the three tickets. Let me check the ground truth for both
engine-side ones first — one of them may already be resolved.


Two findings. Let me look closer at both.


Consistent shape: a message to stderr, then `abort()` or `exit()`. Let me build
the funnel. First the existing exit-code vocabulary.


Now implementing 906. Let me see all thirty death sites at once so I can
classify each.


Clear classification: resource exhaustion, bad caller argument, and genuine
engine bug. Let me build the funnel.


Now routing all thirty sites through it.


Both are correct — the funnel itself, and the quit path that must take no
locks. Let me note why the second bypasses the funnel, then build.


All green. Now a test for it — a handler that's never proven to fire is a
handler nobody can rely on.


906 is done and proven. Now 908 — let me check whether its premise still
holds, since the globals list showed no active-map pointer.


Two programs already share a pool. But 908 is about two *independent* maps. Let
me look at how a test builds one, then prove it.


Now the proof for 908 — two fully independent maps, each with its own pool.


All green. Now 907, the capstone. Let me see how the example builds and runs a
map, since the out-of-tree program must do the same.


Now the out-of-tree build test — the one that proves everything else.


`/dev/shm` is the non-executable tier — that's precisely what the project's
two-tier rule is for. Moving the build to the exec tier.


The map reached for one of this repo's demo boxes — exactly the leak this test
exists to catch. Let me make the consumer own both.


The engine builds and runs outside its own repository. Full suite:


All four things are done and committed. Tree is clean apart from two files that
aren't mine.


## The folds


You were right about the cause, and it was worse than one missing bracket. Three
folds in the body opened and never closed, so the first swallowed everything
after it, and the header had a doubled closer plus one that ran off the end.


**Two of them predate the merge** — the loader and the station header were
already unbalanced before eighteen files became two. Nobody would have seen it;
a stray marker in a file you scroll through looks like nothing. **The third was
mine**: the tool that moved the joints out of the header worked in
blank-line-separated chunks, and folds don't respect blank lines, so an outer
fold's opening line travelled with an inner chunk and left its closer behind.
That same bug had also dragged the wiring call's entire explanation into the
body, where it sat above an unrelated function while the declaration it belonged
to stood bare. Both are fixed.


I didn't patch them. Markers are now discarded wholesale and rewritten from the
code — one fold per definition, from the top of its comment down to the brace
that closes it, named for what it defines. **256 definitions, all named,
balanced.** Shut, the file is a list of what it holds in the order it holds
them; opening one expands that definition and nothing else. Section banners stay
outside every fold so they remain visible as headings. Because it discards
rather than repairs, a fold that has drifted from the code can't survive a run.


## The justification text


About 900 lines gone. Three hundred references to the tickets that built each
piece, and the paragraphs explaining what a thing used to be, why the old shape
was wrong, which alternative was refused, and why the present answer is good.


What stayed is what the code can't say about itself: why a state byte sits
*after* a slot's value rather than before it, which lock a copy is or isn't
under, that a comparator's last port isn't one of the box's parameters. Those
got shorter, not fewer.


**Nine complete transitions were carried out rather than deleted** and filed
under the phase whose work they belong to — a slot learning to say what's
happening to it, destinations becoming an array nobody edits, the statics table
going and taking the process-wide singleton with it. Each names the old state,
why it was that way, the new state, and what changed the answer. Fragments that
couldn't supply all four were residue and stayed in git history.


Proof it changed nothing: the engine compiled before and after, from identical
filenames in separate directories, is byte-for-byte the same object file.


## The three tickets


**A host can hear why the engine stopped.** This needed a funnel before it
needed a handler — thirty places wrote to stderr and then ended the process,
each its own little ending, so there was nowhere to be told from. Collecting
them named a distinction the code was already making silently: most refusals are
a fault outside the engine and exit with a code a caller can act on, while six
in the delivery path are the engine finding a fault in *itself* and abort,
because a core is the evidence. One death stays outside the funnel deliberately
— the quit signal takes no locks, since the reason it arrived may be a lock
nobody will release, and a handler is somebody else's code.


**Two maps in one process needed no code.** The pointer had already gone when
statics moved onto the ports that read them, and nobody had noticed either way,
because a singleton is invisible until two of something exist and nothing had
ever built two. There's a test now.


**The engine builds outside its own repository.** `make test` copies it into a
directory that can't see this one, writes a box and a map and a program from
nothing, and checks the answer. It caught two faults on its first run — which
is the argument for writing it rather than reasoning about it. It was putting
its scratch tree in the RAM tier mounted *noexec*, which is exactly why the
project keeps two tiers. And its map named one of this repo's own demo boxes, so
it would have passed for a reason having nothing to do with portability.


Phase 9 is eight of nine. What's left is ticket 909 — the blueprints describe
behaviour without naming the calls that provide it, and that can't be derived
because the house style keeps function names out of prose.


Still owed, and outside this repo: the train game doesn't build against the
current engine. It's the natural second consumer now that the first one passes.


--------------------------------------------------------------------------------

### User Request 6

I wrote a single edit to the cera.c file to show proper placing - there should
be a newline between each closing vimfold and the opening one below it. Can you
apply that formatting fix to the rest of the src file?


Also there's several clear "sections" in the file, they should each have their
own vimfold as well, so we can condense functionality, not just at the function
and datastructure level but also one level above them, at the "component" tier.
We don't need to justify our vimfold choices inside the text. That can be for a
git commit.


I don't understand what 909 is about, can you explain it in one sentence?


--------------------------------------------------------------------------------

### Assistant Response 6

Let me find your edit first.


Clear — a blank line between a closing fold and the next opening one. Let me
see the section boundaries so I can wrap those too.


Twelve sections. Let me look at the file's own header before I change the tool,
since some of it explains fold choices.


Now updating the tool: section folds, and a blank line between a closing fold
and the next opening one.


Balanced, twelve components, spacing clean. Now trimming the file header — it
still justifies the arrangement and names a script that no longer exists.


Let me check the tool is actually idempotent — the line count moved on a
second run.


Each run adds a blank line — the strip leaves behind the blank the tidy pass
added. Let me make the strip collapse blank runs so the tool is idempotent.


Done. `cera.c` now folds at two tiers, the tool is idempotent, and the object
code is byte-for-byte unchanged.


Shut, the file is twelve lines:


```

/* {{{ The joints — how the engine reaches itself */

/* {{{ 012 — the pool */

/* {{{ 019 — the station table */

/* {{{ 020 — delivery, readiness, routing */

/* {{{ 027 — support for generated code */

/* {{{ 033 — constants, and values from text */

/* {{{ 042 — reading a description */

/* {{{ 050 — reports and the observer */

/* {{{ 051 — a live map written back out */

/* {{{ 052 — changing a running program */

/* {{{ 074 — boxes and maps compiled at run time */

/* {{{ 092 — signals, capture, and the end */

```


Open one and you get that component's definitions; open one of those and you get
the definition and its comment.


Two things worth flagging. The tool **was not idempotent** — it left behind
the blank line each removed marker had sat beside and then added another, so
every run grew the file by a line per component. Fixed, and checked by running
it twice and diffing. And I pulled the paragraph arguing for one translation
unit out of the file's own header, along with a sentence naming the script that
built it — which was deleted three commits ago and would have sent a reader
looking for a file that isn't there.


## What 909 is about


**Reading `issues/completed/` in order tells you what the engine should do, but
never once tells you what any of it is called.**


That's a consequence of your own house style working as intended — the
blueprints say *a station added to a running program is checked and started by
the ordinary readiness check*, which is the right sentence and contains no
symbol. Six issues out of seventy-four name a call at all, and three of those
are phase 9's own, naming calls only as examples of what got moved.


So somebody rebuilding the project from those blueprints knows exactly what to
build and has to invent every signature. The full list of 101 calls does exist
now, generated from the header into `cera.h.info.md` — what's missing is the
arrow pointing the other way, from each blueprint to the calls it produced. That
can't be extracted from the text; it has to be decided one blueprint at a time
by somebody holding both the blueprint and the header.


--------------------------------------------------------------------------------

### User Request 7

Okay so the single source-file looks great. Can we do the same thing with the
header file? A lot of the comments in the header file seem to belong in the
info.md file instead... The header should have very minimal comments. Also it's
function declarations should exactly match the .c file in order and vimfold
structure.


--------------------------------------------------------------------------------

### Assistant Response 7

Let me map what the body defines, component by component, so the header can
mirror it.


A few names show up in two components. Let me check whether those are real
definitions or regex artifacts.


Those are redundant forward declarations in the body — the header already
declares them. Let me look at why they're there.


Leftovers from when this was many files — the reason given no longer exists.
Let me remove them, then rebuild the header.


101 definitions in one clean order. Now let me parse the header into its blocks.


Let me first move the error funnel's definitions into the component where ending
a program lives, so both files agree.


The refold had already split those into separate folds. Let me find them.


Now the header. Let me extract its folds and see what each declares.


The header mixes types and functions inside single folds. Let me parse it into
declaration units directly, the same way I parsed the body.


Now the generator. Above-declaration prose goes to the info file; the header
keeps declarations and fold names.


Let me check the multi-line signatures survived, then compile.


A dependency I broke — the generated-support types use the text types. Types
must keep their existing order; only functions get reordered.


Compiles clean. Full suite:


All green. Let me verify nothing was dropped and see how much comment is left.


59% is still mostly struct field comments. Let me look at one.


Let me move the field documentation into the info file and leave the structs
bare.


The header is now 6 lines of prose. Let me assemble the info file that now
carries everything.


Two fixes needed — the markdown still carries C comment markers, and one
struct lost its name. Let me regenerate cleanly from the old header.


The prose got mis-attributed — a wrong attribution in a reference is worse
than a missing one. Let me key it off the parser that already worked, run
against the old header.


Found it — I was keying off every identifier in the declaration, so a
parameter type pulled in its own prose. Keying off the fold's own label instead.


Attribution is right. Let me clean up the fold labels — a few came out as raw
declaration text.


Types should keep the author's own grouping — only functions need reordering.
Let me rebuild using the old header's fold boundaries for types.


The pool's typedefs live outside any fold in the old header, so my fold-based
pass missed them. Let me collect the loose declarations too.


One label names functions that moved elsewhere. Let me relabel type groups from
what they actually declare.


Now the test that keeps the two files in step.


Only `cera_fail` and `cera_bug` — which are private and correctly absent from
the header. The body scan must skip statics.


The field tables aren't landing — the label and the table key differ. Let me
key off what a declaration *defines*, not what it references.


The prose landed correctly, but some phase-history sentences rode along from
inside the struct bodies. Let me count them.


Done. `cera.h` went from 2,601 lines to 807, of which six are prose.


Shut, the two files fold to the same eleven lines:


```

/* {{{ 012 — the pool */

/* {{{ 019 — the station table */

/* {{{ 020 — delivery, readiness, routing */

/* {{{ 027 — support for generated code */

/* {{{ 033 — constants, and values from text */

/* {{{ 042 — reading a description */

/* {{{ 050 — reports and the observer */

/* {{{ 051 — a live map written back out */

/* {{{ 052 — changing a running program */

/* {{{ 074 — boxes and maps compiled at run time */

/* {{{ 092 — signals, capture, and the end */

```


Open a component on either side and you're in the same part of the engine, with
the calls in the same order. **That order is invisible to a compiler** — a
shuffled header builds exactly as well — so I added a test that reads the
order out of each file and diffs them, naming any call declared somewhere other
than where it's defined. I checked it by moving one declaration and watching it
complain.


Everything that left went to `cera.h.info.md`: what each call does, and every
struct's fields as a table.


Three things fell out of lining the two up:


**Three dead forward declarations.** Public calls re-declared inside the body,
with a comment saying it avoided including a whole header — true when this was
eighteen files, meaningless now that the one header is included at the top of
the one body.


**The error funnel was in the wrong component.** I'd put it among the joints;
it's moved in with the rest of the ending, where the header can mirror it. Its
two private entry points stay declared with the joints, since every component
calls them.


**Types can't be reordered.** I tried moving the text types down beside the text
calls and the build broke immediately — C wants a type declared before use,
and the old order already satisfied that. Types keep their existing order; only
functions follow the body.


One thing I'd flag: keying a paragraph to every identifier in a declaration
attaches it to the wrong call — a parameter's type drags its own explanation
along. I caught it because `cera_task_call_t` came out documented as if it were
a task. Keyed to what a declaration *defines* rather than what it mentions, it
lands right. Worth knowing if you ever regenerate that file.


--------------------------------------------------------------------------------

### User Request 8

So, the cera.h file doesn't need vimfolds for each declaration. The vimfold is
hiding... nothing except the signature. Not ideal! We just need vimfolds for the
larger sections. They're needed for source files, but not header files, because
a header declaration is already just one line.


  Everything that left went to cera.h.info.md: what each call does, and every
  struct's fields as a table.


there should be one info.md file for both the .c and the .h file.


--------------------------------------------------------------------------------

### Assistant Response 8

A fold hiding one line. Let me strip the inner folds — and the banners, which
now just repeat the fold label.


Packed one-liners will read more like an index. Let me only space around the
multi-line declarations.


The old blanks survived. Let me drop them all first, then insert only around
multi-line declarations.


433 lines and it reads like an index. Now test 117 must read declarations rather
than fold markers.


One difference: the `static inline` that lives only in the header. Same
exclusion as the body.


Now merging the two info files into one. Let me see what references them.


Both done. Tree is clean except `CLAUDE.md`, which isn't mine.


## The header


**433 lines**, down from 2,601 at the start of this work. It folds at the
component level and nowhere else — eleven lines when shut, and opening one
gives that component's declarations as a plain list:


```

/* {{{ 012 — the pool */

typedef struct task cera_task_t;

typedef void (*cera_task_call_t)(cera_task_t *t);


struct task { ... };


typedef struct pool cera_pool_t;

typedef void (*cera_pool_finish_t)(void *ctx, cera_task_t *t);

void cera_pool_push(cera_pool_t *p, cera_task_t *t);

cera_task_t *cera_pool_pop(cera_pool_t *p);

int cera_pool_worker_index(void);

...

/* }}} */

```


Two things went besides the per-declaration folds. **The banners** — inside a
fold already labelled with the component's name, four more lines saying it again
is the same sentence twice. And **the blank lines between one-liners**, so a run
of declarations reads as an index; only structs and wrapped signatures get space
around them.


The mirroring test had to change with it — it was reading the header's order
out of fold markers that no longer exist, and now reads the declarations
themselves. Both sides skip anything `static`: the body's private calls, and the
one `inline` that's defined where it's declared and so has no second place to
disagree with.


## One document


`src/cera.info.md` replaces both. It carries how to use the engine, what's
public and what checks it, how the two files are arranged, what's inside the
body that isn't in the header, and then every call and every type with what each
one means — 2,458 lines.


The argument for merging is the same one that made the two files mirror each
other: they're one thing seen twice, and a reader who wants to know what a call
does shouldn't first have to decide which file to open.


I also repointed the table of contents, both file headers, and three blueprints
that named the old files. The generated site rebuilds with zero unresolved
references, and the full suite passes.


--------------------------------------------------------------------------------

### User Request 9

Okay great. What next?


--------------------------------------------------------------------------------

### Assistant Response 9

Let me check the actual state of what's open rather than answer from memory.


The engine is finished work now. Everything open is about **letting somebody
else see it**, and there are two holes with real consequences.


## The two that are actually costing something


**Nothing runs when you arrive.** `./run-demo` prints an apology. The seven
phase demos were deleted on purpose — they demonstrated a batch program, which
is the opposite of what this engine is for — and their replacements (ticket
713) were never built. `make example` works, but it shows one four-station
graph. Somebody who clones this and wants to see a program come up, absorb a
value written from outside, get rewired mid-run, and take a box compiled while
it was running has nothing to type. All of that is built and proven; none of it
is demonstrable.


**The only consumer still doesn't build.** The train game names the Lua
generator that was replaced by the C one. I confirmed it again just now — same
error. The out-of-tree test proves a *new* program can be built with this
engine; nothing has yet proved an *existing* one survives an upgrade, and that's
the more honest question. Fixing it would also be the first real measurement of
what an engine change costs somebody downstream.


## My recommendation, in order


**The train game first.** It's the smallest of the three by far — the
generator moved and its invocation changed — and it converts the phase 9 claim
from "a new program builds" into "a program somebody already wrote still runs."
It's also the only item here that tests the engine rather than the presentation
of it.


**Then the demos (713).** This is the front door and it's currently shut. The
ticket already argues the shape: not thirty-five presses of the space bar while
a program does things to itself, but a program you keep alive and interfere
with. It's the largest of the three and the one that most changes what a visitor
sees.


**Then the viewer (802 and 803).** A build flag leaves a trail in shared memory;
a page reads it and cannot touch what it watches. 802 is self-contained and
useful on its own — even without the page, a trail you can `tail` beats
reading stderr. 803's awkward part is admitted in the ticket: a browser can't
map shared memory, so something has to stand between, and choosing that decides
how much of it is plumbing.


## The rest, and why they're not first


**909** — the blueprints don't name their calls. Real, but it's a day of
reading and it costs nothing until somebody rebuilds the project from the
tickets.


**801** — the canvas draws but can't hand you a file. The writer exists in C;
it needs compiling to WebAssembly and wiring in. Middling size, and it's a door
nobody is currently trying to walk through.


**705, 709, 711** — documentation polish. Two of five interactive pieces
missing, the slideshow, and a renumbering nobody is blocked on.


**107 and 108** — several queues, and a station saying which one its work goes
to. Both say *confer before building*, both have had a central decision reversed
once, and 108 still has seven unanswered questions at the bottom. These are
conversations, not tasks, and they're the only open work that would change the
engine rather than its surroundings.


Want me to start on the train game? It's an hour or two and it ends with a
second thing that runs.


--------------------------------------------------------------------------------

### User Request 10

okay let's skip the train game and the demos. Let's build the viewer. There's
two halves, right? The writer, and the viewer? The writer of course just fills
out the function names from a text string we enter, and it compiles the map file
without checking to see if those files actually exists. The viewer listens on an
HTTP server, right, and it displays things in a browser, with the user able to
re-arrange the boxes to see them clearly, but NOT able to modify them at all.
Right?


  909 — the blueprints don't name their calls. Real, but it's a day of reading
  and it costs nothing until somebody rebuilds the project from the tickets.


I don't think we need this issue file. The blueprints don't need to name their
calls.


  801 — the canvas draws but can't hand you a file. The writer exists in C; it
  needs compiling to WebAssembly and wiring in. Middling size, and it's a door
  nobody is currently trying to walk

  through.


we are!


  705, 709, 711 — documentation polish. Two of five interactive pieces
  missing, the slideshow, and a renumbering nobody is blocked on.


let's wait on these for now.


  107 and 108 — several queues, and a station saying which one its work goes
  to. Both say confer before building, both have had a central decision reversed
  once, and 108 still has seven unanswered

  questions at the bottom. These are conversations, not tasks, and they're the
  only open work that would change the engine rather than its surroundings.


don't worry about these.


--------------------------------------------------------------------------------

### Assistant Response 10

Mostly right, with one correction that matters: there are **three** pieces, not
two. Let me read the tickets rather than paraphrase from memory.


Let me retire 909 first, then correct one thing in your model.


909 is refused and filed, and phase 9 is closed. Now the correction.


**There are three pieces, not two**, and the middle one is the one that doesn't
exist:


Your description of **the writer** is exactly right — that's 801. Boxes are
placed and told what they are by typing a name; the page holds no C, reads no C,
and cannot tell you whether the name you typed exists, because the thing that
would answer is a compiler and there isn't one in a browser. It emits one file:
the map.


**The viewer is 803** — and it reads a trail that **802** leaves. That trail
is the missing middle. Today a running program says nothing about itself except
to stderr; there is no ring, no events, nothing to display. 802 is a build flag
that makes the engine emit a small fixed-size record at each thing worth seeing
— a station ran, a value moved, a buffer grew — into a ring in shared
memory. Without it, 803 has literally nothing to draw.


On **"it listens on an HTTP server, right?"** — that's undecided, and the
ticket flags it as *the first thing to decide*. The trail deliberately lives in
a shared-memory ring precisely so there's no socket, no port, no protocol and no
second thread inside the watched program. But **a browser page cannot map shared
memory**, so something has to stand between, and that something is the whole
question. A server is the first and costliest option: it means this project
gains a long-running program, which nothing else here has.


On **rearranging** — yes, and it's not in tension with anything. Moving a box
on screen changes the layout, not the program. 803's rule is that nothing on the
page can reach back into what it's watching; where you drag the picture is
yours.


**[Asked the user]**


*Q1 — The bridge:* A browser cannot map shared memory. What stands between the
ring and the page?

- A forwarding reader — A small separate program that maps the ring and pushes
  events down a socket the page opens. Keeps "cannot touch" true by construction
  — forwarding is all it can do. Costs: the project gains a long-running
  program.

- A file the page polls — The reader periodically writes the ring's contents
  somewhere the page can fetch. No server of our own, but a page cannot read a
  local file it wasn't handed, so something still has to hand it one repeatedly.

- A snapshot page — The reader regenerates a static HTML page from the ring.
  No live view at all — you reload to see the present. Cheapest to build and
  the weakest thing to look at.

→ **Answered:** A forwarding reader" selected preview:

watched program ──writes──> ring in /dev/shm

                                  │

                            reader maps it

                                  │

                         websocket / SSE

                                  │

                              the page


+ live, any number of viewers

+ reader can only forward

− a server process that must be running

− a port to pick, "How does the viewer learn the shape of the graph it is
drawing?"=(no option selected) notes: It should count the number of boxes in the
map file. It should create a grid that is as close to square as possible,
preferring an extra column to an extra row if necessary. Then it should arrange
the boxes on the canvas with a little bit of padding between them and draw the
wire lines. the user can then drag and re-arrange them as they please, and it
won't be saved. I guess maybe we could add a button that "saves the layout" or
whatever to a simple file alongside the map? file.lay or something? And if a box
is added dynamically at runtime, it should append itself to the box. If the user
has been dragging the boxes around, it should pick a spot to put the box that is
roughly equidistant to the other boxes it's wired to, although... creating boxes
and wiring them are separate steps, so we gotta be careful here. Then it puts
the boxes at that derived spot, making sure that it doesn't overlap any existing
boxes.. Read the answers carefully — they may request clarification, changes,
or that you not proceed — and follow what they actually say.


*Q2 — The graph:* How does the viewer learn the shape of the graph it is
drawing?

- Ask for a dump once — The program can already write its whole graph out as a
  map file. The viewer reads that once and has an exact picture immediately —
  but it needs a second channel to ask for it.

- Learn it from the trail — The trail says a station ran and a value moved
  from one to another, so the shape can be discovered by watching. No second
  channel — but the picture is incomplete until everything has run at least
  once.

- Dump first, trail keeps it current — Read the dump for the initial shape,
  then let rewiring events from the trail update it. Exact from the start and
  stays right when the program is edited while running.

→ *(no answer recorded)*


Forwarding reader it is. And your layout note answers the graph question
implicitly — *"count the boxes in the map file"* means the viewer reads the
map, which is the exact-from-the-first-frame option.


One catch you spotted is real and worth settling before I build: a station is
created and wired as two separate steps, so at the moment a box appears it has
no wires to be equidistant to. Let me record the resolution with the rest.


Now 802, the trail — everything depends on it. Let me read the rest of its
design.


Four open questions in 802. Let me settle them and record the answers, then
build.


Now the ring itself. Header first, as a new component after the ending.


The map struct needs a field for it. An incomplete struct pointer needs no
forward typedef.


Both compile. Now the emit sites — with the emitting guarded so an unwatched
build contains none of it.


Now the emit sites themselves — each at the place the thing actually happens.


Both compile. The ticket asks that an unwatched build be proven to contain none
of the emitting — from the symbols, not from assertion.


Now the test — the ring, and a reader deliberately made too slow.


Three problems, one of them in the engine: my ownership check exempted the same
process, so a second map in one process could seize the ring — and it did,
truncating it. The exemption is wrong.


Both accounts agree exactly. But losing the slow-reader scene means it's
untested — let me add it back as its own run.


The `\n` escapes got eaten by Python. Let me fix them in place.


All five scenes pass, and the loss accounting is exact. Let me wire it into the
build — it needs the flag.


The mirror test caught the new component immediately. Let me match the header to
the body's order.


The component fold structure needs rebuilding after appending. Let me run the
refold tool.


All green — 109 symbols now, and the mirror test verified the new component
lands in the right place in both files.


The trail is built, tested, and committed. That's the middle piece — the
viewer now has something to read.


**What a watched program emits:** ten kinds of event, each at the place the
thing actually happens. It came up; a task became due; a station ran and for how
long; a value crossed from one station's port to another's; a buffer grew; a
station was placed or removed; a wire was drawn or cut; it finished.


**Nothing a reader does can slow it.** A writer that laps a reader overwrites
it. What keeps that honest is that the reader computes exactly what it missed
from the sequence numbers — and the test doesn't take its word for it. After a
run that lapped the ring nineteen times: **lost 18,980, kept 1,024, and the
first one it kept is number 18,981.** The arithmetic closes.


**Two independent accounts of one run agree.** The reader's tally of how many
times each station ran, reconstructed from the trail alone, matches the counters
the program keeps for itself.


**An ordinary build carries none of it**, read out of the object file rather
than claimed: without the flag, neither the emitter nor the clock it reads
exists as a symbol, and the object is three kilobytes smaller.


Two things worth flagging:


**Writing the test found a real fault.** My ownership check — which stops two
programs sharing one ring — exempted the same process, reasoning that a
program reopening its own trail isn't a conflict. But two maps in one process
*are* two programs; that's the whole of what makes them separable. The second
one promptly truncated the ring a reader was already following. Exemption
removed.


**The mirror test earned itself immediately.** I added the new component to the
body and the header, and it failed twice: once because I'd declared the calls in
a different order than I defined them, and once because the component fold
structure hadn't been rebuilt so the body still thought the new calls belonged
to the previous component. Neither would have been caught by the compiler.


Next is 803 itself — the forwarding reader and the page. Two pieces:


**The reader** maps the ring and pushes events down a socket. That's the first
long-running program in this project, and it's small: attach, loop, forward,
detach.


**The page** reads the map file for the graph, lays it out on the near-square
grid, draws the wires, lights stations as events arrive, shows buffer depths,
and says so when the trail reports loss. Drag to rearrange; a pinned box never
auto-moves; optionally save the layout beside the map as `.lay`.


One thing I should ask before building the reader: **what protocol down the
socket?** A raw newline-delimited stream of one JSON object per event is the
simplest thing that works and needs nothing on either side; a WebSocket is what
a browser natively wants but means implementing the handshake and framing in C.
Server-sent events split the difference — plain HTTP, one `data:` line per
event, and the browser has `EventSource` built in. I'd take server-sent events:
no framing to implement, no library, and the page gets automatic reconnection
for free.


Shall I go with that?


--------------------------------------------------------------------------------

### User Request 11

ok


--------------------------------------------------------------------------------

### Assistant Response 11

Building the viewer. Let me check the map format the page has to parse.


Now the forwarding reader. It maps the ring and does nothing but forward.


Now the page — the graph from the map file, the near-square grid, and the
lighting.


Node is broken on this machine, so I'll prove the JS by fetching it end-to-end
instead. First, something worth watching.


Now the end-to-end test — a watched program, the viewer, and a browser's worth
of requests.


Firefox is here. Let me render the page and see it.


Firefox headless is hanging. Let me check whether it works at all here before
blaming the page.


Firefox headless is broken on this machine even for a trivial page — so I
can't render here. Let me fix a real gap I spotted while re-reading, and check
the JS structurally.


All endpoints correct. Let me turn what I just did by hand into a test.


All green — 2,452 events in sequence across 5 kinds. Let me record the state
and commit.


`pkill -f` matched my own command line and killed the shell. Let me clean up
more carefully and redo.


Both halves of the viewer are built, tested, and committed. Phase 8 has one
ticket left — the workbench.


## What you can run now


```sh

make viewer

```


prints two commands. One terminal runs a program with a trail; the other runs
the viewer; you open `http://localhost:8723/`.


**The forwarding reader** maps the ring with the engine's own reader — so the
ring's shape is defined once, not twice — and pushes events as server-sent
events. One thread, one poll loop, and a reader per connected page, so two
people watching don't interfere because neither holds anything the other needs.


**The page** reads the map file for the graph, which is exact from the first
frame including branches that may never run. Near-square grid, extra column
preferred. Stations light as they run and fade; a pip under each input port
fills as its backlog deepens; the header says plainly when the trail reports
loss.


**Dragging pins.** A pinned box is never moved by anything automatic again. A
station that appears mid-run goes on the end of the grid — at that moment it
has no wires — and when its wires arrive it moves once, to a spot equidistant
from what it connects to, nudged until it overlaps nothing. Unless you've
dragged it by then.


**There is no write in it anywhere.** Saving a layout is a download the browser
performs; putting the file beside the map is your own act, after which the
viewer reads it back.


## What's proven and what isn't


The test starts a real program, starts the viewer, and checks the page and its
parts come back, that anything the server doesn't know is refused, and that
thousands of events arrive **in sequence** — out of order would be a picture
that can't be trusted about what happened before what. Last run: 2,452 events, 5
kinds.


**The drawing is not proven.** This machine has no working browser — I tried
headless Firefox and it hangs on a blank page, not just on mine. Everything the
page is *served* is checked; how it *looks* wants you to open it. That's the one
thing I'd like you to do before I call it good.


Two things I'd flag from building it:


**The `.lay` file location changed shape.** You suggested saving beside the map.
The server writing it would have put a write into a program whose entire design
is that it has none — so the browser downloads it and you place it. The viewer
reads it back on load. Same outcome, and the "cannot touch" property stays true
by construction rather than by discipline.


**A reader attaching late loses everything before it arrived** — I saw 3,869
events lost on one attach. That's correct and it's why reading the map file
matters: a viewer that learned the graph from the trail alone would have drawn
nothing at all in that case.


--------------------------------------------------------------------------------

### User Request 12

It looks great! Question - there's two rounded slots on each box, but some of
them have dashed lines and some are solid. What's that about?


Also, I can't see any values flowing through the system. Like, I get that they
probably are, but the point of the viewer is to WATCH them move between boxes.


Can we change the background to a light dark blue? Think about it and it'll make
sense. Then can we add ()'s after the box function name? Or is that the box
title? Can we also put the names of each of the input ports? I want to be able
to watch the mechanism of the ring buffers and such. Basically, it looks great,
can we take another pass and add more of the missing functionality?


Question. Why are there any "missed" events at all? We're writing to a log file,
right? Actually, why do we need two programs running again? We compiled this
version specifically for watching, which is why it's emitting, so why don't we
have it also handle the display-ing, since this is the display build?


--------------------------------------------------------------------------------

### Assistant Response 12

Good questions, and one of them is a real bug. Let me check the facts before
answering.


Both confirmed. Answers first, then the work.


**The dashed slots are a bug.** My CSS says `.station.pinned rect` — which
matches *every* rect inside a station, including the port pips, not just the
outline. So any box you dragged got dashed ports too. That's the difference
you're seeing: dashed means pinned.


**Missed events only happen when a reader attaches late or falls behind.** I
just measured a steady watcher: 969 events, zero gaps. The 3,869 I reported
earlier were all from attaching after the program had been running — the ring
had lapped before the reader existed. It's a fixed-size ring rather than a log
on purpose: a log grows without bound and makes the watched program do disk I/O,
and the whole rule is that watching can't slow what's watched.


**On one program instead of two** — you're right that this is a watch build,
so the usual objection doesn't fully apply. But the property the design rests on
is that the program *never waits for a reader, never learns one is there*. An
HTTP server inside it means a thread, a socket, and clients — the program
would be doing exactly what it promises not to. What I can do is make it *one
command*: a flag that has the watched program spawn the viewer as a child. Same
guarantee, no second terminal.


And `double_it` is a box, which is a plain C function — so `double_it()` is
right.


Let me take the pass.


Now the page — ports on the edges, wires port-to-port, and values you can
actually watch move.


One command, and the viewer dies with the program. One fragility left though:
the page maps event indices onto the map file's station order, so pointing it at
a *different* map would draw a lie. Let me have the program dump its own.


The program now draws itself, and the dump carries more than I was using —
explicit indices and buffer capacities. Let me read those too.


Committed. To see it:


```sh

make

tmp/build/123-a-program-to-watch --trail=/dev/shm/minimal-soramech/live.ring --view

```


Then `http://localhost:8801/` — one command, and the viewer stops when you
interrupt the program.


## The dashed slots


A bug, and thanks for spotting it. `.station.pinned rect` matched *every*
rectangle inside a station, ports included. So any box you'd dragged had dashed
ports too, and the page silently claimed there were two kinds of slot. The dash
means pinned and now applies to the outline only.


## What changed


**Blueprint** — pale lines on blue with the paper's own ruling underneath.
That's what I took "light dark blue" to be pointing at: the issue files here are
called blueprints and read like them, and this is the same drawing with the
machine running inside it. Tell me if you meant something else.


**Ports moved to the edges, and wires now join ports rather than boxes.** Which
port a wire lands on is the whole of what a wire says, so the picture says it.
Each port shows what it *is* — a ring buffer that can back up, a constant that
always holds a value, or no source at all.


**Values move.** A dot crosses the wire it went down. Its position comes from
the curve arithmetic directly rather than asking the browser to measure the path
— a measurement per frame per value is the one thing that would make watching
cost something.


**Buffer depth is derived.** The trail carries no values and no depths,
deliberately. But a value arriving at a ring port is one more waiting, and a
station running takes one from each of its ring ports — that's the readiness
rule the engine itself follows, applied to the events the engine emitted. The
port fills as the backlog grows and turns colour when it gets deep.


**`double_it()`** — you were right, it's a box, which is a plain C function.


## The one I'd underline


Your questions made me look at how the page turns an event into a box, and it
was **counting down the map file**. The trail carries station *indices*. Point
the viewer at a map of the same shape in a different order and every event would
have landed on the wrong station — confidently, with no way to tell. The
watched program now dumps its own live graph and the viewer reads that, so
drawing the right thing is a property of the wiring rather than of who
remembered to pass the right file. The dump also states each index and each
buffer's capacity outright, which the page now reads instead of inferring.


## Still not verified


How it looks. This machine has no working browser — I tried headless Firefox
on a blank page and it hangs, so it isn't my page. Everything the page is served
is checked and its structure with it; the drawing wants your eyes.


--------------------------------------------------------------------------------

### User Request 13

Wow! Looks great! How do the dots know how fast to be? Can we make an example
that intentionally uses the ring buffer system? Like maybe... a box with 3
inputs, and another box that has an iterator output. If < 10 output to the first
slot of the 3, if the output is < 20 then output to the second slot of the 3,
and if the output is < 30 then output to the third slot of the three. If the
output == 30 then reset to 0 and keep going. Once the 3-input-box has all three
filled with at least one value, it'll run itself once. That should show us the
mechanism of the ring buffers better.


also we should be able to zoom the canvas. It shouldn't resize when I drag boxes
around. It should just tile infinitely in all directions as much as we need,
with the initial view upon loading in centered to the average position of all
the boxes. Or rather, to the input box... I guess it doesn't matter so long as
they're in view and not like, really really tiny or whatever. Can we zoom to the
input box upon starting the viewer?


--------------------------------------------------------------------------------

### Assistant Response 13

Three things, and the first one has an honest answer: **the dots don't know.**
`FLIGHT_MS` is a fixed 520ms, so every dot takes the same time regardless of
wire length — a long wire looks faster than a short one. The dot means "a
value went this way, just now" and nothing about speed. I'll make it constant
*velocity* instead, so distance on screen means something.


Let me see what boxes exist before designing the example.


`as_triple` takes three floats — close, but I want ints and a deliberately
*uneven* split. Let me check how comparators route first.


Confirmed — a comparator runs its box, compares the return to the threshold on
its last port, and routes less/equal/greater. That gives a deliberately uneven
split. Let me build the example.


The graph is exactly the shape intended. Let me verify the buffers really do
fill unevenly.


Exactly the demonstration: **170 / 17 / 161** values arriving at collect's three
ports, collect running 17 times — the rate of its slowest feed — while ports
0 and 2 accumulate 153 and 144 and grow from 10 slots to 40. Now the canvas.


Now the camera — pan, zoom, an endlessly tiling grid, and a view that doesn't
move when you drag a box.


Committed. To see the new one:


```sh

make

tmp/build/126-a-mechanism-to-watch --trail=/dev/shm/minimal-soramech/live.ring --view

```


## How the dots knew how fast to be


They didn't. Every dot took the same 520ms whatever wire it crossed, so a long
wire looked *faster* than a short one. Worth being precise about why: **the
trail carries the moment a value moved, not how long it took.** So any speed on
that page is a picture, not a measurement — there's no true answer available.
Given that, the honest picture is the one where distance means something, so
it's fixed *speed* now, bounded at both ends.


## The mechanism example


I built it from the engine's actual primitives rather than the range-routing you
sketched, because a comparator gives less/equal/greater against *one* threshold,
not three ranges. But it lands in the same place, and better:


```

feed ──> gate ─┬─ less    ──────────────> collect.0

        (comparator)

               ├─ equal   ──────────────> collect.1

               └─ greater ──> spread ─┬─> collect.2

                            (iterator) └─> drain

```


`feed` counts 0–29, the threshold is 10, so **ten of every thirty go left,
exactly one goes down the middle, nineteen go right** — and the iterator
halves the right-hand stream again. `collect` has three inputs and can't run
until all three hold a value.


Measured over six seconds: **170, 17 and 161** values arrived at collect's three
ports. It ran **17 times** — the rate of its slowest feed. The two fast ports
were left holding **153 and 144**, and their buffers grew from ten slots to
forty.


That's the thing the engine's buffer report has always shouted about after the
fact, now watchable while it happens. All three station kinds appear too, which
wasn't the point but is worth having.


## The canvas


The jumping was a real bug: the viewBox was recomputed from content bounds every
frame, so dragging a box past the old boundary moved everything. Now there's a
camera — drag the paper to pan, scroll to zoom about the pointer, and the grid
coarsens as you pull back so it doesn't become a wash of lines at low zoom.
Nothing is resized to fit anything.


It opens looking at the entrance, at a scale that frames the whole graph when it
fits and comes in to a readable size when it doesn't.


Still unverified: how any of it looks. No working browser here — that part's
yours.


--------------------------------------------------------------------------------

### User Request 14

  I built it from the engine's actual primitives rather than the range-routing
  you sketched, because a comparator gives less/equal/greater against one
  threshold, not three ranges.


okay for completeness, can we see how you'd build what I describe, using the
built-in primitives? Probably a succession of 3 comparator blocks right?


  The jumping was a real bug: the viewBox was recomputed from content bounds
  every frame, so dragging a box past the old boundary moved everything. Now
  there's a camera — drag the paper to pan,

  scroll to zoom about the pointer, and the grid coarsens as you pull back so it
  doesn't become a wash of lines at low zoom. Nothing is resized to fit
  anything.


the zooming is a bit wonky, can you take another pass at it and try to do a
better more comprehensive system? Just for the zooming, panning feels fine.


also, why does it say this?


11794 events lost — this view is behind


we're writing to a log file right? why are there "missing" events?


--------------------------------------------------------------------------------

### Assistant Response 14

Three things, and the third one I explained badly last time — let me be
concrete.


**It is not a log file.** It's a fixed-size ring in shared memory: 1024 slots
for a small program, and slot N+1024 overwrites slot N. But that number you saw
is almost certainly not "behind" at all — **every page reload starts a new
reader**, and mine started it at event 1, so it reported everything that
happened before you opened the tab as "lost". That's wrong labelling of a normal
situation. Let me fix the engine, not the wording.


The ring is now large enough that the old loss scenario doesn't overflow it —
the test is doing its job. Let me make it genuinely lap, and add a scene for the
new behaviour.


Now the distinction is proven: 234,468 *lost* for a reader that fell behind,
versus *joined at* 234,469 having lost nothing. Let me carry that through to the
page.


Now the zoom. The wonkiness is real — I never normalised `deltaMode`, so a
line-scrolling mouse barely moves and a trackpad lurches.


The ladder map compiles into the binary. Now a program that can watch any map,
rather than another hand-built one.


The ladder works exactly as designed. Let me record it and commit.


Committed. To watch the ladder:


```sh

make

tmp/build/128-watch-a-map --map=127-the-ladder.map \

    --trail=/dev/shm/minimal-soramech/live.ring --view

```


## The range routing — yes, a ladder


You were right. A comparator answers *one* question, so three ranges need two of
them in succession, each taking the values below its own threshold and handing
the rest down:


```

feed ──> under_ten ──less──────────────────> collect.0

                   ├─equal───┐

                   └─greater─┴─> under_twenty ──less──> collect.1

                                              ├─equal───┐

                                              └─greater─┴─> spare ──less──> collect.2

```


Measured over 306 values: `under_ten` saw all 306, `under_twenty` saw the 200
passed down, `spare` saw the 100 left, and `collect` got 106/100/100 and ran 100
times. An even three-way split by range.


The last rung's `equal` and `greater` exits are wired nowhere, because nothing
ever reaches thirty — the counter wrapped first. **An unwired exit discards**,
which is the ordinary case, not an omission.


I made it a **map file** rather than another program, since that's the clearest
expression of the wiring, and added something that runs any map the binary was
built with — so a shape can be watched without a program written for it.


## The zoom


Real bug, and a dull one: I read the wheel's raw number. **A wheel reports
movement in pixels, lines or pages**, whichever the browser picks, so the same
gesture arrives as `3` from a mouse and `300` from a trackpad. One barely
zoomed; the other lurched. It converts first now and steps by a fixed ratio,
bounded so one hard flick is one firm zoom, holding the point under the pointer
still. Plus buttons and a percentage, `+`/`−`/`0`, and `f` or double-click to
frame the whole graph — the way back from zooming into a corner.


## The 11,794


**Those events were not lost.** Every reload starts a new reader, and mine began
at the first event the program ever wrote — so everything before you opened
the tab was reported as loss. Arriving late, described as a fault.


A reader now starts at the oldest event *still in the ring* and reports where it
came in. Loss means what it should: events overwritten while that reader was
already attached. The page shows the two differently — one quietly, one in red
— because only one means the picture can't be trusted. The test proves the
distinction: a reader that never looked lost 234,468; one attaching afterwards
joined at the next event having lost nothing.


I also grew the ring from ~1,000 slots to 65,536 minimum — two megabytes of
shared memory, which is nothing against being told you missed something because
the buffer was small rather than because you were slow.


To be exact about the thing you keep asking, since my earlier answers clearly
didn't land: **it is not a log file and never writes to disk.** It's a
fixed-size ring in shared memory, chosen so the watched program never blocks on
a reader and never grows without bound. A log would make the program do disk I/O
proportional to its own work rate, which is the one thing the design refuses.
The cost of that choice is that a reader who falls genuinely behind loses the
oldest events — and now that's the only thing that says "lost".


--------------------------------------------------------------------------------

### User Request 15

Wow, looking great. Okay instead of the dots moving, can we just have like,
arrows, so we know the wire is active / was recently active? The dots are still
moving along the wire while the box's inputs are drained, which is a little
confusing but it's just a rendering bug. So let's get around it by removing the
dots and just keeping the "live wire" display - maybe we can have arrows that
sit just around the lines that only show up when it's active, and which "move"
toward the destination to show movement but aren't actually representing
individual values walking down the wire?


Also, for the example program, we should just not wire up the == output, or we
should adjust the comparator parameters to account for that. So like instead of
3 wires coming out of under_ten() we'd have the three output ports but only two
are wired up. If value < 10 then wire 1, if value is == whatever then NOP, if
value > 9 then wire 3.


Question. Can we write the 3 outputs of a comparator separately like that? Like
what if I said "if less than 5 do wire 1, if equal to 77 then do wire 2, and if
greater than -200 then do wire 3"? how would that be parsed if so? I mean it's
obviously broken but I want to know about edge-cases.


--------------------------------------------------------------------------------

### Assistant Response 15

Three things. Let me answer the parsing question with the actual parser rather
than from memory — let me probe the edge cases.


The generator only emits; validation happens at wiring and bring-up. Let me look
at what those actually enforce.


Now I can answer that precisely — the routing is four lines of C:


```c

int sign = s->compare(t->out, t->in[t->n_in - 1]);

return sign + 1;

```


**One threshold, one comparison.** Exit 0/1/2 *are* less/equal/greater; that's
fixed in the engine, not in the map. So `if <5 → wire 1, if ==77 → wire 2,
if >-200 → wire 3` isn't expressible — the grammar has nowhere to put a
second or third number. The only number is `in <last> = N`. And `out 3` on a
comparator is refused: *"a port index beyond what this station kind can mean."*


But your instinct about not wiring `equal` is right, and there's a way to make
it genuinely unreachable.


Exit 1 never fires — 130/130/122 across the three ports, only `less` and
`greater` used. Now the arrows.


Committed.


```sh

make

tmp/build/128-watch-a-map --map=127-the-ladder.map \

    --trail=/dev/shm/minimal-soramech/live.ring --view

```


## The dots


You caught a lie, not a rendering bug. **The dots stood for nothing.** The trail
carries no values, so a dot wasn't any particular value; it carries no durations
either, so the speed was invented. A dot still crawling after the destination
had already run and drained is what that looks like from outside.


A wire now says it is *carrying*, not what. It brightens when something crosses
and fades over about a second; while lit, chevrons march toward the destination.
The marching says which way, the brightness says how recently, and the offset is
a function of the clock alone — so every live wire flows at the same rate and
none can be read as tracking one delivery. Direction is stated once at the end
by an arrowhead rather than by anything travelling along it.


## Your question — no, and here's exactly why


The routing is four lines:


```c

int sign = s->compare(t->out, t->in[t->n_in - 1]);

return sign + 1;

```


**One threshold, one comparison.** Exits 0/1/2 *are* less/equal/greater —
fixed in the engine, not chosen in the map. `if <5 → wire 1, if ==77 → wire
2, if >-200 → wire 3` has nowhere to live: the grammar's only number is `in
<last> = N`, and `out 0 - x.0` names a destination and carries no condition at
all.


The other edges I probed:

- **`out 3` on a comparator** — refused: *"a port index beyond what this
  station kind can mean."* A comparator's limit is 3, a plain station's is 1, an
  iterator's is unbounded.

- **A comparator with no threshold** — never runs. The threshold is an
  ordinary input port, so an empty one means the station is never ready. No
  error, just silence.

- **A threshold on a plain station** — accepted at generation; it's just a
  constant on a port the box actually receives.


## The unwired `equal`


You were right to want it, and there's a way to make it *unreachable* rather
than merely unwired — which matters, because an unwired exit **discards**, so
leaving `equal` dangling with integer values silently throws one away.


Doubling every value before the ladder puts the boundary *between* the integers:
nineteen against evens splits at nine-and-a-half. Now each comparator has three
exits, two wires, and an equal branch that cannot fire rather than one that
fires into nothing.


Measured: **130 / 130 / 122** to the collector's three ports, 122 runs, and
**exit 1 never used by either comparator.**


--------------------------------------------------------------------------------

### User Request 16

Okay it looks better, but the wires stay lit for a while after they're done
sending data. They should be as close to live as the shared memory "log"
reports. I know this is true because there was a wire going into port 1 of the
collect box (three_way() is the box function) that was still live while the box
is running. I know this is a bug (probably rendering) because port 2 can only
fill up when port 0 and 1 are fully filled (because of the way we wired the
comparators earlier) yet somehow under_twenty's out 0 and out 2 are both "live"
at the same time.


  One threshold, one comparison. Exits 0/1/2 are less/equal/greater — fixed in
  the engine, not chosen in the map. if <5 → wire 1, if ==77 → wire 2, if
  >-200 → wire 3 has nowhere to

  live: the grammar's only number is in <last> = N, and out 0 - x.0 names a
  destination and carries no condition at all.


ah I see. So let's just keep it as it is, and keep wiring port 0 to a different
location than port 1 and 2.


--------------------------------------------------------------------------------

### Assistant Response 16

That's not a rendering bug in the drawing — it's the decay window outliving
the truth. Let me check the arithmetic before I change anything.


Measured, and your diagnosis is right. A burst on one exit lasts **600ms** —
and my warm window was **900ms**, so exit 0 stayed lit right through exit 2's
entire burst. Both looked live because the decay outlived the truth.


Let me verify the fix against the actual event stream, rather than trusting the
arithmetic.


Overlap down from most of the run to 5.7%, and only at burst boundaries —
where the previous exit genuinely did carry within the window. Let me make that
a test, since it's exactly what regresses silently.


`luajit -e` doesn't pass varargs the way a script file does. Let me make the
replay a proper tool rather than an inline chunk.


The test catches it — 52.5% at the old window, 5.7% at the new one. Full suite
and commit.


Committed. Your reasoning was right and the arithmetic backs it up.


## What was wrong


Not the drawing of any single event — **the decay window outlived the truth.**


I measured the ladder's rhythm: it alternates in bursts of ten values, which at
60ms each is a **600ms burst** on one exit before switching to the other. My
wire stayed lit for **900ms**. So the previous exit was still glowing through
the entire next burst, and two exits of one comparator looked permanently live
— a picture of something that cannot happen, since one value leaves by one
exit.


I'd picked 900ms because it looked pleasant, and never checked it against the
rhythm of anything it would be drawing.


A wire is lit for **160ms** now, a station for 220. The floor isn't zero and
can't be: the server drains the ring on a 40ms poll and the page redraws every
33ms, so ~80ms of lag is structural. What the shorter window buys is that
nothing is shown as live once it has stopped.


## The part worth keeping


This is now checked, without a browser. `scripts/130-replay-the-page.lua`
applies the page's own rule to a captured event stream — **reading the window
out of the page's source rather than restating it**, since two places that must
agree is one too many — and reports how much of the time two exits of one
comparator are lit together. A little is honest, at the handover. A lot means
the drawing is describing the past as the present.


**5.7% now, against 52.5% at the number it replaced.** The test refuses above
20%, and I proved it by putting 900 back and watching it fail.


It's the only check here that tests a drawing, and it needs no browser: what's
being checked is the arithmetic a browser would do.


## On the comparator wiring


Left as it is, as you said — port 0 to one place, port 2 to another, and the
equal exit unreachable rather than merely unwired. That still holds: exit 1
fired zero times across 530 deliveries.


--------------------------------------------------------------------------------

### User Request 17

Okay now I want to show my girlfriend, what do we have to do to make it work on
her computer? I can run it on mine, the viewer can just watch from her monitor.


--------------------------------------------------------------------------------

### Assistant Response 17

Two things stand between you and that, and only one is code.


Confirmed: the server binds to **loopback only**, so her machine can't reach it.
The page itself uses only relative paths, so it'll work from anywhere once the
socket does. Let me add the opt-in.


The declaration missed one (different line shape), and `listen` shadows the
POSIX call. Let me rename it in all three.


**She needs nothing.** No install, no build, no repo — it's a browser opening
a URL. The page asks for everything by relative path and never names a host.


**You need one flag.** The viewer was listening on loopback only, so your
machine was the only one that could see it. That's the right default —
watching shouldn't be the act that puts a program on a network — but it needed
a way to say otherwise.


```sh

make

tmp/build/128-watch-a-map --map=127-the-ladder.map \

    --trail=/dev/shm/minimal-soramech/live.ring \

    --view --listen=all

```


It now prints the addresses for you, so you don't have to go find them:


```

viewer: open http://localhost:8723/

viewer: or from another computer, http://192.168.1.100:8723/

viewer: this port is open to anything that can reach this machine.

        It only ever sends; nothing on the page can reach the program.

```


Send her that second line. I verified both halves: with the flag, the LAN
address answers 200; without it, the same address is refused while localhost
still works.


Two things worth knowing before you try it:


**A firewall may be between you.** I can only confirm the socket is open on this
machine — whether her packets arrive is your network's business. If the URL
hangs rather than refusing, that's what it is.


**Anyone on your network can watch, while it's running.** That's genuinely all
they can do — the server answers five paths, every one a read, and there is no
request that writes anything anywhere. But it does serve the map, which
describes the program's shape, so it's not nothing.


If you want the prettier one, the four-station example is
`123-a-program-to-watch --view --listen=all`. The ladder is the better demo
though — five stations, the comparators visibly splitting the stream three
ways, and `collect` sitting there with two full buffers waiting on the third.


--------------------------------------------------------------------------------

### User Request 18

The HTML documentation is resetting the zoom level everytime the user switches
to a different page.


Okay she's just going to run it from her computer instead. Can you tell me what
to run? Also, are we up to date on git commits so I can push so she can pull?


--------------------------------------------------------------------------------

### Assistant Response 18

Let me look at all three — starting with the docs, since I should see the
actual behaviour rather than guess at it.


The nav is its own scroll region (`overflow-y: auto`) over ~150 entries, and
every page is a full reload — so you lose your place in the index each time.
Let me see what script the pages already carry.


The sweep deleted it — it removes anything no source produces, and it doesn't
know about this file yet.


`make viewer` doesn't touch the docs, so it needs only a C compiler. Let me
prove that from clean.


Fixed, and yes — you're clear to push.


## The documentation


Two things were throwing your place away, and I fixed both.


**The index scroll.** Every page is a whole document, so following a link
discarded everything the browser held — and with ~150 entries in that sidebar,
you lost your position on every click. It now keeps its scroll across pages in
per-tab storage, so it survives a link and vanishes with the tab. On a first
visit there's nothing to restore, so it scrolls the page you're reading into
view instead.


**The text size.** A browser's own zoom is a browser setting — a page can't
read it or keep it, and on some setups it genuinely resets per document with
nothing able to stop it. So there's now a size of its own: buttons bottom-right,
`+` `-` `0` from the keyboard, remembered between pages *and* between visits.


Writing it caught the sweep doing its job — the new file appeared and was
deleted again on the generator's very next line, because the sweep removes
anything no source produced. A file the generator writes without a document
behind it has to be named in that list, which is now stated.


## Git


```

branch:   minimal

unpushed: 4 commits

dirty:    CLAUDE.md only — not mine, I've left it alone

```


`git push` and you're done. (You may want to deal with `CLAUDE.md` first or
leave it; it's been modified since before this session.)


## What she runs


```sh

git clone https://github.com/gabrilend/ceramic-core-engine.git

cd ceramic-core-engine

make viewer

tmp/build/128-watch-a-map --map=127-the-ladder.map \

    --trail=/dev/shm/ceramic-core-engine/live.ring --view

```


Then `http://localhost:8723/` on her machine. Ctrl-C stops both the program and
the viewer.


**She needs a C compiler and nothing else.** I checked that from clean: `make
viewer` builds the four programs without invoking LuaJIT once. LuaJIT is only
for regenerating the documentation site, and the site is committed, so she can
just open `docs/HTML/index.html` in a browser without building anything.


Two things that might bite:


**The RAM directory.** `make viewer` creates `/tmp/<project>` and
`/dev/shm/<project>` for her clone's own name — if she clones to a directory
called something else, use that name in the `--trail` path. Or just use any
writable path; the trail is only a file.


**`/dev/shm` must allow writes** — standard on Linux. If she's on macOS it
won't exist; use `/tmp/live.ring` instead, which works fine since the trail is
only read, never executed.


--------------------------------------------------------------------------------

### User Request 19

okay I tried compiling it on her computer and I get an error that's not present
on mine at line 2648 - variable total is created but not used or something?
What's up with that?


--------------------------------------------------------------------------------

### Assistant Response 19

Let me look at that line and reproduce it, rather than guess at the difference.


Reproduced in principle — let me confirm the exact diagnostic before fixing.


My GCC won't warn even when asked. Let me see if there's a second compiler here
that will.


Reproduced exactly — **clang 18**, same line. She's on a different compiler,
and it caught something mine let through.


This isn't just a fussy warning. Without the watch flag, that loop still walks
every station reading an atomic each — work an unwatched program does for an
event it never emits. The ticket's own rule is that an unwatched program pays
*nothing*.


All four combinations clean. Now the durable fix — the reason this reached her
is that only one compiler was ever looking.


Found it, reproduced it exactly, and it was worse than a warning.


## What happened


She's on **clang**; you and I are on **gcc**. Clang 18 says:


```

cera.c:2648:10: error: variable 'total' set but not used [-Wunused-but-set-variable]

```


Same line you quoted. My gcc 14.2 stays silent about it even when I ask for that
exact warning explicitly — so it never had a chance to reach me.


## Why it's a real bug, not compiler fussiness


The arguments to an emit vanish when watching is compiled out. **Anything
computed on the line above it does not.**


```c

long total = 0;

for (...) total += atomic_load(&s->runs);   /* still runs */

CERA_EMIT(m, CERA_WATCH_DONE, total, ...);  /* becomes ((void)0) */

```


So an unwatched program was **walking every station and reading an atomic each,
on every teardown**, to feed an event it never sends. There was a second one
too: a before-and-after check on every single delivery, to notice buffer growth
for an event that wasn't going anywhere.


That breaks the rule the whole watching mechanism is built on — *a watched
program may be slower; an unwatched one pays nothing.* Her compiler caught a
design violation, not a style nit.


Both are now inside the flag with the emits they feed.


## The part that matters more


The reason it reached her is that **only one compiler was ever looking**. So the
engine is now compiled under every compiler on the machine, with the flag and
without, warnings as errors:


```

== 131-test-a-second-opinion.sh

  clean under gcc clang, watched and unwatched

```


I proved it by putting the fault back and watching clang name it. The half of a
flag that usually isn't compiled is exactly where a mistake sits unseen.


## What to do


You've already pushed the earlier four. **One new commit to push:**


```sh

git push

```


Then on her side:


```sh

git pull

make viewer

tmp/build/128-watch-a-map --map=127-the-ladder.map \

    --trail=/dev/shm/ceramic-core-engine/live.ring --view

```


If anything else fails on her machine, send me the compiler line — the error
text plus `cc --version`. Different compiler is now a thing I can check locally
rather than guess at.


--------------------------------------------------------------------------------

### User Request 20

okay we're done with the "girlfriend showcase" so let's get back to what we were
working on. Where are we at?


--------------------------------------------------------------------------------

### Assistant Response 20

Everything's pushed. Here's the state.


## Done since you last asked "what next"


**Phase 9 closed entirely.** The engine is two files, publishes 101 symbols all
prefixed, everything else is `static` and a test enforces it, a host can hear
why it stopped, two maps run in one process, and it builds from a directory that
can't see this repository.


**Phase 8's viewer is done** — both halves. `802` (the trail: a ring in shared
memory, events emitted only under the build flag) and `803` (the forwarding
reader and the page). Along the way we picked up a mechanism example, a ladder
map, a general "watch any map" program, and a replay tool that checks the
drawing's claims against the events without a browser.


## What's actually open


**Phase 8 — `801`, the workbench.** The last one in the phase. The canvas
draws stations, names them, gives them kinds and boxes, wires them, and keeps
several drawings in browser storage. **It still can't hand you a file.** The
writer exists in C (`scripts/105-mapwrite.c`) and needs compiling to WebAssembly
and wiring into the page. That's the door; everything else in phase 8 is the
window, and the window is finished.


**Phase 7 — three loose ends.** `705` (docs: two of five interactive pieces
missing, though it did just gain the scroll-and-size fix), `709` (slideshow and
transcripts), `711` (renumbering, explicitly low priority, nothing blocked).


**Phase 7 — `713`, and this is the visible one.** `./run-demo` still prints an
apology. The seven phase demos were deleted deliberately and their replacements
were never built. It's the project's front door and it's shut.


**Phase 1 — `107` and `108`.** Several queues, and a station naming which one
its work goes to. Both still say *confer before building*, both have had a
central decision reversed once, and 108 has seven unanswered questions.
Conversations, not tasks — and the only open work that would change the engine
rather than its surroundings.


## Outside the repo


**The train game still doesn't build.** Same error as when we started: it names
the Lua generator that was replaced by the C one. It's the only existing
consumer, and `907` proved a *new* program can be built with this engine but
says nothing about an existing one surviving an upgrade.


## Where I'd go


**`801`**, to finish phase 8 — you said "we are!" walking through that door,
and the viewer being done makes the workbench the obvious other half. The work
is: compile the map writer to WebAssembly, wire it into the canvas, and add the
validation the ticket already specifies. `wasm-ld` is present on this machine,
so the blocker that stalled it originally is gone.


**Then `713`**, because a project whose first command apologises is worse than
it deserves — and the viewer now gives the demos somewhere much better to
point.


Want 801?


--------------------------------------------------------------------------------

### User Request 21

Okay I was reading the documentation with my girlfriend and... well, it needs a
lot of work.


--------------------------------------------------------------------------------

### User Request 22

<local-command-caveat>Caveat: The messages below were generated by the user
while running local commands. DO NOT respond to these messages or otherwise
consider them in your response unless the user explicitly asks you
to.</local-command-caveat>


--------------------------------------------------------------------------------

### User Request 23

<command-name>/config</command-name>

            <command-message>config</command-message>

            <command-args></command-args>


--------------------------------------------------------------------------------

### User Request 24

<local-command-stdout>Set output style to
[1mConcise[22m</local-command-stdout>


--------------------------------------------------------------------------------

### User Request 25

Okay I was reading the documentation with my girlfriend and... well, it needs a
lot of work. Can we go through each document and, paragraph-by-paragraph, come
up with ideas for how to improve it? Let's do one paragraph, one idea, one
paragraph, one idea, etc, for each docs file. Let's start with the overview.


--------------------------------------------------------------------------------

### Assistant Response 25

Two document-level problems first, because they affect every paragraph below:


**It never shows anything.** No code, no picture, no worked example. The README
has both; the overview — the first document in the reading order — has
neither. Someone reads "a box is a plain C function" and never sees one.


**The name is stale.** It says "Minimal SoraMech" throughout; the project's
front door says *ceramic core engine* and the files are `cera.c`/`cera.h`. A
newcomer meets two names in the first two documents.


Now the walkthrough.


---


**¶1 — "an engine for building programs out of small C functions…"**

Lead with the picture from the README instead of prose. Four boxes and two
branches, then the sentence "nobody wrote down that `twice` and `plus` run at
once." Show the claim before making it.


**¶2 — "It is a distillation of the larger SoraMech project…"**

Cut it from the opening, move it to the bottom. A reader who has never heard of
SoraMech is being told what this *isn't* before they know what it is.


**¶3 — "A box is a plain C function you write…"**

Put an actual box next to it: `int add(int a, int b) { return a + b; }`. Three
lines that make "plain C function" concrete and prove the "no registration"
claim in passing.


**¶4 — "A station is one placement of a box in a map…"**

Add the sentence that distinguishes it: *two stations placing the same box are
two independent things* — same code, different buffers, different wiring.
That's the whole reason the word exists and it's currently buried mid-paragraph.


**¶5 — "A task is one invocation…"**

Say what it costs. The README is honest that a task allocates and copies; the
overview presents tasks as free. A reader should meet the trade in the overview,
not discover it later.


**¶6 — "a chain of decreasing lifetime…"**

Make it a three-row table: box / compiled in / forever; station / one placement
/ life of the program; task / one call / microseconds. It's already a table
pretending to be a sentence.


**¶7 — the one rule (blockquote)**

Leave it exactly as it is. It's the best sentence in the document.


**¶8 — "Everything else is a consequence of that sentence."**

Prove it once. Follow the rule to one surprising consequence — "which is why a
box may not remember anything" — so the claim isn't just asserted.


**¶9 — "Nothing polls. Nothing scans for ready work…"**

Split it. The mechanism (the check is the tail of a write) and the consequence
(no ready-queue, no work-stealing) are two thoughts sharing a paragraph, and the
second is the one that surprises people.


**¶10 — "the graph propagates the way a fire propagates…"**

Cut the simile or earn it. Fire spreads to everything adjacent; a delivery
starts *one* station and only when all its ports are full. The metaphor is doing
the opposite of the rule stated three paragraphs above.


**¶11 — "A value is atomic and independent."**

Say what breaks before saying what's true. Open with the question a reader
actually has — *if I send two values through, do the two halves stay
together?* — then answer no.


**¶12 — "easiest to see where a graph splits and rejoins…"**

Draw it. This is the single hardest idea in the document and it's described in
words only. Four boxes and two numbered values would end the confusion in one
glance.


**¶13 — "That is the design, not a gap in it."**

Give the fix its own line, in bold: **things that must stay together have to be
one value.** It's the actionable sentence and it's currently the third clause of
a paragraph.


**¶14 — "Parallelism is not something the programmer arranges…"**

Add the negative case. Every claim here is upside; a reader has no idea when
*not* to use this. The README says it plainly — boxes too small lose to a
function call — and the overview doesn't.


**¶15 — "The cost is that a box cannot keep state…"**

Show the accumulator. "Route a box's output back into its own input" is
abstract; two lines of map file make it obvious and connect back to ¶3.


**¶16 — "What is deliberately absent" (four bullets)**

Add what *is* present and might surprise: a running program can be rewired, a
box can be compiled while it runs, a program can be written to disk and picked
up again. The absences read as limitations without the capabilities beside them.


**¶17 — "Reading order"**

Say what each document answers, not what it describes. "002 describes what a
station is made of" tells me nothing about whether I need it; "002 — why a
station and not just a function" tells me whether to read it.


---


Want me to apply these to 001 now, or keep walking and collect ideas for 002
through 010 first?


--------------------------------------------------------------------------------

### User Request 26

Let's try fixing 001 and see how it goes.


--------------------------------------------------------------------------------

### Assistant Response 26

Let me verify the accumulator actually works before I put it in a document.


The self-wire is accepted; my test was wrong — a designated result only holds
when nothing is wired to it. Let me fan out instead.


Verified — 15, deterministic. The loop needs a seed on the back-edge port or
the station never becomes ready; that's worth documenting too.


Let me check the map in the document actually loads, since I wrote it from
memory of the C version.


The accumulator is nondeterministic in what I *measured* — `last = 6` on one
run. Results come out of a port in no guaranteed order, so "the last one taken"
isn't "the final total". Let me check properly.


Better — it holds *every* running total: **1, 3, 6, 10, 15**. And my first run
took them out in a different order, which is the document's own no-ordering rule
biting the document's own example. Let me say that.


001 rewritten and committed. 180 lines, all 17 ideas applied.


**Two things I got wrong writing it, both caught by running the example:**


The accumulator doesn't hold *one* total — it holds **every running total**,
1/3/6/10/15, one per run. Obvious once seen, and not what the old text implied.


And collecting those five and keeping the last gave **15 four times and 6
once**. A port has no head and no tail, so the take order isn't promised. The
document's own rule caught the document's own example — so both facts are now
stated where a reader will hit them, and the example teaches the no-ordering
rule instead of just asserting it.


The map is real and compiled in: `maps/132-the-accumulator.map`.


**Structure now:**


| was | is |

|---|---|

| three prose paragraphs before anything concrete | picture first, then a three-line box, then a map file |

| "distillation of SoraMech" in ¶2 | a note at the bottom, after you know what it is |

| pairing described in prose, mid-section | its own heading, *"Do two values stay together?"*, with a diagram and the answer **No** |

| "propagates the way a fire propagates" | cut — fire spreads to everything adjacent, which is the opposite of the rule |

| "Minimal SoraMech" throughout | ceramic core engine, with the name note explaining both |

| four absences, no capabilities | absences *and* the three things easy to miss: rewiring, late boxes, capture |

| reading order describing documents | a table saying what each one **answers** |


Next: **002 — stations and ports**?


--------------------------------------------------------------------------------
