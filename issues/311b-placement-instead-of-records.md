# 311b — Placement instead of records

Second child of [311](311-the-registry-dissolved.md). The generator
stops emitting a record per box and emits a **function** per box that
writes a station directly. Everything the record held becomes a
compile-time constant inside it.

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
static void place__math_c__add(station_t *s) {
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

**The table shrinks to two columns.**

```c
static const struct { const char *name; void (*place)(station_t *); }
boxes[] = { { "math.c:add", place__math_c__add }, ... };
```

That is the one name lookup left in the engine, and it survives because
a map file arrives as text and text has to be resolved. Everything else
is reached without a name.

**A struct port's field table is pointed at, not searched for.** Today
a struct constant in a map is read by finding the struct's field table
by type name. The placement function knows the type concretely, so it
writes the pointer onto the port, and the reader follows it. The
by-name struct lookup goes away with the by-name box record.

**What is deleted:** the box record type, the parameter arrays, every
type-name string the engine carried, the stored task size, the
by-name struct search, and the backwards lookup from a shim pointer to
a box name — the dump can ask the station which placement made it, or
carry the name it was placed with, rather than searching a table
backwards.

**Type names survive only where a person reads them.** Error messages
and the dump want to say `vec3` rather than `12 bytes`. Those can read
the name out of the source the binary now carries
([311c](311c-source-rides-in-the-binary.md)), which is the exact text
that was compiled, so a name reported is never a name from a source
that has since changed.

**Argument names are not carried either.** The engine never used one.
A debug report that wants them re-reads the embedded source by
parameter index.

## What this does to hand placement

[210g](210g-one-way-to-build-a-station.md) asks whether placing a
station by hand — stating shapes directly instead of naming a box —
survives. **It does, and this issue is what makes the answer easy.**

A placement function *is* hand placement, written by the generator
instead of by a person. By-name placement is a table lookup that finds
one and calls it. So there are not two doors into the engine; there is
one door with a typed front and a raw back, and the front is built out
of the back.

That means phase 2's station-table tests keep placing stations without
constructing a registry — they call the raw form directly, which is
what they were always doing — and a test of the station table stays a
test of only the station table.

## Suggested implementation steps

1. The generator emits a placement function per box, alongside the
   record, with the record still authoritative. Nothing changes
   behaviour yet; the two can be compared.
2. A test that placing by record and placing by function produce
   identical stations, field for field. This is the proof the
   translation is faithful, and it is cheap while both exist.
3. Placement by name routes through the function; the record stops
   being read.
4. Struct field tables written onto ports at placement, and the by-name
   struct search deleted.
5. The record type, the parameter arrays, the type-name strings, and
   the stored task size deleted. The table becomes two columns.
6. The dump's shim-to-name lookup replaced by the station carrying what
   it was placed as.
7. The word *registry* replaced by *box table* wherever it appears, in
   source and in documents, since the thing it named no longer exists.

## Open questions

- The dump needs to name the box a station was placed as. Carrying the
  string on the station is one pointer per station and is simplest;
  carrying the table index is smaller and needs the table to be
  ordered stably, which it is at build time and is not once
  [310](310-boxes-compiled-at-runtime.md) appends rows at runtime. The
  index is probably right anyway, since a row is never removed, but it
  should be decided rather than assumed.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311c — Source rides in the binary](311c-source-rides-in-the-binary.md),
  which is where type and argument names go once the engine stops
  carrying them
- [303 — Registry emission](completed/303-registry-emission.md), the
  emission this replaces
- [304 — Struct field tables](completed/304-struct-field-tables.md),
  which stop being searched and start being pointed at
- [210g — One way to build a station](210g-one-way-to-build-a-station.md),
  whose open question about hand placement this answers
- [308 — The generator, in C](308-generator-in-c.md), which does the
  emitting
