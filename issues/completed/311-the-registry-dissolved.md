# 311 — The registry dissolved

**This is a parent issue, and it is done.** The table that told the
engine about every box is gone entirely. Not shrunk — gone. A running
program holds no record of what a box is; it holds stations with
function pointers in them, and the text that used to be resolved at run
time is resolved by the generator instead.

**One claim in the original draft did not survive, and the reason is
the most useful thing in this family.** It said a running program would
hold *no name and no lookup*. That turned out to be incompatible with a
requirement nobody had written down yet: **the engine is agnostic about
what runs inside it, so a program must be extendable while it runs.** A
program that can be handed a description next year has to let that
description bind to a box compiled today, which means publishing the
functions that build stations from those boxes — and a program that
publishes names holds names.

So names did not disappear. They moved into the operating system's own
symbol table, which every process already carries, which nothing here
has to write or maintain, and which is the lookup. What remains of this
project's own is 1.6 kilobytes mapping a short name to a full address —
the namespace [311a](311a-boxes-addressed-by-file.md)
established but nobody had written down. It holds no code alive. See
[311d](311d-the-map-becomes-code.md) step 11 for why it stays.

The change reaches the map file format, the generator, the build, and
the runtime-compilation path, so it divides into children rather than
being one ticket.

## Current behavior

The generator reads **whatever sits under the box source directory**,
parses every function declaration it finds, and emits one file holding
a shim per box, a record per box, a field table per struct, and a
three-way comparison per orderable type. That file is compiled in.

So the set of boxes a program can place is fixed at build time, a map
addresses one by a **bare name** that is trusted to hit the right row,
and a program using three boxes out of five hundred carries five
hundred shims.

**What a box record holds today**, and whether a parser could produce
it from source:

| field | from source? |
|---|---|
| name as written in a map | yes |
| parameter count, parameter type names, return type name | yes |
| **shim pointer** | no — compiled code |
| **each parameter's size, the return size** | no — `sizeof` |
| **task size**, the exact allocation for one invocation | no — derived from those |
| **compare function pointer** | no — compiled code |

**And a struct record**, which is the half people forget:

| field | from source? |
|---|---|
| struct name, field names, field kinds, array lengths, nested links | yes |
| **struct size, each field's size** | no — `sizeof` |
| **each field's offset** | no — `offsetof` |

Those field tables are what let a map file write `in 2 = { 1.5, 2.5,
3.5 }` into a struct port without a hand-written parser per type, and
what let the dump turn those bytes back into text.

## The one thing that cannot change

**A size cannot be computed at runtime.** `sizeof` is a compile-time
operator: the compiler evaluates it and burns a literal into the
machine code. A running program has no types at all — C erases every
bit of type information during compilation — so there is nothing left
for `sizeof` to be applied to. You cannot hand a running program the
text `vec3` and get 12 back.

The trick is visible in the generated file's first real line: it
`#include`s the box source **whole**, so the types become visible to
the compiler, and only then writes `sizeof a0`. That is why the numbers
are right, and it happens at build time.

So every size comes from exactly one of two places: a `sizeof`
expression compiled in, or a compiler invoked at runtime. There is no
third door.

**Everything else in the record was avoidable**, and the rest of this
family is the avoiding.

## The thing that turned out not to be irreducible

An earlier draft of this issue said a name has to be resolved at run
time, because a map arrives as text and text has to become a pointer.
That is only true **if a binary must interpret a map it was not built
for**, and it does not have to, because a map is not interpreted at
all.

**A map is a blueprint for the compilation.** The generator reads it
and emits the construction calls directly:

```c
void build_program(cera_map_t *m) {
    cera_map_place(m, 0, place__math_dot_c__add,   PLAIN);
    cera_map_place(m, 1, place__io_dot_c__print,   PLAIN);
    cera_map_wire (m, 0, /*out*/0, /*to*/1, /*port*/0);
    map_set_static(m, 0, /*port*/1, 5);
}
```

**No name survives into the running program.** The text
`math.c:add` was consumed by the generator and became a pointer. The
loader stops being a parser and becomes generated code, which does not
weaken *one way to build a program* — it calls the same construction
surface a person would.

**And editing a running program never needed names either.** Adding a
station means handing over a placement function; wiring means station
indices; writing a constant means bytes. The thing you edit while a
program runs is the in-RAM structure, not the file.

## Where the compiling happens, which is the only thing that varies

