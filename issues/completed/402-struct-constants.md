# 402 — Struct constants in the statics table

## Current behavior

Built, inside the statics module: one generalized reader walking a
field table and brace text together, recursing into nested tables,
never one parser per type. Offsets come from the compiler through
the generated offsetof tables, so the deliberately padded struct
reads correctly. The string-field decision landed as the fixed-size
character array: the bytes live inside the struct, nobody owns
anything, and a value longer than the field is fatal rather than
truncated — the reasoning sits as a comment where the decision lives.
Every malformed case — too many values, too few, a string where a
number belongs — dies at bind time naming the entry and field, each
proven by a forked-child death test. Bytes are proven identical to a
compiled initializer of the same value for the every-kind struct
(primitive, nested, string, wide unsigned).

## Intended behavior

A statics entry may hold a structured value, written with braces and
nesting, and a slot may claim it as a struct.

Turning that text into bytes needs to know where each number lands.
Issue 304 emits exactly that: a table of field offsets and shapes per
struct type. This issue is the reader that walks a field table and a
text value together.

**One reader, not one per type.** The alternative — a parser generated
for each struct — is more emitted code and more places for the
generator to be wrong. A table walked by one routine is the same shape
as the rest of the engine's dispatch decisions.

**Nested structs recurse** into the field table of the type they
contain, which is what lets a static hold something genuinely
structured rather than a flat row of numbers.

**Offsets come from the compiler, never from arithmetic.** Padding and
alignment are the compiler's business. A reader that computes them
itself will agree with the compiler on the machine it was tested on and
disagree somewhere else, producing a struct whose fields are subtly
shifted — which reads as a value that is merely wrong rather than as an
error.

**A mismatch between the text and the shape is fatal at load.** Too
many values, too few, a string where a number belongs. All of these are
knowable when the map loads, and all of them are silent corruption if
they are not caught then.

## Suggested implementation steps

1. The value reader: given a field table and a text value, fill a
   buffer of the type's size.
2. Recursion into nested field tables.
3. Strings, which are the one field kind whose length is not fixed by
   the type. Decide whether a string field is a fixed-size character
   array or a pointer, and say why in a comment where the decision
   lives — a pointer raises the question of who owns the text, which a
   fixed array does not.
4. Error reporting that names the entry, the field, and what was
   expected.
5. Tests comparing the produced bytes against a compiled initializer of
   the same value, for: a flat struct, a nested one, one containing a
   string, and one whose padding is non-obvious.
6. A test that each malformed case fails at load rather than producing
   bytes.

## Related

- Issue 304 — the field tables this walks
- Issue 401 — the table this fills
- [008 — Map file format](../../docs/008-map-file-format.md)
