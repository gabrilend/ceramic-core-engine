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
station reader io.c:read_config p
  in 0 x64 $0
  out 0 - config.0

station config io.c:load p
  in 0 - reader.0
  in 1 -
  in 2 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }
  out 0 - adder.0

station adder math.c:add p
  in 0 - config.0
  in 1 = 5
  out 0 - printer.0
  out 0 - logger.0

station depth compare.c:measure c
  in 0 - adder.0
  in 1 = 100
  out 0 - shallow.0
  out 1 - exact.0
  out 2 - deep.0

station split route.c:spread i
  in 0 - depth.0
  out 0 $0
  out 1 - mailer.0
```

Every wire appears twice — `reader` says its port zero feeds `config`,
and `config` says its port zero is fed by `reader`. Reading one station
tells you what it takes and what it gives without looking anywhere else.

`reader` takes the map's one argument and `split` produces its one
result, both marked with `$` on the port rather than on the station.

The stations this fragment sends values to and does not declare —
`printer`, `logger`, `shallow`, `mailer` — would each carry their own
`in` line in a whole file. It is showing the forms, not a program that
would load.

## Every line announces itself

Three kinds, and the first word of a line always says which:

| first word | the line is |
|---|---|
| `station` | a placement: a name, a box, and a kind |
| `in` | where one of that station's input ports gets its value |
| `out` | where one of its output ports sends one |

**A station line used to be what remained** — anything whose first
word was none of the others. That is a negative definition, and a
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

## A line, and a line that keeps going

**A value in braces may run across as many lines as it needs.** While
its braces are open the reader keeps taking lines, dropping the
leading whitespace of each, so a struct with several fields can be
written the way anybody would want to read one:

```
in 0 = {
    1.5,
    2.5,
    3.5
}
```

This does not weaken *the first word of a line is always a keyword*. A
continuation is not a new line — it is the same line, still being
assembled — and the keyword rule was always about the assembled line.
A file that ends with braces still open is refused, naming the line
where they opened.

**A line the reader cannot hold is refused rather than split.** There
is a limit on one physical line, and reaching it used to mean the tail
became a fresh line as far as the parser was concerned: every line
number after it wrong, and whether anything was noticed at all
depending on where the cut landed. With continuations in place this
only fires on a single token too long to hold, which is a file nobody
meant to write.

## Comments

A `#` starts a comment that runs to the end of the line — **unless it
is inside a quoted string**, where it is an ordinary character. The
same walk that finds the comment counts the braces, so the two cannot
disagree about where a string begins.

Added in the first build pass: the dump (issue 703) writes derived
facts — resolved types, element sizes, station indices — as comments
beside the lines that parse, and the format as originally written had
no way to carry them.

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

### A port may be one of the program's doors

```
in 0 $0        this port is the map's argument 0
out 0 $1       this port is the map's result 1
```

**`$` means: this crosses the map's boundary, at this position.** The
`in` or `out` keyword says which direction, so one notation covers both
ends and there is no second form to learn.

It used to point at a numbered entry in a `statics` section, which was
a second spelling of a constant. That section is gone, and the sigil
now means the one thing everybody guesses it means — it always read
like a shell positional while being nothing of the sort.

**A door is a port, not a station.** The mark used to sit on the
station line as `entry` or `result`, and because a station carries one
value inward — a C function returns one thing — a map taking three
arguments needed three stations running the identity function. Each of
those cost a station, a mutex, a ring buffer, and per value a task, a
dispatch, a call that returns its argument, a readiness check and a
second delivery. All of it was the mark having nowhere smaller to live.

**The number is the door's identity**, chosen by the author rather than
derived from where the line sits. Reorder every line in the file and
nothing changes; delete one and that argument is gone rather than
silently becoming what the next one was. Under the old scheme the order
was the order stations happened to sit in the table, so moving two lines
in a sub-map silently swapped two of the parent's arguments.

**A gap or a repeat is refused** when the program is brought up —
neither was detectable at all before. A station may hold ports of both
kinds: the old refusal, that a station could not be both doors, existed
because the mark was on the station and a station is one thing.

**The number means the same to whoever supplies the value** — a shell,
a C caller, or an enclosing map — the way a C function's first
parameter does not care who called it. That is what lets a map stand
where a box stands.

### What a program's arguments are

**The marked input ports that nothing feeds, in the order their numbers
say.** Derived rather than stored, which is what makes composition work
without bookkeeping: when an enclosing map wires into a sub-map's
argument port, that port stops being an argv slot on its own. There is
no mark to clear and nothing to go stale.