| case | when the map is compiled | toolchain at run time? |
|---|---|---|
| a shipped program | at build time | **no** — it ships as one file |
| a program that runs other programs | when it is handed a map | yes |
| the workbench | it emits a map; whoever runs it compiles it | yes, on that side |
| a box arriving mid-run | when the source arrives | yes |
| a capture, re-run | when the artifact is compiled | yes |

**One mechanism — generator, compiler, load — in every row**, and it
was built. No configuration, no declaration that switches behaviour,
and no second code path in the engine. That rigidity is the point: this
is the minimal implementation, and a thing that behaves one way is
worth more than a thing that behaves two ways well.

**What proves the row above is one mechanism**: a description read as
text, the same description compiled at build time, and the same one
compiled while the program ran are dumped and compared byte for byte —
and the two compiled forms are compared against *each other*, because
that is the comparison that catches them drifting while both still
match the text loosely.

**And the row about a box arriving mid-run is the same code as the row
about a map.** Compiling a description reuses the loader written for a
single function: write the source out, run the generator, run the
compiler that built this binary, open the result. Somebody adding a box
does not need to know whether it is a map of boxes.

## The four changes

| issue | what it does |
|---|---|
| [311a — Boxes addressed by file](311a-boxes-addressed-by-file.md) | a map names `file:function`; basenames resolve, paths settle ties, and generated symbols escape punctuation so no two files collide |
| [311b — Placement instead of records](311b-placement-instead-of-records.md) | the generator emits a placement function per box; the record is deleted, and the word that named it stops standing in for three different things |
| [311c — Source rides in the binary](311c-source-rides-in-the-binary.md) | each box source emitted as a C array, so the binary carries its own text |
| [311d — The map becomes code](311d-the-map-becomes-code.md) | the generator turns a description into construction calls, at build time and while running; the engine's own way of reading one is deleted, and the parser leaves with it |

**311a comes first** because the addressing decides how a box is named
and how its symbol is spelled, and everything else is written against
that. 311b and 311c are independent of each other. **311d needs both**,
since it emits calls to placement functions that 311b defines.

**311d additionally waits on
[212](212-one-way-to-build-a-program.md), and the first three do
not.** What 311d emits is construction calls, and emitting them
against an interface the project has decided to replace would make the
most-read generated file in the project an example of how *not* to
build a program, for however long the replacement took. The
alternatives were considered and written down in 311d itself. So this
family runs in parallel with the construction line for three of its
four children and joins it for the last.

**There is no fifth child for running a map you were not built for**,
and the reason is worth keeping: a draft of this family had one, and it
described a command-line tool wrapping the build. That tool was
unnecessary twice over. Compiling a map is what `make` already does, so
the tool was a shell script; and more importantly, **a program that
runs other programs is a map**, not a tool. It reads a description,
compiles what the description names, starts a fresh map, and feeds it
through its input station — using the construction surface as boxes,
which is what this engine is for. See
[212](212-one-way-to-build-a-program.md), where composing and starting
are separated.

## What this does not give up

**Guarantee [C1](../../docs/058-guarantees.md) survives untouched** —
every size and offset is still computed by the C compiler, never by the
generator, still emitted as `sizeof` and `offsetof` expressions. They
move from a table into a function; they do not become guesses.

**The map file still mentions no types.** Naming a file beside a
function is provenance, not a type declaration, so the rule that a map
never carries a type — and therefore can never be the wrong one of two
disagreeing sources of truth — is intact.

**Names survive where a person reads them, and nowhere else.** A
station keeps a pointer to a string literal saying what it was placed
as, written by its own placement function, so the dump can name it.
That is not a lookup: nothing searches it, and there is no table it
points into.

**Argument and type names are not carried at all.** The engine never
used a parameter name, and under
[309](309-types-by-width.md) it does not use type names either, only
widths. Error messages and the dump read them from the source the
binary now carries.

## Related

- [308 — The generator, in C](308-generator-in-c.md), which emits all
  of this and gains both the placement functions and the map compiler
- [309 — Types compared by width](309-types-by-width.md), which is why
  type names can be dropped from what the engine carries
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  the same generator-compiler-load path at the scale of one function
- [303 — Registry emission](303-registry-emission.md), the
  table this deletes
- [304 — Struct field tables](304-struct-field-tables.md),
  which stop being searched by name and start being pointed at
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  whose construction surface the generated code calls
- [007 — The build path](../../docs/007-datapath-build.md) and
  [008 — Map file format](../../docs/008-map-file-format.md), both of
  which this rewrites
- [009 — Loading](../../docs/009-datapath-load.md), which describes a
  runtime parser that stops existing
