# 210a — The pull path removed

First child of [210](../210-input-port-record.md), and first because
every other child is smaller once it is done. It removes a whole input
kind rather than adding one, which makes it the only piece of the
family whose product is an absence.

**The decision is not made here.** Why the pull path went, what it was
for, the three timings weighed before it went, and what its removal
costs are all in
[056](../../docs/implementation-notes/056-no-pull-path.md). This issue is
the demolition, not the argument for it.

## Current behavior

The engine has two ways a value reaches a box.

**Pushed.** An upstream station runs, delivery carries its result into
a ring buffer, and it waits there until claimed.

**Pulled.** A gatherer port holds no buffer. At the moment a task is
assembled, the thread doing the assembling reaches upstream and runs
the named station's box *inline, on its own stack*, so the value is
fresh at the moment it is used rather than at the moment it was
produced. A clock, a file, a knob somebody turns.

Everything below exists to make that second sentence true:

- **A kind tag** with a gatherer value, and a `source` field on every
  input port holding the upstream station index it pulls from — a
  field that means nothing on the other two kinds and sits on all of
  them.
- **A pull module** carrying the inline run, the recursion down a
  chain, and the depth measurement.
- **A cycle walk** at the moment a gather wire is drawn. A gather
  cycle is a call that never returns, so it would surface as a bare
  stack overflow with nothing said; refusing at wiring time is what
  buys a message naming both stations.
- **A second form of the map file's input line.** `in 0 $3` binds a
  static; `in 0 reader` gathers from the station named `reader`. One
  character apart.
- **A second pass in the loader** that exists partly for this: a
  gather source is a *name*, and a name may belong to a station
  declared further down the file.
- **Three whole-map validations**: a gathered-from station may not
  have ring inputs (nothing could fill them mid-gather), a station may
  not be both pushed into and gathered from, and the seed sweep skips
  anything gathered from.
- **A runtime operation** repointing a gather wire on a live map,
  carrying the cycle walk under the rewiring lock.
- **A timing** charged to the puller rather than the runner, because
  that is genuinely where the cost lands.
- **The engine's one named exception**: a box running without a worker
  having picked up a task for it.

## Intended behavior

**Everything is pushed. There is one way in.**

**The tag loses a value and the numbering closes up.** No hole is left
where the gatherer was. Nothing outside the station header ever saw a
slot kind as a number — the map file spells a kind as text and the
dump writes text back — so the values are an internal detail and
renumbering costs nothing.

**The `source` field leaves the record rather than sitting unread.** A
field nobody writes is a question every future reader has to answer
for themselves, and the answer is not in the code.

**An old map file is refused, not reinterpreted.** This is the part
worth building carefully. The two input-line forms differ by one
character, so a file written for the old engine would otherwise load
cleanly and run as a program its author never wrote. The reader
demands the dollar and, when it does not find one, says what the bare
name used to mean and that there is no pull path any more. A refusal
that explains itself is the difference between a person editing one
line and a person debugging a program that is quietly wrong.

**Three validations go, and what they were is recorded where they
were.** Each described a situation that is no longer describable.
Leaving a comment saying so is what stops somebody reintroducing them
later in search of lost rigour.

**The exception dissolves.** Every box in the engine now runs on a
worker that picked one up. This is the removal's largest quiet
benefit: it closes the only window in which user code ran while
claimed slots were held, which is what lets 210's answer about a
writer dying mid-copy rest on the engine as it is.

**One property is lost rather than relocated, and it must be written
down.** Runtime rewiring holds a single lock across validating an edge
and installing it, because two threads each adding an individually
legal edge could produce an illegal pair — two gather edges separately
fine and jointly a cycle. A test proved exactly one of two racing
threads was refused, every round, across fifty rounds.

After this, no two legal edges can combine into an illegal pair.
Every surviving rewiring rule is a property of one edge and the fixed
shape of one station: type match, port limits, destination-is-a-buffer,
not-wiring-from-a-sink. The lock still earns its place serialising
list surgery; it no longer defends a whole-graph invariant, because
there is not one. **The test is retired on purpose rather than
discovered missing**, with the reason left in the file it left, so it
returns if a whole-graph rule ever does.

**Three demonstrations stop compiling, and that is planned.** Phases
4, 6, and 7 call the removed operations. They cannot be rewritten
first — they are compiled against the engine as it exists — and
rewriting them last means the launcher demonstrates a machine that is
gone. [710](../710-demos-after-the-pull-path.md) exists to close that
window and cannot open before this lands.

## Suggested implementation steps

1. The station header first: the tag value, the `source` field, the
   gather timing counter, the map's chain-depth reading, and the two
   function declarations. Everything downstream then fails to compile,
   which turns the rest of this into a worklist the compiler writes.
2. Delete the pull module and its test outright.
3. Delivery's two dispatch tables lose a row each, and task
   construction loses the case that ran a box inline. The file's own
   header comment is worth correcting here rather than later: it
   claimed the dispatch shape meant a new input kind was a row rather
   than a restructure, and this is the evidence — removing one was a
   row too.
4. The map file reader: refuse the bare-name form with a message
   naming what it used to mean. The description struct loses the field
   that held the source name.
5. The loader: input lines all resolve in the first pass now, because
   an entry number needs nothing else to exist first. The second pass
   keeps only arrows. Delete the three validations and leave a comment
   saying what they were.
6. The runtime gather-repoint operation and its cycle walk, from both
   the rewiring source and the header that declares it.
7. The observation report: the gather column, and the ranking that
   added gather time to box time.
8. The dump: the chain-depth comment and the gathered-input line.
9. Tests: delete the pull path's own test and the joint-cycle race;
   rewrite the round-trip map's gathered input as a static, which
   lands the same value through the same round trip; and **add a
   refusal test for the old input-line form**, which is the one new
   test this issue produces.
10. Prose everywhere it is now wrong: the interface files beside each
    source, the demo boxes whose comments describe themselves as
    gatherable, and the map format document's account of the input
    line.

## What this issue does not do

It does not add the *none* tag, allocate slots at instantiation,
touch the claim, or change how a buffer grows. Those are
[210b](210b-the-port-record.md) onward. The port record after this is
the old record minus one kind, which is deliberately a small place to
stand.

## Open questions

None. The design question — whether to keep pulling — was settled in
[056](../../docs/implementation-notes/056-no-pull-path.md) before this
issue existed, and the only judgment made here was to refuse old map
files rather than reinterpret them, which is written up above.

## Related

- [056 — Why there is no pull path](../../docs/implementation-notes/056-no-pull-path.md),
  the reasoning this carries out
- [210 — What an input port is](../210-input-port-record.md), the parent
- [403 — Gatherer slots](403-gatherer-ports.md) and
  [404 — Gather chains and cycles](404-gather-chains-and-cycles.md),
  which built what this removes
- [407 — Gathering at pickup](407-gather-at-pickup.md), which had
  designed a better *when* for a thing that stopped existing
- [704 — Rewiring while it runs](704-runtime-rewiring.md), whose
  joint-cycle guarantee is the property lost here
- [710 — The demos after the pull path](../710-demos-after-the-pull-path.md),
  which repairs the three demonstrations this breaks
- [008 — Map file format](../../docs/008-map-file-format.md), where the
  input line's second form is recorded as gone
- [002 — Stations and ports](../../docs/002-stations-and-ports.md), where
  the port record is described
