# Conversation Summary: e1dd3730-6b88-4b9f-a4bb-32130c092d39

Generated on: 2026-09-05 17:32:35

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
