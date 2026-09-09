# 601c — The format stops guessing

Three places where the map file reader accepts something it should
refuse, or refuses something it should accept. None of them is a large
change; all three are the same kind of mistake, which is why they are
one issue.

## Current behavior

**A bare word is accepted where a string belongs.** Writing a value
without quotes works:

```
station a read_int_file p result
  in 0 = fire
```

loads, and the dump writes it back as `in 0 = "fire"`. So the format has
two spellings for one value, and the loose one collides with a line that
means something entirely different: `in 0 - fire` draws a wire from a
station called `fire`, and `in 0 = fire` sets a constant. One character
apart, unrelated meanings, and one of them accepts a bare word.

**A value cannot continue past the end of a line.** The reader takes one
line at a time into a fixed buffer, so a brace-delimited struct value has
to fit on one line however many fields it has. Nothing says so; it simply
fails in whatever way the truncation happens to produce.

**A line longer than the buffer is silently split in two.** The reader
fills its buffer and carries on, with no check that what it read ended in
a newline. The tail becomes a fresh line as far as the parser is
concerned, so:

- the line numbers in every message after the split are wrong
- whether an error is reported at all depends on where the cut lands —
  a cut inside whitespace produces a confusing but honest refusal, and a
  cut inside a value can produce two lines that each parse

This is the silent-corruption shape the project refuses everywhere else,
and it exists here only because nobody wrote the check.

## Intended behavior

**A string is quoted, always.** A value that is not a number and not a
brace-delimited struct must be in quotes, and a bare word is refused
naming the line and saying what the two spellings mean. The dump already
writes quotes, so this makes hand-written and dumped files agree rather
than making them differ.

**A brace-delimited value continues until its braces close.** While the
brace depth is above zero the reader keeps taking physical lines,
stripping the leading whitespace of each and joining them. A file that
ends with braces still open is refused, naming the line where they
opened.

This does not weaken *the first word of a line is always a keyword*
(issue [607](completed/607-no-reserved-words.md)). A continuation is not
a new line — it is the same logical line, still being assembled, and the
keyword rule was always about logical lines.

**A physical line that does not end is refused**, naming the line and the
limit rather than splitting it. The limit stays what it is; what changes
is that reaching it is an error instead of a silent second line. With
continuations in place this only fires on a single token longer than the
buffer, which is a file nobody meant to write.

## Suggested implementation steps

1. In the map parser's read loop, check that each buffer fill ended in a
   newline (or at end of file) and refuse when it did not, naming the
   line number and the byte limit.
2. Track brace depth across the assembly of one logical line. While it is
   above zero, read the next physical line, strip its leading whitespace,
   and append. Refuse at end of file with the opening line's number.
3. In the value reader, refuse an unquoted word where a string is
   expected, with a message that names both spellings — the wire form and
   the constant form — because the mistake is almost always one for the
   other.
4. Tests: a struct value spread over four lines loads and dumps back
   identically; a bare word is refused; an unterminated line is refused
   with the right line number; an unclosed brace at end of file is
   refused naming where it opened.

## Related

- [601 — Map file parser](completed/601-map-file-parser.md), whose read
  loop this tightens
- [607 — No reserved words](completed/607-no-reserved-words.md), whose
  keyword rule the continuation must not weaken
- [703 — Map dump](completed/703-map-dump.md), which already writes the
  strict form of all three
- [008 — Map file format](../docs/008-map-file-format.md), which
  documents the loose forms and has to stop
