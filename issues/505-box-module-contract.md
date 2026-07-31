# 505 — The box module contract

## Status

open · phase 5 · sub of 501 (HDL compilation target). Defines
the hardware interface every generated module presents, which
issues 506 (body) and 507 (routing) both build against. Five
open questions.

## Current behavior

A box's runtime interface is a set of input slots, a fire
condition, and one output push. The dispatch layer (issue 304)
pops one value per required port, calls the language spec's
`invoke`, and pushes the returned bytes into every downstream
slot the routing kind selected. Slots are rings with a
`cell_capacity`; a full ring is backpressure that the pusher
observes; a box whose required ports all hold values is queued
to fire. Same-box concurrent fires are explicitly not gated —
one box may be running on several workers at once, each fire
owning its own popped inputs and its own return slot.

All of that is software. There is no hardware interface because
there is no hardware.

## Intended behavior

Every hardware box compiles to one HDL module presenting a
fixed, boring, mechanical interface. Boring is the goal: the
routing cells, the partitioner, the link layer, and the
testbench generator all wire against this shape, and every
irregularity in it becomes a special case in four other places.

```verilog
module sm_box_<id> #(
    parameter DEPTH_<port> = 4      // one per wired input port
) (
    input  wire clk,
    input  wire rst_n,

    // one group per wired input port
    input  wire [W_<port>-1:0] in_<port>_data,
    input  wire                in_<port>_valid,
    output wire                in_<port>_ready,

    // one group per literal port, writable but not handshaked
    input  wire [W_<port>-1:0] lit_<port>_data,

    // single output, plus a branch selector when the routing
    // kind names branches
    output wire [W_OUT-1:0]    out_data,
    output wire [BSEL-1:0]     out_branch,
    output wire                out_valid,
    input  wire                out_ready,

    // fault reporting
    output wire                fault,
    output wire [7:0]          fault_code
);
```

## The handshake

Valid/ready, transfer on the cycle where both are high, and the
producer may not withdraw `valid` once raised. This is the
AXI-Stream discipline, chosen because every tool, every
engineer, and every existing IP block already speaks it, not
because it is clever.

It is also, exactly, what the slot store already does. A slot
with room is `ready`; a push is a transfer; a full ring is
`ready` low and the producer waits. The software runtime has
been simulating this handshake since phase 3 without anyone
calling it that.

## Input FIFOs

Each wired input port gets a FIFO whose depth is the port's
`cell_capacity` from the box JSON — the same number the runtime
already logs in its `slot_alloc` event. One number, two
meanings, no new field.

Depth 1 is legal and means a box that cannot buffer; it stalls
its producer whenever it is busy. Depth 0 would mean a purely
combinational pass-through with no register in the path, which
is refused, because it is how a cycle in the map becomes a
combinational loop and a combinational loop is not a circuit.

## The fire condition

```
fire = (all required ports' FIFOs non-empty)
     & (body is accepting)
     & (downstream will accept, or the output register is empty)
```

Optional ports contribute a `present` bit that the body can
read and do not contribute a term to the AND — the same
semantics the dispatch already has, where an optional port with
no value fires anyway.

On fire, every required port's FIFO pops exactly one entry.
This is the consumed half of issue 324's ruling.

## Literals are not FIFOs

A port with a typed-in value and no wire is *referenced*, not
consumed: the runtime re-reads it on every fire and never
spends it (issue 324). In hardware it is a constant, or a
register the host can write over the bridge when the value
should be tunable at runtime. Either way it has no valid, no
ready, and no pop. The `lit_` port group above exists so a
literal can be made writable without changing the module's
shape.

A port that is both wired and literal keeps the software
meaning: take the queued value when one is waiting, fall back
to the literal when the FIFO is empty. In hardware that is a
mux on FIFO-empty, which is a wonderfully cheap way to
implement "use this unless something computed a better answer."

## Read boxes are not modules

A `read` box with a constant `value` is not a module at all —
it becomes the literal input of whatever it feeds, folded away
at translation time. It never occupies area. This matches the
runtime, where read boxes never queue, never spawn, and never
appear as a task in the transcript.

Two consequences carry over and both need hardware forms:

- A read box is inexhaustible, which a constant naturally is.
- Several read boxes on one port rotate round-robin via a
  per-port counter. In hardware that is a counter and a mux
  over N constants — small, and worth keeping, because it is
  how a user says "cycle through these values" without an
  iterator.

