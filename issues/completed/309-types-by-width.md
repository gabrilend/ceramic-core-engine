# 309 — Types compared by width, not by name

**Renamed from "compared by shape."** Shape comparison — matching kind,
size, and offset field by field — was designed in full and then not
taken. What is taken is simpler and weaker: **a wire is legal when the
two sides are the same number of bytes.** The shape design is kept
below under what was considered, because the reasoning is worth more
than the mechanism was and because the field tables it would have used
exist and are needed elsewhere.

## Current behavior

**Done.** A wire is legal when the two sides count the same number of
bytes. The check is one integer against another, at both places a wire
is checked: the loader, when a map file draws one, and the rewiring
surface, when a running program does.

**What it buys, and the reason for the change.** Two structs with
identical layouts and different names now connect. Before, a box
producing three floats called `triple` could not feed a box taking
three floats called `vec3`, even though the bytes are indistinguishable
and delivery would copy them correctly — an author's only options were
to rename one, or to write a box that took one and returned the other
and did nothing. Both structs live in the box source and exist for the
test, deliberately: the thing being proven is that the *engine* stopped
caring about the name, so the shapes have to be real types the
generator saw.

**Type names still ride along, in the message and nowhere else**, and
both widths ride along beside them:

```
box returns int (4 bytes), slot takes double (8 bytes)
```

*"box returns vec4, slot takes stats"* does not tell a person why those
disagree. Four bytes against eight does.

**The field tables were left where they are.** Nothing here consults
them, and two other things do: the reader that turns brace text into
bytes, and the writer that turns bytes back into text.

**What it widens rather than closes**, recorded in
[058](../../docs/058-guarantees.md) as its own non-guarantee: two types
of the same width and different layouts now wire without complaint. A
struct of four integers connects to a struct of two integers and a
double. That is the accepted cost, and it is sharpest where a value is
a **handle** — a wrong wire between two data types produces a wrong
number, which is visible and local, while a wrong wire into a port
expecting a map handle produces a call through whatever those bytes
were, and every pointer is the same width as every other pointer and
as a `double`.

Shape comparison would close it honestly. It is designed in full below
and the tables it needs have been emitted since phase 3, consulted by
nothing the whole time.
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
step further. [058](../../docs/058-guarantees.md) already sells order,
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
it different. [310](../310-boxes-compiled-at-runtime.md) records the same
correction from its own side.

The design is kept because if anybody ever wants layout disagreements
caught, it is written and the tables it needs have been emitted since
phase 3, unconsulted the entire time.

### The accepted cost is sharpest where a value is a handle

Two same-width structs with different layouts wire without complaint,
and that is stated above as the price. It is a mild price when the
values are **data**: a wrong wire produces a wrong number, which is
visible, local, and eventually noticed.

**It stops being mild when the value is a pointer the engine will call
through.** Once a program can build another program
([212](../212-one-way-to-build-a-program.md)), the surface's operations
exist as boxes, and their inputs are handles: a map handle, a placement
function, a compiled box. On a 64-bit machine every one of those is
**eight bytes** — and so is a `double`, a `long`, a file offset, and a
pointer to something else entirely.

So a wire delivering *any* eight-byte value into a box expecting a map
handle is legal by this issue's own rule, and the failure is not a
wrong number. It is a call through whatever was in those bytes.

**Nothing here is proposed as a fix yet**, because the cheap answer —
wrapping each handle kind in a struct of a deliberately distinctive
size — buys the distinction back by making the type system lie about
sizes, and that is worse than the thing it prevents. Shape comparison
would catch it honestly, and the design for shape comparison is written
below and its field tables have existed since phase 3.

What this section is for is that the cost was accepted while all values
were data, and self-modifying programs change what it costs without
changing what it is. That should be a decision somebody makes rather
than a discovery somebody has.

**It was made, and it was made this way.** The construction operations
became boxes ([212](../212-one-way-to-build-a-program.md)), a program
is named by its address, and the risk stands — recorded in
[058](../../docs/058-guarantees.md) in its own words rather than
folded into the width non-guarantee, because a mis-wire that produces
a wrong number and a mis-wire that produces a write through arbitrary
memory are different facts. Shape comparison was weighed again at that
moment and again not taken. The boxes refuse a null before touching
anything, which is the whole of what can be checked.


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
5. Record the interchangeability in [058](../../docs/058-guarantees.md) as
   a stated non-guarantee, in its own words rather than folded into the
   value-independence one — this is a different fact, about a wire
   rather than about a value.
6. Leave the field tables where they are. Nothing here consults them,
   and two other things do: the reader that turns brace text into bytes
   ([402](402-struct-constants.md)) and the writer that will
   turn bytes back into text ([408](../408-values-back-into-text.md)).

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

- [304 — Struct field tables](304-struct-field-tables.md),
  which already emits everything this needs and has never been asked
- [303 — The registry](303-registry-emission.md), where the
  type names live and where a shape would sit beside them
- [603 — The loader, second pass](603-loader-second-pass.md),
  whose wire check this replaces and whose error message this improves
- [502 — The comparator](502-comparator.md), whose
  return-type check is the other caller
- [212 — One way to build a program](../212-one-way-to-build-a-program.md),
  where a wire drawn at runtime meets the same check
- [801 — The workbench in the browser](../801-browser-workbench.md), which
  loads box sources the running program never compiled
- [058 — Guarantees](../../docs/058-guarantees.md), where the
  interchangeability belongs as a non-guarantee
