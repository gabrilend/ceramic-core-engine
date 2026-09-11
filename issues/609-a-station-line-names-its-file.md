# 609 — A station line names its file

**A box address always says which file the function is in.** The bare
form goes away, and every station line carries a path and a function
name:

```
station doubler (twice)              refused
station doubler (shapes.c:twice)     the only form
```

## Current behavior

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
- [311a](completed/311a-a-box-carries-its-path.md), where the path form
  and its suffix-matching rule came from
- [910 — The engine ships as a compiler](910-the-engine-ships-as-a-compiler.md),
  whose compiler is the thing that would stop needing sources on its
  command line

## Open questions

**Should the compiler then take its sources from the description?** It
could, once every station line names a file. The argument for: nobody
types the same filename twice, and the description is the single
statement of what the program is made of. The argument against: the
command line is where a person says what this particular build is made
of, and a description that reaches out and names files is a description
that can name files that are not there. Undecided, and deliberately not
part of this issue.

**Does a station line naming a file it shares with nothing gain
anything?** A project with one box source writes that filename on every
line. That is repetition without ambiguity to remove, and somebody will
notice.
