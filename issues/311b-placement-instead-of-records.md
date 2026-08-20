# 311b — Placement instead of records

Second child of [311](311-the-registry-dissolved.md). The generator
stops emitting a record per box and emits a **function** per box that
writes a station directly. Everything the record held becomes a
compile-time constant inside it, and the table the records lived in is
deleted rather than shrunk.

## Current behavior

**Placement now runs through generated functions; the deletions
wait.** Steps 1 to 3 are done and the record is no longer read to
build a station.

What stands:

- **A placement function per box**, emitted by the generator, writing
  the shim, the slot sizes, the return size, the type names, the
  comparator's extra port and the comparison — with every number a
  `sizeof` the compiler folds into an immediate. Nothing is stored;
  the numbers were computed while the box was compiled.
- **The two comparator refusals are inside it**, so a box that returns
  nothing or has no comparison is refused wherever it is placed from,
  not only through the by-name door.
- **Placing by name is a lookup that finds one and calls it**, which
  is what makes hand placement the primitive rather than the
  alternative. A box compiled while the program runs carries its own
  placement rows and is found the same way.
- **A test places every box both ways and compares field for field**,
  including as a comparator wherever a box can be one. Sixteen boxes,
  twelve of them comparators, identical.

**Two things the plan did not see**, both found by building it:

**Generated code that builds stations needs the engine's symbols to be
exported.** A box compiled while the program runs arrives as a shared
object and is opened at run time; its placement function calls
straight into the station layer, and a shared object cannot see a
symbol the host executable did not publish. This never came up while
generated code held only shims, because a shim calls the box and the
box is inside the object with it. It appeared the moment generated
code started *building stations*, which is the whole point of a
placement function. Anyone linking a program with this engine inherits
the requirement, and it belongs with the packaging story.

**A symbol built from a path has to be built from a path that means
the same thing everywhere.** The generator is handed absolute paths by
the build, so the first symbols it emitted carried the whole location
of the machine that built them — enormous, and different on every
machine, which makes two generated files impossible to compare. The
generator now takes the project root and shortens against it. A box
compiled at run time has no project root to be relative to and keeps
its own path, which is already unique.

**What the remaining steps wait for.** Deleting the record, the table
and the type-name strings needs there to be no by-name lookup at run
time at all, and that is [311d](311d-the-map-becomes-code.md)'s doing
— it emits the calls, so a placement function is reached by being
called rather than by being found. Until then the record is still
consulted for the two comparator refusals that want to name a return
type, and the table is what by-name placement searches.

### What the record still is

The generator emits a `box_info_t` per box: name, shim pointer,
parameter count, an array of parameter type names and sizes, a return
type name and size, the exact task allocation size, and a comparison
function pointer. Placing a station reads that record and copies the
parts a station keeps.

The record is read **once**, at placement, and never again. Nothing in
the running engine consults it: a station holds its own shim pointer,
its own slot sizes, its own return size, and its own comparison
function, and the station header is deliberately kept free of any
registry reference so that resolving a comparison happens at placement
rather than on the delivery path.

**So the record exists only to be read at the one moment generated code
could just as well do the writing.** That is the whole observation this
issue rests on.

## Intended behavior

**One placement function per box, emitted by the generator.**

```c
static void place__math_dot_c__add(station_t *s) {
    s->box_name = "math.c:add";      /* a literal, for the dump */
    s->call     = add__call;
    s->slots[0].elem_size = sizeof(int);
    s->slots[1].elem_size = sizeof(int);
    s->out_size = sizeof(int);
    s->compare  = int__compare_g;
}
```

Every number is a `sizeof` expression, so **guarantee
[C1](../docs/058-guarantees.md) is untouched** — the compiler still
computes every size and the generator still never guesses. The numbers
move from a table into a function and the compiler folds them into
immediates. They are not stored anywhere at all.

