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

**Both directions are emitted per type by the generator.** For each
registered struct the generator writes two routines — one that reads
the brace text into bytes, one that writes the bytes back out as brace
text — with every offset and every field kind resolved when the engine
is built rather than dispatched on while it runs. This replaces the
generic walk [402](completed/402-struct-constants.md) built, and it is
the larger change, chosen because it puts each type's text grammar in
exactly one place, written by the only thing that knows the type
concretely.

**What the field table is left doing.** It keeps names and sizes,
which is what the registry's own description routine and the
width-based type comparison of [309](309-types-by-width.md) ask of it.
Nothing walks it to read or write a value any more; the two callers
that do so today are the statics reader, which is being replaced, and
the registry's describe-yourself printout, which only wants names.

**Its output must be readable by the reader.** Not approximately —
exactly. Format a value, read it back, and the bytes are identical.
That is the property worth testing hardest, because it is what makes a
dumped program reloadable and what makes a program's results usable as
another program's input with no conversion at all. With both
directions generated from one description, the test is also what keeps
the emitter honest — the two halves are written by the same script and
can disagree only through a bug in it.

**Every kind must round-trip**: signed and unsigned integers of each
width, floating point, a fixed string, and a nested struct. Floating
point is the one that needs deciding rather than defaulting — enough
digits to round-trip exactly is a different number from enough digits
to read nicely, and the reader is what has to be satisfied.

**Strings end at the first NUL, and the scan is bounded anyway.** A
character array is a C string and stays one; nothing is added to the
author's struct to carry a count, because the engine's standing
refusal is to make anyone write a function to fit it. The formatter
walks at most `array_len` bytes and stops there whether or not it
found a terminator, so bytes arriving without one cost a bounded scan
rather than a read past the field. A separate length field would not
improve on that: it can claim more than the array holds, so it would
have to be checked against `array_len` before being trusted, leaving
the same bound plus a second value that can be wrong.

**The text is pure seven-bit ASCII.** Bytes below 0x20, the double
quote, the backslash, and everything from 0x80 up are written as
escapes, so a map file survives any editor, terminal, or transport
that mangles high bytes. The cost is accepted with open eyes: text in
a language that needs bytes above 0x7F is no longer readable or
typable in place — a name in French appears as escape codes rather
than as itself. The reader gains the matching decoder, generated
alongside.

**Padding is not printed.** The emitter names fields and reaches them
by compiler-computed offsets; the holes between them are the
compiler's business and appear nowhere in the text. A struct written
with a seven-byte hole reads back into a struct with a seven-byte hole
because both generated routines took their offsets from the same
`offsetof`, not because the text said anything about it.

**Three callers, one routine.** The dump, which needs it the moment
statics move onto ports. Anything showing a value to a person — a
diagnostic report, a debugger, a workbench. And a program's results: an
output station's values are text for the same reason a static is, since
text resolves its layout when it is read and therefore survives a
rebuild that would silently change what raw bytes meant.

## Suggested implementation steps

1. Teach the generator to emit a formatter per registered struct,
   recursing into nested types by calling the nested type's own
   emitted routine rather than by walking anything.
2. The escape rules, written once in the generator and emitted into
   both halves so they cannot drift: quote, backslash, everything
   below 0x20 and everything from 0x80 up.
3. A round-trip test as the primary one: for every kind and for the
   deliberately padded struct, format then read then compare bytes.
   Include a string holding a quote, a tab, and a byte above 0x7F.
4. The floating-point decision, made explicitly and left as a comment
   where it lives — shortest representation that reads back identical,
   most likely.
5. Move reading into generated code too, and retire the field-table
   walk in the statics reader once nothing calls it.
6. Point the dump at the formatter, and remove the retained original
   string from wherever it still lives once nothing reads it.
7. A test that a dumped program's statics section is accepted by the
   reader unchanged, which is the round trip at program scale rather
   than value scale.

## Open questions

**Answered:**

- *What does the formatter emit for a character array holding bytes
  that are not plain printable text?* Escapes, for a wider set than
  strictly necessary: the quote and backslash because they break the
  grammar, bytes below 0x20 because they have no visible shape, and
  bytes from 0x80 up so that a map file is seven-bit ASCII end to end
  and cannot be damaged by anything that handles high bytes carelessly.
  The price is that non-English text stops being readable in the file
  itself, which is a real loss taken deliberately in exchange for a
  format that never depends on the encoding of the tool looking at it.

  A NUL in the middle of a string stopped being part of this question.
  Character arrays remain C strings, so the formatter stops at the
  first terminator — which is where the author's own C stops reading
  too — and stops at `array_len` regardless, so a field arriving with
  no terminator costs a bounded scan rather than a read past the end.

- *Does the formatter belong beside the reader, or in the generated
  code?* Generated — and reading moves there with it, so each type's
  text grammar exists once, written by the generator that sees the
  type concretely. The generic walk is not wrong and its existence
  proved the grammar is small enough to walk; what decides against
  keeping it is that a walk re-derives at run time what the compiler
  already knew, and this engine's whole habit is to ask the compiler
  instead of re-deriving. Retiring a finished reader to get there is
  the cost, and it is a one-time cost against a permanent one.

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
