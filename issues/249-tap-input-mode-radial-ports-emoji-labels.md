# 249 — Tap input mode: radial-bubble ports and emoji box labels

## Status
open

## Current behavior

The editor is mouse-first. Each box draws a header strip showing
its textual label — an explicit label if set, otherwise a default
derived from the filename and function name — and small port dots
along its edges: inputs on the left, outputs on the right, each
roughly six world-units in radius. Wires are made by click-dragging
from one port dot to another (the drag logic lives in the wire
renderer, `assets/js/006-wires.js`), and per-port details are
edited in the sidebar inspector. The box JSON stores only the
textual label; there is no emoji field, no touch-specific
interaction mode, and no radial menus — on a touch screen the same
small mouse targets are the only affordance.

## Intended behavior

A toggleable tap mode, persisted per map, lives alongside the
desktop mode without changing the data model. In tap mode, tapping
a box fans out a ring of fingertip-sized port bubbles; dragging
bubble-to-bubble forms a wire, tapping a bubble opens a
bottom-sheet port inspector, and tapping away cancels. Box headers
shrink to four user-chosen emoji stored in a new array on the box
JSON, with a body-tap popover revealing the full textual name;
boxes without emoji fall back to a default glyph so existing maps
still open. An optional follow-on suggests the four emoji
automatically from a precomputed embedding lookup.

## Concept

The editor's wire-connection affordance assumes a mouse — small
ports on the side of each box that the user clicks and drags
between. On a touch screen those ports are awkward: hard to hit
precisely, easy to misfire. The full box header eats vertical
space that's even more precious on phone-sized canvases.

**Tap input mode** is a parallel interaction model that the user
toggles into for touch-first editing:

1. **Wires are formed via a radial-bubble menu.** Tap a box and
   a ring of larger bubbles fans out around it, one bubble per
   input port plus one per output port. The bubbles are large
   enough to hit comfortably with a fingertip. Drag a bubble to
   another box's bubble to wire them together; tap a bubble to
   open its inspector menu (same content as the desktop's
   per-port inspector).
2. **Box headers shrink to four emoji.** The box's filename +
   function name (which is long and hard to scan on small
   screens) is replaced by four emoji glyphs the user picks.
   Four characters of visual identity. Tap the box itself (not
   a bubble) to see the full name in a popover.

The mode is intended to live alongside the current
mouse-and-port model, not replace it. A user with a desktop and
a mouse keeps the existing affordances; a user with a tablet or
phone flips to tap mode. The data model (boxes, wires, port
declarations) is unchanged — only the editor's rendering and
interaction code branches on the mode.

## Tap-mode wire connection — the radial bubble menu

When the user taps a box in tap mode:

```
              ┌───────┐
              │ port  │
              │ bubble│
              └───────┘
         ┌───────┐         ┌───────┐
         │ port  │         │ port  │
         │ bubble│         │ bubble│
         └───────┘         └───────┘
                ┌──────────────┐
                │  🧮📊✨🔢   │  ← the box (4 emoji)
                └──────────────┘
         ┌───────┐         ┌───────┐
         │ port  │         │ port  │
         │ bubble│         │ bubble│
         └───────┘         └───────┘
              ┌───────┐
              │ out   │
              │ bubble│
              └───────┘
```

- One bubble per input port, distributed around the top / left /
  right of the box.
- One bubble per output port (or output branch, for comparator /
  iterator boxes), distributed around the bottom.
- Each bubble is sized for a fingertip — roughly 48×48 px at the
  default zoom, scaling with the canvas.
- Bubbles are labeled with the port name, abbreviated if
  necessary.

Two gestures on a bubble:

- **Tap** — opens the per-port inspector menu (literal-value
  field, optional toggle, type selector, etc.). Same content as
  the desktop inspector's port row, just rendered as a sheet
  panel that pops up from the bottom of the canvas.
