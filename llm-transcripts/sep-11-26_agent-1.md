# Conversation Summary: 117289dc-ce30-4938-b136-70f1042cab26

Generated on: 2026-09-11 16:40:59

--------------------------------------------------------------------------------

### User Request 1

hi, can we start work on the cerac compiler?

--------------------------------------------------------------------------------

### Assistant Response 1

Issue 910 is the blueprint and it's ready — the only thing still open in it is
the scratch-directory question, and it's the first thing step 3 needs.

Some grounding before you pick: writing the engine out is ~1 MB of text into
tmpfs, which is nothing. The cost that actually shows up is **compiling**
`cera.c` — a second or two of gcc on every single `cerac` invocation, paid
again for a one-line change to a box source.

**[Asked the user]**

*Q1 — Scratch dir:* Where does cerac's scratch directory live, and is what it
holds kept between runs?
- Kept, named by content (Recommended) — Write the engine to a fixed path
  under the executable RAM tier whose directory name is a digest of the embedded
  bytes — /tmp/<project>/cerac-<digest>/. A different cerac writes a different
  directory, so a stale copy cannot be compiled against by construction rather
  than by deletion. This also lets the compiled cera.o be kept there, which is
  where the real saving is: the second invocation skips the engine compile
  entirely.
- Discarded every run — A fresh directory per invocation under the executable
  RAM tier, removed when cerac exits. The project's habit, and the simplest
  thing to reason about: nothing persists, so nothing can be stale. Every
  invocation recompiles the engine.
- Kept at a fixed path — One directory, same name every time, overwritten when
  the bytes differ. Simplest persistent form, but staleness is prevented by a
  check rather than by construction — if the check is ever wrong, a program
  compiles against an engine it was not built from.
→ *(no answer recorded)*

Good question, and the answer changes the design. Let me check the two things
that decide whether it's possible.

No scratch directory needed — you're right, and I verified it. Three findings:

**The compiler reads a program from stdin.** `gcc -x c - -o out` compiles a
program handed to it on a pipe. The reason a scratch directory looked necessary
is that `cera.c` contains `#include "cera.h"`, and an include is a *filesystem
lookup* — the preprocessor has to open a file by that name. But that lookup
can be dissolved by concatenation instead: put the header's text ahead of the
body's and delete the one include line. This is the same move the engine already
made when eleven files became `cera.c`. I built it and ran it: header, engine
body, generated construction code and `main` as one translation unit down a
pipe, no file anywhere.

**The linker's export list was the other file, and a flag replaces it.**
`-Wl,--export-dynamic-symbol='cera_*'` does what
`--dynamic-list=098-engine-surface.syms` does. Verified identical: 142 `cera_`
symbols exported, 143 total, both ways. The syms file stays in the repository
because its value is the thirty paragraphs of reasoning around the one line.

**And the output is unchanged.** Stripped, the one-translation-unit binary and
the normally-built one are both 154,560 bytes, differing by 412 bytes of text.
Merging costs nothing and gains nothing in the product — it is purely a change
of mechanism.

So the open question dissolves rather than getting answered. Updating the
blueprint before building.

Now the blueprint edits.

Blueprint updated. Now step 1 — the embedding. Let me find the tool that turns
a file into a C string array.

Now adding the shared escaper to gentext.

Now point the box-source emitter at it, removing the duplicate.

Now the `--embed` mode. First the shared declaration.

Now the `--embed` mode in the generator.

Now the function split itself.

Now `cerac` itself.

Now the two-stage build in the Makefile.

Two ordering problems — let me fix them.

Now the cerac rules, placed after `SURFACE` is defined.

Steps 2 and 3 work. Now the generated `main` — I need what the map reader
exposes.

Now the generated `main`.

Now wire it into cerac.

One command, one program. Let me look at the `main` it wrote.

The out-of-tree test compiles every `scripts/*.c` into one generator — and
cerac now has its own `main` there. Fixing that and adding its second half.

Now the other result shapes.

A `const char *` return is already refused at parse time — so that branch in
my emitter is unreachable. Removing it.

While the suite runs, the engine-facing half: cerac needs the two modes the
runtime path calls.

Suite green. Adding back the map-parser include cerac now needs.

Now the map-less `--shared` mode the engine's late-box path needs.

Now the engine's runtime path. First, finding `cerac`.

Now the three call sites.

Now the late-box path.

Request timed out

--------------------------------------------------------------------------------

### User Request 2

please contineu

--------------------------------------------------------------------------------

### Assistant Response 2

Request timed out

--------------------------------------------------------------------------------

### User Request 3

please contineu

--------------------------------------------------------------------------------

### Assistant Response 3

Now the two command buffers — a truncated command is a silently wrong command,
so it should refuse.

Now the companion `.info.md` files the project requires for every source.

The search treats `CERA_COMPILER` as a name only — but the build defines it as
a path. One rule fixes both readings.

Now the compiler's own test.

cerac prints its output path to stdout, which lands in the output of any program
that brings in late code. A compiler should be silent on success.

Now the map-reload failure. Let me find which test and reproduce.

My test clobbered its own box source — and that's a real hazard, not just a
test bug. cerac should refuse to write over an input.

Now step 8 — the documents. Let me see what they currently say.

All tests pass, including the new one. Now the table of contents and README.

Now the issue's current-behavior section and the phase progress page.

Now the open questions the work surfaced, and the phase progress page.

Final verification before committing — full build and suite from clean.

Waiting on the clean build. Here's where things stand.

**`cerac` works.** One command turns a description and some C into a program:

```
$ cerac shapes.map shapes.c
$ ./shapes 7 9 -3 4
14
4.5
3
{ 4, 6 }
```

Four result types, printed one per line in port order, through the same writers
that put a constant into a map file — so `{ 4, 6 }` is also a valid argument
to the next program, which the test checks in both directions.

**Your question removed the scratch directory rather than answering it.** The
three findings, all verified rather than reasoned about:

- `cc -x c -` compiles a program arriving on a pipe. The only thing forcing a
  file to exist was one `#include "cera.h"`, and an include is a filesystem
  lookup — dissolved by putting the header's text in front and deleting the
  line, the same move the engine already made when eleven sources became
  `cera.c`.
- `-Wl,--export-dynamic-symbol='cera_*'` replaces the export list file.
  Identical result: 142 `cera_` symbols exported, 143 total, both ways.
- Stripped, the one-translation-unit binary and the normally-built one are both
  154,560 bytes, differing by 412 bytes of text. Mechanism changed, product
  didn't.

**The engine's three baked-in paths are one name now.** A name with a slash is a
path, used as it stands — that's how this repository points at its own build
tree. A bare name is looked for beside the program and then on `PATH`, which is
what a program `cerac` builds gets, and what makes it relocatable. That was the
actual defect in issue 910.

**Four things turned up that the blueprint didn't anticipate:**

- **A compiler that announces where it put things can't be used by a program.**
  `cerac` printed its output path on success, and the engine runs it as a
  subprocess when a description arrives at run time — so that sentence landed
  in the middle of somebody's results.
- **The default output path can land on an input.** `--emit-c` over `shapes.map`
  wants to write `shapes.c`, and a box source of that name beside it is entirely
  ordinary. It destroyed the source and then failed to compile it, complaining
  about a brace on a line nobody wrote. Found by a test doing it by accident.
  Refused now.
- **A `const char *` result can't happen.** The parser already refuses a box
  returning a borrowed pointer — the bytes live wherever it points and a value
  on a wire has no owner. The branch I wrote to print one was unreachable code
  asserting something untrue.
- **A `const char *`-free reading of the no-results case:** an `out` line needs
  a destination, so a description marking no results just omits the line. That
  program prints nothing of its own and exits zero.

**Five open questions are recorded and not yet worked through** — so the issue
stays in progress, not complete. The two I'd want your answer on first:

1. **Is `--results` the right shape?** A result lands in an array the program
   holds and the engine never grows it, because that's what lets a worker write
   into it without a lock. So a bound must exist; today it's chosen at build
   time (1024 by default, `--results=N`). Overflow is a refusal naming how many
   there were, never a truncation — but somebody who doesn't know how many
   values their program makes has to guess, rebuild, guess again. Alternatives:
   read the count from the environment at startup, or have the generated `main`
   grow its own memory — which costs it being something a person could have
   written by hand.

2. **Does `--main=` earn its place?** It started as scaffolding to test the
   pipeline before the generated `main` existed, and survived because it
   composes with `--emit-c` to mean "my main, joined to the construction code".
   That's genuinely useful — but it's a third route to a place `--emit-c` and
   `--unpack` already reach between them.

--------------------------------------------------------------------------------

### User Request 4

<task-notification>
<task-id>bi0lvvcth</task-id>
<tool-use-id>toolu_01KFc3sukDzSg1aaUmgm9WZM</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/bi0lvvcth.output</output-file>
<status>completed</status>
<summary>Background command "Clean build and full suite" completed (exit code
0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 4

Clean build from scratch, all tests pass — the `undefined_thing` line is the
deliberate refusal test 075 exercises.

**Done, tested, and uncommitted.** Clean rebuild from `make clean`, full suite
green including the new compiler test. 1,476 lines across 31 files — nothing
in the tree is anyone else's work.

I have not committed. The house rule ties a commit to a completed issue, and 910
is genuinely in progress: five open questions recorded in the blueprint, none
worked through. Say the word and I'll commit it as in-progress work with the
transcripts attached — there are two, the session record I appended to and
`sep-11-26_agent-1.md`, which the harness wrote out during the session.

**What the whole thing collapses to:**

| before | after |
|---|---|
| 3 files copied + 9 compiled into a generator + 3 compiler invocations + a hand-written `main` | `cerac shapes.map shapes.c` |
| 3 absolute paths baked in, 2 naming the build machine | 1 name, searched |
| an emitted C file written to scratch on every late compile | nothing on disk but the program |

The out-of-tree test now does its work **twice** — once the old four-file way,
once in one command — so the two are checked against each other rather than
one replacing the other on trust.

**The two questions I'd most like your answer to** (the other three are in the
issue file):

