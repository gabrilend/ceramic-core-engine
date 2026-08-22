# 008 — Map file format

A map is a text file. It names stations, says which box function each
one places, and draws the arrows between them. It carries no types and
no code — only names, numbers, and connections.

**It is a schematic, not a save file.** The file describes how to build
a structure in memory when the program starts; it is a map to a map. It
does not hold data that persists between runs, and nothing in the engine
writes back to it. A running map can be dumped to a new file on demand —
that is a deliberate act with a name, the program's equivalent of a
save-as — and the result is another schematic, describing whatever shape
the map had grown into by then.

## A complete example

```
statics
  0 = 5
  1 = 100
  2 = "config.txt"
  3 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }

station adder math.c:add p
  in 1 $0
  out 0 - printer.0
  out 0 - logger.0

station depth compare.c:measure c
  in 1 $1
  out 0 - shallow.0
  out 1 - exact.0
  out 2 - deep.0

station split route.c:spread i
  out 0 - poet.0
  out 1 - mailer.0

station reader io.c:read_config p
  in 0 x64 $2
  out 0 - config.0

station config io.c:load p
  in 0 $3
  in 1 -
```

## Every line announces itself

Four kinds, and the first word of a line always says which:

| first word | the line is |
|---|---|
| `station` | a placement: a name, a box, a kind, and optionally a door |
| `in` | where one of that station's input ports gets its value |
| `out` | an arrow leaving one of its output ports |
| `statics` | the header of the notation section |

**A station line used to be what remained** — anything whose first
word was none of the other three. That is a negative definition, and a
negative definition can only narrow: every keyword the format ever
gained would take another name away from every map already written,
silently, with the failure appearing as a parse error about something
else entirely. A station called `in` was told there was an input line
before any station.

So the first word is always a keyword and the second is always a name.
**No word is reserved.** A station called `in` is written
`station in keep p`; one called `station` is written
`station station keep p`; neither is a special case.

Indentation still means nothing. The attribute lines are indented
because it reads well, and a file that has been reflowed or pasted
still parses.

## Comments

A `#` starts a comment that runs to the end of the line. Added in
the first build pass: the dump (issue 703) writes derived facts —
resolved types, element sizes, station indices — as comments beside
the lines that parse, and the format as originally written had no
way to carry them.

## The station line

The keyword, then three words: the station's name, the box function it
places, and its kind.

```
station adder math.c:add p
```

`adder` is this placement.

**Three ways to say which box, and the path is not a fallback.** A
bare function name, a basename and a function, or a path and a
function:

```
station adder add p
station adder math.c:add p
station adder src/boxes/math.c:add p
```

A bare file name resolves when exactly one box source anywhere in the
tree is called that. If two are, resolution **refuses and names both
paths**, and the author writes one out in full. It is not a warning
and it does not pick one.

The longer form is a more specific way of saying the same thing rather
than a rescue, so nobody has to guess which is the real one, and an
author who prefers paths everywhere is not fighting the format.

**A bare name is not an address** — it is a name in a namespace nobody
wrote down. Naming the file makes it one, and it is the same
information a reader wants anyway when they go looking for what `add`
actually does.

**The dump writes whichever form is unambiguous**: the bare name when
it resolves to the same box, the whole address when it does not. That
is not tidiness — a box compiled while a program ran lives at a
serial-numbered path in a scratch directory belonging to *that*
process, so writing its address down would name somewhere nothing will
be next time. The bare name is what a later process can act on.

**The basename-first rule has a cost worth naming**: box source
basenames are a flat global namespace, so two files called `math.c` in
different directories cannot both be addressed briefly. That matches
what the project already does — the file index runs across the whole
tree rather than per directory, so a single global ordering of
filenames is already the model.
 `math.c:add` is the C function — the file
it lives in, a colon, and its name. `p`, `c`, or `i` is plain,
comparator, or iterator.

**The file is named because a bare function name is not an address.**
Two box sources may each define a function called `read`, and a map
saying only `read` would be trusting a global namespace nobody
declared. Naming the file makes provenance visible in the file a
person reads, and it is what lets the build know which sources a
program actually needs rather than compiling in every box it can find.