A port that is both marked and wired is fed both ways, and that stays
legal — being an argument is a fact about who *may* deliver, being wired
is a fact about what already does.

```
./program "{ 1.0, 2.0, 2.0 }" 21
```

Text becomes bytes through the same reader that turns `= 5` into a
value, so **struct arguments in brace syntax come along for free** —
nothing was built to make them work. A wrong count is refused saying how
many the program wanted. Quote a struct argument, because it has spaces
in it and the shell would otherwise split it.

### Where a program's results go

**Nothing is held unless somebody asks for it.** A marked output port
behaves exactly like any other unwired output — the value is discarded —
until an embedding caller registers an address and a count to put values
in. Registering is the arrow that was missing.

What that replaced: a marked station used to *hold* every value it
produced, in an array that doubled whenever it filled, shouting from the
first growth that results were piling up and nobody was taking them.
That warning was written instead of a fix.

**Results are not synchronised with each other.** Two results are two
stations on two threads at two unrelated moments. Two arrays filling
side by side look like columns of a table and are not: entry three of
one and entry three of another did not come from the same input. The
rule for anyone building on it is to **make the outputs fungible** — if
two values must stay together, they have to be one value.

**A program need no longer declare a result.** That requirement existed
so a program's interface would be *total*; an interface made of numbered
ports is total by being read, and a map with no result mark produces
nothing outward, which is a complete sentence.

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

**And the station at the other end says the same thing.** Every wire
appears twice, once on each end:

```
station adder math.c:add p
  out 0 - printer.0

station printer io.c:show p
  in 0 - adder.0
```

Reading one station now tells you the whole truth about that station.
Before this, an `in` line said only what a port *held*, never what fed
it, so learning where a value came from meant scanning every other
station in the file for an arrow that named this one — in a large map,
reading the whole file to understand one station.

**The loader refuses when the two disagree**, naming both stations and
saying which line to add. Four mistakes are told apart, because they
are four different things to have done: an arrow with no receiving end,
a receiving end with no arrow, two ends naming different ports, and a
source naming a station the map does not declare.

**The dash is the same dash.** It has always been an arrow, and the
keyword says which way it points — away on an `out` line, toward on an
`in` line. `in 3 -` keeps its meaning exactly: an arrow from nothing.

**Fan-in repeats the `in` line**, the way fan-out repeats the `out`
line. Two wires into one port are two lines with the same port number.

**A constant gains no counterpart.** `in 1 = 5` has no producing
station, so there is nothing to write on the other side. A port is fed
by exactly one of: a constant, one or more wires, or nothing.

**Nothing of this reaches the running program.** The second declaration
is checked while loading and then dropped. At run time a wire still
exists once, as a destination record on the producing station's output
port, because that is the only direction delivery ever asks about — and
two structures that can disagree is what the format is deliberately
paying for here, in the one place it can be paid off before anything
runs.

An arrow whose destination port holds a static **overwrites that
static** rather than queueing into a ring buffer — so a value can
become a constant the destination reads on every later invocation. It
does not make the destination run, because a static never gates
readiness. This is how a constant gets computed at startup instead of
being written here by hand, and **it is how a station remembers**: an
arrow from a station's own output back into its own static input port
holds whatever the last run returned. It is deliberately a property of
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

**A string written down is always quoted, and a bare word is
refused.** `in 0 = fire` and `in 0 - fire` differ by one character and
mean unrelated things — a constant, and a wire from a station called
`fire` — so a format that accepted both spellings of the first would
be putting two of them next to a third that means something else. The
dump has always written the quoted form; requiring it is what makes a
hand-written file and a dumped one agree.

**A command line is the exception, and it is not one.** An argument
arrives from a shell that already decided where the value started and
stopped, so `./program /etc/hosts` needs no further quoting. Nothing
is being relaxed there: the quoting happened, in the shell, which is
the only place that could have done it.

The reader and the writer share one table, so they cannot disagree
about what a backslash introduces.

## There is no statics table

A file used to be able to open with a numbered list of values that
ports pointed at by `$N`. It was **notation**: the engine kept no table
behind it, each value ended up on the port that read it, and two ports
naming one entry got two independent copies.

It went for two reasons. It was a second spelling of a constant that the
dump never wrote, so a hand-written file and a dumped one differed by
something that meant nothing. And `$0` sitting in a file whose other
keywords are English words read exactly like a shell positional while
being nothing of the sort — which is now what it means.

Every value is written on the port that reads it.

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