**1. Is `--results` the right shape?** A result's values land in an array the
program holds, and the engine never grows it — that's what lets a worker write
into it without taking a lock. So a bound has to exist somewhere. Today it's a
number chosen when the program is built: 1024 by default, `--results=N` to
change. Overflow is a refusal naming how many values there actually were, never
a silent truncation. But somebody who doesn't know how many values their program
produces has to guess, rebuild, guess again. Two alternatives: read the count
from the environment when the program starts, which moves the guess from the
builder to the runner; or let the generated `main` grow its own memory, which
costs it being a thing a person could plausibly have hand-written.

**2. Does `--main=` earn its place?** It began as scaffolding — a way to
exercise the pipeline before the generated `main` existed. It survived because
it composes with `--emit-c` to mean "my `main`, joined to the construction
code", which is genuinely useful. But `--emit-c` and `--unpack` already reach
that place between them, and three routes to one destination is how a tool
starts being hard to describe.

--------------------------------------------------------------------------------

### User Request 5

so, the compiler should be all we need to package a release, right? If the user
wants to embed, they can --unpack and use the .c and .h files themselves. They
still need the compiler tho right, if they want to compile things at runtime
yeah? Do we have a script that compiles the compiler? do we need to specify the
C files to the compiler, or can they be found by using the map file?

> - A const char * result can't happen. The parser already refuses a box
> returning a borrowed pointer — the bytes live wherever it points and a value
> on a wire has no owner. The branch I wrote to print one was unreachable code
> asserting something untrue.

I guess we'll have to write to a file if we ever want to return a string. Or
maybe build them as a struct? Is there any overhead when building a struct as
compared to an array? I know that arrays in C are just pointers to the first
element, with some memory reserved after them... so maybe a struct would hold an
array and that could be our "string"? What do you think?

> 1. Is --results the right shape? A result lands in an array the program holds
> and the engine never grows it, because that's what lets a worker write into it
> without a lock. So a bound must exist; today it's chosen at build time (1024
> by default, --results=N). Overflow is a refusal
>      naming how many there were, never a truncation — but somebody who doesn't know how many values their program makes has to guess, rebuild, guess again. Alternatives: read the count from the environment at startup, or have the generated main grow its own memory — which costs it
>      being something a person could have written by hand.


um to answer this I'd need a review of the embedding process.

> 2. Does --main= earn its place? It began as scaffolding — a way to exercise
> the pipeline before the generated main existed. It survived because it
> composes with --emit-c to mean "my main, joined to the construction code",
> which is genuinely useful. But --emit-c and --unpack already
> reach that place between them, and three routes to one destination is how a
> tool starts being hard to describe.

not sure what you're asking here, can you rephrase and expand?

--------------------------------------------------------------------------------

### Assistant Response 5

Good questions — several have answers I should check rather than assert. Let
me test the string one first, since it's the most interesting.

Your instinct was right, and it already works — no change needed.

Both answers measured. Now the packaging questions — let me check what
building `cerac` from scratch actually takes.

All four checkable things checked. Here are the answers.

## Strings: you were right, and it already works

No change needed — a struct holding a char array is exactly the right answer,
and the engine already handles it end to end:

```
$ ./words 5
{ "POSITIVE" }
$ ./direct '{ "hello there" }'
{ "HELLO THERE" }
```

That's `typedef struct { char text[32]; } word;` returned by one box, wired to a
second box that upper-cases it, printed in the map's own value syntax — and
read back in as a command-line argument in the same syntax.

**One correction to the mental model, because it's why this works.** An array in
C is not a pointer. `char text[32]` *is* thirty-two bytes of storage; the name
decays to a pointer to the first element when you use it in most expressions,
which is where the impression comes from. Measured:

```
char[32] on its own          32 bytes, alignment 1
a struct holding char[32]    32 bytes, alignment 1
offset of the array inside the struct: 0
```

**Zero overhead. The struct is byte-identical to the bare array.** Padding only
appears when you add a member that needs alignment — `{ char text[32]; int n;
}` is 36, and that 4 bytes is the `int`, not a wrapper cost.

The reason it must be a struct is narrower than overhead: **C cannot return an
array by value, but it can return a struct by value.** The wrapper exists to
make the bytes returnable, not to make them safe.

And the reason the engine needs the bytes rather than a pointer is the same one
that refuses `const char *`: a value on a wire is copied into the receiving
port's buffer. Copying a pointer copies eight bytes that point at memory the box
has already returned from. Copying a struct copies the text itself. **The string
has to *be* the value, not point at one** — which is the same rule as "things
that must stay together have to be one value" in the guarantees.

The cost is that the width is fixed at compile time. A 32-byte word truncates at
31 characters plus a terminator. Writing to a file is the escape hatch for
genuinely unbounded text, but for names, labels, and short messages the struct
is the answer.

## Packaging a release

**Yes — `cerac` plus its sources is the whole deliverable.** I built it from a
bare directory to check:

**14 files, 628 KB**, and three commands:

```
gcc -I. -o generate  066-gentext.c 068-genparse.c 069-genemit.c \
                     070-generate.c 100-mapparse.c 105-mapwrite.c
./generate --embed 145-embedded-engine.c cera.h cera.c 098-engine-surface.syms
gcc -I. -DCERAC_CC='"gcc"' -o cerac 144-cerac.c 066-gentext.c 068-genparse.c \
                     069-genemit.c 100-mapparse.c 105-mapwrite.c 145-embedded-engine.c
```

**And no, there is no script that does that — that's a real gap.** The project
Makefile knows those three steps, but a release bundle doesn't carry the
Makefile. A release needs a `build-cerac` script of about fifteen lines. I'd
like to write it.

**Your read on the runtime compiler is exactly right.** `--unpack` gets somebody
`cera.c` and `cera.h` to build against by hand — but the moment their program
wants to compile a box or a description *while it is running*, it invokes
`cerac` as a subprocess. So:

| what the program does | what has to be on the machine |
|---|---|
| runs a map it was built with | nothing. No toolchain at all. |
| accepts a new description or box at run time | `cerac`, beside the program or on `PATH` |

The engine looks beside the executable first, then on `PATH`, and
`CERAMIC_COMPILER` overrides both.

## Finding the C files from the map

**Partly derivable today, and I think it should be fully automatic.**

A station line addresses its box three ways, and the middle and last ones name
the file:

```
station louder (shout)            bare function name — no file
station louder (words.c:shout)    basename and function — names the file
station louder (src/words.c:shout)  path and function
```

I verified the `words.c:shout` form builds and runs. So a map written entirely
in the path form already contains its own source list — but the bare form is
the common one and carries nothing.

**My recommendation: `cerac program.map` with no sources named should compile
every `.c` beside the description.** That is not a new idea in this project —
the Makefile already does exactly this for box sources, with the reasoning
written down: *discovered, never listed, so a box cannot exist that the build
silently does not see.* Naming files explicitly stays available for anything
outside that directory.

The one hazard is a `.c` file in that directory that isn't a box source — a
hand-written `main`, or `--emit-c` output — which the parser would reject.
That's a loud failure, not a silent one, so it's tolerable, and it's a further
reason `--emit-c` refusing to write over an input was worth adding.

## The result bound — the mechanism you asked for

Here is what actually happens to a result value, field by field. An output port
is:

| field | type | what it holds |
|---|---|---|
| `into` | `void *` | the caller's array — the engine never allocates it |
| `room` | `int` | how many elements fit |
| `elem_size` | `int` | one value's width, from `sizeof` on the box's return type |
| `taken` | `_Atomic int` | how many values have *claimed* a slot |

The generated `main` declares `static int landing_0[1024];` and registers it
before any worker is released. When a station produces a value for that port,
the worker does one atomic add on `taken`, gets back a slot number nobody else
will get, and `memcpy`s `elem_size` bytes to `into + slot * elem_size`.

**That single atomic increment is the entire synchronisation.** No mutex, no
growth, no reallocation — which is only possible because the address and the
length cannot change while workers are running. A worker handed a slot at or
past `room` writes nothing, and `taken` keeps climbing anyway, which is how the
program can say afterwards *exactly* how many values it lost rather than
guessing.

So the bound must be fixed **before the first worker is released**. That rules
out growing it — any growth means either a lock on the hot path or moving the
array out from under a worker mid-`memcpy`.

**But it does not have to be fixed at build time.** The array could be
`malloc`ed in `main` with its size read from the environment at startup, still
before `cera_map_start`. That keeps every property above, keeps the `main`
something a person could have hand-written, and moves the guess from whoever
builds the program to whoever runs it — which is where the knowledge actually
is. There's precedent: the engine already reads `CERAMIC_WORKERS` the same way.

That's my recommendation, with `--results=N` staying as the compiled-in default.

## The `--main=` question, rephrased

Sorry — that was too compressed. Plainly:

**There are now three ways to build a program with your own `main` instead of
the generated one, and I'm asking whether three is too many.**

1. `cerac --unpack DIR` — take `cera.c` and `cera.h`, write your own
   everything, build it yourself.
2. `cerac --emit-c program.map boxes.c` — get the construction code as a C
   file, drop it into your own build next to your own `main`.
3. `cerac --main=mine.c program.map boxes.c` — hand cerac your `main` and let
   it do the whole build.

The third exists because I needed it to test the pipeline before the generated
`main` was written. It survived because it's convenient. But it does nothing
that (1) and (2) can't, and every option is something somebody has to read about
before they understand the tool.

