# 509 — Resource estimation and device packing

## Status

open · phase 5 · sub of 501 (HDL compilation target). This is
the issue that answers "make each box a cluster of FPGAs, as
few as possible per unit." Six open questions. Expect the
algorithm here to stay heuristic permanently; the goal is that
it be an *honest* heuristic that reports what it did.

## Current behavior

A map has no cost. Boxes are as large as their code, which
nobody measures, and the only resource the runtime accounts for
is memory through the unified allocator. There is no notion of
a device, no notion of a thing not fitting, and no notion of a
wire being expensive.

## Intended behavior

Two numbers exist for every hardware box — what it costs and
what it can hold — and a placement plan assigns boxes to
devices so that every device's budget is respected, no box is
split more than it must be, and as few wires as possible cross
between devices.

## The cost model

Cost is estimated bottom-up from the RTL intermediate, before
any synthesis tool runs, because the placement decision has to
be made before the per-device projects are emitted.

| Resource | What consumes it |
|----------|------------------|
| LUTs | combinational logic: adders, comparators, muxes, barrel shifters, the decode in every state machine |
| Flip-flops | registers between states, FIFO storage, pipeline stages, counters |
| DSP blocks | multipliers wide enough to want one; dividers, expensively |
| Block RAM | arrays above the register threshold, FIFOs above the register threshold, ROMs from `read` boxes |
| I/O pins | links to other devices and to the host |

The estimate is a sum over the intermediate's cells with a
per-cell rule, and per-cell rules are wrong by a factor that
depends on the device family, the synthesis tool, and its
optimisation settings.

So the model is **calibrated rather than guessed**. Synthesis
is run on a corpus of small generated designs, the tool's own
resource report is parsed, and the per-cell coefficients are
fitted to it. The coefficients are data in a file, the
calibration is a script, and the numbers in any document that
quotes them are produced by running that script rather than
typed in — which is this project's standing rule about
statistics in documentation, applied to a place where stale
numbers would cause real damage.

Recalibration is expected whenever a device family or a tool
version changes. The report says which calibration produced an
estimate, so an estimate can never be silently from the wrong
one.

## The device descriptor

A device is a small data file, not code:

```json
{
  "family":     "ecp5",
  "part":       "LFE5U-45F",
  "luts":       44000,
  "flip_flops": 44000,
  "dsp":        72,
  "bram_kbit":  1944,
  "io_pins":    197,
  "max_links":  4,
  "clock_mhz":  100
}
```

Adding a device means adding a file. The packer reads
descriptors; it does not know any part numbers.

## Two directions

### Downward — splitting one box across devices

A box that does not fit in one device is cut into pieces, and
the cut must cross a register so the pieces are separated by a
link rather than by a combinational path. The state machine's
boundaries and the array-to-memory boundaries are the natural
cut points, since both already have registers.

The objective here is the user's phrasing directly: **as few
devices per box as possible.** Every cut costs link latency
inside what the author thinks of as a single function, and
those are the most surprising slowdowns a design can have.

### Upward — packing several boxes onto one device

Boxes that fit together should be together, and neighbours
should be preferred, because a wire between two boxes on the
same die is free and the same wire between two dies costs pins,
latency, a clock-domain crossing, and a credit protocol.

This is graph partitioning with a capacity constraint: minimise
the weight of the cut edge set subject to each part fitting in
a device. Edge weight is the wire's width times how often it
carries a value, which is not knowable statically — but it *is*
knowable from a transcript, and the project already produces
transcripts. A profiling run of the software map gives real
per-wire traffic counts, and the packer uses them when they
exist and falls back to width alone when they do not, saying
which it used.

## The objective, in order

1. Minimise devices per box.
2. Minimise cut wire weight.
3. Balance utilisation across devices.

Lexicographic rather than weighted, because a weighted sum
needs coefficients nobody can defend, and because the ordering
above is a genuine statement of what matters. A tie in the
first is broken by the second, and so on.

## Hard constraints

- Each device's budget on every resource.
- The pin and link budget — a partition with more crossing
  wires than the device has links is invalid, no matter how
  well it packs.
