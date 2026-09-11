# 057 — Packaging the engine as a library

> **Phase 9 executed this survey**, and the parts still undone are still
> described correctly below: the error handler, the out-of-tree build
> test, and the one-map-per-process fix.
> [Phase 9's progress page](../../issues/phase-9-progress.md) says what
> stands today and where the survey's recommendation was overturned.

What it would take to hand this to another project: a survey of the
distance between where the code was and where it would have to be, with
the decisions that had to be made before any of it could start.

The short answer: **the engine is close, and the obstacles are almost
entirely about names and about who owns the process.** No architecture
has to change. What has to change is that roughly forty symbols with
common English names currently escape into whatever links them, that
every error takes the host process down with it, and that two global
variables mean there can only ever be one map.

---

## The link line gained two flags, and they are not optional

A program built with this engine must be linked so that the engine's
own symbols appear in the executable's dynamic table, and so that the
functions nothing reaches are thrown away. Both halves are needed, and
the obvious way to write the first one cancels the second.

**Why exporting is not a detail.** A box compiled while the program
runs arrives as a shared object and is opened at run time. The
generator emits, alongside its call site, a **placement function** that
builds the station: it calls straight into the station layer. A shared
object cannot see a symbol the host executable did not publish, so
without the export such a box loads and then fails to resolve, naming a
function in the engine rather than anything about the box.

**Why it must be narrow.** A symbol in the dynamic table is a root the
section collector cannot touch, because the reason it is exported is
that code which does not exist yet may look it up by name — so the
linker can prove nothing about who calls it. `-rdynamic` exports every
global symbol and therefore declares the whole binary reachable, at
which point `--gc-sections` has nothing left to discard. Hand the
linker `--dynamic-list=src/098-engine-surface.syms` instead: it names
the two families of function that are genuinely public, exports those,
and leaves the rest collectable. That file carries the measurements.

So the distance between this engine and somebody else's machine grew
by two linker settings, and they are the kind that are easy to omit —
one produces a failure that points somewhere else entirely, and the
other produces no failure at all, just a program carrying code it never
runs.

## First, what kind of library this is

This is the thing that decides everything else, so it goes first.

A normal C library is a runtime artifact: you link it, you call it. This
one has **two halves that live in different times**.

**The build-time half.** A consumer writes their own box functions in C.
The generator reads those sources and emits one C file — a shim per
box, a placement function per box that builds a station for it, a field
table per struct, a comparison function per comparable type, and each
source again as text so the binary carries the C it was made from.
Every size in it is written as a `sizeof` expression, so the compiler
computes it and the generator never guesses. **That output cannot be shipped**,
because it is derived from source the library author has never seen. The
consumer must run the generator over their own code, every build.

**The runtime half.** The pool, the station table, delivery, statics,
routing, the way a description comes in, the observer. **Not the map file
parser** — that belongs to the compiler now and is not linked into
anything anybody runs. Run `wc -l src/cera.c src/cera.h` for the size.
This half is ordinary compiled code and could be a static archive
tomorrow.

So the deliverable is not "a library". It is **a library, a code
generator, and a build rule that ties them together** — closer in shape
to a parser generator than to a math library. Any packaging that ships
only the compiled half ships something nobody can use.

**Issue 910 made the three one.** `serac` is a single executable
carrying the generator and the engine's own source as text, so the three
things that had to travel together now travel as one that cannot be
separated. The build rule went with them: `serac` knows the two linker
settings, because it is the thing that knows about them, and a consumer
who omits one can no longer exist. The library is still there for
anybody who wants it — `serac --unpack` writes out exactly the files
this section describes — but taking it is now a choice rather than the
only route.

A consequence that used to be stated here plainly, and no longer
applies: **the consumer's build once depended on LuaJIT**, because the
generator was a Lua script. Issue 308 rewrote it in C. A consumer's
build now needs a **C compiler and nothing else** — it compiles `serac`
and runs it. The generator depends on nothing the engine provides, so
there is no bootstrap problem: it can be built before anything else
exists, and building `serac` is that generator being used once, on the
engine's own files, before it is compiled in.

Regenerating **this project's own HTML documentation** still needs
LuaJIT. That is deliberately out of scope: it is project tooling and
not on the path a consumer walks to build a program. The distinction
belongs wherever the dependency is declared — building the engine
needs a C compiler; regenerating our documentation needs LuaJIT.

---

## What a consumer's build has to do

Three steps, in this order, every build:

1. **Generate.** Run the generator over the consumer's box sources,
   emitting one C file. It must rerun whenever any box source or the
   generator itself changes, and it must write nothing on failure — a
   build that compiles against yesterday's emission is worse than a
   build that stops.
2. **Compile.** The emitted file, the consumer's own program, and
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
| **Engine symbols are common words.** `cera_map_create`, `cera_map_connect`, `cera_map_start`, `cera_map_destroy`, `cera_pool_create`, `cera_pool_push`, `cera_pool_join` and about thirty more are exported unprefixed. | Any host program with its own notion of a map or a pool fails to link, with a duplicate-symbol error naming a function they never wrote. | Mechanical rename, ~40 symbols, touches source, interface files, docs, and issue text. Half a day, done carefully. |
| **Internals are exported too.** `task_build`, `station_out_port`, `static_claim`, `gather_claim`, `map_statics_free`, `cera_stats_box_time` are joints between engine files, not API. | They collide like anything else, and they invite a consumer to call them. | Free, if the amalgamation shape below is taken. |
| **The demo boxes export `add`, `mix`, `keep`, `nudge`, `seven`, `swallow`, `magnitude_squared`.** | These are example code, and `add` is the single most collidable symbol in C. | Exclude `src/boxes/` from the packaged library. Trivial, but it must be deliberate — the build currently wildcards it in. |
| **Every error calls `abort()`.** | A malformed map file, a missing gather source, or an out-of-memory task kills the host application. A library that can end someone else's process on bad input is not embeddable. | Small in code, large in decision. See below. |
| **Five headers that include each other by numbered filename.** | The consumer needs two include paths and has to know that the entry point is `018-station.h`. | Small — one public header. |
| **The generator writes absolute paths** into the emitted file, which `#include`s each box source whole. | *Settled, and not a blocker.* A consumer states the directories they build from and recompiles if they move; anyone who wants relocation builds their own configuration around it. The emitted file is already marked do-not-commit, so nothing durable carries a machine's paths. | none — declare it |
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
report, the buffer report, the shutdown summary, ask a port's current
depth, dump the live map back out as a map file.

**Edit it while it runs** — connect, disconnect, write a static value.
Writing one is not merely configuration: it runs the readiness check on
the station holding it, so a control socket turning a knob is the same
act as a wire delivering into that port.

**Ask about boxes** — find a box record by name, find a struct's field
table, print what was emitted, name the box that owns a shim.

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

## The decisions the survey turned on

**Which compiler, and when one is needed at all — settled.** The engine
requires **GCC**, and it is the same GCC that built the binary: the build
records which compiler it used and every runtime compile invokes that
one. That gives a program exactly one answer to `sizeof` by construction
rather than by checking, which is what matters when a box compiled later
wires into a box compiled earlier.

Clang and Windows are deliberately deferred. Clang runs on Windows in two
modes and needs the platform toolchain either way, so choosing it
relocates the dependency rather than removing it; and loading compiled
code on Windows is `LoadLibrary` and `GetProcAddress` rather than
`dlopen`. What is owed to the deferral is one cheap thing: **the compiler
invocation lives in one place in the source.**

**The toolchain is not a tax on every program.** A map is a build input,
so a program whose map names only boxes the binary already carries never
invokes a compiler — it ships as one file, source text included, and runs
on a machine with no toolchain on it. The compiler is required precisely
when new code is genuinely arriving.

**What happens on an error — settled.** An installable handler, called
with the message immediately before the engine dies, so a host can log
it, flush its own state, and know what happened. **The engine still
stops.** A consumer who wants to handle their own errors restarts it.

What this must never become is an error code returned up a call chain
that a consumer can ignore — a fallback wearing a return type. What the
handler adds is *legibility*: the message used to go to standard error
and the process died, so a host with its own log had no way to capture
it.

**What the prefix is — settled.** `cera_`, applied to everything public,
including in the documents and issue files, which name these functions in
prose.

**Whether two maps can run in one process — settled.** They can. Nothing
in the engine is process-wide, and a test runs two and writes into one.

**Does the engine own the process's threads?** Open. Starting a map
spawns one worker per core by default and the pool ends itself when the
queue drains; a host with its own thread budget needs to know both. The
mechanism for keeping an engine resident exists in the outside-submitter
registration; what is missing is naming it as the lifecycle question it
is.

**Does the pool ship separately?** Open. It has no dependency on anything
above it, so shipping it alone is nearly free and would prove the
layering.

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