**I lean toward keeping it**, for one reason that isn't convenience: options 1
and 2 both require the person to know the two linker settings, and getting one
wrong produces a program that builds fine and then fails at run time when a box
arrives. Option 3 is the only "my main, but you handle the build correctly"
route. If that's persuasive, it stays and I'll document it that way.

---

**Open right now, for you:** shall I write the release build script, and make
sources discoverable from the map's directory? Both are small. Neither is
committed yet — the tree is green but uncommitted, since 910's questions are
still live.

--------------------------------------------------------------------------------

### User Request 6

also can we rename cerac to serac? because serac is pronounced the same as cera.
Meanwhile soramech will continue to refer to a single ceramic program.

yes we should write the release build script. We shouldn't make sources
discoverable from the map's directory - instead, we should remove that first
option, and make users ALWAYS type which file they want to pull their functions
from. We want to remove as much ambiguity and unexpected behavior as we can.

> There are now three ways to build a program with your own main instead of the
> generated one, and I'm asking whether three is too many.

Are these functionally different at all? For example, 1. seems like the case I
envisioned. 2. seems like instead of compiling a map to a binary, it "compiles"
it to a C file maybe? And 3. seems like option 2 with fewer steps, but it
compiles the whole program, soramechs included alongside the user's other code.
Is that right? I think we really only need that first option... For option 1,
the users needs to know "linker settings" - what do you mean by that? Do we
require them to link things a certain way or something? My understanding was
that cera.c and cera.h can be included however the user wants. Also, let's say
they do option 1 and build out their program, embedding a little virtual
soramech. They'd still need cerac - sorry - serac, the compiler, to compile
programs at runtime, but that's not required for pre-compiled boxes. Which
they'd need the compiler for anyway, if they want to compile boxes for their
architecture. So, why include cera.c and cera.h at all, if they're just going to
be compiling them on the go anyway?

> The generated main declares static int landing_0[1024]; and registers it
> before any worker is released. When a station produces a value for that port,
> the worker does one atomic add on taken, gets back a slot number nobody else
> will get, and memcpys elem_size bytes to into + slot *
  elem_size.

> That single atomic increment is the entire synchronisation. No mutex, no
> growth, no reallocation — which is only possible because the address and the
> length cannot change while workers are running. A worker handed a slot at or
> past room writes nothing, and taken keeps climbing
> anyway, which is how the program can say afterwards exactly how many values it
> lost rather than guessing.
>
> So the bound must be fixed before the first worker is released. That rules out
> growing it — any growth means either a lock on the hot path or moving the
> array out from under a worker mid-memcpy.
>
> But it does not have to be fixed at build time. The array could be malloced in
> main with its size read from the environment at startup, still before
> cera_map_start. That keeps every property above, keeps the main something a
> person could have hand-written, and moves the guess from
> whoever builds the program to whoever runs it — which is where the knowledge
> actually is. There's precedent: the engine already reads CERAMIC_WORKERS the
> same way.


okay, uh, yeah --results seems like a bad idea. The result is what's passed to
stdout right? Internal maps when their "result" or "input" boxes (with the $
assignments) are just treated as "unwired" when the internal map is added to an
external map. Right? And external maps are wired to result when they have dollar
sign inputs and outputs. Meanwhile, there's a runtime way to mark something as
being sent to the result or being fed by input, right? If not, there should be.
Part of the wiring function I think. So, with --results, this is for determining
behavior that's only used when embedding, right? Or is it used on the cli, to
determine how many "outputs" we send to stdout? I think we should not have that,
and just trust that the user will ask for a regular cli argument if they want to
know "how many" things should be returned. It's just an argument! They can build
it to their specifications. We don't have to build that sort of thing into the
engine.

> The cost is that the width is fixed at compile time. A 32-byte word truncates
> at 31 characters plus a terminator. Writing to a file is the escape hatch for
> genuinely unbounded text, but for names, labels, and short messages the struct
> is the answer.

why don't we just have "strings structs" of increasing size? Or maybe a struct
that has an undefined-at-compile-time number of elements? Is that a thing? I
think no, in C. Can we have variable sized arrays? I don't think so. So, we need
to just have as many different sizes as we think is necessary. Or is that
something the user will have to handle?

so, I heard that if we include one function from for example math.c it'll
compile in the entire math.c library. Is that true? If so, that's definitely not
what we want haha

--------------------------------------------------------------------------------

### Assistant Response 6

Several of these need measuring rather than asserting. Let me check the math.c
question first — it's the most consequential.

Confirmed, and worse than you'd hope: **50 unused boxes cost 30 KB of text and
all 50 are exported symbols.** Let me find out why, and what it would cost to
fix.

Everything measured. Let me answer, then do the two things you approved.

The files were never committed, so `git mv` has nothing to move — and your
rule is that both versions must be tracked. So: commit the work as it stands
first, then the rename becomes a real, visible move.

Now the rename, as a tracked move.

Now the release build script you approved.

Waiting on the suite. Here are the answers to the rest.

## The `math.c` question — you're right, and it's worse than you'd hope

Measured. One box used, fifty unused sitting in the same file:

```
                          text
one box in the file      92,109
fifty-one in the file   122,134     +30 KB
unused boxes exported as symbols: 50 of 50
```

**But the cause isn't textual inclusion.** The build already puts every function
in its own section and tells the linker to discard unreached ones. What defeats
that is the *export list*: every box gets a placement function published in the
dynamic symbol table, and **a published symbol is a collection root the linker
may never discard** — because the whole reason it's published is that code
which doesn't exist yet may look it up by name.

I measured the three builds of the identical program:

| build | text | `cera_*` published | can it be extended? |
|---|---|---|---|
| exported — what ships today | 122,134 | 169 | yes |
| `--gc-sections`, nothing exported | **65,284** | 0 | no |
| no linker settings at all | 114,647 | 0 | no |

**Sealing it halves the binary.** 47% of that program was boxes and engine
internals nothing reaches, kept alive purely so a description arriving next year
could bind to them.

Both sealed builds still ran correctly. So the cost is exact and the trade is
clean: *extendable at run time* costs 57 KB on that program, and a program that
will never be handed new code shouldn't pay it. That's a real finding and wants
its own issue — a "this program is finished" flag on `serac` that drops the
export list.

## The linker settings — I overstated it, and you were right

**`cera.c` and `cera.h` can be included however you want.** I said option 1
requires knowing the linker settings; that's only true for one case.

- `--gc-sections` is pure optimization. Never required.
- The export list is required **only** if the program will `dlopen` code
  compiled while it runs. Without it, a late box loads and then fails to resolve
  `cera_*`.

I built a program with no linker flags at all and it ran fine. So: if you're
embedding a soramech inside a larger C program and never compiling at runtime,
include the two files however you like and ignore all of this.

## The three ways — are they functionally different?

Your reading is almost exactly right. Precisely:

1. **`--unpack`** — you get `cera.c` and `cera.h`. You write everything, build
   however you want. The engine is a library inside your program.
2. **`--emit-c`** — `serac` compiles the map to a **C file** instead of a
   binary, exactly as you guessed. That file contains the construction calls
   plus a `main`, and `#include`s `cera.h`. You compile it yourself.
3. **`--main=`** — option 2 with the compile done for you, engine included.

**And you're right that option 1 is the one that matters.** 2 and 3 are
conveniences that sit between "you do everything" and "serac does everything".
Given what I just measured — that the linker settings aren't actually required
for the embedding case — my earlier argument for keeping `--main=` collapses.
**I'd drop `--main=` and keep `--emit-c`,** which is the genuinely distinct
thing: "show me the C you would have compiled."

## Why ship `cera.c` and `cera.h` at all?

Because **not everyone needs runtime compilation.** The split:

- *My program **is** a soramech program* → `serac`, one command, nothing else
  on the machine.
- *A soramech is a **component** inside my larger C program* → `--unpack`,
  build it into your own build system, ship a binary to a machine with no
  toolchain.

The second never invokes a compiler and never needs `serac` at runtime. Boxes
compiled ahead of time need no compiler present — you're right about that too.

## `--results` — dropping it, and you've named the reason

Your read of the mechanism is correct on every point: an inner map's `$`-marked
ports stop being argv slots once the outer map wires them (the engine recomputes
that from the wires rather than storing it), and `cera_map_designate_argument` /
`cera_map_designate_result` do it at runtime.

And yes — `--results` is *only* the generated `main`'s array size. It's not
engine behaviour at all. **Dropping the flag.** The generated `main` keeps one
fixed bound and the loud refusal if a program exceeds it; anyone who needs to
care builds their own `main`. As you say: it's just an argument, and they can
build it to their specifications.

## Strings of several sizes

You're right on the C facts. Variable-length arrays exist but can't be struct
members with a `sizeof` the compiler knows. Flexible array members (`char
text[];` as a last member) exist but `sizeof` excludes them — the engine would
copy zero bytes of text, so they're worse than useless here.

**My recommendation: the user declares their own, and the engine ships none.**
The reason is specific: two structs of different sizes are *different types*,
and the engine refuses a width mismatch when wiring. So a supplied family of
`word32`/`word64`/`word256` would fragment the graph — you couldn't wire a 32
into a 64. One line (`typedef struct { char text[64]; } line;`) at whatever
width the program needs keeps every station wireable to every other.

---

Three things I'd like to confirm before doing them: dropping `--results`,
dropping `--main=`, and the "sealed build" flag for programs that will never be
extended. The last is the 47% one.

--------------------------------------------------------------------------------

### User Request 7

