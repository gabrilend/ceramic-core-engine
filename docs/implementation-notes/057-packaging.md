# 057 — Packaging the engine as a library

What it would take to hand this to another project. Nothing here is
built; this is a survey of the distance between where the code is and
where it would have to be, with the decisions that have to be made
before any of it can start.

The short answer: **the engine is close, and the obstacles are almost
entirely about names and about who owns the process.** No architecture
has to change. What has to change is that roughly forty symbols with
common English names currently escape into whatever links them, that
every error takes the host process down with it, and that two global
variables mean there can only ever be one map.

---

## First, what kind of library this is

This is the thing that decides everything else, so it goes first.

A normal C library is a runtime artifact: you link it, you call it. This
one has **two halves that live in different times**.

**The build-time half.** A consumer writes their own box functions in C.
The generator reads those sources and emits a registry — a shim per box,
a record per box carrying its name, parameter types and sizes, return
type, exact task size, and a comparison function per comparable type,
every size written as a `sizeof` expression so the compiler computes it
and the generator never guesses. **That output cannot be shipped**,
because it is derived from source the library author has never seen. The
consumer must run the generator over their own code, every build.

**The runtime half.** The pool, the station table, delivery, gathering,
statics, routing, the map file parser and loader, the observer. About
4,300 lines including headers today — run `wc -l libs/*.c libs/*.h
src/*.c src/*.h` for the current figure. This half is ordinary compiled
code and could be a static archive tomorrow.

So the deliverable is not "a library". It is **a library, a code
generator, and a build rule that ties them together** — closer in shape
to a parser generator than to a math library. Any packaging that ships
only the compiled half ships something nobody can use.

A consequence worth stating plainly: **the consumer's build depends on
LuaJIT**, because the generator is a Lua script. That is not removable
without rewriting the generator in the language of the host build, and
it should be declared loudly rather than discovered.

---

## What a consumer's build has to do

Three steps, in this order, every build:

1. **Generate.** Run the generator over the consumer's box sources,
   emitting one C file. It must rerun whenever any box source or the
   generator itself changes, and it must write nothing on failure — a
   build that compiles against yesterday's registry is worse than a
   build that stops.
2. **Compile.** The emitted registry, the consumer's own program, and
   the engine.
3. **Link and run.** The program loads a map file, starts the pool, and
   the map runs until the work is done.

Step 1 is where the packaging work actually is. Steps 2 and 3 are
ordinary.

---

## What stands in the way

Everything below is real and was found by reading the built code and the
symbol table of a linked test binary, not by guessing.

| in the way | what it does to a consumer | size of the fix |
|---|---|---|
| **Engine symbols are common words.** `map_create`, `map_connect`, `map_start`, `map_destroy`, `pool_create`, `pool_push`, `pool_join` and about thirty more are exported unprefixed. | Any host program with its own notion of a map or a pool fails to link, with a duplicate-symbol error naming a function they never wrote. | Mechanical rename, ~40 symbols, touches source, interface files, docs, and issue text. Half a day, done carefully. |
| **Internals are exported too.** `task_build`, `station_port`, `static_claim`, `gather_claim`, `map_statics_free`, `sora_stats_box_time` are joints between engine files, not API. | They collide like anything else, and they invite a consumer to call them. | Free, if the amalgamation shape below is taken. |
| **The demo boxes export `add`, `mix`, `keep`, `nudge`, `seven`, `swallow`, `magnitude_squared`.** | These are example code, and `add` is the single most collidable symbol in C. | Exclude `src/boxes/` from the packaged library. Trivial, but it must be deliberate — the build currently wildcards it in. |
| **Two process-wide globals.** The active map (so a box can reach the statics table) and the last load's timing. | One map per process, forever, silently. A host that wants two engines gets one, and the second quietly writes into the first. | Medium. Already the first-pass report's second priority: thread the map through the task instead of parking it in a global. |
| **Every error calls `abort()`.** | A malformed map file, a missing gather source, or an out-of-memory task kills the host application. A library that can end someone else's process on bad input is not embeddable. | Small in code, large in decision. See below. |
| **Five headers that include each other by numbered filename.** | The consumer needs two include paths and has to know that the entry point is `018-station.h`. | Small — one public header. |
| **The generator writes absolute paths** into the emitted registry, which `#include`s each box source whole. | *Settled, and not a blocker.* A consumer states the directories they build from and recompiles if they move; anyone who wants relocation builds their own configuration around it. The emitted registry is already marked do-not-commit, so nothing durable carries a machine's paths. | none — declare it |
| **The pool ends itself when work runs out.** | An embedded engine that quietly shuts down when its queue drains is a surprise. The mechanism to prevent it exists — an outside submitter registration — but it is opt-in and undocumented as a lifecycle concern. | Documentation, plus possibly a named "stay resident" mode. |
| **No version anywhere.** | Nothing detects an engine and a map file that disagree about the format. | Small: two macros and a check at load. |

---

