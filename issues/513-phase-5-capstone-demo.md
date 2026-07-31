# 513 — Phase 5 capstone demo: one map, three executions

## Status

open · phase 5 · sub of 501 (HDL compilation target). The
phase's deliverable, not an afterthought — phase demos are part
of the product and are expected to keep feature parity with the
main project. Blocked on essentially everything else in the
phase. Four open questions.

## Current behavior

`./demo.sh [phase]` runs a phase demo from
`issues/completed/demos/`. The existing demos exercise the
software runtime: maps loading, boxes firing, multi-language
wires, the transcript.

## Intended behavior

One map. Three executions. The same bytes out of all three, and
a page of numbers explaining what each one cost.

| Execution | Engine | What it proves |
|-----------|--------|----------------|
| Software | the pool runner, as today | the map is an ordinary map |
| Simulation | the generated HDL under Verilator | the translation preserved meaning |
| Silicon | a real device, if one is present | the whole path, ending in a bitstream |

The third is optional and the demo says clearly when it was
skipped and why. A skipped step that reports success is the
failure mode the project's rules exist to prevent, and a demo
is not exempt.

## The map

An image kernel, because the output is a picture and a picture
is the fastest way to see that three engines agree.

Roughly: read a small greyscale image, run a 3×3 convolution
with a selectable kernel, threshold the result, write the
output image. Wired as boxes so the structure is visible on the
canvas rather than hidden inside one function.

Why this map and not something smaller:

- **It exercises the dialect properly.** Fixed-size arrays
  becoming memories, bounded loops, declared-width arithmetic
  with a wider intermediate for the accumulate, saturation at
  the output. This is exactly the shape issue 506's worked
  example describes, at a size where the array port budget
  actually bites.
- **It has a natural host region.** The kernel selection — "run
  the edge detector, then the blur, then compare" — is control
  logic with no reason to be in silicon, so the demo shows a
  real partition and a real bridge rather than a wholly-silicon
  map that dodges the interesting half of issue 508.
- **It uses routing kinds that matter.** A comparator on the
  threshold, an iterator over the tiles, plain fan-out to the
  writer and to the statistics path.
- **The output is verifiable by eye and by byte.** Three images
  rendered side by side, and underneath them a byte-difference
  count that had better be zero.

## What the demo shows

Per the house rule that phase demos lead with statistics rather
than description, the output is numbers first:

- **Dialect verdicts** — how many boxes are silicon, how many
  host, how many refused, and for a deliberately-refused box
  included on purpose, the full four-part refusal printed. A
  demo that only shows the happy path teaches nothing about
  what the compiler does when you are wrong.
- **Resource usage** — estimated and, when synthesis ran,
  measured, per box and per device, with utilisation
  percentages against the device budget.
- **Placement** — devices used, boxes per device, cut wires and
  their widths, and the objective values the packer achieved.
- **Timing** — cycles per output pixel, clock rate achieved,
  total cycles, and the resulting throughput compared against
  the software run's wall-clock throughput. This comparison is
  the demo's most interesting single number and it may well
  favour the software on a small image, which is worth showing
  honestly rather than choosing an image size that flatters the
  hardware.
- **Equivalence** — vectors run per box, exhaustive or
  sampled, and the byte-difference count between the three
  outputs.

Rendered as an HTML page in the project's documentation
aesthetic, opened in Firefox by the demo script, with the three
images, the bars, and a diff summary. The existing HTML
documentation mirror (issue 258) sets the visual vocabulary and
this page should belong to it rather than invent a second look.

## What the demo runs

A bash script under `issues/completed/demos/`, hard-coded
`${DIR}` overridable by argument, every path relative to it,
reachable from `./demo.sh 5`. Its stages:

1. Run the map in the pool runner; capture transcript and
   output image.
2. Run the dialect check; print the verdict summary including
   the deliberate refusal.
3. Translate, pack, and emit; print the placement report.
4. Generate testbenches from the transcript; run the box-level
   equivalence suite; print results.
5. Simulate the whole design under Verilator with the same
   input; capture output image and merged transcript.
6. Diff the two output images and the two transcripts.
7. If a device and toolchain are present, synthesise, program,
   run, capture, and diff a third time. Otherwise say plainly
   that it was skipped and what would have been needed.
8. Build the HTML page and open it.

Every stage prints its own numbers as it goes, so a run that
fails halfway still tells the user how far it got.

## Open questions

1. **What image, and how large?** Small enough that exhaustive
   per-box equivalence is feasible and simulation finishes in
   seconds; large enough that the throughput comparison is not
   dominated by startup. Those pull in opposite directions and
   the answer is probably two sizes.
2. **Which board, if any?** The open toolchain reaches ice40
   and ecp5, which are small; a 3×3 convolution over a modest
   image fits, and much else would not.
3. **Does the demo include a deliberately refused box?** Argued
   above that it should. The cost is that a demo map contains a
   box that does not work, which needs to be obviously
   intentional to anyone reading the map directory.
4. **Does the demo replace or extend the phase-4 demo?** Feature
   parity across demos is the standing expectation, and a
   hardware demo that has lost the runtime-mutation
   demonstration is a step backwards. The self-healing map from
   phase 4 and this one may want to be the same map, which is
   ambitious and possibly wonderful.

## Suggested implementation steps

1. **Build the map and run it in software.** It should be a
   good map on its own merits before any of this is pointed at
   it.
2. **Write the demo script's software half** — stages 1 and the
   report scaffolding — so the numbers have somewhere to land
   from the beginning.
3. **Add stages as the phase lands them**, each stage arriving
   with its own printed numbers. The demo should be runnable
   and useful from the first stage onward rather than
   appearing at the end.
4. **The HTML page**, once there is more than one number.
5. **The silicon stage last**, gated on a device being present
   and clearly reported when it is not.

## Relevant files

- `demo.sh` and `issues/completed/demos/` — the demo harness
  and its house conventions
- issue 258 (HTML documentation mirror rebuild) — the visual
  vocabulary the report page belongs to
- issue 502 (hardware dialect) — the refusal the demo shows on
  purpose
- issue 509 (resource estimation and device packing) — the
  placement report
- issue 511 (equivalence testing from the run transcript) — the
  diff that is the demo's whole point