<task-notification>
<task-id>b1baxa95p</task-id>
<tool-use-id>toolu_019oq8i3WhdQ4J9hLTwn518Y</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/b1baxa95p.output</output-file>
<status>completed</status>
<summary>Background command "Full suite after the rename" completed (exit code
0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 7

All tests pass after the rename. Committing the rename and the release script
together — both are the packaging change — with the blueprint separate.

**Committed, tree clean, all tests pass.** Three commits:

- the compiler itself, with the transcripts
- the rename — `git mv` so both names are in the record, plus the release
  build script
- the blueprint for station lines naming their file

**`serac` is the name everywhere now** — files, symbols, defines, docs, tests.
The engine's build-time define stayed `CERA_COMPILER`, since it's the engine
asking where its compiler is rather than anything named after the tool.

**The release build script works from a bare bundle.** Fifteen files, 636 KB, in
a directory that can't see this repository:

```
one:   the generator
two:   the engine, as text
three: serac
  carries the engine byte for byte, and invokes cc
```

The last step is a check, not a claim — it asks the compiler it just built to
unpack the engine and compares byte for byte against what went in. A `serac`
carrying a different engine than the one beside it would build programs nobody
could account for.

**The map-format change is blueprinted, not built.** Removing the bare box name
touches every map file, every fenced example in the docs, and the map text
inside tests — the same shape of change the station-line rewrite was, so it
gets its own issue and its own migration tool rather than riding along. It's
`issues/609`. Say when and I'll build it.

I left one question inside that blueprint deliberately unanswered: **once every
station line names its file, should `serac` take its sources from the
description instead of the command line?** Your instinct was "always type which
file" — typing it in the map satisfies that, and then repeating it on the
command line is the same fact twice. But the command line is also where a person
says what *this* build is made of, and a description that reaches out and names
files can name files that aren't there. Both arguments are written down; neither
wins yet.

**Still awaiting your word on three things**, none of them done:

1. **Drop `--results`** — you've argued it out and I agree; it was never
   engine behaviour.
2. **Drop `--main=`** — my case for keeping it rested on the linker settings
   being mandatory, which the measurement showed they aren't.
3. **A "this program is finished" flag** that drops the export list. That's the
   47% one: 122,134 bytes of text down to 65,284 on the program I measured, at
   the cost that nothing can ever be compiled into it afterward.

--------------------------------------------------------------------------------

### User Request 8

there's a "zoom" widget at the bottom right of the documentation pages that
doesn't seem to do anything.

serac is the compiler, the ceramic core engine is the engine itself, cera for
short, and a soramech is a compiled map file. It also refers to a boxmapgraph.

> Both sealed builds still ran correctly. So the cost is exact and the trade is
> clean: extendable at run time costs 57 KB on that program, and a program that
> will never be handed new code shouldn't pay it. That's a real finding and
> wants its own issue — a "this program is finished"
>   flag on serac that drops the export list.

we should assume that if the program has been compiled, that it's "finished" -
though, if we want to add say a second function from math.c, and we want to do
it at runtime by crafting a new box (what's the workflow for that, by the way?)
then we'll need to be able to correctly include another part of the math.c file.

> - The export list is required only if the program will dlopen code compiled
> while it runs. Without it, a late box loads and then fails to resolve cera_

what does dlopen mean?

> And you're right that option 1 is the one that matters. 2 and 3 are
> conveniences that sit between "you do everything" and "serac does everything".
> Given what I just measured — that the linker settings aren't actually
> required for the embedding case — my earlier argument for keeping
> --main= collapses. I'd drop --main= and keep --emit-c, which is the genuinely
> distinct thing: "show me the C you would have compiled."

sounds good.

> The map-format change is blueprinted, not built. Removing the bare box name
> touches every map file, every fenced example in the docs, and the map text
> inside tests — the same shape of change the station-line rewrite was, so it
> gets its own issue and its own migration tool rather
>   than riding along. It's issues/609. Say when and I'll build it.

when! :)

> I left one question inside that blueprint deliberately unanswered: once every
> station line names its file, should serac take its sources from the
> description instead of the command line? Your instinct was "always type which
> file" — typing it in the map satisfies that, and then
> repeating it on the command line is the same fact twice. But the command line
> is also where a person says what this build is made of, and a description that
> reaches out and names files can name files that aren't there. Both arguments
> are written down; neither wins yet.

we should check to see that they're all valid, and if not, we should error.
Otherwise, only use the ones from the map file(s), don't ask for C files at the
command line.

> 1. Drop --results — you've argued it out and I agree; it was never engine
> behaviour.

yes, let's drop

--------------------------------------------------------------------------------

### Assistant Response 8

Clear direction on all of it. Let me start with the bug you found.

Found it — and it's a one-word cause. Every font size in the stylesheet is in
`px`.

Zoom fixed — the control was moving the root font size while every text size
on the page was nailed to a pixel count. Now building 609, with your decision
folded in.

Now the refusal, right where the address is unwrapped.

`die_parse` isn't variadic — the message names the box, so it needs to be.

Now the resolver loses its bare branch, and both writers emit the long form.

Now the two writers — the generator's and the engine's dump, which phase 6
warned carry separate copies of this format.

The engine's dump shortens the address back to a bare name — that's the half
that must change.

This shortening exists for a reason that conflicts with 609. Let me read it
fully before changing anything.

Confirmed: every late box's source is already filed at `<name>.c`, so
`<name>.c:<name>` is a stable address. But wiring that through touches box
matching, and that's a decision the blueprint didn't anticipate — I'll record
it rather than invent it. First, the migration tool.

The tool works. Two corrections before running it for real — completed issues
are history and shouldn't be rewritten.

The harness owns the transcript files and regenerated one, dropping my
hand-appended narrative from the working copy — it's still in the two commits.
I'll stop hand-appending and let the harness write them. Cleaning up dead code
in the tool first.

Now the cases the tool couldn't see — tests that write their own box sources
into heredocs.

The tool mis-resolved one: it matched a test's local `swallow` against a project
box of the same name. Fixing all three, and recording the limitation.

While that runs — recording the conflict the implementation turned up.

Let me answer your two questions while the suite finishes.

## What `dlopen` means

**It loads machine code into a program that is already running, and hands back a
way to call it.**

The normal path: you compile, the linker stitches every function into one
executable, and that set is fixed forever. `dlopen` is the escape from that.
It's a C library call — `dlopen("/path/to/thing.so", ...)` — that takes a
**shared object** (a `.so` file: compiled code that was built to be loadable
rather than standalone), maps its pages into the running process's address
space, and returns a handle. Then `dlsym(handle, "some_name")` asks that handle
for the address of a function or variable by name, and you call through the
pointer it gives back.

It's how a browser loads a plugin, or how a program grows a feature it didn't
ship with.

**Where the engine uses it:** you hand a running program a new description or a
new box. The engine writes the C out, invokes `serac --shared` to compile it
into a `.so`, `dlopen`s that, and `dlsym`s the table describing what arrived.

**And why the export list matters to it.** When the dynamic linker maps that
`.so` in, the new code contains calls to engine functions — placing a station,
drawing a wire. Those functions live in the *host* executable, not in the `.so`.
The linker resolves them by looking up names in the host's **dynamic symbol
table**, which is a list of names the host published when it was built. If
`cera_map_add_station` isn't in that list, the `.so` fails to load, naming the
symbol it wanted. That's the only reason the export flag exists.

## The workflow for crafting a box at runtime

Two calls, and the program hands over C source as a string:

```c
cera_late_compile_source("int triple_it(int v) { return v * 3; }");
cera_map_place_box(m, station, "triple_it", CERA_STATION_PLAIN);
```

What happens between them:

1. The source is **written to the artifact RAM tier first**, before anything is
   done with it — so a dump taken while something is going wrong still has it.
2. `serac --shared` compiles it: shims, a placement function, field tables,
   comparisons.
3. The `.so` lands in the **executable** RAM tier, because `/dev/shm` is usually
   mounted so nothing on it may be executed.
4. `dlopen` with global scope, so a box arriving *later* can bind to this one
   without carrying its own copy.
5. A copy of the source is filed under each box's own name — `triple_it.c` —
   so a *fresh process* reading a dump of this program can find and recompile
   it.

**Your `math.c` case works today**, and this is the mechanism: you hand over the
text of the second function, it compiles, and you place it. What you can't do is
reach into an already-compiled `math.c` and pull out a function that was
compiled but never published — that one is in the binary with no name anyone
can look up.

**Which is exactly the tension in "assume a compiled program is finished."**
Sealing a program drops the export list and halves it, but it also removes the
dynamic symbol table that `dlopen` resolves against — so a sealed program
can't accept a late box at all, not even one carrying its own code, because the
box still needs to call back into the engine. The two can't both be default. My
read: **sealed by default, since most programs never grow**, with a flag for the
ones that will.

While that runs, the three changes you approved.

Now sources from the description, with every named file checked to exist.

The conflict I predicted is now failing the suite. Resolving it the way 609 says
looks right — a late box's address becomes the file its source is actually
filed under.

Now the `box_came_late` helper the dump needs.

The generator's resolver needs the spilled sources filed under the same name the
dump writes.

`address` is `path:function`, but the source table is keyed on the path alone
— so my lookup was silently finding nothing.

Now the duplicate-struct problem my own comment predicted. The real fix is the
one you asked for — serac reads the sources from the description, so the
engine stops passing them.

**`serac acc.map` — no C files on the command line at all — builds and
runs.** That's your design working end to end.

But it surfaced a fork I shouldn't decide alone, and **the suite is currently
red** on one test because of it.

## The fork

When a map says `029-demo-boxes.c:keep`, what does the file part mean?

**(a) Exactly where the file is, relative to the description.** serac joins the
two and opens it. No searching, nothing inferred — which is your stated
principle. The cost: this project keeps maps in `maps/` and boxes in
`src/boxes/`, so every line becomes `../src/boxes/029-demo-boxes.c:keep`. Or the
maps move next to the boxes.

**(b) Enough of a path to identify the file, matched as a suffix at a slash.**
`029-demo-boxes.c` finds `src/boxes/029-demo-boxes.c`; `apes.c` can't reach
`shapes.c`. Lines stay short. The cost: serac has to walk the description's
directory tree looking, which is a search — the thing 609 exists to remove.

The generator already does (b). serac now does (a). **That disagreement is
what's red:** a map resolves for the generator and then serac can't find the
file.

My read is (a), because it's what you asked for and because it makes the
description literally true rather than approximately true. But it means every
map in the project gains a relative path, and I'd rather you choose that than
discover it.

## What stands right now

| | |
|---|---|
| zoom widget | **fixed** — the control moved the root font size while every text size was nailed to a pixel count; the stylesheet is in `rem` now |
| 609, bare names refused | **done** — reader, resolver, both writers |
| migration tool | **done** — 32 addresses in 7 files, plus the heredoc cases by hand |
| `--results`, `--main=` | **dropped** |
| sources from the description | **done and working**, subject to the fork above |
| late boxes | **mechanism done** — a late box is now addressed by the file its source is actually filed under, `triple_it.c:triple_it`, which is stable across processes; blocked on the fork |
| suite | **red**, one test, on the fork |

Three things the implementation turned up that weren't in the blueprint, all
recorded in 609:

- **The dump and the reader disagreed.** The dump shortened late-box addresses
  to bare names *deliberately* — a late box's recorded address is a
  serial-numbered scratch file belonging to a dead process. 609 hadn't
  anticipated it. Resolved by writing the address its source is genuinely filed
  under.
- **My own migration tool mis-resolved one address.** A test's local `swallow`
  matched a project box of the same name, because the tool can't see boxes
  written into heredocs. Fixed, and the limitation is now written into the tool
  so the next person doesn't trust it blindly.
- **Filing one source under several box names makes the compiler read it several
  times**, and it refused the structs as defined twice. Handing serac only the
  description fixed it — a description names each of its files once.

Also worth knowing: **the harness owns the transcript files and regenerates
them**, so the narrative I'd been hand-appending got overwritten in the working
copy. It's preserved in the two commits already made; I've stopped
hand-appending.

--------------------------------------------------------------------------------

### User Request 9

The file paths should be relative to the map file. If you include two map files
in different locations, it should search for the files that particular map
includes in paths relative to those locations. We should add something at the
top of the map files if we want that says "include this file or directory" and
in doing so, it will search in that directory. It's like creating a shortcut.
Here's an example:

math = /home/ritz/soramech/libs/math/
curves = libs/curves.c

then, when using them, it'd be like this:

station add math/arithmetic.c:add.c
[blah blah blah]

station turn curves:rotate.c
[blah blah blah]

does that make sense? the idea is, we're creating a shortcut to those files that
the user can define as they please. The curves example looks in the same
directory as the map file (presumably, /home/ritz/soramech/)

> - Filing one source under several box names makes the compiler read it several
> times, and it refused the structs as defined twice. Handing serac only the
> description fixed it — a description names each of its files once.

explain this more?

--------------------------------------------------------------------------------

### Assistant Response 9

Your alias design makes relative paths bearable — without it every in-tree map
would say `../src/boxes/...` on every line. Writing the blueprint, then building
it.

Now the reader learns the shortcut block.

Now both callers use that one resolver, and the writer emits the block.

Now the writer emits the block, and this project's maps gain a shortcut.

Your alias design is built. While the suite runs, here's the answer to your
other question.

## The duplicate-sources problem, explained properly

It starts with something the engine does on purpose for a good reason.

**When a box is compiled at runtime, the engine saves its source under each
box's name.** Say you hand it this:

```c
typedef struct { float x; float y; } same_shape;
same_shape build_same(int v) { ... }
int        use_same(same_shape s) { ... }
```

That's one piece of text defining a struct and two boxes. The engine compiles it
from a scratch file called `box-4127-3.c`, and then writes **the whole text
twice more** — once as `build_same.c`, once as `use_same.c`.

**Why twice:** a dump of this program names its stations by box. A *fresh
process* reading that dump sees `build_same` and has no idea what file it came
from — `box-4127-3.c` belonged to a process that has ended. So the engine
files the source under a name that can be guessed from the box alone. One source
defining three boxes gets three identical copies.

**What I broke.** When I made the spill hand those by-name copies to the
compiler, the compiler was given `build_same.c` *and* `use_same.c` — two
files, byte-identical, each containing `typedef struct { ... } same_shape;`. It
read both and refused: **struct 'same_shape' defined twice.** Which is correct
— from the compiler's side those genuinely are two declarations of one type,
and it has no way to know they're the same file wearing two names.

**Why handing over only the description fixes it.** A description names each
file it uses once:

```
station make (build_same.c:build_same)
station take (build_same.c:use_same)
```

Both stations name the *same* file, the compiler now derives its source list
from the description, and the deduplication is a property of how a source list
gets built rather than something anyone has to remember. `use_same.c` still
exists on disk for recovery-by-name, and simply isn't read unless a description
asks for it.

**The general shape, which is the part worth keeping:** the copies exist so a
box can be found *by name alone*, and they're safe exactly as long as nothing
reads more than one of them. Having the description choose the files is what
guarantees that, because a description says *which file*, not *which box* — so
the several names one file answers to collapse back into one file.

The shortcut check reads `rest` before skipping whitespace.

The shortcut works — and the error message earns its keep by naming what the
address resolved to. One map the migration regex missed.

**Your example works, verbatim.** `2 × 90 = 180, + 7 = 187`:

```
math   = libs/math/
curves = libs/curves.c

station turn (curves:rotate)
station sum  (math/arithmetic.c:add)
```

A directory shortcut and a file shortcut, both resolved relative to the map
file, no C files on the command line — `serac turn.map` and that's the whole
build.

One deliberate departure from what you typed: **brackets stayed.** Issue 608
made them mandatory last session (`station turn (curves:rotate)` rather than
`station turn curves:rotate`) because the brackets are the one thing that tells
the box address from a name by looking. Say the word if you'd rather drop them.

And two things I decided that you should overrule if they're wrong:

- **A trailing slash is what makes a shortcut a directory.** `math = libs/math/`
  is a directory; `curves = libs/curves.c` is a file. Written, not worked out
  — asking the filesystem would make a map read differently depending on
  what's on disk that day.
- **A shortcut line needs no keyword.** It's recognised by the second word being
  `=`, which no station line can have (a station's second word is its name). So
  it costs nothing from the namespace station names live in.

The two open questions are in the blueprint: whether a shortcut to a missing
directory fails when declared or when used, and whether a blank line is
*required* between the block and the stations or merely usual.

While that runs, the tests 610 asks for.

Same heredoc blind spot from the earlier migration. Let me find every one of
them rather than fix one at a time.

C-string map text the tool can't see either. Let me sweep all of them at once
rather than one per test run.

98 of them across 8 files. Extending the migration tool rather than hand-editing
— that's what it's for.

96 of 98, and the two skipped are correct — `sevn` is a deliberate typo in a
refusal test, `quadruple` a box defined at runtime. Cleaning the dead attempts
out of the tool, then running it.

Your alias design is built and working. Summary while the suite runs.

**Your example, verbatim, building and running:**

```
math   = libs/math/
curves = libs/curves.c

station turn (curves:rotate)
station sum  (math/arithmetic.c:add)
```

`serac turn.map` — no C files named — gives `187` for input 2. Directory
shortcut and file shortcut, both relative to the map.

**Three decisions I made that you should overrule if wrong:**

- **Brackets stayed** (`station turn (curves:rotate)`). 608 made them mandatory
  last session because they're what distinguishes the box address from the
  station name by eye. Your example dropped them.
- **A trailing slash makes a shortcut a directory.** Written, not probed —
  asking the filesystem would make a map read differently depending on what's on
  disk that day.
- **No keyword for a shortcut line.** It's recognised by the second word being
  `=`, which no station line can have. Costs nothing from the namespace station
  names live in.

**What the build turned up that the blueprint didn't foresee:**

The generator and the compiler had **two different ideas of what a filename
means** — the generator matched it as a path suffix, the compiler joined it to
the map's directory. A map could satisfy one and be refused by the other. They
now both call one function, and the generator's suffix matching is gone. That
was the fork I asked you about; your answer settled it.

**And my migration tool was blind twice over, in ways worth recording.** It
couldn't see boxes written into shell heredocs (it mis-resolved a test's local
`swallow` against a project box of the same name), and it couldn't see map text
inside C string literals — 98 addresses across 8 test files. I extended it to
handle the C strings rather than hand-editing them, and it got 96; the two it
skipped are a deliberate typo in a refusal test and a box defined at runtime,
both correct to skip. The heredoc blindness is now written into the tool's own
documentation, since it's not fixable — the tool can't parse a shell script's
runtime behaviour.

**Still open, both recorded in 610:** whether a shortcut to a missing directory
fails when declared or when used, and whether the blank line between block and
stations is required or merely usual.

--------------------------------------------------------------------------------

### User Request 10

<task-notification>
<task-id>bk3a4qb6n</task-id>
<tool-use-id>toolu_01SoArQJeL8shTxdUoutJY23</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/bk3a4qb6n.output</output-file>
<status>completed</status>
<summary>Background command "Full suite after the sweep" completed (exit code
0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 10

The longer addresses overflowed a fixed buffer in a test. Let me find every one
of those rather than just this one.

A real symbol mismatch, and the message names it exactly. The absolute path my
tool spliced in is wrong for maps the *engine* compiles — it relocates them.

This test checked the three address forms. There's one form now, so it needs
rewriting rather than patching.

Recording the consequence this turned up — it's not obvious and it will bite
someone.

Your alias design works; I'm finishing the migration fallout. One finding worth
your attention, because it changes how a description for a *running* program
must be written.

## A description handed to a running program is relative to somewhere it has never been

Everywhere else, "relative to the description" means relative to the file you
wrote. But when a program is handed a description at runtime, the engine
**copies it into a scratch directory, spills every box source it carries in
beside it, and compiles it there.** So its paths resolve against *that*
directory — a station line has to say `src/boxes/math.c:add`, matching the
layout the program's sources were filed under, not anything relative to where
the text came from.

The rule underneath is unchanged and consistent: *a description's paths are
relative to the description, and the engine moves the description to where its
sources are.* But it means text written by hand for a running program is written
against **that program's source layout**, which is a thing the program knows and
a person has to be told.

**An absolute path does not rescue it**, and this is the sharp edge. Naming the
real file on disk compiles against the real file — and the symbol that comes
out carries the absolute path rather than the short one the running program
published. The shared object then loads and fails to resolve, naming a symbol
two hundred characters long:

```
undefined symbol: cera_box__sl_mnt_sl_mtwo_sl_programming_sl_ai_dsh_playground_sl_
minimal_dsh_soramech_sl_src_sl_boxes_sl_029_dsh_demo_dsh_boxes_dot_c__seven__place
```

I found it by making exactly that mistake — my migration tool spliced absolute
paths into every test's map text, which is right for descriptions `serac`
compiles directly and wrong for descriptions the engine relocates. Both are now
in 610.

**Also: the dump stopped shortening.** It used to write a bare function name
whenever that resolved uniquely, so "a map written briefly stays brief". There's
no brief form now, so it writes the address whole — which is what makes a
dumped map reloadable at all.

The remaining work is mechanical: the tests split into the ones whose
descriptions relocate and the ones whose don't, and each group wants a different
form of address. Suite is running.

--------------------------------------------------------------------------------

### User Request 11

<task-notification>
<task-id>b711mg2ex</task-id>
<summary>Monitor event: "test suite outcome"</summary>
<event>053-test-inside 15
086-test-input-station 3
094-test-a-program-inside-another 3
082-test-one-construction-path 4
101-test-capture 26
SUITE-DONE</event>
If this event is something the user would act on now, send a PushNotification.
Routine or benign output doesn't need one.
</task-notification>

--------------------------------------------------------------------------------

### User Request 12

<task-notification>
<task-id>b711mg2ex</task-id>
<tool-use-id>toolu_01LmXduGrwCTUND8g2TvMsS2</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/b711mg2ex.output</output-file>
<status>completed</status>
<summary>Monitor "test suite outcome" stream ended</summary>
</task-notification>

--------------------------------------------------------------------------------

### User Request 13

<task-notification>
<task-id>b1ebcqz0g</task-id>
<tool-use-id>toolu_01CpY7qTNnvT5HqPVVE7pThJ</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/b1ebcqz0g.output</output-file>
<status>completed</status>
<summary>Background command "Use the relocation-relative form in every
relocating test" completed (exit code 0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 13

Recording the better option this exposed, before I put it to you.

**One test failing: `075-test-latebox`.** Everything before it passes. I've
stopped rather than pushed further, because what's left is a decision that's
yours.

## The problem, exactly

A generated symbol carries **the file the box was compiled from**:

```
host published:   cera_box_box_dsh_20009_dsh_3_dot_c__triple_und_it__place
description asks: cera_box_triple_und_it_dot_c__triple_und_it__place
```

A late box is compiled from a serial-numbered scratch file, so that's the name
baked into its symbol. But that file belongs to a process that has ended — so
a dump can't name it, which is why the dump used to shorten to a bare function
name instead. Bare names are gone. **Whatever a late box's file is called, the
compile and the description have to agree about it**, and none of the three
options I wrote into 609 quite delivers that.

## A fourth option, which building the other three exposed

**Name the file after its own contents** — a digest of the source text,
`late-a91f3c2e.c`:

- The address is right from the start, because the file it names *is* the file
  that was compiled. No rewriting, no special case in the dump, no extra rule in
  box matching — all three of which I wrote and all three of which are
  scaffolding around the real problem.
- **One source is one file**, however many boxes it defines. The copy-per-box
  that exists today — the thing that made the compiler read the same structs
  twice and refuse them — stops being needed at all.
- The same source compiled twice is the same file. Nothing accumulates.
- A fresh process reading a dump finds it, because that's where the previous
  process put it.

**What it costs:** somebody reading a dump sees `late-a91f3c2e.c:triple_it`
where they used to see `triple_it`. That's a real loss of legibility and it's
the only argument against.

All four options are written into 609 with this reasoning. My recommendation is
the fourth — it's the one that removes the problem instead of arranging around
it, and it retires a mechanism rather than adding one.

## Where things stand

| | |
|---|---|
| zoom widget | fixed |
| 609 — every station line names its file | built; one loose end, above |
| 610 — paths relative to the map, plus shortcuts | built, both kinds working, your example verbatim |
| `--results`, `--main=` | dropped |
| sources from the description | working — `serac turn.map` with no C files named |
| migration | 96 of 98 addresses by tool, the rest by hand |
| suite | **red on one test**, waiting on the decision above |
| commits | three landed; everything since is uncommitted |

Two smaller things also still open, both in 610: whether a shortcut to a missing
directory fails when declared or when used, and whether the blank line between
the block and the stations is required or merely usual.

--------------------------------------------------------------------------------

### User Request 14

> - A trailing slash makes a shortcut a directory. Written, not probed —
> asking the filesystem would make a map read differently depending on what's on
> disk that day.

I think the slash at the end should be optional - some users use it, some don't.
If we're pointed at a directory, we should treat it as a directory. and
"treating it like a directory" just means "search below here" - if we have a
file, and we append slashes after it, it doesn't do anything. libs/math.c/add
doesn't make sense. libs/math.c:add is the correct syntax. And, if we're making
a shortcut for that, the difference between this:

math = /home/ritz/soramech/libs/

and

math = /home/ritz/soramech/libs

doesn't seem that important to me. In both cases, they'd resolve like this:

math/math.c:add

and

math/math.c:add

the first example would add a double slash, which we should account for, like
Bash:

math//math.c:add

and

math/math.c:add

should be equivalent.

> A description handed to a running program is relative to somewhere it has
> never been

what do you mean by "a description"?

> A late box is compiled from a serial-numbered scratch file, so that's the name
> baked into its symbol. But that file belongs to a process that has ended —
> so a dump can't name it, which is why the dump used to shorten to a bare
> function name instead. Bare names are gone. Whatever a
> late box's file is called, the compile and the description have to agree about
> it, and none of the three options I wrote into 609 quite delivers that.

the dump must write the .c files that will be built into the program when it is
resumed later. Put them in a new directory right next to the dump. Make sure the
dump'd map file correctly points at them.

--------------------------------------------------------------------------------

### Assistant Response 14

Three things. The slash one first — your rule is simpler than mine and removes
a concept.

Now the slash collapsing, so `math//math.c` and `math/math.c` are one path.

**All three give 8.** `libs/`, `libs`, and a doubled slash in the address itself
— equivalent, like bash. And `is_dir` is gone entirely: a shortcut is now just
a piece of path that stands in for the first segment of an address, which is one
rule instead of two kinds.

## "A description" — I've been using a word the project doesn't define

**A description is a map file.** The text that says which box feeds which —
stations, ports, arrows. I've been using it because the docs do (`docs/009`
calls loading "from a description to a running program"), but I never defined it
and it's doing real work in sentences where the specific word matters.

Your vocabulary, as you gave it:

| term | what it is |
|---|---|
| **serac** | the compiler |
| **the ceramic core engine**, **cera** | the engine itself |
| **a soramech** | a compiled map file; also a boxmapgraph |
| a **box** | one C function |
| a **station** | one placement of a box in a map |

So "description" = the map file's content, the thing before it's compiled. Where
I said *"a description handed to a running program is relative to somewhere it
has never been"*, in your words that's: **you hand a running program some map
text; the engine copies that text into a scratch directory, writes the box
sources beside it, and compiles it there — so the paths in that text resolve
against the scratch directory, not against wherever you wrote it.**

I'll say "map file" or "map text" from here rather than importing a word that
needs a footnote. If you'd rather I keep "description" as the project's term for
map-text-before-it's-compiled, say so and I'll define it in the docs instead.

## The dump writing its own sources

Your instruction is the right shape and it retires the content-digest idea I was
about to recommend. What it means concretely:

```
grown.map
grown.map.sources/
    src/boxes/029-demo-boxes.c      the boxes the program was built with
    late/triple_it.c                the ones that arrived while it ran
```

…with every station line in `grown.map` pointing into that directory, relative
to the dump. A dump becomes self-contained: hand somebody those two things and
they can rebuild the program.

That's a bigger change than the slash fix — it moves where the engine spills
sources, changes what the dump writes, and has to keep generated symbols
agreeing with what the running program published. **I've not started it.** I
want to flag one thing before I do, because it decides the shape:

**The symbol a box compiles to carries the path it was compiled from.** For a
box the program was *built* with, the running program has already published a
symbol from its own build layout — so a resumed map has to address that box by
a path that reproduces that same symbol, not by wherever the dump put a copy.
Late boxes have no such constraint, because nothing published them yet.

So either the dump's source directory has to mirror the program's build layout
exactly (so both produce the same symbol), or the two kinds of box get addressed
differently in the same file. The first is cleaner and I think it's what you
mean — the directory beside the dump *is* the program's source tree,
reconstructed. Confirm and I'll build it that way.

--------------------------------------------------------------------------------

### User Request 15

> A description is a map file. The text that says which box feeds which —
> stations, ports, arrows. I've been using it because the docs do (docs/009
> calls loading "from a description to a running program"), but I never defined
> it and it's doing real work in sentences where the
>   specific word matters.

