# 245 — Wire color by cross-language classification

## Status
complete (code-only — visual verification is on the user)

## Current behavior

Wires are coloured by **language pairing across the edge**, with
custom-translation shims overriding the language signal. Three
classifications drive the palette:

- **`native`** — producer and consumer share a language. Slate-blue
  (`#4a6080`), the historical default.
- **`json`** — cross-language wire. Forest green (`#228b22`). The
  runtime JSON-encodes at the producer and decodes at the consumer;
  the colour is a standing reminder of that cost.
- **`custom`** — destination input port has a user-written
  translation shim attached (issue 246's `custom_translation` field;
  defensive support — works whether 246 has landed or not).
  Goldenrod (`#e8a317`); dominates the language signal because user
  code is the more interesting thing to surface.

Existing branch wires (`lt` / `eq` / `gt` etc.) compose with the
classification: branch colour drives the stroke, classification
drives a small 4 px dot at the bezier midpoint. Both signals are
visible on the same wire.

A bottom-left screen-space legend keeps the colour vocabulary
visible without panning or zooming away from it.

## Concept

Wires are colored by **language pairing across the edge**, with a
third color for wires whose destination port has a user-written
custom translation (issue 246):

- **Forest green** (`#228b22` or similar) — cross-language wire.
  Producer and consumer are different languages; the runtime
  translates via JSON at the wire. A reminder that this hop costs
  encode + decode.
- **Dark gray** (`#666` or similar; "forest-gray" is the verbal
  cue) — same-language wire. Producer and consumer share a
  language; native bytes pass without translation.
- **Bright goldenrod cheddar** (`#e8a317` or similar — calibrate
  on canvas) — the destination input port has a custom
  translation shim attached. This signal overrides the green/gray
  classification because user-written code is the more interesting
  thing to surface: it tells the box author "this wire's value
  passes through my own decode logic before invoke sees it."
- **Existing branch colors** (`lt` / `eq` / `gt` / iterator
  branches) remain — they encode routing semantics, which is
  orthogonal to language classification and custom shims.

When a wire is both branch-colored AND language-classified, the
two signals compose. One workable composition: branch color drives
the wire's stroke, language classification drives a small marker
(a colored dot at the midpoint, or a dashed-vs-solid line style).
The implementation picks the form; the rule is that both signals
are visible on the same wire.

## How the editor computes the classification

For each connection `{from_box, to_box, to_input, ...}`:

1. Look up the destination input port on `to_box` (the port named
   `to_input`). If that port has a `custom_translation` field set
   (issue 246), the wire is **goldenrod**, regardless of language
   pairing. Skip the rest.
2. Look up `from_box`'s box JSON; read its `lang` field.
3. Look up `to_box`'s box JSON; read its `lang` field.
4. If either box is a `read` / `write` / `data` kind (no `lang`
   field), treat the wire as cross-language — those boxes deal in
   language-agnostic bytes and any consumer's spec reads them
   through its JSON bridge.
5. Otherwise: same-language iff `from_box.lang === to_box.lang`.

The editor already has every box's JSON in memory (state populated
by the HTTP backend). The classification is one comparison per
connection at draw time — no separate index needed.

## Relevant files

- `assets/js/006-wires.js` — wire rendering. Line 121–122 is the
  current color-picking site. Add the language classification
  there.
- `assets/js/002-boxes.js` — box state, source of the `lang` field
  for each box.

## Suggested implementation sequence

1. Add a `wire_classification(from_box, to_box)` helper returning
   `"native"` or `"json"`, in `assets/js/006-wires.js` or wherever
   it composes with the rest of the draw loop.
2. Update the color-picking branch in `006-wires.js` to use the
   classification when `from_branch` is null. Pick the two
   concrete hex values for forest-green and forest-gray.
3. For branch wires (`from_branch` set), pick a composition: keep
   the branch color as the stroke, add a midpoint dot in the
   classification color. Or pick the dashed-vs-solid route. The
   issue's only requirement is both signals visible.
4. Add a legend somewhere unobtrusive — bottom-left corner of the
   canvas, behind the existing zoom indicator — so the color
   meaning isn't tribal knowledge.
5. Verify on `maps/driver-test` (multi-language map) and on a
   single-language map (everything one color).

## Open questions

- Exact hex values for forest-green and forest-gray. Pick something
  that reads well against the dark canvas background and doesn't
  collide with the branch palette.
- Branch + classification composition: stroke vs. dot vs. dash.
  Whichever is more legible.
- Whether `read` / `write` / `data` boxes should also visually
  carry a "language-agnostic" badge somewhere on the box itself,
  so users can see at a glance which boxes will trigger JSON
  translation when wired to typed call boxes. Out of scope for
  this issue; a separate UX ticket if it comes up.

## Relevant documents

- `issues/312-same-language-wire-fast-path.md` — runtime
  classification this mirrors visually
- `docs/007-architecture.md` — wire format section
