# 408 — Values back into text

The mirror of [402](completed/402-struct-constants.md). That issue
built one generalized reader that walks a field table and brace text
together and produces correctly laid-out bytes. This is the same walk
in the other direction.

## Current behavior

**Both directions exist, as runtime walks over the field tables.** The
opening of this issue used to say the engine could not turn bytes into
text; that stopped being true when statics moved onto their ports
([401](completed/401-static-ports.md)), which could not be done without
building the writer, because deleting the table deleted the original
strings the dump had been echoing.

What stands:

- **The writer mirrors the reader**, walking the same field table in
  the same order, recursing into nested structs, taking every offset
  from the compiler. What comes out is what would go back in.
- **It is asked for its length and then written**, rather than filling
  a fixed buffer, because a struct constant has no useful upper bound
  and a fixed buffer would quietly truncate exactly the values most
  worth reading.
- **The floating-point decision is made and its reasoning sits where it
  lives**: seventeen significant digits for a double, nine for a float,
  which is what round-trips each. That was step 4 of this issue.
- **The dump calls it**, so a constant a runtime write changed dumps as
  what it now *is*. The old table-and-number form printed what the file
  had said, which was a hole admitted in its own comment.

**What is left is one correctness hole and one change of technique.**

**The hole: there are no escape rules, on either side.** The writer
emits a string constant as its characters between two quotes, and the
reader has no notion of a backslash. So a string containing a quote, a
backslash, or a control character does not round-trip — it produces a
map file that reads back as something else or fails to parse, and
nothing anywhere says so. This is not a restructuring; it is a value
the engine will silently corrupt, and it is the reason this issue
should not be left sitting.

**The technique: both directions are runtime walks**, and the plan is
for both to be generated code per struct instead, with the escape
rules written once in the generator and emitted into both halves so
they cannot drift. That half of the work belongs beside
[311b](311b-placement-instead-of-records.md), which is where field
tables stop being searched by name and start being pointed at — the
same change of technique arriving for the same reason. Doing it before
that lands means writing an emitter twice.

So the issue splits cleanly: **fix the escaping now, move the
generation with the registry work.**

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
width-based type comparison of [309](completed/309-types-by-width.md) ask of it.
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

**Taken first, because it is a correctness hole rather than a change
of technique:**

1. **The escape rules, in both the writer and the reader**: quote,
   backslash, everything below 0x20 and everything from 0x80 up.
   Written as one pair of routines with one table between them, so the
   two cannot disagree about what a backslash introduces. Until the
   generator emits them (step 5) they are hand-written and shared, and
   the comment says which of the two arrangements they are in.
2. **A round-trip test at value scale**, which is the primary test for
   this issue: for every kind and for the deliberately padded struct,
   format then read then compare bytes. Include a string holding a
   quote, a tab, and a byte above 0x7F — the three cases that fail
   today.
3. **A round-trip test at program scale**: a dumped program's constants
   are accepted by the reader unchanged, and the program that comes
   back is the program that went in.

**Then, with the registry work
([311b](311b-placement-instead-of-records.md)), because that is when
field tables stop being walked and start being pointed at:**

4. Teach the generator to emit a formatter per registered struct,
   recursing into nested types by calling the nested type's own
   emitted routine rather than by walking anything.
5. Move the escape rules into the generator, emitted into both halves
   from one description, so they cannot drift apart later.
6. Move reading into generated code too, and retire the field-table
   walk in the statics reader once nothing calls it.

**Already done, recorded so nobody does them twice:**

7. ~~The floating-point decision~~ — seventeen significant digits for a
   double, nine for a float, with the reasoning as a comment beside the
   code that does it.
8. ~~Point the dump at the formatter~~, and ~~remove the retained
   original string~~ — both went with the statics table.

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
- [401 — Static input values](completed/401-static-ports.md), which creates the
  need by removing the retained text
- [703 — The map dump](completed/703-map-dump.md), the first caller
- [209 — The output station](completed/209-map-output-collection.md), where a
  program's results are text for the same reason
