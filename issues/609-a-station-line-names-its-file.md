# 609 — A station line names its file

**A box address always says which file the function is in.** The bare
form goes away, and every station line carries a path and a function
name:

```
station doubler (twice)              refused
station doubler (shapes.c:twice)     the only form
```

## Current behavior

**Built.** The reader refuses an address with no file, the resolver has
one form, the writers emit it, a migration tool rewrote every
description, document and test, and the compiler takes its source list
from the description rather than from a command line. What a file name
*means* turned out to be a second question, and it is
[610](610-a-map-says-where-to-look.md).

## How it stood before

**Three forms, and two of them can be ambiguous.** A station line
addresses its box as a bare function name, a basename and a function, or
a path and a function. The last is exact; the first two are searched
for.

A bare name that matches one function in one file resolves. A bare name
that matches functions in two files is refused at build time, naming
both paths and asking the author to write one out in full — so the
ambiguity is caught rather than guessed at, but it is caught *late*, and
only when the collision happens to exist. A description written against
one set of sources and compiled against another can quietly mean a
different function, because nothing in the description said which it
meant.

**And the description does not say what it needs.** Every map in this
project is written in the bare form, so a description carries no record
of which C files it was written against. The compiler is told them on
the command line instead, and the two can disagree with no complaint as
long as the names happen to resolve.

**Every map file in the repository uses the bare form**, along with the
fenced examples in the documents and the map text inside tests. They all
change.

## Intended behavior

**One form, and it is the exact one.** A station line's box address is
`path:function`, where the path is matched as a suffix beginning at a
slash — which is what lets `shapes.c:twice` reach
`src/boxes/shapes.c:twice` while `apes.c` cannot reach `shapes.c`. That
matching rule already exists and is unchanged; what changes is that the
part before the colon stops being optional.

**A bare name is refused, naming what replaced it**, the way the old
station-line spelling was when
[608](completed/608-the-station-line-reads-at-a-glance.md) changed it:

```
map.map:1: 'twice' does not say which file it is in — write it as
           'shapes.c:twice'
```

**What this buys, beyond removing the ambiguity.** A description then
contains its own source list. Somebody reading a map can see what it is
built from without being told, and the compiler could take the sources
from the description rather than from its command line. Whether it
*should* is left open below — this issue is about what a station line
says, not about what the compiler does with it.

### The alternatives, and why not

**Keep the bare form and lean on the collision refusal.** It already
catches the case where two files define the same function name, so
nothing is silently wrong today. Rejected because it only catches a
collision that happens to exist in the sources named on one command
line — a description compiled against a different set of sources next
year resolves differently with no complaint, and the description never
said what it meant.

**Let the compiler discover sources by globbing the description's
directory.** Considered as the way to stop typing filenames twice.
Rejected: a glob sweeps up whatever is in the directory, which for this
project would include a hand-written `main` and anything `--emit-c`
wrote. Making the description say what it uses is the version of the
same idea with nothing inferred.

**Keep both forms and prefer the exact one.** Rejected on the grounds
this issue exists for: two spellings of one thing is the ambiguity, not
the resolution of it.

## Suggested implementation steps

1. **The reader refuses a bare name**, naming the line and saying what
   the form is. The three-form resolution in the emitter loses its bare
   branch at the same time, so there is one rule rather than a reader
   and a resolver that could disagree.
2. **The writers emit the long form.** The format has two writers — the
   generator's map writer and the engine's own dump — and phase 6
   recorded that they carry separate copies of this format with nothing
   deriving one from the other. Both change, and the round-trip test is
   what catches it if only one does.
3. **A migration tool**, the way
   [608](completed/608-the-station-line-reads-at-a-glance.md) had one:
   map files, fenced examples in documents, and map text inside C string
   literals in tests, rewritten as one pass. It needs the sources to
   resolve each bare name against, which is the same resolution the
   emitter does today — so the tool can borrow it rather than guess.
4. **Every map, document and test**, through the tool.
5. **Tests.** A bare name is refused naming the replacement. Both
   writers emit the long form. A dump reloads and dumps to the same
   text. A path that is a suffix at a slash resolves and one that is a
   suffix mid-word does not.

## Related documents and tools

- [008 — Map file format](../docs/008-map-file-format.md), which
  describes the three forms
- [608 — The station line reads at a glance](completed/608-the-station-line-reads-at-a-glance.md),
  the same shape of change to the same line, with the migration tool and
  the two-writers lesson it turned up
- [311a](completed/311a-boxes-addressed-by-file.md), where the path form
  and its suffix-matching rule came from
- [910 — The engine ships as a compiler](910-the-engine-ships-as-a-compiler.md),
  whose compiler is the thing that would stop needing sources on its
  command line

## Open questions

**Answered: the compiler takes its sources from the description.** Every
station line names its file, so the command line stops carrying the same
fact a second time. Each file the description names is checked to exist
before anything is parsed, and a description naming a file that is not
there is refused saying which — the worry that a description could reach
for something absent is answered by looking rather than by not reaching.

**Still open, and it is the one thing this blueprint did not see
coming: what address does a box compiled at run time carry?**

A box compiled while a program runs is written to a serial-numbered file
in a scratch directory belonging to *that* process — `box-4127-3.c` —
and its address is built from that path. The dump therefore shortens
such an address back to the bare function name on the way out, on
purpose, because a later process reading the dump would otherwise be
told to look in a file that no longer exists.

That shortening is now writing a form the reader refuses. So the dump of
a grown program cannot be read back, and the choice is:

- **Give a late box the address its source is actually filed under.**
  Every late box already has its source copied to `<name>.c` in the
  scratch directory precisely so a later process can find it, so
  `<name>.c:<name>` is both a legal address and the true one. What it
  costs is that box matching has to recognise it, since the row's
  recorded address is still the serial path.
- **Let the dump keep writing a bare name for a late box**, as a
  documented exception. Cheapest, and it reintroduces the second form
  this issue exists to remove.
- **Have the late-compile path file the source under the box's name
  before compiling it**, so the address is right from the start. Cleanest
  reading, but one source may define several boxes and then there is no
  single name to file it under.
- **Name the file after the source itself** — a digest of its text, so
  `late-a91f3c2e.c`. This is the option the other three were groping
  toward, and it came out of building them:

  - The address is right from the start, because the file it names is
    the file that was compiled. No rewriting, no special case in the
    dump, no extra rule in box matching.
  - **One source is one file**, however many boxes it defines. The
    copy-per-box that exists today — and that made the compiler read the
    same structs several times and refuse them — stops being needed at
    all.
  - The same source compiled twice is the same file, so nothing
    accumulates.
  - A fresh process reading a dump finds `late-a91f3c2e.c` because that
    is where the previous process put it.

  What it costs is that a person reading a dump sees a digest where they
  used to see a name. That is a real loss and it is the only argument
  against.

**Why this is now blocking rather than theoretical.** The published
symbol carries the file the box was compiled from, and a description
naming a different file compiles to a different symbol. So the dump and
the binary disagree, and the shared object loads and fails to resolve:

```
undefined symbol: cera_box_triple_und_it_dot_c__triple_und_it__place
```

The host published the serial-file symbol; the description asked for the
filed-under-its-name one. **Whatever a late box's file is called, the
compile and the description have to agree about it**, which is the thing
none of the first three options quite delivered.

Nothing here is decided, and the first looks right without being
obviously right.

**Does a station line naming a file it shares with nothing gain
anything?** A project with one box source writes that filename on every
line. That is repetition without ambiguity to remove, and somebody will
notice.