we should fix that in the docs. Thanks for catching it.

> The symbol a box compiles to carries the path it was compiled from. For a box
> the program was built with, the running program has already published a symbol
> from its own build layout — so a resumed map has to address that box by a
> path that reproduces that same symbol, not by
> wherever the dump put a copy. Late boxes have no such constraint, because
> nothing published them yet.
>
> So either the dump's source directory has to mirror the program's build layout
> exactly (so both produce the same symbol), or the two kinds of box get
> addressed differently in the same file. The first is cleaner and I think it's
> what you mean — the directory beside the dump is the
>   program's source tree, reconstructed. Confirm and I'll build it that way.


um, we don't know where the dump is going to be ran from when it's resumed. So
we shouldn't rely on the original paths. We should write new paths, that point
at the dumps functions/ directory. The "late" compiled and pre-compiled
functions should be treated the same - they should look the same once the dump
is over. If a box is unneeded because it was removed from the program, it should
not be included in the dump. The only thing we should need to run a dump is the
serac compiler.

--------------------------------------------------------------------------------

### Assistant Response 15

Understood, and point four resolves the symbol question entirely: **if serac is
all you need, the dump is never loaded into a running program — it's compiled
into a fresh one.** There are no host symbols to match. Let me do the docs fix
you asked for first.

