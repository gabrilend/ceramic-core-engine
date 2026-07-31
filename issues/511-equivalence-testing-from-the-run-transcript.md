# 511 — Equivalence testing from the run transcript

## Status

open · phase 5 · sub of 501 (HDL compilation target). This is
the issue that makes the phase's central claim checkable rather
than asserted. Five open questions.

## Current behavior

A run writes `last-run.jsonl`: a `task_start` per fire, a
`push` per delivery with a result field, slot allocations, and
— under `SORAMECH_LOG_VALUES` — the values themselves. A
transcript normaliser and a `check_jsonl` diff harness already
exist (issue 311) so a run's transcript can be compared against
an expected one despite concurrency reordering the lines.

The runner's own tests use this. Nothing else does, and nothing
generates anything from it.

## Intended behavior

Every software run of a map is a test suite for that map's
hardware. The transcript's recorded values become stimulus and
expected results; the generator emits testbenches; the
simulator runs them; a mismatch names the box, the vector, and
the source line.

This turns issue 501's ground rule 3 — equivalence is tested,
not asserted — into a build step rather than a discipline.

## What "equivalent" means, exactly

Worth pinning down before building anything, because the word
does a lot of work and could reasonably mean several things.

**Equivalent means: for the same input values, a box produces
the same output bytes.**

It does not mean the same timing — hardware has a clock and
software does not. It does not mean the same order of fires —
the pool runner is concurrent and its order is already
nondeterministic run to run, which is why the normaliser
exists. It does not mean the same number of cycles between
anything and anything else.

Three levels, each with a different scope:

| Level | Scope | Catches |
|-------|-------|---------|
| Cell | one routing cell against a software model | the seven routing kinds, especially the two that moved |
| Box | one generated module against its C function | the front end, the lowering, the type rules |
| Map | the whole design against a whole software run | the wiring, the FIFOs, the backpressure, the bridge, deadlock |

## Box-level: vectors from the transcript

With value logging on, a transcript contains, for each fire,
what arrived on each port and what left. That is a test vector.
The generator walks a transcript, groups by box, dedupes, and
emits a testbench that presents each vector on the module's
input ports, waits for the handshake, and compares the output.

Its virtue is that the vectors are *real* — they are what the
map actually did, including the values a person would never
have thought to try. Its limitation is the same fact from the
other side: a transcript covers only what happened. A branch
never taken in the recorded run is a branch never tested.

So transcript vectors are one source among four:

- **Recorded** — from real runs, deduped and accumulated across
  runs into a growing corpus.
- **Exhaustive** — and this is the quiet reward for declared
  widths: a box with a 12-bit and an 8-bit input has 2^20
  possible inputs, which is a million simulator cycles, which
  is seconds. Narrow types make exhaustive testing *ordinary*.
  Where the input space is small enough, testing it completely
  should simply be the default, and the threshold should be a
  number in a config rather than a judgement call.
- **Boundary** — zero, maximum, the wrap point, the value one
  below and one above every constant the box compares against.
  The front end knows every constant in the source, so these
  are generated rather than chosen.
- **Random** — for the wide cases, seeded and recorded so a
  failure reproduces.

The C side of each vector is produced by compiling the box's
function as it already is and calling it — the software half of
the dual compilation, used as the oracle. No separate model, no
hand-written expected values, nothing to keep in sync.

## Map-level: two transcripts, one diff

Run the map in the pool runner with value logging; record the
external inputs and the transcript. Run the generated design
under Verilator with the same external inputs, with the trace
stream (issue 508) producing events in the same JSONL shape.
Normalise both and diff.

The interesting failures live here, not at box level: a FIFO
one entry too shallow that deadlocks under backpressure, a
fan-out that drops a value when one consumer is slow, a routing
cell whose counter is per-instance when it should be per-box, a
link that reorders. None of those are visible in a
box-in-isolation test, and all of them are the kind of bug that
takes a week to find on real hardware.

The normaliser needs one addition: hardware events carry cycle
counts where software events carry timestamps, and the
canonical form has to drop both while keeping causal order.
Issue 508's merged-transcript ordering question is the same
question and should get one answer.

