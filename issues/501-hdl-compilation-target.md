# 501 — HDL compilation target (parent)

## Status

open · phase 5 · concept captured 2026-07-31. Nothing is
implemented and nothing is decided beyond the ground rules
below; twelve open questions gate the first line of code.
Sub-issues 502 through 513 carry the per-feature detail.

## Current behavior

A C box is compiled by the C language spec (issue 307) into a
`.so` that the pool runner dlopens and calls through a function
pointer. The compiler is the system `cc`, the target is the host
CPU, and the only artifact is machine code. Nothing in the tree
knows that a box could be anything but software: there is no
notion of a bit width narrower than a C type, no notion of a
clock, no notion of a box occupying a device, and no second
compiler to disagree with the first. A map's structure —
boxes holding functions, wires carrying single values between
them, ports that hold a value until the box has a full set —
is a description of a machine that is currently only ever
built out of threads.

## Intended behavior

A map, or a marked region of one, compiles a **second time** —
into synthesizable HDL — from the same C source that the pool
runner already compiles and runs. The user writes ordinary C.
If they write C whose meaning in hardware is undefined,
ambiguous, or unbuildable, the hardware compiler stops with an
error naming the file, the line, the construct, and the
hardware reason. If it does not stop, the emitted HDL computes
the same values the C computed, and that claim is checked by
generated testbenches rather than asserted.

The unit of hardware placement is the box. A map becomes a set
of devices with links between them, arranged the way the canvas
was arranged: the wire a user drew between two boxes becomes,
when those boxes land on different devices, a physical link
carrying the same value the software wire carried.

## The dual-compliance rule

One source, two compilers, one meaning. Stated as three
obligations:

1. **The source is valid C.** It compiles with `cc`, runs in the
   pool runner, and can be linted, unit-tested, and debugged by
   a toolchain that has never heard of SoraMech. This is issue
   307's rule ("SoraMech bends to the language; the language
   never bends to SoraMech") carried into the hardware target
   unchanged.
2. **The source is valid HDL input.** Every construct it uses
   has a defined translation into registers, wires, arithmetic,
   memories, and state transitions.
3. **The two agree on values.** For the same inputs, the
   software box and the hardware module produce the same output
   bytes. Not the same timing — the same values.

Obligation 3 is the one that bites, and it is the reason this
phase is not simply "write a C-to-Verilog translator." The
places C and silicon disagree are not exotic:

- **Integer promotion.** C widens everything narrower than
  `int` to `int` before arithmetic, so `uint8_t a = 200, b = 100;
  a + b` computes 300 and then truncates on assignment. Verilog
  arithmetic is the width of its operands, so the same
  expression on two 8-bit signals is 44 with no intermediate
  300 anywhere. The two agree only if the C result is always
  stored back into a declared-width variable before it is used
  again — which becomes a dialect rule, not a hope.
- **Signed overflow.** Undefined behavior in C; two's-complement
  wrap in hardware, always. Resolved by compiling hardware boxes
  with `-fwrapv`, which makes C promise what the silicon already
  does.
- **Evaluation order.** C leaves the order of side effects
  within an expression unspecified; hardware evaluates a
  combinational expression all at once. Resolved by forbidding
  side effects inside expressions, after which the question
  cannot be asked.
- **Real numbers.** A C `double` and any FPGA floating-point
  core will not agree bit-for-bit except under conditions no
  box author should have to reason about. Resolved by fixed
  point, which is exact in both worlds.

## Why the software side sometimes has to move

Two of the shipped routing kinds compute in `double`: the
`weighted` kind normalises a user's weight array and picks by
cumulative band, and the `nonlinearity` kind auto-calibrates
bounds from a ring buffer and applies tanh or sigmoid. There is
no fixed-point hardware curve that reproduces libm's `tanh` to
the last bit, and there is no reason to want one.

So the rule inverts: **where the two worlds cannot agree, the
software adopts the hardware's arithmetic.** A map in hardware
mode uses a fixed-point, piecewise-linear S-curve in the pool
runner too — the same table the silicon holds. The software
result changes slightly; the two targets now agree exactly; and
the equivalence test in issue 511 (golden-transcript
testbenches) can be a strict byte comparison rather than a
tolerance. A tolerance is a fallback, and fallbacks are
warnings, and warnings are errors.

This inversion is the phase's least obvious consequence and the
one most likely to surprise a reader: turning on the hardware
target changes what the software does.

## The three regions of a map

