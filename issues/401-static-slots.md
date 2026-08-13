# 401 — Static input values

## Current behavior

REOPENED. The capability is built and correct; where the value lives is
wrong, and that is what this issue now covers.

Built, in its own statics module: a numbered table of entries held on
the map behind one mutex. A static input is always full, never affects
readiness, and claiming copies without consuming — proven by a station
running fifty times off one entry, driven only by its buffer side, and
by an all-static station that delivery can never wake. Binding requires
the port to know its type, which only registry placement provides, so
hand-placed stations cannot bind statics.

One departure from the original design, reasoned in the first-pass
report: entries hold parsed bytes shaped by the first port that binds
them, rather than text re-parsed at every claim. Runtime mutation
(issue 405) writes bytes, and a table that is sometimes text and
sometimes bytes is two tables wearing one name.

**What is now wrong with it.** The table is state held by the map, and
map-level mutable state is what forces a running process to contain
exactly one map. It is also a second mechanism for something the wiring
already expresses: one value read by several stations is a thing the
graph can say, by putting the value on a station and having everyone
gather from it, where a reader can see it.

## Intended behavior

**A static value belongs to the input port that reads it.**

A station is one instantiation of a box, wired its own way. Its input
ports are its own — one may be fed by a wire from upstream, another may
hold a value that is simply always there, a third may pull from a
gatherer. Which of those a port is, and what it holds, is a property of
that port on that station and of nothing else.

**Nothing is shared, so nothing needs a table.** Binding a port to an
entry in the map file's `statics` section copies that value into the
port. From that moment the entry has done its job; the running map holds
no statics table at all.

**Claiming happens under the station's own mutex**, alongside the
buffered values, in the same window rather than after it. This is
strictly better than what exists: one lock instead of two, and the
static half of an input set becomes as mutually consistent as the
buffered half already is.

**A static port is always full and is never consumed.** Both properties
survive unchanged — they are the reason the kind exists, and the
readiness dispatch keeps the row that answers "yes" without looking.

**Sharing, when it is wanted, is drawn.** One station holds the value;
everyone who needs it gathers from it. This costs a station and gains
visibility: a constant that five stations read appears in the wiring as
five wires rather than as five references to a number that appears
nowhere in the shape of the map.

**Something has to be able to turn bytes back into text.** The statics
table today keeps both the parsed bytes *and* the original string the
file gave it, and the dump writes that string back out. Once a value
lives on the port with no table and no retained text, the dump has
nothing to print — so it needs a formatter that walks a field table and
produces text, which is the exact mirror of the reader that walks a
field table and produces bytes.

That reader exists; the writer does not, and nothing has needed it
before. It is one piece of work with more than one caller: the dump,
anything showing a value to a person, and eventually a program's
results, which are text for the same reason a static is — text
resolves its layout when it is read, so it survives a rebuild that
would silently change what raw bytes meant.

**A hazard stops being expressible.** Today an entry's bytes are shaped
by the type of whichever port binds it first, so two ports of different
types may point at one entry and each read those same bytes their own
way — a footgun that currently exists as a warning in
[008](../docs/008-map-file-format.md) that a reader has to know about.
Once each port parses the file's text into its own storage at its own
type, there is nothing shared for two ports to disagree about. The rule
does not get better documented; it stops being a rule.

**Statics remain slightly discouraged.** They are how a comparator gets
a fixed threshold and how a read box gets a path. A map carrying a great
deal of its behavior in constants is a map whose behavior is not visible
in its wiring — which is the same argument that moves them onto ports.

## Suggested implementation steps

1. Take the port's own storage from
   [210b](210b-the-port-record.md), which provides it — this issue
   spends that room rather than building it, so binding becomes a copy
   into the port rather than an index into a table.
2. Move the claim into the readiness walk, under the station's mutex,
   beside the ring-buffer pop. The claim dispatch table's static row
   stops being an absence and becomes an ordinary copy, which is one
   of the two nulls that record replaces.
3. Have the reader copy from the file's numbered entries into each
   binding port as it goes, retaining nothing afterward — one call on
   the construction surface per bound port, the same call a debugger
   would make ([212](212-one-way-to-build-a-program.md)).
4. Remove the table, its mutex, and its teardown from the map.
5. The bytes-to-text formatter, walking a field table the way the
   reader does in the other direction, with the dump as its first
   caller — because the moment step 4 lands, the dump has no retained
   string to print.
5. A test that two ports bound to one file entry are genuinely
   independent afterwards — writing one does not disturb the other,
   which is the observable difference from today.
6. A test that the existing behaviors survive: a station driven only by
   its buffer side reads the same static value every run, and an
   all-static station is never woken by delivery.

## Related

- Issue 405 — changing a static value while the program runs, which
  moves with it
- Issue 402 — struct constants, whose parsing is unaffected
- [002 — Stations and slots](../docs/002-stations-and-slots.md)
- [008 — Map file format](../docs/008-map-file-format.md), where the
  `statics` section becomes explicitly a notation for initial values
- [058 — Guarantees](../docs/058-guarantees.md), where the consistency
  window this narrows is recorded
