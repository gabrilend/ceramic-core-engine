# 507 — Routing kinds as hardware cells

## Status

open · phase 5 · sub of 501 (HDL compilation target). Two of
the seven kinds force a change to the software runtime; those
are the interesting ones. Six open questions.

## Current behavior

Seven routing kinds ship, all implemented in the dispatch layer
in C, all deciding at fire time which of a box's outgoing wires
receives the output: `plain`, `comparator`, `iterator`,
`randomizer`, `weighted`, `distributor`, `nonlinearity`. Three
of them keep per-box mutable state (a counter, or a ring buffer
of recent values); two of them compute in `double`; one of them
reads the fill level of downstream slots at the moment it
fires; and one of them changes the value on the wire rather
than only choosing where it goes.

## Intended behavior

Each kind becomes a small synthesizable cell that sits between
a box's body and its output wires, taking `(data, valid)` from
the body and producing `(data, branch, valid)` toward the
wires — the interface issue 505 defines. One cell per kind,
each a shipped and separately verified primitive rather than
generated code, because there are seven of them and they never
vary.

Where a kind cannot be reproduced exactly in hardware, issue
501's ground rule 6 applies: the software moves to the
hardware's arithmetic, so the two agree exactly rather than
approximately.

## The cells

### `plain` — fan to every wire

No decision, no state. The value goes to every consumer, and
the only hardware question is the one issue 505 already raised:
whether `valid` is held until all K consumers have accepted, or
each gets a skid buffer. Everything else about this kind is
free.

### `comparator` — pick by numeric threshold

A comparator against a constant and a one-hot branch select.
The multi-band form is a chain of comparators against a sorted
constant array, which synthesis reduces well.

The one wrinkle is what "numeric" means. Software compares the
producer's output after parsing it as a number; hardware
compares a bus. With declared widths (issue 503) the bus *is*
the number and the parse disappears, but signedness has to come
from the declared type rather than from the value, and the
zero-width equality bands that doubled thresholds create need
their exact-match semantics pinned in both targets.

Cost: a few comparators. This is the kind to build first.

### `iterator` — round-robin over N ports

A modulo-N counter and a demux. Advances on each accepted
output. Exactly the software behaviour, exactly reproducible,
no arithmetic worth arguing about.

The counter is per box instance, which matters if issue 505's
replication path is ever taken: two instances of the same box
each holding their own counter do not round-robin the way one
box with one counter does. That is a real divergence and it is
the strongest argument for keeping replication off by default.

### `randomizer` — hashed pseudo-random pick

The software hashes a per-box counter and takes it modulo N.
Hardware's natural answer is an LFSR, which is a different
sequence.

Three ways out, and this is open question 5 of issue 501:

- **Reimplement the software hash in HDL.** An integer mix
  function is a handful of XORs, shifts, and multiplies —
  entirely buildable, exactly reproducible, and the multiply
  costs a DSP block for something nobody needs to be precise.
- **Move the software to an LFSR.** Ground rule 6, applied.
  The software's randomizer changes its sequence; nobody was
  depending on a specific sequence, only on the spread.
- **Exempt the kind from bit-exact equivalence** and test its
  distribution instead.

The second is the honest one and the cheapest, and it costs a
one-line change to a shipped routing kind that has no promised
sequence.

### `weighted` — probability-weighted pick

The software normalises a `double` weight array and does a
cumulative-band lookup. Hardware cannot do that, and should not
try.

The fix: the weights are compiled, once, at translation time,
into a fixed-point cumulative threshold table — the
normalisation happens on the host, in whatever precision, and
the device holds the answers. Then the runtime pick is a
pseudo-random draw compared against a small constant table,
which is a few comparators and a mux.

For the two targets to agree, the software runtime must use the
*same* compiled table rather than recomputing in `double`.
That is a change to the shipped `weighted` kind and it is
strictly an improvement: the same map produces the same
sequence of branch picks in both targets, and the weights stop
being re-normalised on every fire.

### `distributor` — least-busy pick

The software samples the fill level of every downstream slot at
fire time and picks the emptiest, with a counter breaking ties.

In hardware, on one device, that is a small comparator tree
over the FIFO occupancy counts — cheap, and exactly the same
decision.

Across devices it is a genuine problem, and it is worth being
blunt about it. The fill level of a FIFO on another chip is not
knowable now; it is knowable as of one link latency ago. A
distributor whose branches land on different devices is making
its decision on stale information, and no amount of care fixes
that — it is the speed of light and a serialiser.

Three answers, none free:

- **Refuse the placement.** A distributor's consumers must all
  land on the same device as the distributor. The partitioner
  (issue 509) treats this as a hard constraint. Simple, honest,
  and occasionally impossible to satisfy.
- **Accept staleness** and document that a distributor spanning
  a link balances on delayed information, which for a
  load-balancer is usually fine and is exactly the kind of
  quiet approximation this phase exists to avoid.
- **Credit-based flow control** over the link, where each
  consumer advertises credits and the distributor counts what
  it has spent. This is what a real interconnect does, it is
  exactly right, and it is a protocol rather than a cell.

Lean on refusing the placement first, because a hard error that
the partitioner can explain beats a subtle behavioural
difference that only shows up under load.

### `nonlinearity` — value-transforming auto-calibrated gate

The most demanding kind, and the one that best shows why ground
rule 6 exists. Software keeps a ring buffer of the last
`memory` values, derives bounds from it, normalises the input
against those bounds, applies `tanh` or a sigmoid, and emits
`input × score`.

In hardware:

- the ring buffer is a small memory with a write pointer;
- the bounds are running minimum and maximum, which is two
  comparators and two registers, and is *not* the same as
  recomputing min and max over the window when a value leaves
  the window — so the exact update rule has to be pinned in
  both targets rather than assumed;
- the normalisation is a subtract and a reciprocal-multiply,
  where the reciprocal comes from a small lookup rather than a
  divider;
- the curve is a piecewise-linear table in fixed point;
- the final multiply is a DSP block.

There is no arrangement under which a fixed-point piecewise
table equals libm's `tanh` to the last bit. So the software
adopts the table. A map in hardware mode uses the same
piecewise-linear curve in the pool runner, with the same
breakpoints and the same fixed-point format, and the two agree
exactly.

This is the change most likely to surprise someone: turning on
the hardware target changes the numbers a purely software run
produces. It is also the only way to keep the equivalence test
a strict comparison instead of a tolerance, and a tolerance is
a fallback wearing a lab coat.

## What the cells share

All seven present the same interface, hold their state per box
instance, reset with the module, and are parameterised by width
and branch count. They are shipped source, tested standalone
against a software model of the same kind, and instantiated by
the translator rather than generated by it. Seven small
verified things beat one large generator.

## Open questions

1. **Which of the three randomizer answers?** Lean on moving
   the software to an LFSR.
2. **Does the weighted table live in the box JSON after
   compilation, or is it recomputed identically by both
   targets from the weights?** Storing it makes the agreement
   structural; recomputing keeps the box JSON as the user
   wrote it.
3. **What is the nonlinearity's exact bounds-update rule?**
   Running min/max that never shrinks, min/max over the live
   window recomputed on eviction, or an exponential envelope.
   The three behave differently and the current software
   implementation is the specification by accident rather than
   by decision.
4. **How many breakpoints in the curve table?** Precision
   against area, and once it ships it cannot change without
   changing everyone's results.
5. **Do stochastic kinds need a seed per box, and does it come
   from the map or the device?** A fixed seed makes runs
   reproducible, which is worth a great deal during testing and
   is a bad default for anything sampling in production.
6. **Does the distributor constraint make some maps
   unplaceable?** If so, the error has to name the distributor,
   its consumers, and why they could not be kept together.

## Suggested implementation steps

1. **`plain` and `comparator`**, with testbenches driven from
   the software model. These two cover most real maps.
2. **`iterator`**, and with it the per-instance-counter
   question, resolved in writing.
3. **The fixed-point curve and table work** from issue 503,
   since `weighted` and `nonlinearity` both need it.
4. **Change the software `weighted` to use a compiled table**,
   and prove the branch sequence is identical in both targets
   before building the cell.
5. **Change the software `nonlinearity` to the piecewise
   table** under hardware mode, with a documented note on the
   before-and-after numbers, then build the cell.
6. **`randomizer`**, after the LFSR-versus-hash decision.
7. **`distributor` last**, with the placement constraint landed
   in the partitioner before the cell is trusted.

## Relevant files

- `src/012-dispatch.c` — every routing kind's current
  implementation, and the specification-by-accident this has to
  reproduce or deliberately replace
- `docs/002-map-model.md` — the user-facing description of the
  seven kinds, which changes if `weighted` or `nonlinearity`
  changes
- issue 253 (nonlinearity refactor, auto-calibration, gated
  output) — the design history of the kind that moves the most
- issue 503 (fixed-width and fixed-point types) — the
  arithmetic these cells are built from
- issue 509 (resource estimation and device packing) — the
  distributor's placement constraint
- issue 511 (equivalence testing from the run transcript) — the
  standalone cell tests and the whole-map check
