# 610 — A map says where to look

**A description resolves its own file paths, and may name shortcuts for
the places it looks.** Every path in a map is relative to the map file
itself, and a block at the top gives short names to directories and
files so a line does not have to spell out a long path every time.

```
math   = /home/ritz/soramech/libs/math/
curves = libs/curves.c

station add  (math/arithmetic.c:add)
station turn (curves:rotate)
```

`math` names a directory, so what follows the slash is a file inside it.
`curves` names a file, so what follows the colon is a function in it.
Both are the same mechanism: a short name standing for a path the author
wrote once.

## Current behavior

**Built.** Shortcuts parse, both kinds resolve, one function answers the
question for both callers, the writer emits the block, and this
project's own descriptions use one. What remains are the two questions
at the end, which have not been worked through.

## How it stood before

**Every station line names its file** as of
[609](609-a-station-line-names-its-file.md), and the compiler takes its
source list from the description rather than from a command line. What
609 did not settle is what a file name in a description *means*, and two
answers are in the tree at once:

- **The compiler** joins the name to the description's own directory and
  opens exactly that. Nothing is searched.
- **The generator's resolver** treats the name as a suffix of a box's
  path, matched at a slash — so `029-demo-boxes.c` finds
  `src/boxes/029-demo-boxes.c`.

A description can therefore resolve for one and not the other, and that
is what it does today: this project keeps descriptions in `maps/` and
box sources in `src/boxes/`, so every map in it resolves for the
generator and fails for the compiler, naming a file that is not beside
it.

**And a path relative to the description is unbearable without
shortcuts.** Written out, the maps here would each carry
`../src/boxes/029-demo-boxes.c` on every station line — a long path,
repeated, saying the same thing every time, and wrong the moment
anything moves.

## Intended behavior

### Paths are relative to the description

**The description's own directory is what every path in it is relative
to**, and nothing is searched for. A name in a map is where the file is,
not a hint about where it might be. The generator's suffix matching goes
with the same change that makes the compiler's rule the only rule.

An absolute path is still an absolute path, which is what makes a
shortcut to somewhere outside the project possible at all.

### A block of shortcuts at the top

**Before any station, a line of the form `name = path` gives that path a
short name.** The path is resolved like any other — relative to the
description, or absolute.

A shortcut may name either kind of thing, and which it is decides how it
is used:

| in the block | in a station line | what it means |
|---|---|---|
| `math = libs/math/` | `math/arithmetic.c:add` | a **directory**; the rest of the path goes inside it |
| `curves = libs/curves.c` | `curves:rotate` | a **file**; the function follows the colon directly |

The distinction is written rather than inferred: a shortcut whose path
ends in a slash is a directory, and one that does not is a file. Working
it out from the filesystem would mean a description that reads
differently depending on what happens to be on disk.

**Two descriptions in different places resolve differently, and that is
the point.** Each resolves its own paths against its own directory and
its own shortcuts, so a description can be moved with the sources it
names and keep working, and two descriptions in one build need not agree
about what `math` means.

### What a shortcut is not

**Not a search path.** A shortcut is one place, named. Nothing tries
several and takes the first that answers, because that is the behaviour
609 removed from box addressing and it would be no better here.

**Not inherited.** A description that brings another description inside
it does not lend it shortcuts. Each says where to look for itself, and a
description whose meaning depended on who loaded it would not be a
description.

**Not usable before it is declared.** The block comes before the
stations, which is also the order somebody reads it in.

### The alternatives, and why not

**Let the compiler search the description's directory tree**, so a bare
filename finds a file wherever it sits underneath. Rejected for being
the thing 609 exists to remove, one level up: a name that resolves by
looking around is a name that can resolve differently tomorrow.

**Put the shortcuts in a separate file beside the description.** It
would let several descriptions share one set. Rejected because a
description that cannot be read without a second file is not the
self-contained thing the rest of this design leans on — the directory
holding a description, its sources and the compiler is meant to be
something somebody hands to somebody else.

**Take the paths from the command line after all.** Rejected already,
and this issue exists because of it: the command line is a second place
for a fact the description already states.

## Suggested implementation steps

1. **The reader learns the block.** A `name = path` line before any
   station, stored on the description. A name used twice, a name that is
   also a station keyword, and a line after the first station are each
   refused naming the line.
2. **One resolver, asked by both callers.** A function that turns a
   box address into the path of the file holding it, given the
   description. The compiler calls it to find sources; the generator
   calls it instead of matching suffixes. One rule, in one place — the
   two-writers lesson from phase 6, applied before it can bite.
3. **The generator's suffix matching goes.**
4. **The writer emits the block**, and the round-trip test covers a
   description that has one.
5. **This project's own maps gain a shortcut** for `../src/boxes`, which
   is the migration and also the first real use.
6. **Tests.** A directory shortcut and a file shortcut each resolve. A
   path with no shortcut resolves against the description's directory.
   An absolute shortcut reaches outside the tree. Two descriptions in
   two directories resolve the same relative path to two different
   files. A shortcut declared twice, used before declaration, or naming
   a file that is not there is refused, naming the line.

## Related documents and tools

- [609 — A station line names its file](609-a-station-line-names-its-file.md),
  which made every address name a file and left what a name means open
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  the block
- [910 — The engine ships as a compiler](910-the-engine-ships-as-a-compiler.md),
  whose compiler resolves the paths

## What building it turned up

**A description handed to a running program is relative to somewhere it
has never been.** Everywhere else, "relative to the description" means
relative to the file somebody wrote. But a description handed to a
running program does not stay where it was written: the engine copies
it into a scratch directory, spills every box source it carries in
beside it, and compiles it there. So its paths are resolved against
*that* directory, and a station line has to name the box by the path the
program's own sources were filed under — `src/boxes/math.c:add` — rather
than by anything relative to where the text came from.

That is coherent and it is not obvious. The rule underneath is the same
one everywhere: **a description's paths are relative to the description,
and the engine moves the description to where its sources are.** What it
means in practice is that a description written for a running program is
written against that program's source layout, which is a thing the
program can say and a person writing the text by hand has to know.

**An absolute path does not rescue it.** Naming the real file on disk
compiles against the real file, and the symbol that comes out carries
the absolute path rather than the short one the running program
published — so the shared object loads and fails to resolve, naming a
symbol two hundred characters long. Found exactly that way.

**And the dump stopped shortening.** It used to write a bare function
name whenever that resolved uniquely, so a map written briefly stayed
brief. There is no brief form any more, so it writes the address whole.

## Open questions

**Does a shortcut to a directory that does not exist fail at read time
or at use?** Refusing when it is declared is the earlier message and
catches a typo in a shortcut nothing happens to use. Refusing when it is
used names the station line, which is what somebody is actually looking
at. Both are defensible and this has not been decided.

**What separates the block from the stations?** A blank line is what the
example above uses and what a person writes anyway. Requiring one is a
rule; not requiring one means the first station line is what ends the
block. Undecided.
