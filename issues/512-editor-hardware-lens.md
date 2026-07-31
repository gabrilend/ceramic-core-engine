# 512 — The editor's hardware lens

## Status

open · phase 5 · sub of 501 (HDL compilation target). Purely a
viewer: it computes nothing and reads files other issues
produce. Four open questions.

## Current behavior

The canvas colours boxes by kind, colours wires by
cross-language classification (issue 245), and shows a
selected box's fields in the inspector. A compile button
(issue 222) triggers the pipeline. A per-box statistics lens is
designed but not built (issue 247). Nothing on the canvas knows
whether a box could be hardware, what it would cost, or where
it would live.

## Intended behavior

A lens toggled on the canvas that answers four questions
without leaving the editor:

- **Can this box be hardware?** A badge per box: silicon, host,
  or refused. Refused is the interesting state and gets the
  most visual weight, because it is the one with an action
  attached.
- **What does it cost?** Estimated LUTs, flip-flops, DSPs, and
  memory, with a bar against the target device's budget so
  "this box is a third of the chip" is a glance rather than a
  calculation.
- **Where does it live?** Device assignment as a colour or a
  grouping outline, with wires that cross between devices drawn
  as links rather than as wires — because a crossing costs
  latency and the user should see it on the canvas, not
  discover it in a timing report.
- **Why was it refused?** The dialect checker's four-part
  refusal — construct, hardware reason, what to write instead,
  escape hatch — shown against the box, with the source line
  clickable through to the existing source viewer (issue 215).

## The lens is a viewer

Stated as a constraint because it is easy to violate. The lens
runs no analysis. It reads:

- the per-box **verdict file** from the dialect checker (issue
  504),
- the **placement plan** from the packer (issue 509),
- the **manifest** from the build (issue 510),

and it renders them. If the editor wants a number that is not
in those files, the answer is to make the producing tool write
it, not to compute it in JavaScript. This is the project's
standing separation between generating data and viewing data,
and the hardware path has three separate producers already, so
the temptation to shortcut will be real.

A consequence worth designing for: the lens has stale states.
Before any hardware compile has run there is nothing to show,
and after a source edit the verdict is out of date. Both must
be visibly distinct from "this box is fine" — an absent verdict
must never render as a pass. Showing nothing where a refusal
belongs is exactly the silent-fallback failure the project's
rules exist to prevent, wearing a user-interface costume.

## Port widths on the canvas

Once ports carry declared widths (issue 503), the width is the
single most useful thing to show on a nodule, and it is the
thing a user gets wrong most often. A wire between a 12-bit
producer and an 8-bit consumer is a load-time error, and it
should be visible on the canvas before it ever reaches a load.

The existing wire-colour vocabulary (issue 245) already
communicates a per-wire property; width mismatch wants a
different channel — a marker at the endpoint rather than a
colour along the wire — so the two can be read at once.

## The compile button grows a target

The compile control (issue 222) gains a target selector:
software, hardware, or both. Hardware runs the dialect check,
the translation, the packing, and the emit; the button reports
which stage failed when one does, since "compile failed" across
a four-stage pipeline is not a report.

## Open questions

1. **Is the lens a toggle over the existing canvas, or a
   separate view?** A toggle keeps one mental model of the map
   and crowds the canvas. A separate view has room and splits
   attention.
2. **What does a box that has never been checked look like?**
   Distinct from passed and from refused, and unobtrusive
   enough that a map with no hardware intent is not covered in
   badges.
3. **Should the lens offer to fix things?** A button on a
   refused box that sets `hardware: false` and moves it to the
   host region is genuinely useful and is also one click away
   from a user silently accepting a demotion they did not
   understand. If it exists, it should require reading the
   refusal first.
4. **Does the lens show estimated or measured resources?** The
   estimate is available immediately; the synthesis report is
   accurate and arrives minutes later. Showing both, labelled,
   is probably right and is two data sources instead of one.

## Suggested implementation steps

1. **Render the verdict file**: badges and refusals only. This
   is useful on its own and needs nothing from the packer.
2. **The refusal panel**, with the four parts and the
   source-line link.
3. **Port widths on nodules**, and the mismatch marker.
4. **Placement colouring and cut-wire rendering**, once a plan
   file exists.
5. **The resource bars**, estimate first, measured later.
6. **The compile target selector and per-stage failure
   reporting.**

## Relevant files

- `assets/js/004-inspector.js` — the inspector this extends
- `assets/js/` canvas modules — box rendering, wire rendering,
  the KIND_COLOR vocabulary
- issue 245 (wire colour by cross-language classification) —
  the existing per-wire visual channel
- issue 247 (debug build and per-box statistics lens) — the
  sibling lens, and the pattern for a toggled overlay
- issue 215 (view box source) — the source viewer the refusal
  links into
- issue 222 (compile button and assets directory) — the control
  that grows a target
- issues 504, 509, 510 — the three producers whose files this
  reads