## What the public surface should be

The symbol table of a linked binary shows what escapes today. Sorting it
into what a consumer should be able to call, versus what is internal
machinery, gives this:

**Build a map by hand** — create it, place a box by name, wire an output
port to an input port, convert an input port to a static and give it a
value. (The gatherer conversion this listed is gone with the pull path;
see [056](056-no-pull-path.md).)

**Load a map from a file** — the one call most consumers will use, plus
the parser and its cleanup for anyone who wants the parse without the
loading.

**Run it** — start (which creates the pool and releases it), wait,
destroy. Inject a value from outside. Count what the seed sweep enqueued.

**Watch it** — start and stop the observer thread, print the station
report, the buffer report, the shutdown summary, ask a slot's current
depth, dump the live map back out as a map file.

**Edit it while it runs** — connect, disconnect, write a static value.
Writing one is not merely configuration: it runs the readiness check on
the station holding it, so a control socket turning a knob is the same
act as a wire delivering into that port.

**Ask about boxes** — find a box record by name, find a struct's field
table, print the registry, name the box that owns a shim.

**The pool on its own** — create, push, pop, release, join, destroy,
register an outside submitter, ask the worker index and count, read the
queue statistics.

**Internal, and should stop being visible:** task construction, port
lookup, the static and gather claim helpers, statics teardown, the
box-timing hook the generated code calls, and both globals.

One observation from sorting that list: **the pool has no dependency on
stations at all.** It moves opaque structs and calls one function
pointer per struct. It already lives in `libs/`, which the table of
contents describes as "the pool, and anything reusable". It could ship
as its own artifact, usable by someone who wants a work queue and none
of the rest — and doing so would prove the separation is real rather
than merely intended.

---

## Three shapes it could take

### A. Amalgamation — one `.c` and one `.h`

The whole engine concatenated into a single translation unit, plus a
single public header. The consumer drops two files into their tree.

This is the shape I would take, for four reasons, one of which is
decisive:

**It solves the symbol problem almost for free.** One translation unit
means every function not named in the public header can be marked
`static`. The internals stop existing as linker symbols entirely — no
renaming, no discipline required, no way to regress. The renaming job
shrinks from every symbol to only the genuinely public ones.

**No include paths, no header set.** The awkwardness of installing
headers named `018-station.h` disappears, because they stop being files.

**It preserves the reading order.** The numbered files are a story meant
to be read in sequence. An amalgamation is that story in one file, with
a banner at each seam naming the original. Emitting `#line` directives at
each seam keeps compiler errors and debugger backtraces pointing at the
real numbered sources, so nothing is lost by reading the pieces instead.

**One file is one thing to vendor.** No install step, no package
manager, no version skew between a header and an archive.

The cost is that any change recompiles the whole engine. At this size
that is a fraction of a second, and it is a cost paid by the consumer's
build rather than by ours.

### B. Headers plus a static archive

The conventional shape: an `include/` directory and a `libsoramech.a`.
Familiar to every C programmer, works with every build system, and
allows partial recompilation.

But it does nothing about symbols — every internal stays exported unless
each one is individually marked, which is discipline that decays. And it
needs an install step, an include path, and a story about which header
is the entry point.

### C. Source drop-in — a submodule or a vendored subtree

Copy the repository in, add its sources to your build. Zero packaging
work, and the consumer can read and step through everything.

It is the worst of the three for the actual problem: every symbol
collides, the demo boxes come along, and the consumer inherits the
project's directory layout and numbering scheme inside their own tree.
Worth supporting as a fallback, never as the recommendation.

| | **A. amalgamation** | **B. headers + archive** | **C. source drop-in** |
|---|---|---|---|
| files the consumer takes | 2 | a directory + an archive | the repository |
| internal symbols hidden | **free — one TU** | one by one, by hand | not at all |
| include paths to configure | none | one or two | two |
| install step | none | yes | none |
| partial rebuilds | no | yes | yes |
| reading order preserved | **yes, with banners** | yes | yes |
| debugger points at real files | with `#line` | yes | yes |
| demo boxes excluded | by the script | by the script | by the consumer, if they notice |

---

## What the packaging script would do

A sibling of the generator, in Lua, taking the project root and an
output directory — same argument conventions as everything else here.

**1. Emit the public header.** Concatenate the public declarations in
numbered order under one include guard, dropping the sections the
headers themselves mark as internal joints between engine files. This is
not a copy: the header is derived, so it cannot drift from the sources
it describes.

**2. Emit the amalgamated source.** Concatenate the engine's C files in
numbered order — `libs/`, then `src/`, excluding `src/boxes/` and
`src/generated/`. At each seam, a banner naming the original file and a
`#line` directive so diagnostics point home. Strip the inter-file
`#include` lines, since the headers are now above in the same file.

**3. Hide the internals.** Read the public header for the names it
declares; mark every other file-scope definition `static`. The list is
*derived from the header*, not maintained by hand, so a function added
to the engine is private by default and becomes public only by being
declared in the header — which is the correct default and the one that
cannot rot. The generator already parses C declarations for exactly this
kind of work, so the machinery has precedent in the project.

