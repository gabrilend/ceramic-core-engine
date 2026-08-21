# 310 — Boxes compiled while the program runs

## Current behavior

**Built.** A box's C source
can be handed to a running program and a station can place it. The
path is the one this issue described and every step of it runs the
same program the build runs: the generator turns the source into a
registry source, the compiler turns that into a shared object, the
dynamic linker loads it, and its rows join the table stations are
placed from.

**Which compiler is not incidental.** The build bakes in the one that
built the binary and this uses that one, so a program has exactly one
answer to `sizeof` **by construction** rather than by checking. That
was decided under *Which compiler, and at what setting* below and is
the reason a second, faster in-process compiler was refused.

**The table grows by adding a block and never moves a row.** The
generated array is the first block and is const; later rows live in
blocks of their own, published by one pointer write so a reader
walking the list either sees a block complete or does not see it at
all. Generated rows are searched first, deliberately: bringing in new
code can never shadow a box a map already depends on.

**Seven things are proven**, in `tests/075-test-latebox.c`:

- a box written after the program started is placed, wired to a box
  the binary was built with, and delivers
- its widths came from a compiler rather than from its signature
- a struct it defines, laid out like one the program already knows and
  named something the program has never heard, wires and arrives intact
- a late box of the wrong width is refused by the same rule and the
  same message a build-time box gets
- **a late box whose struct disagrees in *layout* at the same width is
  accepted**, which is written deliberately — see below
- a source the generator refuses and a source the compiler refuses are
  both refused here, and neither leaves a row behind
- a dump taken after a box was added **reloads in a fresh process**

**That fifth one asserts a wrong answer on purpose.** Two structs of
three floats in different orders have the same width, so the wire is
legal and the fields arrive transposed. That is the accepted cost of
[309](309-types-by-width.md)'s width comparison, recorded as
a non-guarantee, and pinned by a test so nobody later mistakes it for
an oversight. If shape comparison is ever built, that test is what
fails, and its failure is the good news.

**The source is filed twice.** Once under a serial number, which is
what was handed over, and once per box under that box's own name,
which is how a **later** process finds it. When a name is not found,
recovery compiles it back from the saved source — and **says out loud
that it did**, because a fallback nobody was told about is the shape
this project treats as an error. That is what makes the dump of a
grown program a real record rather than a description of something
unreproducible.

**Two RAM tiers, and the reason had to be rediscovered.** Source text
goes to the read tier and the compiled library to the execute tier,
because `/dev/shm` is commonly mounted so that nothing on it may be
executed — so a shared object written there compiles and then fails to
load, with a message about mapping a segment that says nothing about
the cause. The project's two-tier rule existed; what it was *for* was
found by breaking it.

**Unloading is built, on the mechanism 214 provides.** A box added
while the program ran can be unloaded, closing the library its code
came in. It is refused while any station in the given map places it —
unloading code a station names is exactly the crash this avoids — and
the harder half, that a worker may be *inside* that code right now, is
answered by the per-worker counter the scrapyard uses. That counter
deliberately spans a whole task rather than a delivery walk, which is
what lets one number answer "might somebody be inside this box" and
"might somebody be inside this station" at once.

**What it checks is the map you hand it.** A process running several
maps could have another one placing this box, and nothing here can see
that, because nothing in this engine is process-wide and there is no
list of running maps to consult. Unloading a box another map places is
the caller's to avoid — stated rather than defended, on the same
footing as reaching into a program by anything other than its input
and output stations.

**And [216](216-removing-a-station.md) made this worth much more than
it looked.** The original reasoning was that a station's box cannot
change and a station cannot be removed, so unloading served only a box
compiled and then never placed. Stations can be removed now, so a box
really can stop being used, and unloading reclaims code from programs
that reshape themselves rather than only from ones that guessed wrong
at startup.

## Intended behavior

**A box's C source can be handed to a running program, and a station
can place it.**

The path is not exotic and each step already exists somewhere:

1. Take the C source for one or more boxes.
2. Run the generator over it, producing the shim, the box record, any
   field tables, and any comparisons — exactly what the build does.
3. Compile that to a shared library, by invoking a compiler.
4. Load it and look up the shim.
5. Add the row to the registry.

**A function signature is not enough on its own, and it is worth being
precise about why.** A signature gives you names: the box's name, its
parameter type names, its return type name. What it cannot give you is
**sizes and offsets** — how many bytes a slot holds, how far into a
struct a field sits, how large a task must be. Those are what the
engine actually runs on, and the standing rule is that the compiler
computes every one of them and the generator never guesses. So the
signature supplies the paperwork and a compiler supplies the numbers.
There is no version of this that skips the compiler.