**A bare file name is tried first; a path settles ties.** `math.c`
resolves if exactly one box source anywhere in the tree is called
that. If two are, the reader refuses and names both paths, and the
author writes one of them out in full:

```
station adder src/boxes/math.c:add p
```

Both forms are legal at any time; the path is not a fallback but a
more specific way of saying the same thing. **The dump writes whichever
is unambiguous** — the bare name when it resolves uniquely, the full
path when it does not — so a dumped map always reloads into the
program it came from while staying as readable as it can be.

A collision that appears *while the program runs*, because a box
source arriving late shares a basename with one already loaded, does
not disturb stations already placed: those resolved their names when
they were placed. It makes bare references to that name ambiguous from
then on, and the next one refuses with both paths named.

Names rather than numbers, because a map is read by people and because
it makes an error message legible: *"adder → printer.0: box returns
int, port takes float."* The names cost one lookup table that is
discarded once loading finishes.

The kind is written rather than inferred. A comparator *is* inferable —
it is the station with one more input port than its function has
parameters — but that means forgetting the threshold line silently
demotes a comparator to a plain box that routes everything one way. One
letter of redundancy buys an error instead of a wrong answer.

## Input lines

**And an optional `@N` says where an iterator had got to.** An
iterator takes its exits in turn, and which one is next is **the one
memory a station keeps** — the only thing about a station that is
neither its shape nor a value sitting on a port.

```
station spread split i @2      the next value takes exit 2
station spread split i result @2      either order; both read the same
```

It is written only when it says something: zero is where an iterator
starts, and no other kind of station has a position to be in. A
capture that reset it would produce a file of exactly the right shape
whose next value went to an exit it was never going to — which is the
worst way for a written-down program to be wrong, because nothing
about the file looks incorrect.

**A port is a ring buffer unless a line says otherwise.** There are
four ways for a line to say otherwise, and all carry the port index:

```
in 1 $0        port 1 holds the value written at statics entry 0
in 1 = 5       port 1 holds 5
in 2 = { 1.5, 2.5, 3.5 }       and a struct is written the same way
in 3 -         port 3 has no source yet
in 4 x64 $1    port 4 reads statics entry 1, starting 64 slots deep
in 5 [7, 9]    port 5 is a buffer with two values waiting in it
```

So `split` above has no input lines at all — every port is an ordinary
ring buffer and there is nothing to say about them.

Port indices match the box function's parameter order, so the loader
can check that no line names a port the function does not have.

**The first two forms differ only in where the text comes from.** `$0`
fetches it from the statics section, which is notation for writing a
value once and pointing several ports at it; `=` carries it on the
line. Either way the text is parsed into that port's own storage, at
that port's own type, and nothing is retained afterwards.

The `=` matches the statics section's own `N = value`, so a value is
spelled the same way wherever it is written. It is the form **the dump
writes**, because a dump has values sitting on ports and no entry
numbers to point back at — inventing a numbered section to refer to
would be notation the engine made up rather than something a person
wrote.

The `$` is there because `in 1 0` reading as "static entry zero" is not
something anyone will guess a year from now.

**The fourth form says what the port is *holding*, not what it is.**
Every other line here describes the program's shape; this one describes
its contents, which is the difference between a schematic and an image
of a running program (issue 712). A buffer with work waiting in it
writes that work down, so the program can be picked up again rather
than only rebuilt.

Brackets rather than braces, because braces already mean a struct
value and a queue is a different kind of thing — several values where
a constant has one. The values are comma separated and a struct value
has commas inside it, so a reader finds the separator by reading one
value and seeing where it ended; there is no way to split the line
first and read afterwards.

**The order is the order the file gives, and it means nothing**, which
is worth saying plainly rather than leaving to be assumed. A port has
no head and no tail: [058](058-guarantees.md) promises nothing about
the order values leave one, because a reader takes whichever ready slot
it finds near a hint. A capture writes the slots as it finds them and a
revival puts them back that way, which is exactly as faithful as the
engine is. A file that appeared to promise an order would be inventing
a guarantee at the moment of being written.

**The third form is a port with no source at all.** A station can be
created before it is wired, so a port may be unconfigured — a state,
not a value, in which the station simply never becomes ready. The dump
writes those out rather than omitting them, because the dump's value is
that it says what is actually there, and a half-built program should
dump to a faithful record of a half-built program.