Now the dump. Writing the blueprint first — this changes what a dump *is*.

Good — one lookup covers both kinds. Now the dump.

Now the piece that writes the sources out and decides their names.

Now point the late-box test at it and see whether a dump really builds with
nothing but serac.

The failure is upstream of the dump: hand-written map text can't name a late
box, because only the program knows the address it published. Under 611 that's
the dump's job — so the test should prove it that way.

`cera_map_add_station` reuses any station whose box slot is empty — so two
adds in a row return the same index. Add-then-place is the contract.

Two bugs, both mine: I pass the relative directory name to the writer, which
needs the full path. Only the *address* should be relative.

The dump is now correct. Let me check 611's actual claim — that serac alone
can build it.

**611's claim holds: a dump plus serac is a program.** `12` — that's `4`
tripled by a box compiled at runtime, passed through a box the binary was built
with, in a directory that never ran the original program.

```
grown.map
grown.functions/
    029-demo-boxes.c     the box it was built with
    box-30065-0.c        the box that arrived while it ran
```

Both written out identically, exactly as you asked. Only the sources its
stations actually place — the program held several other boxes and none of
them are there.

**One thing left, and it's a real simplification rather than a patch.** The
engine's own reload path (`cera_map_load_file`) still *relocates* map text into
a scratch directory before compiling it — which is why the dump's relative
paths break when a running program reads its own dump back. That relocation
existed to give the spilled box sources somewhere to live. **A map that names
its own sources needs no spilling at all**, so the relocation should go: compile
the map where it sits. That's the last red test.