**The box table becomes a growable table.** It is indexed by name when
a station is placed and by row afterward, so it takes the same paging
shape as everything else that grows: add a block, never move what is
already there, and readers resolving a row are never disturbed. That is
the fourth or fifth use of the same pattern in this engine, and by now
it should be a shared piece rather than a fifth hand-rolled one.

[311](../311-the-registry-dissolved.md) shrinks that table to two columns
— a name and a placement function — which makes growing it cheaper than
this issue assumed and changes nothing about the shape.

### Which compiler, and at what setting

**One compiler per program, and it is the compiler that built the
binary.** The build records which one; every runtime compile invokes
that same one. So there is exactly one answer to `sizeof` in a given
program **by construction** — not checked, not mitigated, structurally
impossible to violate.

**That is GCC, for now.** Not because it is best but because deciding
between compilers is work nobody needs yet. Clang, and Windows, are
deferred; what is owed to that deferral is one thing only, and it is
cheap: **the invocation lives in one place in the source**, so adding a
second is a local edit rather than a hunt. That is not an abstraction —
there is nothing yet to abstract over — it is just not scattering the
word `gcc` across three files.

**Runtime compiles use low optimization; the build uses high.**
Optimization level cannot change struct layout — layout is fixed by the
platform ABI, and `-O0` and `-O2` are the same compiler emitting the
same offsets — so this varies compile speed freely with no risk at all
to the sizes. Build time is allowed to be slow. Runtime compilation is
not.

**A second compiler was considered and refused, and the refusal is
worth keeping.** TinyCC, as `libtcc`, compiles C in-process — no
subprocess, no shared object, no `dlopen`, roughly an order of
magnitude faster on small inputs — and it is built for exactly this.
What it costs is a second answer to `sizeof`. For standard types there
would be no disagreement, since struct layout is ABI rather than
compiler; the divergence lives in bitfields, `long double`, packed
attributes, and extensions TinyCC does not implement, where the usual
outcome is a refusal to compile rather than a wrong number. The width
check would catch the remainder. It was still refused, because *one
answer by construction* is a better property than *two answers and a
net*, and because `-O0` recovers most of the speed for free.

**If runtime latency still hurts, the move that keeps this decision is
to take the compile off the critical path** — hand the source over,
return immediately, let the station go live when the compile finishes.
The engine is already asynchronous by construction: a box that is not
ready yet is a station that is not ready yet, which is a state it
already has a word for.


**A box arriving late needs to report the width of each input and of
its output, and that is the whole of what the type system asks of it.**
Under [309](309-types-by-width.md) a wire is legal when the two sides
count the same number of bytes, and those numbers come from the same
place they always came from: a `sizeof` the compiler computed while
compiling this box. So a runtime box supplies exactly what a build-time
box supplies, by exactly the same means.

**It introduces no new kind of error**, which is worth stating because
an earlier draft of this issue said the opposite. Two structs of the
same width and different layouts already wire without complaint at
build time — that is the accepted cost of comparing widths. A box
compiled later increases the *opportunity* for that to happen, since
two separately-written sources are likelier to disagree than one, but
the failure is the same failure and it is already on the books. Shape
comparison would have closed it, here and everywhere else equally; it
was declined for reasons that have nothing to do with this issue, and
declining it does not make this one more dangerous than the ground it
stands on.

What a runtime box must still supply beyond widths is mechanical: a
name to be found by, a shim to call, and — if its return type is
orderable — a comparison, for a comparator to use.

**The generator has to be reachable at runtime**, which is what
[308](308-generator-in-c.md) delivers by making it a standalone C
program with no dependency on the engine. As a Lua script it is a build
tool; as a C program it is something the engine can call, and it is the
same parser either way — which is the point, because a second parser
would eventually disagree with the first about what a box is.

## Suggested implementation steps

1. The box table as a growable table, paged, with lookup and placement
   reading it safely while it grows.
2. Compile-and-load as a deliberate call: take source, produce a shared
   library in the RAM-backed build tier, load it, resolve the shim.
   Failures name the compiler's own output rather than summarising it.
3. Adding a registry row: name, shim, per-parameter widths, return
   width, task size, and a comparison if there is one. Nothing about
   the new box's types is checked against the program's, because
   [309](309-types-by-width.md) checks widths when a wire is drawn and
   there is nothing earlier worth asking.
4. A test that a box written after the program started is placed,
   wired, and delivers values byte-identically.
5. A test that a box whose parameter is a different **width** from what
   feeds it is refused, naming both types and both widths — the same
   refusal a build-time box gets, arriving by the same path.
6. A test that a box whose struct disagrees in *layout* with one
   already loaded, at the same width, **is accepted and delivers
   scrambled fields.** Written deliberately, because this is the
   accepted cost of comparing widths and a test that pins it is how
   somebody later discovers it was a decision rather than an oversight.
7. The source saved to the RAM-backed directory at the moment the box
   is compiled, named so a reloader can find it from the box's name
   alone. A test that a dump taken after a runtime box is added reloads
   in a fresh process.