**It is a bare dash, and the dash was chosen over the alternatives on
what this format already says elsewhere.** No value here is ever a lone
dash, so it cannot be read as one; and the dash already means *wire* on
an out line, so `in 3 -` reads as a wire that is not there yet. The
word *none* was the runner-up and lost because it can collide with a
future type or box called `none`, and because every other exception
this format writes is punctuation rather than a word. Issue 210b.

**There used to be a second form** — `in 0 reader`, meaning this port
pulls its value from the station named `reader` when it is needed. The
pull path is gone, and with it that line. A station whose value another
station reads now simply has an arrow drawn to it, and if the
destination port is a static, the arrow overwrites it rather than
queueing.
[056](implementation-notes/056-no-pull-path.md) is why.

**A file still using that form is refused, not reinterpreted.** The
reader demands the dollar and says what the bare name used to mean, so
an old map stops with an explanation rather than loading into
something its author did not write. This matters more than it looks:
the two forms differ by one character, and the wrong one would have
loaded and run.

### What a station may be called

A name is an arbitrary label. **The engine never reads one** — every
wire is an index — and nothing mechanical anywhere depends on one. Two
stations in one program may share a name and nothing about the program
is worse for it.

The one place a name does any work is inside a *file*, because text
has no indices and an arrow written down has to say something. So two
rules follow, and both are about the file rather than about the
program:

**A file may not name two stations the same thing.** An arrow to
`gate` in a file with two `gate` lines cannot say which it means, so
the reader refuses it.

**Every name is available.** A station may be called `in`, `out`,
`statics` or `station`, because a name never occupies the position a
keyword occupies — see *Every line announces itself* above.

**And the dump makes names unique on the way out.** A program can
legitimately hold two stations called `gate` — bringing one
description inside another twice produces exactly that — and writing
it down needs labels the file can tell apart. The second one is
written with a suffix; nothing is lost, because a label carrying no
meaning can be spelled differently without the program changing. The
names on the running program are untouched.

### A station may be one of the program's doors

A fourth word on a station line says that this station is where the
outside delivers, or where the program's results come from:

```
station gate    keep      p entry     the outside delivers here
station answer  double_it p result    results wait here to be taken
station middle  add       p           an interior station, which is most
```

**`entry` and `result` rather than `in` and `out`**, and the reason is
worth stating because the obvious choice is the wrong one. Those two
words already name a *port* on the indented lines beneath a station. A
file in which one word means a port in one place and a whole station
in another reads perfectly well and round-trips wrong, which is
exactly the failure a depth followed by a dash produced before it was
given a form of its own.

A station may be one door or neither. Being both is refused: a program
whose entrance is its exit is somebody having named the wrong station.

**Every program has at least one `result`, and a file that names none
is refused.** Not because the engine needs it — a program with no
result station would run perfectly — but because without the
requirement there are two different ways to produce nothing, and only
one of them is legible. A program that does all its work by side
effect and a program whose author forgot the results look identical
from outside: a box is a C function, and nothing about it says whether
it touches the world.

The parallel is a C function returning void. It still declares its
return, and the declaration is what a caller reads. Here the station
is the declaration, and **what flows through it is a separate
matter** — a `result` station with nothing wired into it is the
ordinary way to say "this program produces nothing", and it is exactly
as valid as one carrying a value.

An entrance is not required, because a program that takes no arguments
is a complete thought. A program that produces nothing has to say so.

**A program may have several of each.** A box returns one value, so a
station has one output port, so one station is one result. A program
producing three things has three stations marked `result`, each with
its own inputs — which gets the readiness check that already exists
rather than needing a station whose ports come in groups.

**What the marks buy.** A parent composing this program wires to its
doors and never names anything inside it; rename an interior station
and nothing outside breaks. And two things that used to assume the
worst stop doing so — a station whose buffered inputs no arrow feeds
is no longer warned about when it is a declared entrance, and a
program in which nothing can start is no longer refused when it has
one, because both of those were written when there was no way for a
program to say it expected to be fed.

### What a program's arguments are

**The input ports of the stations it declared as entrances**, in
station order and then port order. A program with two entrances of one
port each takes two arguments, and a person reading the file can see
which is which.

