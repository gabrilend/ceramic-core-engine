# 608 — The station line reads at a glance

A station line is four bare words separated by spaces, and two of the
four are hard to see: the kind is one letter at the far end, and the
box function is a word that looks like every other word on the line.
Both are fixed by the same rewrite — **the kind moves into the
keyword, and the box function gets brackets around it.**

```
                          before                    after

plain        station feed cycle_thirty p        station    feed      (cycle_thirty)
comparator   station under_ten keep c           comparator under_ten (keep)
iterator     station split route.c:spread i @2  iterator   split     (route.c:spread) @2
```

## Current behavior

**Built.** The kind is the first word of a station line, spelled out, and
the box function is bracketed. Five kind words parse — `station`,
`comparator`, `comp`, `iterator`, `iter` — and the two writers emit only
the three long ones.

**There turned out to be two writers, not one.** The generator's map
writer and the engine's own dump each had a copy of the kind letter and a
copy of the station-line format. Nothing tied them together, so changing
one left the other emitting the old shape — which surfaced as a map the
engine had just written being refused by the reader that had just loaded
it. Both carry the new form now; the duplication itself is untouched, and
is the sort of thing that will bite again.

**The old form is refused, and one refusal covers the whole shape.** A
line written the old way is wrong in two places at once — a bare box and
a trailing letter — and the bare box is what the reader meets first. The
box check looks one word ahead, so `station a keep p` is told its kind
moved to the front rather than being told about brackets and then about
the letter on a second attempt.

**A tool migrates a file wherever its map text lives**
(`scripts/141-station-lines.lua`): a `.map` file, a fenced example in a
markdown document, and a C string literal inside a test are all the same
rewrite, because the tool unwraps a literal before migrating and wraps it
again afterwards. One pass, no state between lines, and a line already in
the current form does not match the old shape at all — which is what
makes it safe to run twice. It has a `--check` mode.

Four places the tool could not reach were done by hand: map text built
mid-expression inside `fputs` and `printf` calls, and one assertion
comparing against dumped output.

**The wire-migration tool learned the new shape too.** It recognises a
station line in order to know where to insert the `in` lines a station is
owed, and it matched only the word `station`. Left alone it would have
gone on finding plain stations and silently skipping every comparator and
iterator — with `--check` reporting the file clean.

## Intended behavior

### The keyword carries the kind

A line is its kind word, then a name, then a bracketed box, then at
most an iterator's position:

```
station    feed      (cycle_thirty)
comparator under_ten (keep)
iterator   split     (route.c:spread) @2
```

| first word | the line is |
|---|---|
| `station` | a plain placement — runs its box, sends the result to every wired exit |
| `comparator`, `comp` | a placement that asks less / equal / greater against its last port and takes exit 0, 1 or 2 |
| `iterator`, `iter` | a placement that takes its wired exits in turn |
| `in` | where one of that station's input ports gets its value |
| `out` | where one of its output ports sends one |

**This spends the rule from
[607](607-no-reserved-words.md) rather than weakening it.**
That issue made the first word of a line always a keyword and the
second always a name, and the reason it gave for paying a word on
every station line was that a grammar where names never live where
keywords live can gain a keyword later without quietly taking a name
away from every map already written. This is the first change to
collect on that promise, and it collects in full: a station called
`iterator` is written `station iterator (keep)`, a comparator called
`station` is written `comparator station (keep)`, and neither is a
special case. The keyword set grows from four to six; the set of legal
station names does not shrink by one.

**The trailing letter is refused, not accepted alongside.** Keeping
both would leave two spellings of one fact, which is what
[601b](601b-the-dollar-sign-means-the-boundary.md) removed
when it deleted the `statics` section and
[601c](601c-the-format-stops-guessing.md) removed when it
made strings always quoted. The refusal names the replacement —
*a station's kind is the first word of its line now; write
`comparator` in place of the trailing `c`* — because every map written
before this change has a letter on every station line, and "unexpected
trailing word" would send its author hunting for a typo they did not
make.

### The box function is bracketed

**Brackets around the box address say *this is the code*.** They
delimit both ends, which is what the complaint needs: a space marks
where one word stops and cannot mark which word matters. `(keep)`
is not the same shape as `under_ten` even at a glance, even in a map
of thirty stations, even when the box is addressed by bare name.