Not every box can be silicon, and the design does not pretend
otherwise. A map in hardware mode partitions into three regions:

| Region   | What lives there | How it is built |
|----------|------------------|-----------------|
| Silicon  | `call` boxes in the hardware dialect; all routing kinds; `read` boxes with constant values | generated HDL modules on one or more devices |
| Host     | Lua and Bash boxes, LLM boxes, file-backed `read`, disk-writing `write`, anything the dialect rejects | the pool runner, unchanged |
| Bridge   | wires that cross between the two | a link with a transport, a framing, and a flow-control story (issue 508) |

The partition is a property of the map plus the dialect
checker's verdict, not a thing the user hand-assigns box by
box — though they can pin a box to a region when they want to.

## What a box becomes in silicon

A short version; the detail lives in the sub-issues.

- Each input port becomes a small FIFO with a valid/ready
  handshake — the slot store's ordering ring, in registers. The
  `cell_capacity` the runtime already logs at `slot_alloc` time
  becomes the FIFO's depth.
- The box's fire condition is the AND of "valid" across its
  required ports. Optional ports contribute a presence bit and
  not a term in the AND.
- A literal port — a value typed into the box with no wire
  attached — becomes a constant or a writable register, never a
  FIFO. This is issue 324's ruling (referenced versus consumed
  inputs) with an obvious hardware reading: consumed means pop,
  referenced means read.
- The C function body becomes a datapath plus a small state
  machine (issue 506).
- The routing kind becomes a cell downstream of the body that
  decides which output wire the value leaves on (issue 507).
- Fan-out is wire replication with a per-consumer accept, since
  a hardware fan-out cannot deliver to a busy consumer and
  forget about it the way a software push into a ring can.

## The device-packing goal

The user's framing: *make each box a cluster of FPGAs, as few
as possible per unit.* Two directions fall out of that, and
issue 509 owns both:

- **Downward** — a box larger than one device is split across
  several, and the split should use as few as it can, because
  every cut is a physical link and every physical link is
  slower than a wire on-die by orders of magnitude.
- **Upward** — several small boxes share a device when they fit,
  and the packing prefers to keep neighbours together, because
  a wire between two boxes on the same die costs nothing and the
  same wire between two dies costs pins, latency, and a
  clock-domain crossing.

The objective function is therefore lexicographic: minimise
devices per box first, then minimise cut wires, then balance
utilisation. Which is a graph-partitioning problem with a
resource constraint, and it is the part of this phase most
likely to stay heuristic forever.

## Ground rules

1. **The hardware dialect adds no SoraMech-specific syntax.**
   Whatever declares a bit width must be legal, meaningful C on
   its own — a typedef, a struct bitfield, a static assertion —
   never a macro that expands to nothing in software and to
   magic in hardware. A reader with no knowledge of this project
   must be able to read a hardware box's source and know what it
   computes.
2. **Errors, never inference.** When a construct's hardware
   meaning is ambiguous, the compiler stops. It does not guess a
   width, does not insert a conversion, does not fall back to a
   host implementation. Errors over fallbacks, applied to a
   compiler.
3. **Equivalence is tested, not asserted.** Every hardware box
   ships with a testbench generated from a real software run's
   transcript. A box whose testbench has never been run is not
   a hardware box yet.
4. **The map is the topology.** Placement follows the graph;
   wires become links; the canvas the user drew is a floor plan.
5. **Hardware mode is a property of a region, not of a
   language.** The dialect check applies where the user asked
   for hardware and nowhere else. C boxes outside the region
   stay ordinary C boxes with no new restrictions at all.
6. **When the two worlds cannot agree, the software moves.**
   See above.
7. **The HDL target is plural.** Verilog is the first backend,
   not the interface. The translator emits a target-neutral RTL
   intermediate; per-language emitters consume it, in the same
   plugin shape `langs/` already uses for Lua, C, and Bash.

## Sub-issues

- **502 — the hardware dialect: a synthesizable C subset.** What
  is permitted, what is forbidden, and the hardware reason for
  each rule.
- **503 — fixed-width and fixed-point types.** The declaration
  scheme that is honest C and unambiguous HDL, and the
  arithmetic agreement rules that hang off it.
- **504 — the C front end and dialect checker.** One parser,
  two consumers: the checker that rejects, and the translator
  that emits.
- **505 — the box module contract.** Port handshakes, FIFOs,
  the fire condition, literals, optional ports, reset.
- **506 — datapath and state-machine generation.** Expressions
  to logic, statements to states, arrays to memories, loops to
  unrolled or iterated hardware.
