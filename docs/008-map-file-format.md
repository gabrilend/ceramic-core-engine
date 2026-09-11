# 008 — Map file format

A map is a text file. It names stations, says which box function each one
places, and draws the arrows between them. It carries no types and no
code — only names, numbers, and connections.

**It is a schematic, not a save file.** It describes how to build a
structure in memory when the program starts. Nothing in the engine writes
back to it. A running map can be dumped to a new file on demand, and the
result is another schematic, describing whatever shape the map had grown
into by then.

## A complete example

```
station reader (io.c:read_config)
  in 0 x64 - 0$
  out 0 - config.0

station config (io.c:load)
  in 0 - reader.0
  in 1 -
  in 2 = { 5, 2.0, { 0, 0, 0 }, "hey there", 2 }
  out 0 - adder.0

station adder (math.c:add)
  in 0 - config.0
  in 1 = 5
  out 0 - printer.0
  out 0 - logger.0

comparator depth (compare.c:measure)
  in 0 - adder.0
  in 1 = 100
  out 0 - shallow.0
  out 1 - exact.0
  out 2 - deep.0

iterator split (route.c:spread)
  in 0 - depth.0
  out 0 - 0$
  out 1 - mailer.0
```

Every wire appears twice — `reader` says its port zero feeds `config`,
and `config` says its port zero is fed by `reader` — so reading one
station tells you what it takes and what it gives without looking
anywhere else. `reader` takes the map's one argument and `split` produces
its one result, both marked with `$` on the port rather than on the
station.

The stations this fragment sends to and does not declare would each carry
their own `in` line in a whole file. It is showing the forms, not a
program that would load.

## Every line announces itself

| first word | the line is |
|---|---|
| `station` | a plain placement — runs its box, sends the result to every wired exit |
| `comparator`, `comp` | a placement that asks less / equal / greater against its last port and takes exit 0, 1 or 2 |
| `iterator`, `iter` | a placement that takes its wired exits in turn |
| `in` | where one of that station's input ports gets its value |
| `out` | where one of its output ports sends one |

The first word is always a keyword and the second is always a name, so
**no word is reserved**. A station called `in` is written
`station in (keep)`; one called `comparator` is written
`comparator comparator (keep)`; neither is a special case. The format has
gained four keywords since that rule was made and took no name away from
any map when it did.

Indentation means nothing. A file that has been reflowed or pasted still
parses.

**A value in braces may run across as many lines as it needs.** While its
braces are open the reader keeps taking lines and dropping the leading
whitespace of each:

```
in 0 = {
    1.5,
    2.5,
    3.5
}
```

A continuation is not a new line — it is the same line, still being
assembled — so the keyword rule is untouched. A file that ends with
braces still open is refused, naming the line where they opened. A line
too long to hold is refused rather than split, naming the line and the
limit.

## Comments

A `#` starts a comment that runs to the end of the line — **unless it is
inside a quoted string**, where it is an ordinary character. The same
walk that finds the comment counts the braces, so the two cannot disagree
about where a string begins.

The dump writes derived facts — resolved types, element sizes, station
indices — as comments beside the lines that parse.

## The station line

**The kind, the name, and the box in brackets.**

```
station    feed      (cycle_thirty)
comparator under_ten (keep)
iterator   split     (route.c:spread) @2
```

`under_ten` is this placement. `(keep)` is the C function it runs, and
`(route.c:spread)` is one named in full — the file it lives in, a colon,
and the function within it.

**The brackets are required**, and they say *this is the code*. Every
other field on the line is a name somebody chose; this one exists on
disk. A space marks where one word stops and cannot mark which word
matters, which is why the delimiter is at both ends. An unbracketed word
where the box belongs is refused, and so is a bracket that does not close
— a station line is one line, and unlike a braced value it does not
continue onto the next.

**Spacing means nothing**, here as everywhere in this format. The columns
above are one author's taste; the dump writes one space between fields
and lets the name column go ragged, because aligning would mean measuring
a whole description before emitting its first line.

### Which box, and three ways to say it

```
station adder (add)
station adder (math.c:add)
station adder (src/boxes/math.c:add)
```

**A bare name is not an address** — it is a name in a namespace nobody
wrote down. Two box sources may each define a `read`. Naming the file
makes provenance visible in the file a person reads, and it is what lets
the build know which sources a program needs rather than compiling in
every box it can find.

