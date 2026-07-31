# 506 — Datapath and state-machine generation

## Status

open · phase 5 · sub of 501 (HDL compilation target). Consumes
the syntax tree from 504 (C front end and dialect checker) and
emits into the RTL intermediate defined in 510 (RTL
intermediate, backends, and toolchain). Plugs into the module
shape from 505 (box module contract). Six open questions.

## Current behavior

A C box's body is compiled to machine code by `cc` and called
through a function pointer. Its statements execute in sequence
on one core; its locals live in registers or on the stack as
the register allocator decides; its arrays live wherever
`malloc` or the stack put them. None of that is visible to
SoraMech, and none of it needs to be.

## Intended behavior

The same body becomes a **datapath** — the arithmetic, drawn as
wires and operators — plus a small **state machine** that says
which parts of the datapath are active in which clock cycle.
The output is target-neutral RTL intermediate, not Verilog
text; the emitters in issue 510 turn it into whichever HDL was
asked for.

## The two halves

An expression has no time in it. `a * b + c` is a shape: two
operators, three inputs, one result, all settling within one
propagation delay. Lowering an expression is therefore a direct
structural walk of the tree — no scheduling, no decisions.

A sequence of statements does have time in it, because a
statement can depend on the one before it and because some
operations cannot finish in one cycle. Lowering statements is
therefore scheduling, and scheduling is where every hard
question in this issue lives.

## Worked example

```c
sm_u12 clamp_scale(sm_u12 x, sm_u8 gain) {
    sm_u20 wide = (sm_u20)x * (sm_u20)gain;
    if (wide > 4095) return 4095;
    return (sm_u12)wide;
}
```

Three things happen, and each is a different kind of lowering:

- `(sm_u20)x * (sm_u20)gain` — the casts are zero-extensions,
  free, and the multiply is a 12×8 producing 20 bits. The
  declared width of `wide` sizes the multiplier exactly. This
  is the payoff from issue 503: no inference, no guessing, no
  "the tool chose 32 bits."
- `wide > 4095` — a comparator against a constant, which
  synthesis will reduce to testing the top eight bits.
- The two `return`s — one mux, selected by the comparator,
  feeding the module's output register.

The whole function is combinational apart from the multiply's
latency, so it lowers to one state, or to zero states with the
result registered at the module boundary. The state machine
here is a formality. Most well-written boxes look like this,
which is the case for keeping the scheduler simple.

## Scheduling: one state per basic block

The first scheduler is the simplest one that is correct.

1. Split the function into basic blocks at branches and merges.
2. Give each block one state.
3. Within a state, everything is combinational and happens at
   once.
4. Transitions between states follow the control-flow edges,
   with the branch condition selecting the next state.
5. A variable read in a state other than the one that wrote it
   becomes a register. A variable written and read in the same
   state stays a wire.

This produces working hardware with an obvious relationship to
the source, which is worth more at the start than producing
fast hardware. Its weakness is that a long straight-line block
becomes one enormous combinational cone that will not close
timing at any useful clock rate.

Two escapes, in order of when they should be built:

- **Splitting.** When a block's estimated delay exceeds the
  target period, cut it into several states. Needs a delay
  model, which the cost work in issue 509 has to produce
  anyway.
- **Chaining and pipelining.** Pack independent operations
  into the same cycle across block boundaries, and let a new
  fire enter before the previous one leaves. This is where a
  real HLS scheduler lives, and it should not be attempted
  until something real is running slowly enough to justify it.

## Operations that take more than a cycle

| Operation | Form | Notes |
|-----------|------|-------|
| add, subtract, compare, shift by a constant, bitwise | combinational | free, in the sense that they cost area and no time |
| shift by a variable | combinational barrel shifter | area grows with width |
| multiply | one to several cycles | maps to DSP blocks when the device has them and the widths fit |
| divide, modulo by a non-constant | many cycles | its own sub-state-machine; the cost model shouts about it |
| memory read | one cycle, registered output | the address must be stable a cycle early, which the scheduler must honour |
| memory write | one cycle | port conflicts constrain the schedule |

Each multi-cycle operation forces a state boundary, so the
operator library and the scheduler are written together. The
operators themselves are shipped, verified primitives — same
argument as the FIFO in issue 505 — and every generated design
instantiates the same tested divider rather than a fresh one.

## Arrays become memories

A fixed-size array becomes registers when it is small enough to
be cheap and a block memory when it is not. The threshold is a
cost-model number, not a rule.

Once it is a block memory, the array has a **port budget**:
typically one or two accesses per cycle, total. A loop body
that reads three array elements no longer fits in one state and
the scheduler must spread it, or the translator must refuse and
explain. This is the most common way a perfectly reasonable C
function turns out to be an unreasonable circuit, and the error
message matters as much as any in issue 502:

```
hardware translation: box 'convolve' (src/convolve.c:22)
  this loop body reads 'window' three times and writes it once.
  'window' is a block memory with 2 ports, so the body needs
  2 cycles per iteration rather than 1.
  the translator has scheduled it that way -- iteration
  latency 2, total 2 x 64 = 128 cycles.
  if you need 1, split 'window' into two arrays, or declare it
  small enough to live in registers.
```

Note that this one is not a refusal. The translator did the
right thing and said so. Refusals are for constructs with no
meaning; this has a meaning that is merely slower than the
author might have assumed, and telling them is the whole job.

## Loops

A loop with a compile-time constant bound has two lowerings,
and the choice is the largest single lever on area and speed
in the whole system:

- **Unrolled** — N copies of the body, all at once. Fast, and
  N times the area. Correct only when the iterations do not
  depend on each other, which the front end can check by
  looking at what each iteration reads and writes.
- **Iterated** — one copy of the body, a counter, and a
  loop-back edge in the state machine. Small, and N times the
  latency.

Default to iterated, because area is the resource that runs out
first and a design that does not fit cannot be measured. Let the
user ask for unrolling per loop.

A data-dependent loop is always iterated, with the declared
maximum from issue 502 as a trap counter: exceeding it raises
the module's fault line rather than spinning forever.

## Calls

A call to another dialect function is either inlined — simple,
and the area multiplies with each call site — or emitted once
as a submodule shared between call sites, which needs
arbitration when two sites might want it in the same cycle.

Inline by default. Share when the callee is large and the cost
model says the arbiter is cheaper than the copies, which is a
decision that should be a per-function annotation before it is
ever automatic.

## Two properties worth stating

**The C is still the C.** Nothing in this lowering changes what
the software half computes. The software half is `cc`'s output
of the same file, and its meaning is C's. This lowering exists
to produce a circuit that agrees with that meaning, and issue
511 is what proves it did.

**Widths are never inferred.** Every bus in the emitted RTL
traces back to a declared width in the source. When the
translator cannot find one, it stops. This is the difference
between a compiler and a tool that produces something
plausible.

## Open questions

1. **Where does the target clock period come from?** Splitting
   long combinational blocks needs a number, and the number is
   a property of the device and the design, which the user has
   to state before anything can be scheduled.
2. **How is unroll requested?** A per-loop annotation is
   inevitable, and issue 501's ground rule 1 says it may not be
   a SoraMech pragma in the source. That leaves the box JSON,
   keyed by something stable — which for a loop means a name or
   a line number, and line numbers move.
3. **Is the delay model per-device or generic?** A generic
   model is portable and wrong by a factor that varies with the
   family. A per-device model is right and is one more thing to
   maintain per device.
4. **Do we chain operations across basic blocks in the first
   version?** No is the right answer and it will produce
   designs slow enough that somebody immediately asks again.
5. **Can a box body read a value that outlives one fire?**
   Issue 502 refuses mutable `static` for good reasons, and an
   accumulator box wants exactly that. If it is ever permitted,
   this issue owns the register and its reset behaviour.
6. **What is the RTL intermediate's exact shape?** Owned by
   issue 510, needed here first. A netlist of typed nets, cells,
   and a state-transition table is the obvious answer; the
   question is whether it is expressive enough for the
   emitters or so low-level that the VHDL backend produces
   unreadable output.

## Suggested implementation steps

1. **Expression lowering alone**, into the intermediate, tested
   by simulating the emitted combinational logic against the C
   for exhaustive or random inputs. The type-agreement test from
   issue 503 is this test's ancestor and should be reused.
2. **Straight-line statement lowering**, one state, no control
   flow.
3. **Control flow** — `if`, `switch`, basic blocks, the
   variable-lives-across-states rule that creates registers.
4. **Constant-bound loops, iterated.** Then unrolled.
5. **The operator library** — multiply and divide as shipped
   primitives with their own tests.
6. **Arrays as registers**, then arrays as memories with the
   port budget and the scheduling it forces.
7. **Calls, inlined.** Sharing later, if ever.
8. **The delay model and block splitting**, once something real
   fails to close timing.

## Relevant files

- issue 504 (C front end and dialect checker) — supplies the
  tree
- issue 505 (box module contract) — the wrapper this body sits
  inside
- issue 509 (resource estimation and device packing) — the cost
  and delay models this scheduler asks questions of
- issue 510 (RTL intermediate, backends, and toolchain) — the
  output format
- issue 511 (equivalence testing from the run transcript) — the
  proof that any of this is right