- **507 — routing kinds as hardware cells.** One synthesizable
  cell per shipped routing kind, including the two that force
  the software to move.
- **508 — the host bridge and capability partitioning.** What
  cannot be silicon, how it stays reachable, and what a wire
  becomes when it crosses.
- **509 — resource estimation and device packing.** Per-box cost
  model, the partitioner, the placement plan.
- **510 — RTL intermediate, backends, and toolchain.** The
  target-neutral IR, the Verilog / VHDL / SystemVerilog
  emitters, and the build path out to simulation and bitstreams.
- **511 — equivalence testing from the run transcript.** The
  golden-vector harness that turns every software run into a
  hardware test suite.
- **512 — the editor's hardware lens.** Synthesizability,
  estimated cost, partition colouring, and dialect errors on
  the canvas.
- **513 — phase 5 capstone demo.** One map, three executions,
  identical outputs.

## Open questions to resolve before implementation starts

Each sub-issue carries its own framing; this is the
parent-level inventory.

1. **Which HDL is the reference backend?** Verilog-2005 reaches
   every tool including the open ones; SystemVerilog is far
   more readable and less error-prone to generate; VHDL is what
   a large part of the professional world actually uses. Lean
   Verilog-2005 as the emitter that everything else is checked
   against, with SystemVerilog as the pretty one.
2. **How wide is a wire?** Software wires carry arbitrary bytes.
   Hardware wires cannot. Is every wire a fixed word — 32 bits,
   64 bits — or is each wire's width derived from the producing
   box's declared return type? The typed answer is better
   hardware and a much bigger change to the box schema.
3. **What happens to values with no hardware form?** Strings,
   JSON, and variable-length payloads flow freely on software
   wires. Are they banned at the silicon boundary, forced onto
   the host, or given a length-prefixed streaming form?
4. **One clock or many?** A single global clock is simplest and
   is a lie the moment two devices are involved. Per-partition
   clocks with crossing FIFOs at every cut wire is correct and
   costs a synchroniser on every link.
5. **Do stochastic routing kinds need bit-exactness?** The
   randomizer hashes a per-box counter; a Verilog LFSR is not
   that hash. Either reimplement the hash exactly in HDL, or
   declare stochastic branches exempt from the equivalence test
   and check their distribution instead.
6. **May a hardware region contain a cycle?** Circular maps are
   a documented, supported feature. A combinational loop is
   illegal in hardware, but every box's input FIFO is a
   register, so a cycle through boxes is fine. Confirm there is
   no path that skips the FIFO.
7. **How does a box declare its hardware intent?** A `hardware:
   true` field on the box, a region marked on the canvas, a
   per-map flag, or inferred from the dialect checker passing.
8. **Is phase 5 done at simulation, or at a bitstream?** A
   Verilator-verified phase is a complete and honest deliverable
   and needs no hardware on the desk. A bitstream needs a chosen
   family and a board.
9. **Which device family first?** The open toolchain
   (yosys / nextpnr, ice40 and ecp5) keeps the whole build
   reproducible and scriptable; vendor tools reach the big parts
   where an interesting map would actually fit.
10. **Does the equivalence test compare per-box, whole-map, or
    both?** Per-box is easy to generate and diagnoses precisely.
    Whole-map catches the wiring, the routing cells, and the
    bridge, which is where the interesting bugs will be.
11. **What is a hardware box's transcript?** The JSONL event
    stream is load-bearing for debugging. Emitting it from
    silicon costs pins and memory. Debug-build-only, like issue
    247's statistics lens, or always on a slow path?
12. **Does a `write` box exist in silicon at all?** A write's
    meaning is "land this on the filesystem," which a device
    does not have. Either every write is a bridge send, or a
    device-local sink kind appears (a memory-mapped register, a
    UART, an LED) and `write` gains a hardware flavour.

## Relevant files

- `langs/c/spec.c` — the C language spec, the software half of
  the dual-compilation story
- `src/010-graph-loader.c` — where box JSON becomes `box_t`,
  including the per-box compile hints a hardware flag would
  join
- `src/009-slot-store.c` — the ordering ring that becomes a
  FIFO in silicon
- `src/012-dispatch.c` — the fire condition and routing-kind
  selection that become the module's control logic
- `docs/002-map-model.md` — the box kinds and routing kinds this
  phase has to reproduce in hardware
- `docs/007-architecture.md` — the module stack the new
  compiler sits beside