8. Unloading, on the retire-sweep-free mechanism from
   [214](214-destinations-without-a-lock.md), with the per-worker
   counter widened to span the whole task rather than the delivery
   walk. A test that a box compiled, never placed, and unloaded frees
   its library while a saturated pool runs.

## Open questions

**Answered:**

- *Where does the compiler come from?* **Assume it is there — but only
  when it is actually needed, which is rarer than this issue first
  assumed.** The engine calls a C compiler at runtime and does not
  negotiate about it. What answers the packaging concern is an install
  script that builds the dependencies from source into a local place,
  written once the system is finished rather than designed around now.

  **[311d](../311d-the-map-becomes-code.md) narrows when that bites.** A
  program whose map names only boxes the binary already carries never
  invokes a compiler at all and ships as one file. The toolchain is
  required precisely when new code is genuinely arriving, which is the
  only time anyone could reasonably expect otherwise. So the dependency
  stopped being a tax on every program and became the price of one
  capability.

- *Which compiler, and is a fast in-process one worth a second answer
  to `sizeof`?* **GCC, the same one that built the binary, at low
  optimization; and no.** Written up under *Which compiler, and at what
  setting* above, including why TinyCC was refused despite being built
  for exactly this, and why varying the optimization level is free
  where varying the compiler is not.

  Discovery, if the engine ever has to find a compiler rather than
  being told, must **say what it picked** — *no compiler recorded;
  using `/usr/bin/gcc`* — because silently falling through a list is
  the shape of fallback this project treats as an error.

- *Can a loaded box ever be unloaded?* **Yes, by retire, sweep, free —
  the mechanism [214](214-destinations-without-a-lock.md) builds for
  destination arrays**, including the scrapyard's own lock and the rule
  that nothing is acquired while holding it. Unloading a shared library
  out from under a worker running its code is the same lifetime problem
  wearing different clothes.

  **Two things about it are not the same, and both matter.**

  The liveness test is a different window. The destination sweep asks
  whether a worker is inside a *delivery walk*; this has to ask whether
  a worker is inside a *box*, which happens earlier in the same task.
  Rather than carry two counters, one per worker bumped at the start
  and end of the whole task — call, finish hook, and free — is odd
  across both windows and answers both questions. Less precise for
  destinations, since an array becomes freeable slightly later than it
  strictly must; one mechanism instead of two, on a path nothing
  measures.

  **And unloading turned out to be worth much more than this issue
  first thought.** The original reasoning ran: a station's box cannot be
  changed and a station cannot be removed, so unloading serves only the
  box that was compiled and then never placed — narrow, but worth
  building anyway, since the alternative is code accumulating forever
  in a program whose whole point is being edited while it runs.

  [216](216-removing-a-station.md) removes that limit. A station can be
  taken out, so a box really can stop being used, so unloading reclaims
  code from programs that reconfigure rather than only from programs
  that guessed wrong once at startup. **The two share their hardest
  step**: waiting until no worker is inside the thing being freed,
  which is one per-worker counter answering the same question about a
  box and about a station. Build it once.

- *Does a runtime-added box survive a dump and reload?* **Yes, because
  its source is written down when the box is created, not when a dump
  asks for it.** Every box compiled at runtime has its source saved to
  the RAM-backed directory the moment it comes into existence, treated
  exactly like a log: written as it happens, ephemeral, gone at reboot.

  **Writing it at creation rather than at dump time is the whole
  decision**, and it is the same one [106](../106-stopping-on-purpose.md)
  makes about opening the report's destination during startup. A dump
  may be taken while the program is dying, and a failure path is the
  worst possible moment to discover that something needs saving. The
  artifact exists before anybody needs it.

  So a dumped program reloads into any process that can see that
  directory, which includes a fresh process on the same machine and
  excludes one after a reboot — the same lifetime a log has, stated
  rather than implied.

## Related

- [308 — The generator, in C](308-generator-in-c.md), which is what
  makes the parser callable at runtime rather than only at build time
- [309 — Types compared by width](309-types-by-width.md), a hard
  prerequisite — by-name comparison makes this corrupt silently
- [311 — The registry dissolved](../311-the-registry-dissolved.md), which
  shrinks the table this makes growable to a name and a pointer, and
  narrows when a toolchain is needed at all
- [303 — The registry](303-registry-emission.md), the table
  this makes growable
- [306 — Build integration](306-build-integration.md), whose
  no-partial-output guarantee applies here too: a failed generation
  must leave nothing loadable
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which made the frozen registry a visible limit
- [801 — The workbench in the browser](../801-browser-workbench.md), which
  reads C files the running program never compiled
- [057 — Packaging](../../docs/implementation-notes/057-packaging.md),
  where a runtime dependency on a compiler has to be weighed