A `path`-backed read box reads its file once at graph load. On
a device there is no file, so it becomes a ROM initialised from
those bytes at build time — which preserves the semantics
exactly, since the runtime already caches the bytes for the
whole run and ignores edits to the file mid-run.

## Concurrency, and what a worker thread becomes

The runtime lets one box fire on several workers at once. That
freedom has a hardware price and a hardware equivalent, and the
choice is per box:

| Software behaviour | Hardware form | Cost |
|--------------------|---------------|------|
| one fire at a time | the module blocks new input until the body finishes | none; latency becomes throughput |
| overlapping fires, no shared state | the body is pipelined; a new fire starts each cycle | registers between stages |
| many fires at once | the module is replicated N times behind a small dispatcher | N times the area |

The third row is literally what the thread pool does — N
workers running the same code on different data — and seeing it
show up as "instantiate the module N times" is the clearest
statement of what a thread pool is that this project has
produced.

Which row a box gets is a cost decision that belongs with the
resource model (issue 509), driven by an initiation-interval
target the user can set. The default is the first row: correct,
smallest, slowest.

## Reset

Synchronous, active-low, and it clears FIFOs, the body's state
machine, and the output register. It does not clear literal
registers written over the bridge, because those are
configuration rather than state — a distinction that will be
argued about and should be written down now rather than
discovered later.

## Faults

Every way a box can fail in software — a bad parse, an
overflowing output, an unbounded loop — has to become something
observable in silicon, because a device that stops has no
stderr. The `fault` / `fault_code` pair is that channel: raised
and held until reset, carried to the host over the trace path
(issue 508), and surfaced on the transcript as the same kind of
event the software runner would have written.

The codes are a short enumerated list shared by the generated
modules, so a fault means the same thing regardless of which
box raised it. The declared-iteration-bound trap from issue 502
is the first entry.

## Open questions

1. **Fan-out accept.** One output feeding K consumers: hold
   `valid` until all K have accepted (simple, and the slowest
   consumer stalls everyone), or give each consumer a one-entry
   skid buffer (decoupled, costs K registers, and changes the
   order values arrive relative to software). The first is
   closer to what the runtime does; the second is what any
   hardware engineer would build.
2. **Does `out_branch` belong on the box or on the routing
   cell?** Keeping it here means every module carries branch
   wires it may not use. Splitting it means two module shapes.
3. **What is `W_OUT` when the routing kind transforms the
   value?** The `nonlinearity` kind emits `input × score`, not
   the input, so the output width is the cell's business and
   not the body's.
4. **Should the fault channel carry a payload?** A code is
   cheap and tells you which rule broke. A payload — the loop
   counter, the offending value — is what actually shortens a
   debugging session, and costs a bus.
5. **How does a box get its instance count?** Declared by the
   user, inferred from a throughput target, or fixed at one
   until somebody measures something.

## Suggested implementation steps

1. **Write the module template by hand, first, for one real
   box**, and simulate it. Everything in this issue is a claim
   until a generated module and a hand-written one look the
   same.
2. **Build the FIFO** as a shipped, verified primitive with a
   parameterised width and depth — one file, tested alone. Every
   generated design instantiates it dozens of times, so it is
   the highest-leverage thing in the phase to get right.
3. **Emit the port list** from the box JSON alone, with a stub
   body, and check it against the hand-written module.
4. **Wire the fire condition and the pops**, still with a stub
   body, and prove backpressure works under simulation: a slow
   consumer must stall the whole chain without dropping a
   value.
5. **Literals, optional ports, and the wired-plus-literal mux.**
6. **The fault channel and its first code.**
7. Only then hand the body over to issue 506.

## Relevant files

- `src/009-slot-store.c` — the ring this FIFO is a silicon copy
  of, including `cell_capacity`
- `src/012-dispatch.c` — the fire condition and the pop rules
  being reproduced
- `docs/002-map-model.md` — read-box semantics, optional ports,
  the single-output rule
- issue 324 (multi-fire boxes consume their literal inputs) —
  the consumed-versus-referenced ruling this implements
- issue 506 (datapath and state-machine generation) — the body
  that plugs into this contract
- issue 507 (routing kinds as hardware cells) — the cell that
  hangs off the output
