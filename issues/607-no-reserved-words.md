# 607 — No reserved words

A station cannot be called `in`, `out` or `statics`. Found by naming a
door `in` and being told there was an input line before any station
([217](completed/217-a-program-inside-another.md)).

Low priority — the three words are unlikely names and the workaround
is to pick another. It is written up because **the limitation is a
symptom rather than the problem**, and the problem is worth naming
even if nobody ever spends the afternoon.

## Current behavior

**Built. No word is reserved.**

Every line announces itself: `station`, `in`, `out`, `statics`. The
first word of a line is always a keyword and the second is always a
name, so a station may be called `in`, `out`, `statics` or `station`
and none of them is a special case. A test builds a program whose four
stations are called exactly those, runs it end to end, writes it down
and reads it back — because a name that parses and does not round-trip
is a name that only half works.

**What changed is the definition rather than the list.** A station
line used to be *what remains*, and the three words were only the
symptom: a negative definition can only narrow, so every keyword the
format ever gained would have taken another name away from every map
already written, silently, with the failure appearing as a parse error
about something else. A line that begins with none of the four is now
refused naming all four, rather than being treated as a station and
failing further along.

**Indentation still means nothing**, which was worth keeping. The
attribute lines are indented because it reads well, and a file that
has been reflowed or pasted still parses.

### What stood before



A station line was defined negatively, and that is where the
limitation came from.

The reader looks at the first word of a line and dispatches:

| first word | the line is |
|---|---|
| `statics` | the header of the notation section |
| `in` | an input line, attaching to the station above |
| `out` | an output line, attaching to the station above |
| anything else | **a new station** |

The last row is the whole difficulty. A station line is *whatever is
not one of the other three*, so the set of legal station names is the
set of all words minus a list — and that list can only grow. Every
keyword the format ever gains takes another name away from every map
anybody has already written, silently, with the failure showing up as
a parse error about something else entirely.

The three words today are a small tax. **A definition that can only
narrow is the thing to fix**, not the three words.

## Intended behavior

**Every line begins with a word that says what kind of line it is, and
a name never occupies that position.**

```
statics
  0 = 5

station gate    keep      p entry
  in 1 = 1000
  out 0 - other.0

station other   double_it p result
```

A station named `in` is then written `station in keep p`, and a
station named `station` is written `station station keep p`, and
neither is a special case — the first word is *always* a keyword and
the second is *always* a name, so no word is ever both.

**What that buys is a definition that cannot narrow.** The grammar
becomes total: four line kinds, each announced, nothing defined as the
absence of the others. A keyword added later takes no name away from
anybody, because names do not live where keywords live.

**It costs one word on the least numerous line kind.** A map has more
input and output lines than station lines, so putting the marker on
the station line is the cheaper half — and the alignment it invites
makes a map easier to read down rather than harder.

### The alternatives, and why not

**Mark the attribute lines instead** — `  .in 1 = 1000`. Exactly as
total, and the cost lands on the more numerous lines. It reads like a
configuration format rather than like a description, which is the only
thing against it, and that is taste rather than an argument.

**Let indentation decide.** Attribute lines are already indented in
every map anybody has written, so a rule saying *column zero starts a
station* would cost nothing to adopt. It is rejected for adding
significance to whitespace: a file that survives being reflowed,
pasted, or emitted by a tool that trims is worth more than the
characters saved, and the format's own page currently promises that
indentation means nothing.

**Look at the second word.** An input line is `in` followed by a
*number*; a station named `in` is followed by a box name. One token of
lookahead disambiguates every case that exists today, at no cost to
any file. It is rejected for what it does to the rule rather than for
what it costs: *the first word decides* becomes *the first word
decides, unless*, and a rule with one exception is where a rule with
three exceptions comes from. It is the tempting cheap answer and it is
worth writing down as refused so nobody re-proposes it as new.

**Quote a colliding name** — `"in" keep p entry`. This does not remove
the limitation; it adds an escape hatch nobody learns about until they
have already hit the thing it escapes. The dump would additionally
have to know when to quote, which is a second rule in a second place.

## Suggested implementation steps

1. **Done.** The reader takes `station` as a fourth keyword and
   requires it, so the station line stops being the default case. A
   line beginning with none of the four is refused naming all four.
2. **Done.** The dump writes it.
3. **Done.** Every map text in the tests and under `maps/` carries it.
4. **Done.** A program whose four stations are called `in`, `out`,
   `statics` and `station` runs end to end and survives being written
   down and read back.
5. **Done.** The format's page gains the four line kinds as a table
   and says why the definition changed. The sentence about
   indentation stands, because it is still true.

## Open questions

**Answered: should the word be `station` or something shorter?**

`station`. It is the noun the whole project uses, so it needs no
explaining, and it aligns the three columns after it. `at`, `box` and
`s` were considered and each would be a second word for a station,
invented for the one file a person actually reads — which is worse
than the seven characters by a distance that does not depend on how
large the map is.

## Related

- [217 — A program inside another](completed/217-a-program-inside-another.md),
  where the collision was found
- [601 — The map file parser](completed/601-map-file-parser.md), whose
  dispatch this changes
- [703 — The map dump](completed/703-map-dump.md), which writes the
  other half
- [008 — Map file format](../docs/008-map-file-format.md), which this
  rewrites