## Where the tolerance is not

There is no tolerance. A comparison that passes when two
numbers are close is a fallback, and the project's rule is that
fallbacks are warnings and warnings are errors.

This is affordable only because issue 507 moves the software's
`weighted` and `nonlinearity` arithmetic onto the same
fixed-point forms the hardware uses. Without that move, the
`nonlinearity` kind would force a tolerance, the tolerance
would have to be tuned, and a tuned tolerance is a place bugs
live. With it, every comparison in this issue is a byte
comparison.

The one exception is the stochastic exemption from issue 507,
and if the randomizer moves to an LFSR on both sides then even
that disappears and the exemption is deleted rather than
documented.

## When a test fails

The message needs four things, and issue 510's provenance
requirement exists to supply the fourth:

```
equivalence: box 'clamp_scale' vector 1184 of 4096 (exhaustive)
  inputs:    x = 3900 (sm_u12), gain = 200 (sm_u8)
  C:         4095
  HDL:       3808
  the divergence enters at the multiply feeding 'wide'
  (src/clamp_scale.c:2) -- the emitted product is 12 bits wide,
  the declaration says 20.
```

The first three are mechanical. The fourth — pointing at the
net whose value first differs, and at the source line it came
from — needs the simulator to dump internal signals and the
intermediate to carry provenance, and it is the difference
between a test suite and a debugging tool.

## Where it runs

`make test` gains the cell and box levels, since they need only
a simulator and finish in seconds. The map level needs a full
design and belongs in the same place the existing integration
fixtures live, run by `scripts/run-unit-tests.sh` and the
integration harness.

Tests are cheap, and these are generated rather than written,
which makes them cheaper than usual. There is no reason for a
hardware box to ever exist without one.

## Open questions

1. **Where does the vector corpus live and how does it grow?**
   A file per box under the map directory accumulating deduped
   vectors from every run is a test suite that improves by
   itself, and is also a file that grows without bound and
   ends up in git.
2. **What is the exhaustive threshold?** 2^20 is seconds; 2^24
   is minutes; 2^32 is not happening. The number should be
   configurable and should have a stated default with a reason.
3. **How are external inputs to a map-level run recorded and
   replayed?** A map fed by a timer, a file, or an LLM box is
   not reproducible without capturing what came in, and the
   capture format is a small design of its own.
4. **Does the map-level test require the bridge, or can a
   whole-silicon map be simulated with no host at all?** The
   second is much simpler and only covers maps with no host
   region, which early ones might not have.
5. **How is a deadlock reported?** A simulation that stops
   producing output has to be distinguished from one that
   finished, and the diagnosis a user needs is which FIFO was
   full and which box was waiting on it — which the simulator
   knows and the harness has to be told to ask for.

## Suggested implementation steps

1. **Cell-level tests first**, for `plain` and `comparator`,
   with a software model in C and a testbench driving the same
   vectors. These exist before any box is translated and they
   validate the simulator plumbing.
2. **The vector extractor**: transcript in, per-box vector sets
   out, deduped.
3. **The testbench generator** for one box, using the module's
   port list from the intermediate.
4. **The C oracle harness** — compile the box's function,
   call it with each vector, record the expected output.
5. **Boundary and exhaustive vector generation**, both derived
   from the front end's knowledge of widths and constants.
6. **Wire the box level into `make test`.**
7. **Map-level co-simulation**, with the normaliser extension
   for cycle-stamped events.
8. **Failure diagnosis**: internal signal dumping and the
   first-divergent-net report.

## Relevant files

- `src/013-jsonl-events.c` — the transcript writer whose
  `LOG_VALUES` output is this issue's raw material
- `scripts/run-unit-tests.sh` and the `check_jsonl` harness —
  the existing test plumbing this joins
- issue 311 (integration tests and run output) — the normaliser
  and diff harness being extended
- issue 507 (routing kinds as hardware cells) — the software
  moves that make a strict comparison possible
- issue 508 (host bridge and capability partitioning) — the
  trace stream that gives hardware events their transcript
- issue 510 (RTL intermediate, backends, and toolchain) — port
  lists, provenance, and the simulator targets