**4. Copy the generator** and the map file format document, because the
consumer needs both at build time.

**5. Emit a worked example** — one box, one map file, one program that
loads and runs it, and a Makefile fragment showing the three-step build.
The example is the real documentation of how the two halves fit
together.

**6. Prove it.** Build the packaged library outside the repository,
against the example, and run it. Packaging that has not been compiled
somewhere else is a guess. This wants to be a test script beside the
others, so a change that breaks the packaged shape fails the ordinary
test run rather than being discovered by whoever tries to use it.

---

## Decisions that have to be made first

**0. Which compiler, and when one is needed at all — settled.** The
engine requires **GCC**, and it is the same GCC that built the binary:
the build records which compiler it used and every runtime compile
invokes that one. That gives a program exactly one answer to `sizeof`
by construction rather than by checking, which is the property that
matters when a box compiled later has to wire into a box compiled
earlier.

Clang and Windows are deliberately deferred. Clang runs on Windows in
two modes and needs the platform toolchain for headers and libraries
either way, so choosing it would relocate the dependency rather than
remove it; and loading compiled code on Windows is `LoadLibrary` and
`GetProcAddress` rather than `dlopen`, which is work that has nothing
to do with compiler choice. What is owed to the deferral is one cheap
thing: **the compiler invocation lives in one place in the source**, so
adding a second is a local edit.

**And the toolchain is not a tax on every program.** Since
[311d](../../issues/311d-the-map-as-manifest.md) makes a map a build
input, a program whose map names only boxes the binary already carries
never invokes a compiler at all — it ships as one file, source text
included, and runs on a machine with no toolchain on it. The compiler
is required precisely when new code is genuinely arriving, which is
[310](../../issues/310-boxes-compiled-at-runtime.md)'s path and the
only case where anyone would expect otherwise.

**1. What happens on an error — settled.** An installable handler,
called with the message immediately before the engine dies. The host
gets to log it, flush its own state, and know what happened. **The
engine still stops.** A consumer who wants to handle their own errors
restarts the engine; that is the entire recovery story, and it is
deliberate rather than a gap.

What this must never become is an error code returned up a call chain
that a consumer can ignore — that is a fallback wearing a return type.
A wrong answer which keeps flowing is worse than no answer, and that
does not soften because the caller would find it convenient. Breaking
loudly is the feature: a map that is wrong should stop being a running
program.

What the handler actually adds is *legibility*. Today the message goes
to standard error and the process dies, so a host with its own log has
no way to capture what happened or why. The handler closes that gap and
opens no other one.

**2. Can there be two maps in one process?** Today, no, because of two
globals, and nothing says so. Either fix it — thread the map through the
task, which the first-pass report already wants for other reasons — or
state the restriction in the header where someone will read it before
they build on the assumption. Fixing is better; the packaging is a good
excuse, since a library with a hidden singleton is a defect that only
shows up in someone else's program.

**3. Does the engine own the process's threads?** Starting a map spawns
one worker per core by default and the pool ends itself when the queue
drains. A host application with its own thread budget needs to know
both. The mechanism for keeping an engine resident already exists in the
outside-submitter registration; what is missing is naming it as the
lifecycle question it is.

**4. What is the prefix?** `sora_` is already used by a few symbols and
by every include guard. Applying it to everything public is the obvious
choice; it just has to be done once, everywhere, and grepped for
stragglers — including in the documents and issue files, which name
these functions in prose.

**5. Does the pool ship separately?** It has no dependency on anything
above it. Shipping it alone is nearly free and would prove the layering.

---

## Rough order of work

Ordered so that each step is useful even if the next never happens.

1. **Exclude the demo boxes and the generated registry** from whatever
   gets packaged. One line of intent, and it stops the worst collisions.
2. **The single public header**, derived rather than written. Immediately
   useful inside the project too, since it settles what is API and what
   is machinery.
3. **The amalgamation script**, using that header to decide what stays
   visible. This is where the symbol problem mostly dies.
4. **The prefix**, applied to what is left visible.
5. **The error handler hook.**
6. **The worked example and the out-of-tree build test**, which is what
   turns all of the above from plausible into demonstrated.
7. **The one-map-per-process fix**, which is worth doing on its own
   merits and is not on the critical path for a first package.

Steps 1 through 3 are the ones that change the shape of the thing. The
rest is finishing.

---

## Related

- [007 — The build path](../007-datapath-build.md), the generator and
  what it emits
- [008 — Map file format](../008-map-file-format.md), which a consumer
  needs at runtime
- [009 — Loading](../009-datapath-load.md), the entry point most
  consumers will use
- [006 — Scheduling](../006-datapath-scheduling.md), for the thread
  lifecycle question
- `../../notes/first-pass-report.md`, where the process-global map is
  already named as a debt
