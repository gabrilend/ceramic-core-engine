# 712 — Capturing a running program

A program can be put down and picked up again: stopped where it stands,
written out whole, and restarted with every value back where it was.

This is not an extension of the dump. The dump produces a **schematic**
— what the program is shaped like — and this produces an **image**, in
the sense Smalltalk and Emacs use the word: a program frozen and
revived rather than described and rebuilt.

## Current behavior

**The dump writes the shape and nothing else.** It walks the live
station table — never any remembered file text — and writes stations,
box names, kinds, wiring, and the constants sitting on ports. The map
file format document says so plainly: *it is a schematic, not a save
file; it does not hold data that persists between runs.*

**What is therefore lost when a program ends:**

- every value sitting in a ring buffer, which is all the work in flight
- every task already built and waiting in the pool
- each iterator's cursor — the station header calls it *the one memory
  a station keeps*
- the statistics counters

So a program that has been running for an hour and has half its work
queued up can be described, and cannot be resumed.

## Why this engine can do what most cannot

The usual obstacle to capturing a running program is **hidden state**:
data scattered through static variables, open handles, buffers nobody
registered, all of it invisible to any walk of the program's own
structures.

**This engine forbids exactly that.** The first of its three nouns says
a box *is not permitted to remember anything between calls*. There is
no hidden state to miss, by construction — everything a program is
lives in the graph, and the graph is already walkable, because the dump
already walks it.

That rule was written for other reasons entirely. It turns out to be
what makes this feasible, and that is worth recording: **the constraint
that made boxes simple is the same one that makes a program
serializable.**

## Intended behavior

**Capture is drain-then-walk, with a bound.**

1. Stop building new tasks. The pool's brake already exists.
2. Let every task already running finish and deliver its result into
   whatever ports it feeds.
3. Wait for the pool to go quiet.
4. **If it does not go quiet, capture anyway**, and say what did not
   finish.
5. Walk everything: graph, constants, buffer contents, cursors.

**Draining rather than freezing is what keeps this tractable.** A task
in flight is a shape nothing else in the format describes — a copy of
its inputs, detached from the buffers they came from, plus which box
was about to run. Letting it land first means the captured state is
only ever *values sitting in ports*, which is bytes and a port index.

**But a program that cannot drain is exactly when a capture is worth
most**, so the wait is bounded rather than unconditional. Past the
bound, the artifact is written with a header saying it is incomplete,
naming every station still busy:

```
# INCOMPLETE CAPTURE
# 2 tasks were still running and did not finish:
#   parser  (station 7)
#   hasher  (station 12)
# Their input values are lost and their results were never
# delivered. Everything below is otherwise accurate.
```

**The incompleteness is stated, never inferred.** A reviving program
that met an incomplete artifact silently would be a program quietly
missing work somebody computed. This is the same rule the engine
applies everywhere: a fallback is a warning and a warning is an error,
so the artifact says what it lost and the revival says it out loud.

### The map file learns to carry queued values

A port that is a ring buffer with values waiting needs a form. The
format already has `in 1 = 5` for a constant sitting on a port; queued
values are the same idea, several deep and in order.

This is the one genuinely new piece of format vocabulary the whole
issue needs, and it should look like what it is — a port holding a
queue rather than a port holding a setting.

### Data file, or a new binary

**Both, and which one is not a preference — it follows from whether the
program grew code.**

- **No boxes were added while it ran** → the artifact is a **data
  file**, and the same binary reloads it. No compiler is involved at
  any point. This is the common case and it is the one that should be
  fast and unremarkable.
- **Boxes were added while it ran** → their C has to be compiled in for
  the artifact to stand alone, so the capture produces **a new binary**
  with those functions built in like any other box, their source
  embedded beside the rest.

That second case needs the toolchain, and it needs it precisely when
new code genuinely arrived — which is the rule
[310](completed/310-boxes-compiled-at-runtime.md) and
[311d](completed/311d-the-map-becomes-code.md) already set, arrived at
independently. A capture does not make the toolchain a new dependency;
it inherits the one that was already there.

### A report rides along

Separate from the artifact and meant for a person: what the buffers
held and how deep, which stations had run how many times, which box
functions were added or changed while it ran, and what did not drain.
The counters and buffer depths that
[701](completed/701-buffer-growth-reporting.md) and
[702](completed/702-station-statistics.md) already gather are most of
it, and [106](completed/106-stopping-on-purpose.md) already writes something of
this shape on the way out of a dying program.

## Suggested implementation steps

1. The drain: stop building tasks, wait for quiet, with the bound and
   the list of what was still busy. Most of this is
   [106](completed/106-stopping-on-purpose.md)'s sequence with a different ending.
2. The map format's queued-value form, in the reader and the writer
   together.
3. Capture of buffer contents and iterator cursors, appended to what
   the dump already writes.
4. Revival: a load that fills ports rather than leaving them empty, and
   that refuses loudly on an artifact marked incomplete unless the
   caller says it accepts one.
5. A round trip with values in flight: run a program until its buffers
   are deep, capture, revive, and prove the second run produces exactly
   what the first would have.
6. The binary-producing path, for a program that added boxes: their
   source compiled in like any other box, and a test that the revived
   binary needs no toolchain of its own.
7. The report, drawn from counters that already exist.

## Open questions

- **How is the drain's bound expressed?** A clock is the obvious answer
  and this project has refused one before — the reasoning that keeps a
  timeout out of the shutdown path applies with equal force here, since
  any number is wrong on a faster machine and wrong differently on a
  slower one. A count of idle sweeps, or a number of times the pool was
  observed unchanged, may be the honest form. Undecided.
- **Are the statistics counters restored or reset?** A revived program
  that reports a million runs has not run a million times in this
  process, and a revived program reporting zero has thrown away the
  history the capture existed to keep. It may want both numbers,
  which means the report gains a column rather than the engine gaining
  a decision.
- **Does a revived program keep its identity for the transcripts and
  the demos?** Two programs revived from one artifact are the same
  program twice, and nothing currently says whether that matters.
- **What does an incomplete artifact do to the round-trip guarantee?**
  Capture, revive, capture is not expected to produce identical text
  when the first capture lost work, and the property should be stated
  as holding only for complete captures rather than quietly failing for
  the other kind.

## Related

- [703 — Dumping the loaded map](completed/703-map-dump.md), the
  schematic this stands on and does not replace
- [106 — Stopping on purpose](completed/106-stopping-on-purpose.md), whose
  stop-the-world sequence this reuses with a different ending
- [310 — Boxes compiled while the program runs](completed/310-boxes-compiled-at-runtime.md),
  which is why a captured program may contain code the binary does not
- [311c — Source rides in the binary](completed/311c-source-rides-in-the-binary.md),
  which already puts every box's text in the artifact
- [311d — The map becomes code](completed/311d-the-map-becomes-code.md), whose
  rule about when a toolchain is needed this inherits exactly
- [701 — Buffer growth reporting](completed/701-buffer-growth-reporting.md)
  and [702 — Station statistics](completed/702-station-statistics.md),
  which already gather what the report needs
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  the queued-value form
- [058 — Guarantees](../docs/058-guarantees.md), where the round trip
  is promised and will need qualifying
