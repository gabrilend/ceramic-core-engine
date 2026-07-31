# 508 — The host bridge and capability partitioning

## Status

open · phase 5 · sub of 501 (HDL compilation target). Five open
questions. The transport choice is deliberately left last,
because everything above it can be built and tested over a
loopback.

## Current behavior

Every box in a map runs in the same process. A Lua box, a C
box, and a Bash box are all reached through the same dispatch
and the same slot store; the language spec layer (issue 303)
is the only thing that knows they differ, and it hides the
difference behind one `invoke`. A wire between two boxes is a
pointer and a push.

## Intended behavior

A map in hardware mode is cut into a **silicon region** and a
**host region**, and every wire that crosses the cut becomes a
link with a transport, a framing, and flow control. The cut is
decided by capability — what a box needs in order to run — not
by the user drawing a boundary, though the user can pin a box
to a side when they know something the compiler does not.

The pool runner keeps running the host region exactly as it
runs a map today. It does not learn about devices, links, or
clocks. That is the design's main constraint on itself.

## What cannot be silicon

| Box | Why | Where it goes |
|-----|-----|---------------|
| Lua and Bash `call` boxes | an interpreter is not a circuit | host |
| LLM boxes (issues 210, 254–257) | a model is not a circuit either | host |
| A C box the dialect checker refused | issue 502 said why, in detail | host, with the refusal shown |
| `write` boxes with a filesystem path | a device has no filesystem | host, via a bridge send |
| `read` boxes with a `path` | the bytes are read once at load and cached for the run, so they are constant by the time anything fires | silicon, as a ROM |
| `read` boxes with a `value` | already a constant | silicon, folded into the consumer |
| `map` encapsulation boxes | the loader flattens them before anything else sees them | not a case |
| Timer boxes (issue 251) | a counter is the most natural thing a device does | silicon, once they exist |

The default assignment is therefore mechanical: a box is
silicon if it is a hardware-dialect C box, a routing decision,
or a constant; host otherwise. Nothing is guessed and nothing
is demoted quietly — a C box that *could* be silicon but failed
the dialect check does not slide over to the host with a
shrug. The compile stops, the user reads the refusal, and the
user decides whether to fix the source or mark the box
`hardware: false`.

## The link is a pair of boxes

A wire crossing the cut becomes two proxy boxes inserted by
the partitioner, one on each side:

- On the **host** side, an ordinary box kind — working name
  `link` — whose fire sends its input over the transport and
  whose output is whatever comes back. It sits in the pool
  runner's graph like any other box, holds slots like any other
  box, and appears in the transcript like any other box.
- On the **silicon** side, a module presenting issue 505's
  standard interface whose body is a serialiser and a
  deserialiser.

This is deliberately the least interesting possible design, and
that is its whole virtue. The runtime gains no concept of
"remote"; it gains one box kind. Everything the runtime already
does about backpressure, ordering, faults, and transcripts
keeps working because nothing about the graph's shape changed.

It also matches the project's standing preference for
expressing mechanism as boxes and wires rather than as runtime
API — the same argument phase 4 makes for graph mutation.

## Framing

A frame carries: destination box and port, a length, a
sequence number, and the payload. The destination is a slot
index rather than an id string, because ids are editor-only
metadata with no runtime role — the same ruling phase 4 makes,
applying cleanly here.

Sequence numbers exist for a reason worth spelling out: the
software runtime's push into a ring is ordered, and a link that
reorders would silently change a map's behaviour. Ordering is
part of the wire's meaning, so the link preserves it or fails
loudly.

## Flow control

Credits. Each side advertises how much room the destination
FIFO has; the sender spends credits and stalls at zero.

The alternative — send and hope, drop on overflow — is a
fallback, and a fallback that loses values silently is the
worst kind. A full link stalls its producer exactly as a full
slot ring stalls a push today.

## The trace stream

