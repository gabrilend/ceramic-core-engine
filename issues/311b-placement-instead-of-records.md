# 311b — Placement instead of records

Second child of [311](311-the-registry-dissolved.md). The generator
stops emitting a record per box and emits a **function** per box that
writes a station directly. Everything the record held becomes a
compile-time constant inside it, and the table the records lived in is
deleted rather than shrunk.

## Current behavior

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
parameter name, and under [309](309-types-by-width.md) it does not use
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

1. The generator emits a placement function per box, alongside the
   record, with the record still authoritative. Nothing changes
   behaviour yet; the two can be compared.
2. A test that placing by record and placing by function produce
   identical stations, field for field. This is the proof the
   translation is faithful, and it is cheap while both exist.
3. Placement routes through the function; the record stops being read.
4. The name literal written onto the station, and the dump's backwards
   shim-to-name lookup deleted.
5. Struct field tables written onto ports at placement, and the by-name
   struct search deleted.
6. The record type, the table, the parameter arrays, the type-name
   strings, and the stored task size deleted.
7. The word *registry* removed from source and documents, since the
   thing it named no longer exists in any form.

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
- [308 — The generator, in C](308-generator-in-c.md), which does the
  emitting
