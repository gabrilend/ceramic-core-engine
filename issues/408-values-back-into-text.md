# 408 — Values back into text

The mirror of [402](completed/402-struct-constants.md). That issue
built one generalized reader that walks a field table and brace text
together and produces correctly laid-out bytes. This is the same walk
in the other direction.

## Current behavior

The engine can turn text into bytes and cannot turn bytes into text.

The reader exists and is thorough: one routine walking a field table
and a brace expression together, recursing into nested structs, taking
every offset from the compiler through the generated `offsetof` tables
so a padded struct reads correctly, and dying at bind time naming the
entry and the field for every malformed shape. Nothing needs the
opposite direction, so nothing does it.

**Where it has been getting away with that** is that a statics entry
keeps *both* halves: the parsed bytes, and the original string the file
gave it. So when the dump writes a program back out, it prints the
string it kept. It is not formatting a value; it is repeating one.

That stops working the moment a static's value moves onto the port that
reads it ([401](401-static-slots.md)). There is no table then, no entry,
and no retained string — only bytes on a port and a field table
describing their shape. The dump would have nothing to print.

## Intended behavior

**One routine that walks a field table and a value together and
produces text**, recursing into nested structs, emitting the brace form
the reader accepts.

**Its output must be readable by the reader.** Not approximately —
exactly. Format a value, read it back, and the bytes are identical.
That is the property worth testing hardest, because it is what makes a
dumped program reloadable and what makes a program's results usable as
another program's input with no conversion at all.

**Every kind the field table knows must round-trip**: signed and
unsigned integers of each width, floating point, a fixed string, and a
nested struct. Floating point is the one that needs deciding rather
than defaulting — enough digits to round-trip exactly is a different
number from enough digits to read nicely, and the reader is what has to
be satisfied.

**Padding is not printed.** The field table names fields and their
offsets; the holes between them are the compiler's business and appear
nowhere in the text. A struct written with a seven-byte hole reads back
into a struct with a seven-byte hole because the reader consults the
same table, not because the text said anything about it.

**Three callers, one routine.** The dump, which needs it the moment
statics move onto ports. Anything showing a value to a person — a
diagnostic report, a debugger, a workbench. And a program's results: an
output station's values are text for the same reason a static is, since
text resolves its layout when it is read and therefore survives a
rebuild that would silently change what raw bytes meant.

## Suggested implementation steps

1. The formatter, over the same field table structure the reader walks,
   with the same recursion into nested types.
2. A round-trip test as the primary one: for every kind and for the
   deliberately padded struct, format then read then compare bytes.
3. The floating-point decision, made explicitly and left as a comment
   where it lives — shortest representation that reads back identical,
   most likely.
4. Point the dump at it, and remove the retained original string from
   wherever it still lives once nothing reads it.
5. A test that a dumped program's statics section is accepted by the
   reader unchanged, which is the round trip at program scale rather
   than value scale.

## Open questions

- A fixed string field containing bytes that are not printable — a
  length-prefixed blob someone declared as a character array. The
  reader accepts a quoted string; the formatter has to produce
  *something* for arbitrary bytes, and escaping them is a small format
  decision with a long tail.
- Does the formatter belong beside the reader, or in the generated
  code? Beside the reader keeps one file that knows about field tables.
  Generated would let the compiler see each type concretely, which is
  how everything else in this engine avoids guessing — but the reader
  already proves a generic walk is enough.

## Related

- [402 — Struct constants](completed/402-struct-constants.md), the
  reader this mirrors, whose field-table walk and error style this
  follows
- [304 — Struct field tables](completed/304-struct-field-tables.md),
  which emits the offsets and kinds both directions read
- [401 — Static input values](401-static-slots.md), which creates the
  need by removing the retained text
- [703 — The map dump](completed/703-map-dump.md), the first caller
- [209 — The output station](209-map-output-collection.md), where a
  program's results are text for the same reason
