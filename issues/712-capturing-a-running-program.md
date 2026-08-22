# 712 — Capturing a running program

A program can be put down and picked up again: stopped where it stands,
written out whole, and restarted with every value back where it was.

This is not an extension of the dump. The dump produces a **schematic**
— what the program is shaped like — and this produces an **image**, in
the sense Smalltalk and Emacs use the word: a program frozen and
revived rather than described and rebuilt.

## Current behavior

**A running program can be put down and picked up again.** Everything
it holds is captured and revived — the values waiting in every buffer, and each iterator's
place in its exits, which the station header calls *the one memory a
station keeps*. A dump writes
what each ring-buffer port is holding, in brackets, and reading it back
puts those values into the port through the ordinary delivery — so the
readiness check runs and the tasks that form are the tasks that would
have formed. A program with work in flight can be written down, picked
up in a fresh process, and go on from where it was.

Proven three ways: work waiting survives the round trip; capturing the
revived program produces the same text again; and a queue of struct
values survives with its inner commas intact, which is the case that
decides whether the format can be read at all.

**A gap in the engine surfaced doing it, and is closed.** A station
whose buffer had filled while another port was empty started exactly
one task when that other port finally filled, and stranded the rest.
The one rule says a station runs whenever every port holds a value, and
asking once is only enough when values arrive one at a time. Now the
question is asked until the answer is no — except of a station whose
every port is a constant, which is ready forever because a constant is
never consumed.

**What is still lost when a program ends:**

- every task already built and waiting in the pool, unless it drained
- the statistics counters
- boxes that arrived while it ran, if the artifact has to stand alone
  without a toolchain

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

1. **Done.** Two doors, because there are two situations and pretending
   otherwise would make one of them lie.

   The polite one shuts the entrance, lets everything in flight finish
   and deliver, waits for the workers to go home, and writes. What it
   produces is **complete by construction** — nothing was running when
   it was written.

   The other writes immediately, whatever is happening, and the
   artifact states what it lost: a header naming every station a worker
   is still inside, and the plain sentence that their inputs are gone
   and their results were never delivered.

   **Neither invents a bound**, which is the open question below
   answered by not needing an answer. It is
   [106](completed/106-stopping-on-purpose.md)'s sequence with a
   different ending, and it reuses what was already there: the entrance
   shuts, the pool drains to the last-sleeper rule, and the pool
   already knew which station each worker was inside.
2. **Done.** The waiting-value form, `in 5 [7, 9]`, in the reader and
   the writer together, and on the format's own page. Brackets because
   braces already mean a struct value; a queue is a different kind of
   thing.
3. **Done.** Each ring port is asked what it is holding, and each
   iterator where it had got to — written on the station line as `@N`,
   only when it says something, and refused on anything that is not an
   iterator because there is nothing for it to mean.
4. **Done.** Reading a description back puts the values into their
   ports through the ordinary delivery, so the readiness check runs and
   nothing is reconstructed.

   And an artifact marked incomplete is **refused through the ordinary
   door**, because a program quietly missing results somebody computed
   is the failure this engine refuses everywhere. There is a second
   door for salvaging one, with a different name, so that whoever uses
   it has said out loud that they know what is missing. The question is
   asked of the *file* rather than of the description, because the
   marker is a comment and a comment is by definition not part of what
   a file says about a program.
5. **Done.** Work
   waiting survives the round trip; the revived program finishes the
   work it was carrying and produces the answers the first would have;
   a queue of struct values survives with its inner commas intact; and
   an iterator comes back pointing where it had got to.

   Three more scenes came out of it, pinning the readiness rule that
   the revival exposed — thirty values draining when a constant
   arrives, a constant written its own value still counting, and a
   station of only constants running once per change rather than
   forever.
6. The binary-producing path, for a program that added boxes: their
   source compiled in like any other box, and a test that the revived
   binary needs no toolchain of its own.
7. The report, drawn from counters that already exist.

## Open questions

**Answered: how is the drain's bound expressed?** **It is not. The
bound belongs to whoever asked for the capture, and the reasoning was
already written down next door.**

The waiting path says it plainly: *it borrows a clock rather than
inventing one*, because whatever sent the signal already has a timer,
and that supervisor's clock is the only one in the system that knows
how long is too long for this deployment. A guess made in the engine is
wrong on a slow machine and wrong differently on a fast one.

Nothing about a capture changes that. "Quiet" is decidable exactly —
the pool knows when the queue is empty and every worker is asleep — so
the only unbounded case is a box that never returns, and no signal
inside the process can tell that from a box that is merely slow. The
second interrupt already escapes, and the report already names which
station each worker was stuck in.

**Outstanding:**
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
