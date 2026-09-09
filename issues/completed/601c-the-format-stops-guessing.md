# 601c — The format stops guessing

Three places where the map file reader accepts something it should
refuse, or refuses something it should accept. None of them is a large
change; all three are the same kind of mistake, which is why they are
one issue.

## Current behavior

**Built.** All three refusals are in place, along with a fourth mistake
the same walk turned out to be making.

**A string written down is quoted, and a bare word is refused** naming
the spelling it collides with: `in 0 = fire` and `in 0 - fire` differ by
one character and mean unrelated things.

**A command line is not a map file.** Text handed over by a shell
arrives as one whole argument with the quoting already done, so it is
taken as itself. The distinction is a parameter on the one
text-to-bytes routine both callers share, so there is still one reader
rather than two that can drift.

**A brace-delimited value continues across as many physical lines as it
needs**, leading whitespace of each continuation dropped. A file ending
with braces open is refused, naming the line where they opened.

**A physical line the reader cannot hold is refused, not split.** The
tail used to become a fresh line as far as the parser was concerned —
every line number after it wrong, and whether anything was noticed
depending on where the cut landed.

**And a `#` inside a quoted string is a character.** The comment scanner
used to search for the first `#` anywhere on the line, which quietly
truncated any value containing one. Both questions the reader has —
where a comment starts, and how much the braces opened — now come from
one walk that tracks the quote state, so they cannot disagree about
where a string begins.

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