I've stopped there rather than start it at the end of a long stretch. Everything
else stands:

| | |
|---|---|
| slash optional, doubled slashes collapse | done — `libs/`, `libs`, `math//math.c` all give 8 |
| `is_dir` gone entirely | a shortcut is just a piece of path standing in for the first segment |
| vocabulary defined in the docs | seven words, including *description* = map file |
| dump writes its own `functions/` directory | done, verified by serac alone |
| suite | red on one test — the reload path above |

Three things I decided along the way that you can overrule:

- **The directory is named after the map** — `grown.functions/` beside
  `grown.map`, not a bare `functions/`, so two dumps can share a directory
  without one eating the other's code.
- **A source is written out per *file*, not per box.** A box's source may need a
  struct or helper sitting beside it, so a file cut down to one function may not
  compile.
- **A box whose source the program doesn't hold is a loud warning during the
  dump**, naming the file — because the dump being written is about to be one
  nobody can build.

Both open questions from 610 are still open, plus two new ones in 611: whether a
dump should carry boxes a growing program *might* place but currently doesn't,
and the directory naming above.

--------------------------------------------------------------------------------

### User Request 16

please continue

> - The directory is named after the map — grown.functions/ beside grown.map,
> not a bare functions/, so two dumps can share a directory without one eating
> the other's code.

sounds good

> - A source is written out per file, not per box. A box's source may need a
> struct or helper sitting beside it, so a file cut down to one function may not
> compile.

yes, and also boxes might share functions, structs, and helpers, so we want one
file that multiple boxes might use.

> - A box whose source the program doesn't hold is a loud warning during the
> dump, naming the file — because the dump being written is about to be one
> nobody can build.

Can you tell me when this would be the case?

  - whether a dump should carry boxes a growing program might place but
    currently doesn't,

um, no? When do we even include boxes a growing program might place but
currently doesn't? Where are they stored? How would that work?

--------------------------------------------------------------------------------

### Assistant Response 16

Both questions are good ones. Let me answer, then continue.

Both good questions — answers below, after this. Continuing with the reload
path, which I now think collapses rather than needing patching.

Now the engine: loading a map *file* compiles it where it sits, whole.

The function never got inserted — my guard skipped it. Placing it with the
other late-compile code, where its helpers live.

**A dump of a grown program reloads in a fresh process.** That was the red test.
Full suite now.

Your two questions, while that runs.

## "When would a box's source not be held?"

**I can't construct a case, and I think that means the warning shouldn't be a
warning.**

Every box that gets placed by name arrives through a generated placement
function, and the same emission that writes that function also writes the box's
source out as text. Three routes in, all covered:

- **Compiled into the binary** — the build emits every box source as a C
  string array alongside the code.
- **Compiled at runtime from source text** — the emission carries the source
  the same way, and it goes into the late block.
- **Arrived in a shared object** — same emission, same text.

The only station with no source is one placed by hand through the low-level call
with no name given, and that one has no `box_name` either — the dump writes
`?placed-by-hand?` and never reaches the lookup.

So the branch is in the same category as the `const char *` result printer I
deleted earlier: **code asserting a possibility the design doesn't have.** By
the project's own convention it should either be an `abort` — the engine
finding a fault in itself, where a core is the evidence — or go away. I'd make
it an abort, since "a station has a name but no source" would mean the emitter
and the source table have come apart, which is worth a core.

Say which and I'll change it.

## "Boxes a growing program might place but doesn't — how would that even work?"

**You're right, and the question was badly posed.** Here's the thing I was
actually gesturing at:

A program carries the source text of *every* box it was built with — all of
them, whether any station places them or not. That's what makes
`cera_late_compile_map` work: a description arrives naming a box nothing has
placed yet, and the program can write that source out and compile it. So the
boxes are stored, in the binary, as text.

My question was whether a dump should write out all of that carried text, or
only the sources its stations actually place. It writes only what's placed —
**your answer, and it was already the behaviour.** I'll close the question
rather than leave it sitting there looking undecided.

The reason it crossed my mind: a dumped program resumed by serac can only ever
place the boxes in its `functions/` directory, so it has lost the ability to
grow into the others. But that's correct — a dump says what a program **is**,
and "what it could have become" isn't a property of the program, it's a property
of the binary that wrote it.

## And the reload path collapsed rather than needing a fix

The thing that made dumps unreadable was that loading a map file **relocated**
it into a scratch directory before compiling, breaking its relative paths. That
relocation existed to give spilled box sources somewhere to live — and **a map
that names its own sources needs no spilling at all.**

So the two ways a description enters a program are now split by one fact —
whether it has a home:

| | text handed to a running program | a map file |
|---|---|---|
| has a home | no | yes |
| what happens | copied to scratch, sources spilled beside it | compiled where it sits |
| what's compiled | build functions only, bound to the host's boxes | the whole program, every box from the files the map names |
| can fail to bind | yes | **nothing binds, so no** |

That last row is the one that matters. It's why a dump builds with serac and
nothing else, and it retires the entire question of whether two paths produce
the same symbol. The cost is a second copy of any box the loading program
already had — which is free, because a box remembers nothing between calls, so
two copies are indistinguishable.

--------------------------------------------------------------------------------

### User Request 17