**A bare file name resolves when exactly one box source anywhere in the
tree is called that.** If two are, resolution **refuses and names both
paths**. It is not a warning and it does not pick one. The path is a more
specific way of saying the same thing, not a rescue.

A collision appearing *while the program runs*, because a box source
arriving late shares a basename, does not disturb stations already
placed: those resolved their names when they were placed. Bare references
to that name are ambiguous from then on.

**The dump writes whichever form is unambiguous.** A box compiled while a
program ran lives at a serial-numbered path in a scratch directory
belonging to *that* process, so writing its address down would name
somewhere nothing will be next time. The bare name is what a later
process can act on.

**The cost**: box source basenames are a flat global namespace, so two
files called `math.c` in different directories cannot both be addressed
briefly. The file index already runs across the whole tree rather than
per directory, so a single global ordering of filenames is the model
anyway.

### The kind, and an iterator's position

The kind is written rather than inferred. A comparator *is* inferable —
it is the station with one more input port than its function has
parameters — but then forgetting the threshold line silently demotes it
to a plain box that routes everything one way. Saying it outright buys an
error instead of a wrong answer.

**Two spellings for each of the two routing kinds**, because a person
writing a map by hand types the kind on every station line:

| you may write | the dump writes |
|---|---|
| `station` | `station` |
| `comparator` or `comp` | `comparator` |
| `iterator` or `iter` | `iterator` |

The short forms are read and never written, so a file a program produced
has one spelling per kind while a file a person typed may have either.
They are not a second *notation* — no map means anything different for
carrying one.

**An optional `@N` says where an iterator had got to**, which is **the
one memory a station keeps**:

```
iterator spread (split) @2      the next value takes exit 2
```

Written only when it says something: zero is where an iterator starts,
and no other kind has a position. A capture that reset it would produce a
file of exactly the right shape whose next value went to an exit it was
never going to.

### What a station may be called

A name is an arbitrary label. **The engine never reads one** — every wire
is an index. Two stations in one program may share a name.

The one place a name works is inside a *file*, because text has no
indices and an arrow written down has to say something. So:

**A file may not name two stations the same thing.** An arrow to `gate`
in a file with two `gate` lines cannot say which it means.

**The dump makes names unique on the way out.** A program can
legitimately hold two stations called `gate` — placing one description
twice produces exactly that — and the second is written with a suffix.
Nothing is lost, because a label carrying no meaning can be spelled
differently without the program changing. The running program's names are
untouched.

Names rather than numbers, because a map is read by people and because it
makes an error message legible: *"adder → printer.0: box returns int,
port takes float."*

## Input lines

**A port is a ring buffer unless a line says otherwise.** Five ways to
say otherwise, and all carry the port index:

```
in 0 - feed.0        port 0 is fed by feed's output port 0
in 1 - 0$            port 1 is the map's argument 0
in 2 = 5             port 2 holds 5
in 3 = { 1.5, 2.5 }  and a struct is written the same way
in 4 -               port 4 has no source yet
in 5 x64             port 5 is a buffer starting 64 slots deep
in 5 x64 - feed.0    a depth and a source on one line
in 6 [7, 9]          port 6 is a buffer with two values waiting in it
```

Port indices match the box function's parameter order, so the loader can
check that no line names a port the function does not have.

**Everything after the dash is where this port's values come from.** A
station and a port name a wire; a number with a `$` says outside; nothing
at all says nowhere yet. No value in this format is ever a lone dash, so
a bare dash cannot be misread.

### A constant

**Written on the line that holds it, with `=`.** The text runs to the end
of the line, because a struct value has spaces in it, and it is parsed
into that port's own storage at that port's own type. There is no table
of values elsewhere in the file to point at.

Sharing one value between ports is drawn rather than written: one station
holds it and everyone who needs it has an arrow from it, which costs a
station and gains a wire somebody can see in the picture.

### A port holding values

**The bracket form says what the port is *holding*, not what it is.**
Every other line describes the program's shape; this one describes its
contents, which is the difference between a schematic and an image of a
running program.

Brackets rather than braces, because braces mean a struct value and a
queue is several values where a constant has one. Values are comma
separated and a struct value has commas inside it, so a reader finds the
separator by reading one value and seeing where it ended.

