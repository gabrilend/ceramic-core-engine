# 401 — Static input values

## Current behavior

**Done.** A static value belongs to the input port that reads it, and
the table is gone.

Binding is one call naming a station, a port, and text; it parses at
the port's own registry type into the port's own storage. The parse
happens into scratch and is installed under the station's mutex, so a
malformed value never half-overwrites a working one and no concurrent
claim sees a value mid-parse. Claiming is a copy under the station's
mutex beside the ring pops — the claim dispatch table's static row was
a null meaning "resolved later, outside the lock", and it is now an
ordinary function, which is the last of the two holes issue 210b set
out to close.

The old behaviors survive and are still proven: a station running fifty
times off one constant driven only by its buffer side, an all-static
station that delivery can never wake, brace text becoming bytes
identical to a compiled initializer, and four malformed constants each
dying where they were given. Binding still requires the port to know
its type, which only registry placement provides.

**Three things came out with the table**, and each is now a test.

- Two ports written from one file entry are independent afterwards:
  writing one does not disturb the other. Under the table both read the
  same bytes, shaped by whichever bound first, so two ports of
  different types could read one value each their own way. That hazard
  stopped being expressible rather than being better documented.
- An invocation's inputs are all claimed in one window. A static used
  to be read after the station's mutex was released, because its value
  lived behind a second lock that could not nest inside the first — so
  a box reading two statics could get values that were never
  simultaneously true. That was a stated non-guarantee and is now
  guarantee T3.
- Two maps run in one process and do not see each other. Carried by
  issue 405, which the next section explains could not be separated.

**Issue 405 landed with this one and had to.** Removing the table
breaks the runtime write, which addressed an entry number in it; the
write has nowhere to live until the value is on a port. The two are one
piece of work and were attempted as one.

**Not done:** an arrow delivering into a static port, which
[004](../../docs/004-datapath-statics.md) describes and which is still
refused at load time and fatal at delivery. The write call it needs
exists and takes the right lock; what is left is teaching delivery to
call it and removing the load-time check. It belongs to 405.

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
[008](../../docs/008-map-file-format.md) that a reader has to know about.
Once each port parses the file's text into its own storage at its own
type, there is nothing shared for two ports to disagree about. The rule
does not get better documented; it stops being a rule.

**Statics remain slightly discouraged.** They are how a comparator gets
a fixed threshold and how a read box gets a path. A map carrying a great
deal of its behavior in constants is a map whose behavior is not visible
in its wiring — which is the same argument that moves them onto ports.

## Suggested implementation steps

1. ~~Take the port's own storage from
   [210b](210b-the-port-record.md), which provides it — this issue
   spends that room rather than building it~~ — **inverted, and the
   inversion is the useful record here.** 210b was blocked on this
   issue instead: a port cannot be given room for a value that lives
   somewhere else, and the map file's `$n` form names a table this
   issue deletes. Both could not be second. Whoever removes the table
   is the one who has to know what replaces it, so this issue builds
   the port-side storage it spends — a byte buffer beside the slots,
   allocated at placement, plus the characters a string constant points
   at. 210b's static half unblocked the moment it existed.
2. Move the claim into the readiness walk, under the station's mutex,
   beside the ring-buffer pop. The claim dispatch table's static row
   stops being an absence and becomes an ordinary copy, which is one
   of the two nulls that record replaces.
3. Have the reader copy from the file's numbered entries into each
   binding port as it goes, retaining nothing afterward — one call on
   the construction surface per bound port, the same call a debugger
   would make ([212](../212-one-way-to-build-a-program.md)).
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
- [002 — Stations and ports](../../docs/002-stations-and-ports.md)
- [008 — Map file format](../../docs/008-map-file-format.md), where the
  `statics` section becomes explicitly a notation for initial values
- [058 — Guarantees](../../docs/058-guarantees.md), where the consistency
  window this narrows is recorded
