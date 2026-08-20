# 311 — The registry dissolved

**This is a parent issue.** The registry stops being one global table
of records compiled in at build time and becomes almost nothing: a
name and a function pointer per box, with everything else folded into
generated code where it is used. The change reaches the map file
format, the generator, the build, and the runtime-compilation path, so
it divides into children rather than being one ticket.

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

## The two things that cannot change, and why

**A size cannot be computed at runtime.** `sizeof` is a compile-time
operator: the compiler evaluates it and burns a literal into the
machine code. A running program has no types at all — C erases every
bit of type information during compilation — so there is nothing left
at runtime for `sizeof` to be applied to. You cannot hand a running
program the text `vec3` and get 12 back.

The trick is visible in the generated file's first real line: it
`#include`s the box source **whole**, so the types become visible to
the compiler, and only then writes `sizeof a0`. That is why the numbers
are right, and it happens at build time.

So every size comes from one of exactly two places: a `sizeof`
expression compiled into the binary, or a compiler invoked at runtime.
There is no third door, and
[310](310-boxes-compiled-at-runtime.md) already says so.

**A name read from text at runtime needs a lookup.** A map file arrives
at a binary that has never seen it, carrying text. Turning
`math.c:add` into a call requires resolving a name at runtime, and
resolving a name *is* a table — calling it something else does not
remove it. The only escape would be compiling the map into C so no name
survives, and maps being data rather than build inputs is what the
whole project rests on.

**Both of those are load-bearing, and the design below keeps them.**
What it removes is everything else.

## What the registry becomes

**One table, two columns.**

```c
static const struct { const char *name; void (*place)(station_t *); }
boxes[] = {
    { "math.c:add",      place__math_c__add },
    { "math.c:multiply", place__math_c__multiply },
};
```

Every number moves into the placement function as a compile-time
constant the compiler folds:

```c
static void place__math_c__add(station_t *s) {
    s->call = add__call;
    s->slots[0].elem_size = sizeof(int);
    s->slots[1].elem_size = sizeof(int);
    s->out_size = sizeof(int);
    s->compare  = int__compare_g;
    s->slots[0].fields = NULL;          /* a struct port would name one */
}
```

**This is the whole trick and it is worth saying why it works.** The
registry was never part of the running engine — a station copies what
it needs at placement and never consults it again; the station header
is deliberately registry-free and resolves its comparison function
*at placement*. So the record existed only to be read once, at the one
moment generated code could just as well do the writing.

**Nothing else needs a name lookup, which is what makes one table
enough.** A comparison function is written in at placement. A struct's
field table is reached through the port that was placed, not searched
for by type name. So exactly one name resolution survives in the whole
engine, and it has two columns.

That is small enough that *registry* is the wrong word for it, and the
word should be retired along with the record. It is a **box table**.

## The four changes

| issue | what it does |
|---|---|
| [311a — Boxes addressed by file](311a-boxes-addressed-by-file.md) | a map names `file:function`; bare basenames resolve, paths settle ties, collisions are fatal |
| [311b — Placement instead of records](311b-placement-instead-of-records.md) | the generator emits a placement function per box; the table shrinks to two columns |
| [311c — Source rides in the binary](311c-source-rides-in-the-binary.md) | each box source emitted as a C array, so the binary carries its own text |
| [311d — The map as a manifest](311d-the-map-as-manifest.md) | the build reads the maps to know what to include; the linker decides what ships |

**311a comes first** because the addressing decides what the table's
key is, and everything else is written against that key. 311b and 311c
are independent of each other. **311d comes last**, because it is the
only one that changes what the build does rather than what the
generator emits.

## What this does not give up

**Guarantee [C1](../docs/058-guarantees.md) survives untouched** —
every size and offset is still computed by the C compiler, never by the
generator, still emitted as `sizeof` and `offsetof` expressions. They
move from a table into a function; they do not become guesses.

**The map file still mentions no types.** Naming a file beside a
function is provenance, not a type declaration, so the rule that a map
never carries a type — and therefore can never be the wrong one of two
disagreeing sources of truth — is intact.

**Argument names are not needed and are not kept.** The engine never
uses a parameter name, and under
[309](309-types-by-width.md) it does not use type names either, only
widths. Names survive for error messages and the dump, and those can
re-read them from the source the binary now carries.

## Related

- [308 — The generator, in C](308-generator-in-c.md), which is what
  emits all of this and gains the placement functions
- [309 — Types compared by width](309-types-by-width.md), which is why
  type names can be dropped from what the engine carries
- [310 — Boxes compiled while the program runs](310-boxes-compiled-at-runtime.md),
  whose growable table this shrinks, and which is the path a box takes
  when a map names one the binary does not carry
- [303 — Registry emission](completed/303-registry-emission.md), the
  table this dissolves
- [304 — Struct field tables](completed/304-struct-field-tables.md),
  which stop being searched by name and start being pointed at
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which made the frozen registry a visible limit
- [007 — The build path](../docs/007-datapath-build.md) and
  [008 — Map file format](../docs/008-map-file-format.md), both of
  which this rewrites