- **Drag** — initiates a wire connection. As the finger moves,
  a wire-in-progress follows the touch point. Drag onto another
  box's bubble (the target box's radial menu pops up
  automatically once the drag enters its bounding circle) and
  release on the destination port bubble to complete the wire.

Cancellation: tap outside any bubble or box to close the radial
menu; release a drag over empty canvas to cancel the wire.

### Multiple boxes selected

If the user has multiple boxes selected and taps the radial-menu
toggle, each selected box shows its own radial bubbles
simultaneously. Useful for wiring up a freshly-created cluster
without tap-cycling.

### Discoverability

A small "tap mode" icon in the editor toolbar toggles the
behavior. The icon's state is persisted per-map (some maps the
user edits on a tablet, others on a desktop — the mode follows
the device choice for that map).

## Four-emoji box labels

In tap mode, the box header renders as four emoji glyphs
instead of the filename + function name. The four emojis are
chosen by the user via the box's inspector — a 4-cell editable
field, each cell a tap-to-pick emoji selector.

```
┌──────────────┐
│  🧮📊✨🔢   │
└──────────────┘
```

Visual properties:

- Four cells, each a single emoji glyph at large size.
- Default ordering reads left-to-right (no semantic meaning to
  position; the user picks four glyphs that read together).
- Box footprint shrinks accordingly — emoji-mode boxes are
  smaller on the canvas, fitting more boxes in view on small
  screens.
- Tap the box body (not a bubble) → popover shows the full
  filename + function name and any other identifying metadata.
- Hover/long-press → same popover (for tablet stylus users).

### Round-trip with desktop mode

A box has both a textual label and four emoji slots in its JSON.
Desktop mode shows the textual label; tap mode shows the emoji.
The two render independently from one stored shape:

```json
{
    "id": "compute_avg",
    "label": "compute_avg",      // textual, shown in desktop mode
    "emoji": ["🧮", "📊", "✨", "🔢"],   // shown in tap mode
    "...": "..."
}
```

A box with no emoji set in tap mode falls back to a default
glyph (the first letter of the function name framed in a
circle, say) so existing maps don't break when opened in tap
mode.

## Optional: auto-generated emoji labels via word embedding

A more ambitious enhancement: when the user creates a new box
or hasn't picked their emoji yet, the editor **suggests four
emoji** by matching the box's textual identity against a pre-
computed embedding space.

Sketch:

1. Compute a text embedding of the box's `<filename>__<fn>`
   string (e.g., `compute_avg__main`).