They also make the line survive a change it cannot survive now. The
box address is the one field on the line whose spelling is open —
[108](../108-choosing-where-a-box-runs.md) contemplates saying more about
where a box runs — and a bracketed field can grow without the line
becoming ambiguous, where a bare positional field cannot.

**Brackets are required, not optional.** A form that is legal both
ways is a form the dump has to choose between and a reader has to
recognise twice. An unbracketed third word is refused naming what to
write instead.

**Brackets do not make the box address into a quoted string.** What is
inside is the same address it was — bare name, basename and function,
or path and function — with the same resolution rules and the same
refusal when a basename is ambiguous. The brackets are punctuation on
the line, not a new notation for what is inside them.

**An unclosed bracket is refused, on its line, by position.** This is
the rule braced values already have
([601c](601c-the-format-stops-guessing.md)) with one
difference worth stating: a braced value may continue across lines and
a bracketed box may not. A station line is one line. There is no
station whose box address is long enough to want wrapping, and
allowing it would put a continuation rule in a second place for no
gain.

### What both changes cost

**The columns go ragged.** The three kind words are seven, ten and
eight characters, so a file mixing kinds no longer has its names
starting in one place. [607](607-no-reserved-words.md)
argued that the `station` keyword aligned the columns after it for
free; it did that by being one word, and it is three now. The writer
emits one space and does not pad — see the open questions. A person
writing by hand may align a file however they like, since indentation
and spacing still mean nothing.

**Every map in the tree changes**, including the ones that live as C
string literals inside tests. That is what step 6 is for.

### The alternatives, and why not

**Leave the letter and document it better.** The letter is not
ambiguous, only terse. Rejected because terseness in the column that
moves is the actual complaint, and no amount of documentation moves a
field from the end of a line to the front of it.

**Put the kind second — `station comparator under_ten (keep)`.** Keeps
one keyword and spells the kind out. Rejected because it puts a word
that is not a name in the position 607 reserved for names: the second
word would sometimes be a kind and sometimes a station's name, which
is exactly the *the first word decides, unless* shape that issue
refused.

**Trailing empty parentheses — `station adder math.c:add()`.** Reads
like a C function and is one character cheaper. Rejected on two
counts: it marks only the end of the field, where the complaint is
that the start of it is invisible; and empty parentheses say *takes no
arguments* to every C reader, which is false of nearly every box in
the project.

**A sigil on the name — `under_ten:c`.** Shorter than any of this.
Rejected because it makes the kind a decoration on a name rather than
a statement, and the project already spends a colon on box addresses,
where it means something else.

## Suggested implementation steps

1. **The keyword dispatch gains the kind words.** In the map file
   parser's line dispatch — the loop that looks at the first word and
   chooses a handler — the three kinds and their two abbreviations map
   to a station kind and reach the station handler with that kind
   already decided. A word-to-kind table rather than a chain of string
   comparisons, so a fourth kind is a row.
2. **The station handler stops reading a kind and starts reading
   brackets.** It takes the kind as an argument and reads a name, a
   bracketed box address, and at most one `@N`. Reading the bracketed
   field is its own small routine, because the refusals belong to it:
   no opening bracket, no closing bracket, nothing between them.
3. **A bare trailing `p`, `c` or `i` is refused by name**, and so is
   an unbracketed box address, in the same place that already refuses
   `entry` and `result` for having moved to the port.
4. **The `@N` refusal stops naming a letter.** It says only kind `i`
   takes its exits in turn; it should name the `iterator` keyword.
5. **The map writer emits the keyword and the brackets.** One space
   between fields, no padding — the kind word is whatever length it
   is.
6. **A migration tool, not a hand pass.** Modeled on the one that
   wrote the receiving end of every wire — same shape: read a map,
   move the letter from the end of each station line to the front as a
   word, bracket the box address, leave every other line untouched,
   safe to run twice, with a `--check` mode that writes nothing and
   exits non-zero if anything would change. It has to reach the map
   texts that live as C string literals inside the tests as well as
   the files under `maps/`, which is the harder half and the reason
   the earlier tool is worth reading before starting.