**There is no table.** An earlier draft of this issue kept a two-column
one — a name and a placement pointer — for resolving text at run time.
[311d](311d-the-map-becomes-code.md) removes the need by having the
generator emit the calls, so a placement function is reached by *being
called*, never by being found.

**A station carries the name it was placed as, and that is not a
lookup.** The literal is written by the placement function itself, so
the dump reads a field rather than searching anything. It costs a
pointer per station; the string is read-only data the compiler was
going to emit either way, nothing is allocated, and nothing is freed.

**A struct port's field table is pointed at, not searched for.** Today
a struct constant in a map is read by finding the struct's field table
by type name. The placement function knows the type concretely, so it
writes the pointer onto the port and the reader follows it. The by-name
struct search goes away with the by-name box record.

**What is deleted:** the box record type, the whole table of them, the
parameter arrays, every type-name string the engine carried, the stored
task size, the by-name struct search, and the backwards lookup from a
shim pointer to a box name — the station knows its own name now.

**Type and argument names are not carried.** The engine never used a
parameter name, and under [309](completed/309-types-by-width.md) it does not use
type names either, only widths. Error messages and the dump read them
out of the source the binary carries
([311c](311c-source-rides-in-the-binary.md)) — which is the exact text
that was compiled, so a name reported can never come from a source that
has since changed on disk.

## What this does to hand placement

[210g](210g-one-way-to-build-a-station.md) asks whether placing a
station by hand — stating shapes directly instead of naming a box —
survives. **It does, and this issue is what makes the answer easy.**

A placement function *is* hand placement, written by the generator
instead of by a person. So there are not two doors into the engine;
there is one door, and by-name placement was only ever a way of finding
which generated hand-placement to call — a way that
[311d](311d-the-map-becomes-code.md) now performs at generation time.

That means phase 2's station-table tests keep placing stations without
constructing any table, which is what they were always doing, and a
test of the station table stays a test of only the station table.

**A station placed directly, with no name given, has nothing for the
dump to write on its station line.** The placement functions the
generator emits always write one. A test that places by hand and gives
none is a program that cannot be dumped faithfully — which is honest,
since nothing on disk describes it either.

## Suggested implementation steps

1. **Done.** The generator emits a placement function per box,
   alongside the record. One correction to the plan: they cannot be
   emitted with *nothing* referring to them, because a build with
   warnings as errors rejects a function nobody calls — so the table
   that by-name placement needs anyway arrived in the same step.
2. **Done.** Every box placed both ways and compared field for field,
   as a plain station and as a comparator wherever it can be one.
3. **Done.** Placement routes through the function; the record is no
   longer read to build a station.
4. The name literal written onto the station, and the dump's backwards
   shim-to-name lookup deleted.
5. Struct field tables written onto ports at placement, and the by-name
   struct search deleted.
6. The record type, the table, the parameter arrays, the type-name
   strings, and the stored task size deleted. **Waits on
   [311d](311d-the-map-becomes-code.md)**: while a running program
   still resolves a name, something has to be searched.
7. The word *registry* removed from source and documents, since the
   thing it named no longer exists in any form. Waits on the same.

## Open questions

None outstanding. The one this issue carried — whether a station should
remember its box by row number, by name string, or by a backwards scan
— dissolved when the table did. There is no row to number and nothing
to scan; the placement function writes the name and the dump reads it.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311c — Source rides in the binary](311c-source-rides-in-the-binary.md),
  which is where type and argument names go once the engine stops
  carrying them
- [311d — The map becomes code](311d-the-map-becomes-code.md), which
  calls these functions and is why no table is needed to find them
- [303 — Registry emission](completed/303-registry-emission.md), the
  emission this replaces
- [304 — Struct field tables](completed/304-struct-field-tables.md),
  which stop being searched and start being pointed at
- [210g — One way to build a station](210g-one-way-to-build-a-station.md),
  whose open question about hand placement this answers
- [308 — The generator, in C](completed/308-generator-in-c.md), which does the
  emitting