**The order is the order the file gives, and it means nothing.** A port
has no head and no tail: [058](058-guarantees.md) promises nothing about
the order values leave one. A capture writes the slots as it finds them
and a revival puts them back that way. A file that appeared to promise an
order would be inventing a guarantee at the moment of being written.

### A port with no source

A station can be created before it is wired, so a port may be
unconfigured — a state, not a value, in which the station never becomes
ready. The dump writes those out rather than omitting them: a half-built
program should dump to a faithful record of a half-built program.

**It is a bare dash**, which the dash already means on an `out` line, so
`in 3 -` reads as a wire that is not there yet.

### A starting depth

Every port's ring buffer starts with room for ten values and grows. A
port may be told to start deeper, written **before the source** as a
count:

```
in 0 x64 - 0$    the map's argument 0, room for 64 to begin with
in 1 x256 -      no source yet, room for 256 when it gets one
in 2 x8 = 5      a constant, and eight slots standing idle behind it
in 3 = 5         a constant, and the default ten
in 4 x64         a buffer, fed by arrows, 64 deep
```

It reads as *sixty-four of them*, the way a parts list writes a quantity.
Only `x` is accepted.

**Before the source, and the reason is mechanical.** The `= value` form
runs to the end of the line, so a depth written after a value would be
part of the value. One rule for all the source forms beats a rule with an
exception in it.

**A depth may sit beside any source form, including the dash**, because
every port owns ring slots whatever its tag says. That standing buffer is
what makes changing a port's source a field write rather than an
allocation.

**A depth says it for the chain below it.** The stations this port feeds
are sized to match, and the ones they feed, so an author who knows a
burst is coming says so once. Each kind passes it on the way its own
routing works: a plain station gives the whole backlog to **every** wire
on its output port, because fan-out duplicates rather than divides; an
iterator divides it among exits it takes in turn; and a **comparator
stops it**, because the data chooses which of its three exits fires and
any of them could take everything.

What a station passes on is the **minimum** over the ports that gate it:
fed a hundred from one side and ten from the other, it runs ten times.
**A wire carries it too**, so a station added to a running program is as
deep as one the file started with, and a file sizes itself.

**The dump writes one only when it is not the default**, and **it is a
hint, never a requirement** — growth covers any figure that turns out
wrong.

## A port may be part of the map's interface

```
in 0 - 0$        this port is the map's argument 0
out 0 - 1$       this port is the map's result 1
```

**`$` means: this crosses the map's boundary, at this position.** The
`in` or `out` keyword says which direction, so one notation covers both
ends. They are **input ports and output ports**; there is no separate
word for a marked one.

**The number is the port's identity**, chosen by the author rather than
derived from where the line sits. Reorder every line and nothing changes;
delete one and that argument is gone rather than silently becoming what
the next one was. Two stations in one map taking `0$` and `1$` is
ordinary and needs no coordination.

**Being an argument and being fed by a wire are independent.** A port
that is both is fed both ways and is not a command-line slot.

**A gap or a repeat is refused** at bring-up. Two ports both claiming
argument one is refused, and so is argument two with no argument one.

**A placed map's marks are its own, not the enclosing program's.** They
say *this port is a useful place to put values in or take them out of
this description*. So placing one description twice does not give the
parent two argument zeros, and ignoring a placed map's result costs
nothing. A part's marked ports are reached through the part.

**The dump renumbers them on collision**, the way it makes station names
unique: a file is one flat description with no parts in it, so two placed
copies would otherwise write two argument zeros. The author's own number
is kept wherever it is still free.

**The number means the same to whoever supplies the value** — a shell, a
C caller, or an enclosing map — the way a C function's first parameter
does not care who called it. That is what lets a map stand where a box
stands.

### What a program's arguments are

**The marked input ports that nothing feeds, in the order their numbers
say.** Derived rather than stored: when an enclosing map wires into a
sub-map's argument port, that port stops being an argv slot on its own.
There is no mark to clear and nothing to go stale.

```
./program "{ 1.0, 2.0, 2.0 }" 21
```

Text becomes bytes through the same reader that turns `= 5` into a value,
so **struct arguments in brace syntax come along for free**. A wrong
count is refused saying how many the program wanted. Quote a struct
argument, because the shell would otherwise split it.

### Where a program's results go

**Nothing is held unless somebody asks for it.** A marked output port
discards like any other unwired output until an embedding caller
registers an address and a count.