<task-notification>
<task-id>beo9s4d8l</task-id>
<tool-use-id>toolu_014ogVFsnbamdukA4gfan11B</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/beo9s4d8l.output</output-file>
<status>completed</status>
<summary>Background command "Full suite" completed (exit code 0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 17

Relocation is gone, so absolute paths work again — and are now correct,
because nothing binds to host symbols. Reverting those tests.

One left — the misspelling test's expected message, now that the address is
absolute.

Both your questions answered, and the design point you pushed on turned out to
be the load-bearing one.

## The box-source warning: unreachable, so it's now a fault

I couldn't construct a case. Every box placed by name arrives through a
generated placement function, and **the same emission that writes that function
writes the box's source out beside it** — whether it was compiled into the
binary, compiled from text at runtime, or arrived in a shared object. A station
placed by hand with no name has no address either, so it never reaches the
lookup.

So a station with an address whose source isn't held would mean the emitter and
the source table have come apart. That's the engine finding a fault in itself,
and the project's convention for that is `cera_bug` — abort, leave a core,
because the core is the evidence. Changed.

Same category as the `const char *` result branch I deleted: **code asserting a
possibility the design doesn't have.**

## "Boxes it might place but doesn't" — badly posed, and you closed it

They're stored as text in the binary, all of them, which is what lets a
description arrive naming a box nothing has placed yet. My question was whether
a dump should write all that out or only what's placed. It writes only what's
placed — your answer, already the behaviour. Closed in the issue.

The reason it crossed my mind is worth keeping: a dump resumed by serac can only
place the boxes in its `functions/` directory, so it has lost the ability to
grow into the others. But that's correct. **A dump says what a program is; what
it could have become is a property of the binary that wrote it.**

## And the reload path collapsed

This is the part worth your attention. Loading a map file used to **copy it into
a scratch directory** and spill the program's own box sources beside it —
which is exactly why a dump's relative paths broke when anything read one back.
That copying existed to give the spilled sources somewhere to live, and a map
that names its own sources needs no spilling.

So the two ways a description enters a program now split on one fact — whether
it has a home:

| | text handed to a running program | a map file |
|---|---|---|
| has a home | no | yes |
| what happens | copied to scratch, sources spilled beside it | compiled where it sits |
| what's compiled | build functions only, bound to the host's boxes | **the whole program**, every box from the files the map names |
| can fail to bind | yes | nothing binds, so no |

That last row is why a dump builds with serac and nothing else — and it
**retires the symbol-matching question entirely**, after it had already cost
three attempts at an answer. The cost is a second copy of any box the loading
program already had, which is free: a box remembers nothing, so two copies are
indistinguishable.

Also worth noting: `--shared` and `--external-boxes` are now separate flags.
They were one, and inferring "bind to a host" from "make a shared object" is
what made a self-contained map impossible to compile.

Suite is running; I'll report where it lands.

--------------------------------------------------------------------------------

### User Request 18

> So the branch is in the same category as the const char * result printer I
> deleted earlier: code asserting a possibility the design doesn't have. By the
> project's own convention it should either be an abort — the engine finding a
> fault in itself, where a core is the evidence — or
> go away. I'd make it an abort, since "a station has a name but no source"
> would mean the emitter and the source table have come apart, which is worth a
> core.

sounds good.

> A program carries the source text of every box it was built with — all of
> them, whether any station places them or not. That's what makes
> cera_late_compile_map work: a description arrives naming a box nothing has
> placed yet, and the program can write that source out and compile it.
>   So the boxes are stored, in the binary, as text.

How can we have a box without a station to place it? I'm confused. How do we
build that into the program? The map file is the source of truth for boxes and
what they do and how they're wired, so where in the map file would an unplaced
box go? Doesn't defining it in the map file define it as a station?

--------------------------------------------------------------------------------

### Assistant Response 18

Your confusion is well-founded — the design has a seam there, and it's the
same seam as the 47% measurement from earlier.

## How a box exists without a station

**Two different things decide the two different facts:**

| what | what decides it |
|---|---|
| which **boxes** exist | every non-`static` function in every `.c` file the build is given |
| which **stations** exist | the map file |

A box source is a file, and *every* function in it becomes a box — shim,
placement function, field tables, source text carried. The map places some of
them. Nothing places the rest, and they're in the binary anyway.

Concretely, in this project right now:

```
functions in one box source:   18
distinct boxes the maps place:  5
```

So thirteen boxes in that one file exist with no station. Not by design — as a
**consequence** of the unit being the file. You can't say "compile this function
and not that one" because a box may need a struct, a helper, or an include
sitting beside it in the same file; that's the same reason a dump writes whole
files.

**So "the map file is the source of truth" is true for wiring and false for
existence.** The map decides what runs and what feeds what. The `.c` files
decide what exists.

## Where those unplaced boxes actually get used

Two places, both real:

1. **`cera_map_place_box(m, station, "name", kind)`** — building a program
   through the API instead of a map file. The test I rewrote earlier does
   exactly this.
2. **A map arriving at runtime** naming a box no map placed at build time.

## And this is the 47% finding, in different clothes

Remember the measurement: fifty unused functions in a box source cost **30 KB of
text**, and sealing the binary took it from 122,134 bytes to 65,284. Those fifty
unused functions are precisely these — boxes with no station.

They survive because every box gets an **exported** placement function, and an
exported symbol is a collection root the linker may never discard. That export
exists so a map compiled next year can bind to a box compiled today.

So the chain is: *unit is the file* → unplaced boxes exist → they're
exported so late code can find them → the linker can't discard them → half
the binary.

**And the load path change we just made undercuts the reason.** A map file is
now compiled *whole* — it defines every box it names rather than binding to
the host's. Nothing binds, so nothing needs to have been exported. The export
list now only serves **text handed to a running program**, which is the narrower
case.

Which means "seal it by default" — the thing you said earlier, that a compiled
program should be assumed finished — just got cheaper to do than when I
measured it. A sealed program can still load a map *file*, because that path
binds to nothing. It only loses the ability to accept bare map *text*.

I'd like to make that the default, with the export kept only when asked for.
Worth its own issue, and I haven't started it.

--------------------------------------------------------------------------------

### User Request 19

<task-notification>
<task-id>btporojnj</task-id>
<tool-use-id>toolu_012iB7aMvGWakMkcsKbNJLyw</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/btporojnj.output</output-file>
<status>completed</status>
<summary>Background command "Run the suite to completion" completed (exit code
0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 19

One test left, and it's asserting something that's no longer true. Let me see
exactly what it claims.

The test's claim is about structure; the box address is provenance. Let me keep
the byte-for-byte check on everything that is structure.

Two tests need the same rule — so it goes in one place, not two.

That's just my missing `-D`; the Makefile supplies it. Now 096.

Text has no home, so a shortcut written for the original directory can't survive
relocation. Let me see what the test hands over.

The test slurps a map file and hands over the *text* — losing the file's home,
so its shortcut points nowhere. Making the text say where to look absolutely.

Wrong form — for *text*, the address must match the program's own source
layout, which is exactly what I documented in 610. Spill-relative, not absolute.

The distinction you pushed on turned out to be the thing that keeps biting, so
I've written it down where it will be read.

**Text and a file are genuinely different, and the difference is one fact:**

| | text handed to a running program | a map file |
|---|---|---|
| has a home | no | yes |
| paths are relative to | the scratch directory the engine picks | itself |
| so addresses must be written | against **the program's own source layout** | however you like |
| what gets compiled | build functions only, bound to the host's boxes | the whole program |
| absolute paths | **wrong** — they name the real file, produce a different symbol, and fail to load | fine |

That last row is the trap, and I fell into it twice in one session: absolute
paths look like the safe answer and are exactly wrong for text, because the
symbol a box compiles to carries the path it was compiled from, and it has to be
the path the running program already published.

A caller who doesn't want to think about any of this **hands over the file
instead** — it has a home and needs no help.

That's now in `docs/008`, `docs/009`, and issue 610, and both affected tests say
it in a comment where somebody editing them will see it.

Also: the dump-comparison rule that two tests needed is a shared header rather
than two copies. Its own documentation says why — this project has relearned
"a rule with two copies is two rules that agree until one is edited" from the
map format's two writers and from the generator's escaping, and writing it down
a third time was cheaper than learning it a third time.

Suite is running. Once it's green I'll commit — there's a lot uncommitted now
(609, 610, 611, the slash change, the vocabulary, the zoom fix), and it wants to
land in separate commits per concern rather than one pile.

--------------------------------------------------------------------------------

### User Request 20

<task-notification>
<task-id>b842qio1r</task-id>
<tool-use-id>toolu_011eKK6yZyrrRedYXNeWioQX</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/b842qio1r.output</output-file>
<status>completed</status>
<summary>Background command "Full suite" completed (exit code 0)</summary>
</task-notification>

--------------------------------------------------------------------------------

### User Request 21

<task-notification>
<task-id>bvc67dv6f</task-id>
<summary>Monitor event: "whole suite outcome"</summary>
<event>exit=2
== 096-test-a-map-becomes-code
== 101-test-capture
[exited with code 0]
SUITE-DONE</event>
If this event is something the user would act on now, send a PushNotification.
Routine or benign output doesn't need one.
</task-notification>

--------------------------------------------------------------------------------

### User Request 22

<task-notification>
<task-id>bvc67dv6f</task-id>
<tool-use-id>toolu_017mpCKxRMhTLUve8ScwHQCE</tool-use-id>
<output-file>/tmp/claude-1000/-mnt-mtwo-programming-ai-playground-minimal-soramech/117289dc-ce30-4938-b136-70f1042cab26/tasks/bvc67dv6f.output</output-file>
<status>completed</status>
<summary>Monitor "whole suite outcome" stream ended</summary>
</task-notification>

--------------------------------------------------------------------------------

### Assistant Response 22

That last fix is worth noting, because it's the design paying off rather than me
patching around it.

The capture test had a box that arrived at runtime and a map naming it
`(quadruple)` — a bare name, the old spelling. The fix wasn't a special case.
It was **writing the box's source into a file beside the description and naming
it like any other**:

```
station four (quadruple.c:quadruple)
```

No second spelling for a box with an unusual history. That's exactly what 611
says a dump does, arrived at independently by a test that needed the same thing.

Suite is running. While it finishes, here's the state of the uncommitted work:

| change | what it is |
|---|---|
| zoom widget | the control moved the root font size; every text size was nailed to pixels |
| **609** | every station line names its file; migration tool; one resolver instead of two |
| **610** | paths relative to the map, shortcuts, optional trailing slash, slashes collapse |
| **611** | a dump is a map file plus a `functions/` directory — serac is all you need to resume |
| vocabulary | seven words defined in the overview, *description* among them |
| `--results`, `--main=` | dropped |
| sources from the description | `serac turn.map`, no C files on the command line |

Plus one thing that fell out and I want to flag again, because it's larger than
it looks: **loading a map file no longer binds to anything.** It compiles the
whole program from the files the map names. That's what made dumps work, and it
means the export list — the thing costing 47% of a binary — now only serves
map *text* handed to a running program. Sealing by default got much cheaper than
when I measured it. Separate issue, not started.

Once green I'll commit in separate pieces: the zoom fix, 609, 610, 611, and the
vocabulary, each with the transcripts.

--------------------------------------------------------------------------------
