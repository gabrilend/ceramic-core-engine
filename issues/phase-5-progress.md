# Phase 5 Progress — The hardware target

## Goal

A C box compiles a second time, into synthesizable HDL, from
the same source the pool runner already compiles and runs. C
that has no hardware meaning is a compiler error with a reason,
an alternative, and an escape hatch. C that passes produces HDL
that computes the same values, and the sameness is proven by
generated testbenches rather than claimed. A map becomes a
structure of devices arranged the way the canvas was arranged.

Origin: the phase was requested on 2026-07-31 in one sentence —
*the user writes in C, optionally compilable to an HDL; if they
do anything that isn't possible or doesn't make sense
semantically, it gives a compiler error; the result is both
compliant C and, translated, fully compliant HDL; then each box
becomes a cluster of FPGAs, as few as possible per unit, and we
orchestrate structures of computation, simulation, or design.*
Everything in this phase is an attempt to be worthy of that
sentence.

## Status

**In design. Nothing is implemented.** The thirteen issue files
below are blueprints; the ground rules in 501 are settled
enough to build against, and the fifty-odd open questions
across the set are not. The single blocking one is the
declaration scheme in 503 — every other issue reads widths
through whatever it settles on.

## In-scope issues

| ID  | Title                                              | Status |
|-----|----------------------------------------------------|--------|
| 501 | HDL compilation target (parent)                    | open · ground rules drafted · 12 open questions |
| 502 | the hardware dialect: a synthesizable C subset     | open · rule table drafted · 5 open questions |
| 503 | fixed-width and fixed-point types                  | open · **blocking** · three candidate schemes, choice not made |
| 504 | the C front end and the dialect checker            | open · waits on 502 and 503 |
| 505 | the box module contract                            | open · module interface drafted |
| 506 | datapath and state-machine generation              | open · waits on 504 |
| 507 | routing kinds as hardware cells                    | open · two kinds force a software change |
| 508 | the host bridge and capability partitioning        | open · testable over loopback before any transport exists |
| 509 | resource estimation and device packing             | open · the "as few FPGAs per box as possible" issue |
| 510 | the RTL intermediate, backends, and toolchain      | open · Verilog first, VHDL as the neutrality test |
| 511 | equivalence testing from the run transcript        | open · the issue that makes the phase's claim checkable |
| 512 | the editor's hardware lens                         | open · pure viewer |
| 513 | phase 5 capstone demo: one map, three executions   | open · blocked on everything |

## Ground rules

The full statement is in 501. In short:

1. The dialect adds no SoraMech-specific syntax to the source.
2. Errors, never inference.
3. Equivalence is tested, not asserted.
4. The map is the topology.
5. Hardware mode is a property of a region, not a language.
6. When the two worlds cannot agree, the software moves.
7. The HDL target is plural; Verilog is a backend, not the
   interface.

Rule 6 is the one that will surprise a reader: turning on the
hardware target changes what a purely software run computes,
because the `weighted` and `nonlinearity` routing kinds move
off `double` and onto fixed-point forms both targets can
perform identically. That change buys a strict byte comparison
in the equivalence tests instead of a tolerance, and a
tolerance is a fallback wearing a lab coat.

## Phase goal checklist

- [ ] The declaration scheme for fixed-width types is chosen,
      and a test proves C and emitted HDL agree on add,
      subtract, multiply, shift, and compare at the wrap
      boundary
- [ ] The dialect rule table exists as data, and the dialect
      document is generated from it
- [ ] Every refused construct has a fixture proving the checker
      stops with the right four-part message
- [ ] The front end parses the dialect and names, rather than
      merely rejecting, the constructs outside it
- [ ] One hand-written box module simulates correctly, and a
      generated one matches it
- [ ] The FIFO, the operator primitives, and all seven routing
      cells are shipped, verified components
- [ ] A box translates end to end and passes exhaustive
      equivalence against its own C
- [ ] A wire crossing the host boundary makes a round trip over
      a loopback with credits and ordering intact
- [ ] Hardware trace events merge into `last-run.jsonl` and
      pass the existing diff harness
- [ ] A multi-box map places onto devices with a readable plan
      and an honest report of what it cost
- [ ] The Verilog and VHDL backends produce designs that
      simulate identically
- [ ] Whole-map co-simulation diffs clean against a software run
- [ ] The capstone demo shows one map, three executions, and a
      byte-difference count of zero

## What has to be decided first

Ordered by how much else depends on it:

1. **The fixed-width declaration scheme** (503). Blocks
   everything.
2. **Wire width and what happens to values with no hardware
   form** (501 q2, q3; 508 q2). Decides whether the box schema
   changes.
3. **Whether `int` is permitted in the dialect** (502 q1).
   Decides how hard the checker is and how much a hardware box
   looks like an ordinary one.
4. **Simulation or bitstream as the phase's finish line** (501
   q8). Decides whether a board and a device family are on the
   critical path.
5. **The randomizer's hash-versus-LFSR question** (507 q1) and
   the nonlinearity's bounds rule (507 q3). Both change shipped
   software behaviour and should be decided together.

## Documentation to write once the shape settles

Following the phase-4 precedent of not writing the datapath
document until the open questions resolve:

- `docs/datapath-hardware-compilation.md` — source to verdict
  to intermediate to HDL to device, the whole path in one
  place.
- A hardware-dialect section in `docs/005-writing-boxes.md`,
  generated from the rule table.
- A note in `docs/002-map-model.md` wherever `weighted` and
  `nonlinearity` are described, if and when their arithmetic
  moves.