The JSONL transcript is load-bearing for debugging this project
(issue 311), and a device with no transcript is a device nobody
can debug. So hardware boxes emit trace events — fire, push,
fault — over the same bridge, out of band from data frames, and
the host merges them into the same `last-run.jsonl` the
software runner writes.

Merging is the interesting part. Host events carry a host
timestamp; device events carry a cycle count. The merged
transcript needs both, plus enough correlation to interleave
them honestly, and it must never claim an ordering it cannot
support. A device event and a host event with no causal
relationship between them have no true order, and writing one
down is a lie a reader will act on.

Cost is real: trace costs pins, memory, and bandwidth. So it
follows the same pattern as issue 247's statistics lens —
present in a debug build, compiled out of a release build, with
the difference visible in the build's name rather than hidden
in a flag.

## Transport

Left until last on purpose. The layers above — proxy boxes,
framing, credits, trace merging — are testable over a loopback
that never touches hardware, and should be finished there
first.

| Transport | For | Against |
|-----------|-----|---------|
| Loopback (host process to simulator) | needs no hardware; the whole stack is testable under Verilator | not a link at all |
| UART | trivially available on every board; a few wires | slow enough to dominate everything |
| SPI or a parallel GPIO bus | simple, faster, still no IP required | pin-hungry, board-specific |
| PCIe / AXI DMA | the real answer on a real accelerator card | vendor IP, real complexity |
| Ethernet | works between devices as well as to the host | a whole networking stack |

Device-to-device links, when a map spans several FPGAs, are the
same layers over a different transport — which is why framing
and credits are specified independently of the wire.

## Open questions

1. **Is `link` a real box kind in the schema, or a synthetic
   box the partitioner inserts that never appears in a map
   directory?** A real kind is visible, inspectable, and
   something a user could place by hand. A synthetic one keeps
   the schema smaller and keeps generated structure out of the
   user's map.
2. **What happens to a value with no hardware form at the
   boundary?** A string on a wire heading into silicon is issue
   501's open question 3, and this is where it becomes
   concrete: refuse at load, or stream it length-prefixed and
   let the receiving box deal with a byte stream.
3. **Does the host see one link or many?** One multiplexed
   channel with per-wire virtual circuits is fewer moving parts
   on the transport and needs an arbiter; one channel per
   crossing wire is simpler to reason about and does not scale
   past a handful.
4. **How is the merged transcript ordered?** See above. This
   needs a stated rule, not an implementation detail.
5. **Does a `write` box get a device-local form?** Writing to a
   memory-mapped register, a UART, or an LED is a real thing a
   user will want, and it is not "write to a path." Either
   `write` grows a hardware flavour or a new sink kind appears.

## Suggested implementation steps

1. **Capability assignment**, as a pure function from the graph
   plus the dialect verdicts to a per-box region label. No
   transport, no code generation — just the labelling, and a
   report the user can read.
2. **The proxy-box pair over a loopback**, with the host side
   as an ordinary box in the pool runner and the silicon side
   as a Verilator model. Prove a value makes a round trip.
3. **Framing and sequence numbers**, with a test that a
   deliberately reordering link is detected rather than
   tolerated.
4. **Credits**, with a test that a slow consumer stalls the
   producer and loses nothing.
5. **Trace events out of the simulated device**, merged into
   `last-run.jsonl`, checked by the existing transcript
   normaliser and diff harness (issue 311).
6. **A real transport**, chosen once there is a real board.

## Relevant files

- `src/013-jsonl-events.c` — the transcript writer the merged
  stream has to satisfy
- `src/012-dispatch.c` — the push path a `link` box's fire
  imitates
- `src/010-graph-loader.c` — where the partitioner's inserted
  boxes would have to appear in the graph
- issue 311 (integration tests and run output) — the transcript
  normaliser and diff harness
- issue 247 (debug build and per-box statistics lens) — the
  debug-versus-release pattern the trace stream follows
- issue 509 (resource estimation and device packing) — the
  consumer that tries to make this cut as small as possible