**Results are not synchronised with each other.** Two results are two
stations on two threads at two unrelated moments. Two arrays filling side
by side look like columns of a table and are not. **Make the outputs
fungible** — if two values must stay together, they have to be one value.

**A program need not declare a result at all.**

## Output lines

```
out 0 - printer.0
```

Port zero of this station delivers to port zero of `printer`. Repeat the
line to fan out; one port may carry any number of destinations.

**And the station at the other end says the same thing:**

```
station adder (math.c:add)
  out 0 - printer.0

station printer (io.c:show)
  in 0 - adder.0
```

**The loader refuses when the two disagree**, naming both stations and
saying which line to add. Four mistakes are told apart, because they are
four different things to have done: an arrow with no receiving end, a
receiving end with no arrow, two ends naming different ports, and a
source naming a station the map does not declare.

**The dash is the same dash** — an arrow, with the keyword saying which
way it points. **Fan-in repeats the `in` line**, the way fan-out repeats
the `out` line. **A constant gains no counterpart**: a port is fed by
exactly one of a constant, one or more wires, or nothing.

**Nothing of this reaches the running program.** The second declaration
is checked while loading and dropped. At run time a wire exists once, as
a destination record on the producing station's output port, because that
is the only direction delivery ever asks about — two structures that can
disagree is what the format pays for here, in the one place it can be
paid off before anything runs.

An arrow whose destination port holds a static **overwrites that static**
rather than queueing, so a value can become a constant the destination
reads on every later invocation. It does not make the destination run,
because a static never gates readiness. This is how a constant gets
computed at startup, and **it is how a station remembers**: an arrow from
a station's own output back into its own static input port holds whatever
the last run returned. It is a property of the arrow rather than of the
box, so it shows up in this file instead of happening invisibly inside C.

Port numbers stay explicit because they mean something: a comparator's
ports are less, equal, and greater in that order, and an iterator's are
the sequence it walks.

## Strings, and what a backslash means

| written | is |
|---|---|
| `\"` | a quote |
| `\\` | a backslash |
| `\n` | a newline |
| `\t` | a tab |
| `\r` | a carriage return |
| `\xNN` | the byte with that hexadecimal value, **exactly two digits** |

Everything below 0x20 and everything from 0x80 up is written in the
`\xNN` form, so **a map file is seven-bit ASCII end to end** and cannot
be damaged by anything that handles high bytes carelessly. The price is
that non-English text stops being readable in the file itself — a real
loss, taken in exchange for a format that never depends on the encoding
of whatever tool is looking at it.

**Two digits, always.** C's own `\x` consumes as many digits as it can
find, which makes `"\x41" "2"` and `"\x412"` mean different things and one
of them a compile error.

**A string is always quoted, and a bare word is refused.**
`in 0 = fire` and `in 0 - fire` differ by one character and mean
unrelated things — a constant, and a wire from a station called `fire`.

**A command line is the exception, and it is not one.** An argument
arrives from a shell that already decided where the value started and
stopped, so `./program /etc/hosts` needs no further quoting.

The reader and the writer share one table, so they cannot disagree about
what a backslash introduces.

## What reading a map checks

**Reading a map happens in the generator, not in a running program**, so
every failure below stops a build while somebody is still looking at the
file. All are fatal and all name the offending station:

- A box function no source provides, named with the file it was expected
  in.
- A bare file name matching more than one box source, with both paths
  named.
- An arrow to a station or port that does not exist.
- More input lines than the box has parameters, or a port index out of
  range.
- A comparator whose box returns a type with no compare function.

**A wire whose two ends disagree about width is not checked here at all,
and that is deliberate.** Reading a map is how the build learns which
functions to compile in, and nothing more. Whether the shape they are
wired into is finished is a run-time question, because **a half-wired map
is a legitimate map** — `in 2 -` is an ordinary state, and a program
assembled one arrow at a time is what the construction surface exists to
allow. So a wire is checked **when it is drawn**: at startup for the ones
a map wrote, at the moment of the edit for the rest.

A cycle in the push direction is legal — it is how anything repeats — and
needs a finite companion input to ever stop.

## Related

- [007 — The build path](007-datapath-build.md), where this file is read
  and turned into code
- [009 — Loading](009-datapath-load.md), what happens at startup
- [140 — A map inside a map](140-a-map-inside-a-map.md), what a placed
  map's marked ports mean
