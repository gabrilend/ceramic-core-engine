# 309 — Types compared by width, not by name

**Renamed from "compared by shape."** Shape comparison — matching kind,
size, and offset field by field — was designed in full and then not
taken. What is taken is simpler and weaker: **a wire is legal when the
two sides are the same number of bytes.** The shape design is kept
below under what was considered, because the reasoning is worth more
than the mechanism was and because the field tables it would have used
exist and are needed elsewhere.

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

**Only the first of those two is fixed here.** Width comparison lets
identical layouts wire and does nothing about disagreeing ones — it
widens the hole rather than closing it, since same-width types of every
kind now pass. That is a deliberate trade and the reasoning is below;
what it leaves behind belongs to
[310](310-boxes-compiled-at-runtime.md), where a box arriving with its
own idea of a struct is no longer theoretical at all.

## Intended behavior

**Two types are compatible when they are the same number of bytes, and
nothing else is consulted.**

The size is already there — the registry carries a `sizeof` the
compiler computed for every parameter and every return, and that number
is what the engine has always actually run on. Cells are that many
bytes, a delivery copies that many bytes, a task is allocated for
exactly that many. **The check stops asking a question the engine does
not otherwise care about.**

**Names stay, for people.** Every message keeps naming both types,
because *"box returns vec4, slot takes stats — 16 bytes against 12"* is
the sentence somebody can act on. What changes is that the name is
reported rather than decided upon.

**Why this and not shape.** Both fix the problem that identically-laid-
out types with different names cannot be wired. Shape comparison also
catches layout disagreements, which width does not. It was declined
anyway because **every stricter check eventually makes somebody write a
box that takes one type and returns another and does nothing** — an
adapter written for no reason except to satisfy the engine. This design
does not ask for those. Somebody bringing C they already had should be
able to bring it unchanged, and a rule that is one sentence long — *the
bytes must be the same count* — is a rule they can hold in their head
while doing it.

**What is caught, which is more than it sounds like.** Any mismatch
that changes the width still fails loudly at the moment the wire is
drawn: an `int` into a `double` is four against eight and refuses. Most
type mistakes are width mistakes.

**What is not caught, stated plainly rather than discovered.** Two
types of the same width are interchangeable, silently and completely.
On a 64-bit machine that means `int` against `float`, and `long`
against `double` against a pointer. A `1.0f` delivered into an `int`
slot arrives as `1065353216`, with no error anywhere, ever. Two structs
with the same fields in a different order have the same total width and
will wire, putting every field in the wrong place. **The delivery path
is a memory copy and it will copy whatever it is told to.**

This is the same bargain the rest of the engine makes, extended one
step further. [058](../docs/058-guarantees.md) already sells order,
timing, and pairing; this sells the last thing standing between a value
and a wire that fits it. What is bought is that nothing anybody wrote
in C has to be adjusted to fit through.

### What was considered instead

Shape comparison: for a primitive its kind and its size, for a struct
the ordered list of its fields' kinds, sizes, and offsets, recursing
into nested structs, ignoring every name. The generator already emits
exactly this, per struct, with every offset an `offsetof` and every
size a `sizeof`, so the padded case with its seven-byte hole reads
correctly.

It would have caught both same-width primitives and reordered structs.

**It is not a prerequisite for anything, including compiling boxes
while the program runs.** That was claimed while this issue still
proposed shape comparison, and it was wrong in a way worth recording: a
box arriving late reports the width of each input and its output, by
the same `sizeof` the compiler computes for every other box, so it asks
nothing the type system cannot answer. Two same-width structs with
different layouts already wire at build time — that is the accepted
cost here — and a box compiled later makes it likelier without making
it different. [310](310-boxes-compiled-at-runtime.md) records the same
correction from its own side.

The design is kept because if anybody ever wants layout disagreements
caught, it is written and the tables it needs have been emitted since
phase 3, unconsulted the entire time.

## Suggested implementation steps

1. The wire check compares the two sizes instead of the two name
   strings, at every place a wire is checked: the loader, the runtime
   construction surface, and the comparator's return-type check. This
   is a smaller change than the issue it replaces — the sizes are
   already sitting in the registry beside the names.
2. Error messages report both names **and** both widths, since a person
   reading *"box returns vec4, slot takes stats"* cannot see why those
   disagree, and *"16 bytes against 12"* tells them.
3. A test that two identically-laid-out, differently-named structs wire
   together and deliver byte-identical values, which is the capability
   this adds and the reason for the change.
4. A test that two types of different width are refused, naming both.
5. Record the interchangeability in [058](../docs/058-guarantees.md) as
   a stated non-guarantee, in its own words rather than folded into the
   value-independence one — this is a different fact, about a wire
   rather than about a value.
6. Leave the field tables where they are. Nothing here consults them,
   and two other things do: the reader that turns brace text into bytes
   ([402](completed/402-struct-constants.md)) and the writer that will
   turn bytes back into text ([408](408-values-back-into-text.md)).

## Open questions

**Answered — all three dissolved with the move to width:**

- *Are a 16-byte text field and 16 signed bytes the same shape?* The
  same **width**, so yes, they wire. The question only existed under
  shape comparison, where kind was part of the answer. Nothing consults
  kind now.
- *Does the comparison belong to the generator or the engine?* The
  engine, at the moment a wire is drawn. There is nothing left to
  precompute — it is one integer against another.
- *Does the registry need to store a shape beside the name?* No. It
  already stores the size, which is now the whole of what a wire is
  checked against.

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