7. **The wire-migration tool learns the new station line.** It
   recognises a station line in order to know where to insert the `in`
   lines a station is owed, so its pattern for one has to change in
   the same pass. If it does not, it silently stops finding stations
   in migrated maps and writes nothing — the worst shape of failure
   this format has, because its `--check` mode would report clean.
8. **Tests, and everything that runs.** Each of the five kind
   spellings loads and places the right kind; a map survives dump and
   reload unchanged; a trailing letter is refused with the message
   that names the replacement; an unbracketed box is refused; an
   unclosed bracket is refused naming the line; the reserved-word
   test's list of station names widens to include `comparator`,
   `iterator`, `comp` and `iter`, running end to end and
   round-tripping. Beyond the new tests, **every existing test that
   carries map text has to be migrated and pass**, along with the
   phase demos and the README — the build being green is the actual
   finish line here, not the parser accepting the new form.
9. **The format page rewrites its station-line section**, drops the
   legend sentence, and gains the three keyword rows.

## Related documents and tools

- [008 — Map file format](../../docs/008-map-file-format.md) — the
  station line section and the line-kind table both change
- [607 — No reserved words](607-no-reserved-words.md) — the
  rule this spends
- [601 — Map file parser](601-map-file-parser.md) — the line
  dispatch and the station-line handler
- [601b](601b-the-dollar-sign-means-the-boundary.md),
  [601c](601c-the-format-stops-guessing.md) — the precedent
  for refusing an old form by name, and the brace rules the bracket
  rules answer to
- [502 — Comparator](502-comparator.md),
  [504 — Iterator](504-iterator.md) — what the two words
  mean at run time; neither changes
- [703 — Map dump](703-map-dump.md) — writes the other half
- [108 — Choosing where a box runs](../108-choosing-where-a-box-runs.md) —
  open, and the reason the box field is worth bracketing before it
  grows
- The wire-migration script under `scripts/` (the "both ends" tool) —
  the model for step 6, including how it reaches map text inside C
  string literals

**To find every line this touches**, rather than trusting a count that
will go stale: `grep -rn "^ *station " maps/ tests/ docs/ README.md`
from the project root, plus the kind-letter function in the map writer
and the station-line handler in the map parser.

## Open questions

**Answered: the short spellings exist, and the dump writes the long
ones.** All five kind words parse — `station`, `comparator`, `comp`,
`iterator`, `iter` — and the writer only ever emits `station`,
`comparator`, `iterator`. A person typing a map by hand types the kind
on every station line, which is where the saving is; a program reading
one back never sees a short form, which is where the single spelling
matters. The two are not a *different notation*: no file means
anything different for having `comp` in it, which is what separates
this from the second spellings the format has deleted before — the
`statics` section pointed at a value by number and a bare word could
be read as either a wire or a string, and both of those changed what a
file meant.

**Answered: `station` is the plain kind.** The unmarked case gets the
unmarked word, and the two kinds that route rather than run are the
ones that announce themselves. The word does double duty — it is the
noun for all three kinds and the name of one — and that is accepted
rather than overlooked: `plain` as a fourth word would be a second
spelling with nothing behind it, and refusing `station` outright would
change every plain line in the tree for a distinction only a document
ever needs to draw.

**Answered: round brackets.** `(keep)`, `(math.c:add)`,
`(src/boxes/math.c:add)`. The thing inside is a C function and round
brackets are what a function wears; nothing else in the map format
uses them. The objection that a C reader might hear *takes no
arguments* applies to trailing empty parens, which is the form this
rejects, not to a pair wrapped around a name.

**Answered, with a condition: the old form is refused at once.** A
trailing `p`, `c` or `i` and an unbracketed box address are both
errors that name the replacement, with no period where the two
grammars both parse. The condition attached to the answer is that the
migration has to reach **the tests and the map texts written as C
string literals inside them**, not only the files under `maps/` — the
half of step 6 that is easy to leave for later is the half that makes
the build stop.

**Answered: no padding for now.** The writer puts one space between
the kind word and the name, and the name column comes out ragged in a
file that mixes kinds. Columns are worth something to read but not
worth a writer that measures a whole file before it starts emitting,
and the ragged form is the one that can be tidied later without
anything already written becoming wrong.