Nothing in the file declares argument *types*, and nothing needs to:
an entrance runs an ordinary box, so its ports are typed by that box's
parameters. That keeps the format's standing rule that it carries no
types anywhere.

**Arguments arrive as text and become bytes through the same reader
that turns a constant into bytes** — the one that walks the field
table the generator emitted, using compiler-computed offsets. So a
struct argument is written the way a struct constant is:

```
./program '{ 1.0, 2.0, 2.0 }' 21
```

and the messages naming a field that was wrong are the same messages.

Two refusals rather than two silences. A count that does not match is
refused, saying how many the program wanted, because half a command
line is a program waiting forever for the rest. And a program that has
already finished is refused rather than handed arguments nobody will
run — which happens when nothing held a standing promise across the
moment the workers were released.

### A starting depth, which any of the three may carry

Every port's ring buffer starts with room for ten values and grows when
it needs to. A port may be told to start deeper, written **before the
source** as a count:

```
in 0 x64 $0      reads statics entry 0, room for 64 to begin with
in 1 x256 -      no source yet, room for 256 when it gets one
in 2 x8 = 5      a constant, and eight slots standing idle behind it
in 3 = 5         a constant, and the default ten
in 4 x64         a buffer, fed by arrows, 64 deep
```

**The last of those — a depth with nothing after it — says only how
deep.** The port's source is the default, arrows, so there is nothing
else for the line to say.

It had to exist, and the reason is worth keeping because it is a
round-trip failure of the kind that is easy to build and hard to
notice. The dump wrote a deepened buffer as `x64 -`, and a bare dash
means a port with **no source at all**. Two different things were
spelled the same way, so a program with a deepened buffer could be
written down and could not be read back — it returned with that port
unwired, and any arrow into it was then refused. The refusal was at
least loud; what it named was the wrong thing entirely.

It reads as *sixty-four of them*, the way a parts list writes a
quantity. Only `x` is accepted; a star was briefly allowed as a second
spelling and withdrawn, because one way to say a thing is the habit
everywhere else in this engine.

**Before the source rather than after, and the reason is mechanical.**
The `= value` form runs to the end of the line, because a struct value
has spaces in it, so nothing can follow it — a depth written after a
value would be part of the value. Putting the depth first gives one
rule that holds for all three forms, and one rule beats a rule with an
exception in it. This was written the other way round when the spelling
was chosen and corrected when it was built.

**A depth may sit beside any source form, including the dash**, because
the two are independent: every port owns ring slots whatever its tag
currently says. That standing buffer is what makes changing a port's
source a field write rather than an allocation, and it is decided in
issue 210b.

**The dump writes one only when it is not the default.** This format
writes exceptions, and a port at ten slots is not one; `x10` on every
line would be noise a reader learns to skip past.

**It is a hint and never a requirement.** Growth covers any figure that
turns out wrong, so nobody has to be right, and omitting it costs
nothing but a slightly slower start on a port that turns out to run
deep. What the old *ring buffers carry no capacity* rule was keeping
out of this file was a **required** tuning number an author had to get
right; an optional one they may supply is a different thing.

## Output lines

```
out 0 - printer.0
```

Port zero of this station delivers to port zero of the station named
`printer`. Repeat the line to fan out; one port may carry any number of
destinations.

An arrow whose destination port holds a static will, once issue 405
lands, **overwrite that static** rather than queue into a ring buffer —
so a value can become a constant the destination reads on every later
invocation. It does not make the destination run, because a static
never gates readiness. This is how a constant gets computed at startup
instead of written here by hand, and it is deliberately a property of
the arrow rather than of the box, so that it shows up in this file
instead of happening invisibly inside C.

Port numbers stay explicit because they mean something: a comparator's
ports are less, equal, and greater in that order, and an iterator's are
the sequence it walks.

## Strings, and what a backslash means

A string constant is quoted, and inside the quotes five characters are
spelled with a backslash:

| written | is |
|---|---|
| `\"` | a quote |
| `\\` | a backslash |
| `\n` | a newline |
| `\t` | a tab |
| `\r` | a carriage return |
| `\xNN` | the byte with that hexadecimal value, **exactly two digits** |