- A `distributor` routing kind and all of its consumers on the
  same device (issue 507), unless credit-based balancing is
  built.
- Anything the user pinned.

A constraint that cannot be satisfied produces a refusal that
names the constraint, the box, and the number that did not fit.
"Placement failed" alone is useless; "box `convolve` needs 61k
LUTs and the largest configured device has 44k; split it by
declaring its inner loop iterated rather than unrolled" is a
next step.

## The algorithm

Start with the simplest thing that produces a valid plan and
report its objective values so a human can tell whether it did
badly:

1. **Seeded greedy.** Walk the graph in topological order,
   filling the current device until something does not fit,
   then starting a new one. Topological order keeps producers
   and consumers adjacent, which is most of the benefit of a
   good partitioner for free.
2. **Fiduccia–Mattheyses refinement.** Repeatedly move the
   single box whose relocation most reduces cut weight while
   keeping every part feasible, with a lock to prevent cycling.
   This is the classic answer, it is not hard, and it turns a
   mediocre greedy plan into a decent one.
3. **Stop there** until a real map is placed badly enough to
   justify more.

## The plan

The output is a file, in JSONL like everything else here, and
it is the input to the emitters (issue 510), the editor lens
(issue 512), and the build. It records: per-box device
assignment, per-box estimated resources, per-device totals and
headroom, the cut wire list with widths and estimated traffic,
which calibration produced the estimates, and the objective
values achieved.

Also a human report, because a table of numbers a person can
read is what makes a bad plan visible.

## Open questions

1. **How is the corpus for calibration built?** Generated
   designs sweeping one cell type at a time is the clean
   answer; real box designs are more representative and less
   separable.
2. **Does traffic weighting need a transcript, or should width
   alone be the default?** Requiring a profiling run before
   placement is a real usability cost and gives a much better
   plan.
3. **Is intra-box splitting in phase 5 at all?** It is the part
   of the user's framing that is most distinctive and the part
   that needs the most machinery. Packing several small boxes
   onto one device is far easier and covers early maps
   completely.
4. **How does a user pin a box to a device?** A field on the
   box JSON is obvious and makes the map directory carry
   deployment detail that is arguably not the map's business.
5. **What is the unit of "cluster"?** The user's framing says a
   box becomes a cluster of FPGAs. If the common case turns out
   to be many boxes per device rather than many devices per
   box, the vocabulary in the docs should say so plainly rather
   than keep an aspirational shape.
6. **Does the packer know about clock domains?** If each device
   has its own clock (issue 501, open question 4), then a
   partition boundary is a clock-domain crossing and every cut
   wire needs a synchroniser, which costs and which the cost
   model has to include.

## Suggested implementation steps

1. **The device descriptor format and two real descriptors**,
   so the packer has something to read before it exists.
2. **A naive cost model** — hand-written coefficients, openly
   wrong — so placement can be built and tested end to end
   without waiting on calibration.
3. **The seeded greedy packer** and the plan file.
4. **The human report**, early, because it is how every
   subsequent bug in this issue gets found.
5. **The calibration script**: generate corpus, run synthesis,
   parse reports, fit coefficients, write the coefficient file
   with a timestamp and a tool version.
6. **FM refinement**, measured against the greedy plan on a
   real map so the improvement is a number rather than a
   belief.
7. **Traffic weighting from a transcript.**
8. **Intra-box splitting**, if it is in scope at all.

## Relevant files

- issue 506 (datapath and state-machine generation) — produces
  the intermediate the cost model reads, and asks the delay
  model questions
- issue 507 (routing kinds as hardware cells) — the
  distributor's locality constraint
- issue 508 (host bridge and capability partitioning) — decides
  which boxes are candidates for placement at all
- issue 510 (RTL intermediate, backends, and toolchain) — where
  the synthesis reports that calibrate the model come from
- issue 511 (equivalence testing from the run transcript) — the
  same transcripts that supply traffic weights
- issue 247 (debug build and per-box statistics lens) — the
  existing per-box measurement surface this extends
