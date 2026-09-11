# Conversation Summary: 117289dc-ce30-4938-b136-70f1042cab26

Generated on: 2026-09-11 14:47:18

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