Everything below 0x20 and everything from 0x80 up is written in the
`\xNN` form, so **a map file is seven-bit ASCII end to end** and
cannot be damaged by anything that handles high bytes carelessly. The
price is that non-English text stops being readable in the file
itself; that is a real loss, taken deliberately, in exchange for a
format that never depends on the encoding of whatever tool is looking
at it.

**Two digits, always.** C's own `\x` consumes as many digits as it
can find, which makes `"\x41" "2"` and `"\x412"` mean different
things and one of them a compile error. Two digits covers every byte
and never runs on into the next character.

**Unquoted text is taken as itself**, which is what lets somebody
write `in 0 = config.txt` without ceremony — and is why a value that
needs escaping has to be quoted, since an unquoted backslash is just a
backslash.

The reader and the writer share one table, so they cannot disagree
about what a backslash introduces.

## The statics table

Entries are numbered and hold a value's shape. Nothing more.

```
statics
  0 = 5
  2 = "config.txt"
  3 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }
```

**The table carries no types.** A port that references an entry knows
what type it is, because the box function's parameter at that position
says so, and the generator knows what that is. The text is read into
bytes when the port is **bound**, walking the field table the
generator emitted for that struct — once, not on every claim. Text
resolves its layout when it is read, which is what makes it survive a
rebuild that would silently change what the same bytes meant; doing
that work per claim would pay for the property on every value instead
of once.

This is the same reason the wiring carries no types: if the table said
`int` where the box wanted `float`, there would be two sources of truth
and the file would be the one that was wrong.

**The section is notation and nothing else**: a way to write a value
once while describing the map, and point ports at it by number. It is
resolved as the file is read, and the running program holds no table.

That is worth stating plainly because it used to be otherwise, and the
difference is visible. Each port that names an entry gets **its own
copy** of that value, so two ports written from one entry are
independent from the moment they are written — changing one cannot
change the other, and neither can be shaped by the other's type. The
engine used to keep the table alive for the whole run, shared, behind a
mutex of its own, and two consequences followed that were never
features: two ports of different types could reference one entry and
read the same bytes each their own way, and a box could write to the
table, which was a back channel around "a box cannot remember" and, by
requiring a process-wide pointer to the running map, the reason a
process could hold only one program.

**Sharing, when it is wanted, is drawn.** One station holds the value
and everyone who needs it has an arrow from it. That costs a station
and gains visibility: a constant five stations read appears in the
wiring as five wires rather than as five references to a number that
appears nowhere in the shape of the map.

A dump therefore has no `statics` section. Every constant is written
out beside the port that holds it, from its bytes — including anything
a runtime write changed, which the old form could not say.

## What reading a map checks

**Reading a map happens in the generator, not in a running program.**
A map is a blueprint for the compilation: the generator turns it into
the construction calls it describes, so every failure below stops a
build while somebody is still looking at the file, rather than stopping
a program on a stranger's machine. Issue 311d.

All of these are fatal and all name the offending station:

- A box function no source provides, named with the file it was
  expected in.
- A bare file name matching more than one box source, with both paths
  named and the instruction to write one out in full.
- An arrow to a station or port that does not exist.
- More input lines than the box has parameters, or a port index out of
  range.
- A comparator whose box returns a type with no compare function.

**A wire whose two ends disagree about width is not checked here at
all, and that is deliberate.** Reading a map is how the build learns
which functions to compile in, and nothing more. Whether the shape
those functions are wired into is finished is a run-time question and
has to stay one, because **a half-wired map is a legitimate map** —
`in 2 -` is an ordinary state, and a program assembled one arrow at a
time is exactly what the construction surface exists to allow. A
reader that refused an unfinished map would refuse the program
somebody is in the middle of writing.

So a wire is checked **when it is drawn**: at startup for the ones a
map wrote, and at the moment of the edit for the ones a running
program draws. Same check, both times.

Two rules used to sit here and no longer can be stated: a box fanning
out to both a gatherer and a ring buffer, and a cycle among gather
wires. With nothing pulled, neither situation is describable. A cycle
in the push direction stays perfectly legal — it is how anything
repeats — and needs a finite companion input to ever stop.
## Related

- [007 — The build path](007-datapath-build.md), which is where this file is read and turned into code
- [009 — Loading](009-datapath-load.md), what happens to this file at startup