2. Compute embeddings of every emoji in a curated list (the
   emoji's keyword tags + common descriptions concatenated).
   Embeddings are precomputed at editor build time and shipped
   as a static lookup table.
3. For each emoji, compute cosine similarity between the box's
   embedding and the emoji's embedding.
4. Pick the top four most-similar emojis.
5. Present them in the box's emoji slots, with each slot
   independently swap-able by the user.

This is a sometimes-works heuristic. A box named `parse_json`
probably gets 🗂️📄🔎🧩 or similar; a box named `compute_avg`
gets something like 🧮📊∑✨. The user corrects when the
heuristic misses — same affordance as the manual emoji picker.

The embedding model and the static lookup table are an editor-
side dependency. Candidate models: a small local model
(MiniLM, all-MiniLM-L6) bundled with the editor; or an
embedding lookup precomputed offline and shipped as a JSON.
The precomputed-lookup option is heavier on disk but adds no
runtime model dependency — likely the right tradeoff for an
editor whose value proposition is "runs as a Lua HTTP server
+ static HTML."

This part is explicitly an enhancement, not part of the first
slice. The base feature (4 emoji slots, user picks each one)
is what the user actually needs; the auto-suggest is a polish
follow-on.

## Relationship to existing issues

- **Issue 207 (source-file-browser)** — tap-mode interactions
  for browsing files are out of scope here but worth keeping
  consistent; if tap mode lands, the browser should grow
  compatible affordances later.
- **Issue 217 (variadic toggle gates)** — the inspector toggles
  introduced there have desktop-mouse affordances; the tap-mode
  inspector panel needs equivalents (probably larger buttons
  rendered in a sheet).
- **Issue 235 (literal value replaces port name)** — the
  desktop-mode visual; tap-mode shows the literal value inside
  the port bubble itself when set.
- **Issue 239 (literal port shrinks to nodule)** — same pattern;
  in tap mode the nodule becomes a smaller-style bubble.
- **Issue 245 (wire color by classification)** — wires in tap
  mode use the same color rules.

## Relevant files

- `assets/index.html` — editor entry; tap-mode toggle in
  toolbar.
- `assets/js/002-boxes.js` — box rendering; branches on tap-mode
  for header (text vs. emoji) and footprint.
- `assets/js/005-app.js` — interaction state; tap handler;
  per-map mode persistence.
- `assets/js/006-wires.js` — wire rendering; touch-drag for
  bubble-to-bubble wiring.
- `assets/js/008-radial-bubbles.js` (new) — radial menu logic.
- `assets/js/009-emoji-picker.js` (new) — emoji selector
  component.
- `src/006-server-main.lua` — HTTP backend; reads / writes the
  `emoji` array on box JSONs.

## Suggested implementation steps

The original planning sequence below is the concrete step list.

### Suggested implementation sequence

1. **Box JSON schema**: add the `emoji` array field (four
   strings, each a single grapheme cluster). Persist via the
   existing HTTP backend.
2. **Tap-mode toggle**: toolbar button + per-map mode state.
   In tap mode, the desktop port-and-wire affordances are
   suppressed.
3. **Emoji rendering**: in tap mode, box header renders the
   `emoji` array (or a default if absent). Tap-the-body popover
   shows the full label.
4. **Emoji picker**: inspector for the box has four cells, each
   a tap-to-pick emoji selector.
5. **Radial bubble rendering**: on tap, fan out a circle of
   port bubbles around the selected box.
6. **Bubble tap → inspector sheet**: rendering and content for
   the bottom-sheet port inspector.
7. **Bubble drag → wire**: touch-drag handler; target-box
   bubble-popup-on-hover; release-to-connect.
8. **Multi-box selection radial behavior**: optional; defer if
   straightforward composition doesn't fall out.
9. **(Enhancement)** Auto-suggest emoji via embedding lookup:
   ship a precomputed emoji-embedding table; pick top-4 on box
   creation if user hasn't set the emoji field.

## Open questions

- **Emoji rendering on the canvas**: Canvas2D vs. DOM. Emoji
  glyphs render differently per OS — Apple's emoji look
  distinct from Google's. Worth picking a strategy (CSS DOM
  overlays vs. Canvas2D text) early.
- **Bubble layout when port count is high**: a box with 8 input
  ports needs 8 bubbles around it; the radial fan may overflow
  the screen. Probably acceptable — the user can drag the box
  to give it room before tapping — but the design should
  validate on a worst-case box (say 12+ inputs).
- **Bubble tap vs. drag disambiguation**: a quick tap and a
  short drag look similar to the touch handler. The editor
  needs a threshold (movement distance) that doesn't accidentally
  start a wire on a fingertip wobble. Test with real fingers,
  not the desktop browser's touch emulation.
- **Persistence of the tap-mode state**: per-map (suggested
  above), per-user, or per-device? Probably per-map is right —
  the user picks the mode based on what they're editing.
- **Embedding model choice for auto-suggest**: small bundled
  model vs. precomputed lookup vs. external service. The
  precomputed lookup is the cheapest at runtime and matches
  the editor's "static assets" deployment shape.
- **Multilingual / accessibility for emoji labels**: emoji
  carry cultural reading conventions; a user who can't easily
  perceive the chosen emoji needs an alternative (the textual
  label is always available via the body-tap popover, so this
  is probably fine).
