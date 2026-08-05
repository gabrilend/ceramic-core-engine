# 309 — Types compared by shape, not by name

## Current behavior

Every type check in the engine is a string comparison.

The generator emits, for each box, the text of each parameter's type and
the text of its return type, and the registry carries them. When the
loader checks a wire it compares those two strings; when they differ it
refuses, and the message reads *"head → wrong.1: box returns int, slot
takes double"*. The names ride along **for the error message** — the
sizes are what the engine actually runs on, and four bytes against four
bytes is not a sentence anybody can act on.

Underneath that, the generator already emits something far better and
nothing consults it. Every struct in the box sources gets a field table:
per field a name, an offset, a size, and a kind — signed, unsigned,
floating, fixed string, or a nested struct pointing into the same table
— with every offset an `offsetof` expression and every size a `sizeof`,
so the compiler computes all of it and the deliberately padded case with
its seven-byte hole reads correctly. **The engine knows the exact shape
of every value it moves and decides compatibility by comparing labels.**

Two consequences, one already biting and one about to.

**Identical shapes with different names cannot be wired.** A box
producing a struct of four integers called `vec4` cannot feed a box
taking a struct of four integers called `stats`, even though the bytes
are indistinguishable and the delivery path would copy them correctly.
The author's options are to rename one, or to write a box that takes
one and returns the other and does nothing.

**Different shapes with the same name pass the check and corrupt.** Two
structs both called `vec3`, compiled separately with different field
layouts, compare equal by name. Today that requires one build to
disagree with itself, so it is theoretical. It stops being theoretical
the moment a box can be compiled at runtime and loaded into a running
program — which is the direction the workbench and the runtime
construction surface both point.

## Intended behavior

**Two types are compatible when their shapes match, and nothing else is
consulted.**

A shape is what the field table already records: for a primitive, its
kind and its size; for a struct, the ordered list of its fields' kinds,
sizes, and offsets, recursed into nested structs. Field *names* are not
part of a shape and neither is the struct's own name. A four-integer
struct is a four-integer struct — whether its fields are called x, y, z
and g or strength, agility, wisdom and fondness, the pattern of the data
is the same and that pattern is what a wire carries.

**Names stay, for people.** Every message that names a type keeps naming
it, because *"box returns vec4, slot takes stats — four signed 4-byte
fields against three"* is the sentence somebody needs. What changes is
that the name is reported rather than decided upon.

**Padding is part of the shape and this is not negotiable.** Offsets
come from the compiler through the generated tables, so two structs with
the same fields in a different order, or the same fields with different
alignment, have different shapes and do not match. This is not
pedantry — the delivery path is a memory copy, so anything that
disagrees about where a field sits produces silently wrong values rather
than an error.

**What this costs, and it should be stated rather than discovered.**
Two types with identical layout and unrelated meaning become
interchangeable, silently. A four-float position wires into a four-float
colour without complaint. That is the price of caring about the pattern
of the data rather than the label on it, and it is chosen rather than
overlooked: names are what one author called something, and a wire is
about what the bytes are. Anyone who wants the label enforced can make
the shapes differ.

**This is what makes a box loadable at runtime safely.** A box compiled
after the program started brings its own idea of every struct it
touches. Comparing names would accept a mismatched layout and corrupt;
comparing shapes catches it at the moment the wire is drawn, which is
the only moment anybody can still do something about it.

## Suggested implementation steps

1. A shape comparison over two field-table entries: kinds, sizes, and
   offsets, recursing into nested structs, ignoring every name. Its own
   tests first, including the padded case, the nested case, the
   same-fields-different-order case, and the fixed-string case, because
   everything above it trusts the answer.
2. Primitives compared the same way — kind and size — so one routine
   answers for every type rather than one for structs and one for the
   rest.
3. The wire check calls it instead of comparing strings, at every place
   a wire is checked: the loader, the runtime construction surface, and
   the comparator's return-type check.
4. Error messages rewritten to report both names *and* the first place
   the shapes diverge, since "these do not match" is much less useful
   than "field two is four bytes here and eight bytes there".
5. A test that two identically-shaped, differently-named structs wire
   together and deliver byte-identical values, which is the capability
   this adds.
6. A test that two identically-named, differently-shaped structs are
   refused, which is the corruption this prevents.
7. Record the interchangeability in [058](../docs/058-guarantees.md) as
   a stated non-guarantee.

## Open questions

- A fixed string field carries its length. Are a 16-byte string field
  and a 16-byte array of signed bytes the same shape? They occupy the
  same bytes and mean different things, and the field table can tell
  them apart — so this is a choice about how far the principle goes.
- Does the comparison belong to the generator, emitted once per pair it
  can see, or to the engine, computed when a wire is drawn? The
  generator cannot see every pair, since a runtime-loaded box brings new
  types — which argues for the engine.
- The registry currently stores type names as text for messages. Does it
  also need to store a shape, or is a pointer into the struct table
  enough for everything except primitives?

## Related

- [304 — Struct field tables](completed/304-struct-field-tables.md),
  which already emits everything this needs and has never been asked
- [303 — The registry](completed/303-registry-emission.md), where the
  type names live and where a shape would sit beside them
- [603 — The loader, second pass](completed/603-loader-second-pass.md),
  whose wire check this replaces and whose error message this improves
- [502 — The comparator](completed/502-comparator.md), whose
  return-type check is the other caller
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  where a wire drawn at runtime meets the same check
- [801 — The workbench in the browser](801-browser-workbench.md), which
  loads box sources the running program never compiled
- [058 — Guarantees](../docs/058-guarantees.md), where the
  interchangeability belongs as a non-guarantee
