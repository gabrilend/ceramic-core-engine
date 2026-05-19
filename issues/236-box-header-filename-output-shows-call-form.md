# 236 — Box header shows filename; output label shows `fn_name()` call form

## Status
open

## Current behavior

A call box's header (the bold text at the top of the box on the
canvas) currently displays the box's label — by default, the
function name (e.g. `concat`, `read_text`, `prompt_ollama`). The
output port on the right edge has a generic label (`output` /
`result` / whatever the parser produced).

For a user reading a map, the header gives one of the two pieces
of relevant identity (the function), but the other piece — *which
file the function comes from* — is missing from the visible box.
You can see `concat` and not know whether it is `libs/text.lua`'s
concat or some other module's concat without opening the inspector
and reading `ref`.

The single output label, `output`, is generic and conveys nothing
about what's being computed.

## Intended behavior

Reinterpret the two text slots on a box:

- **Box header (top)** = the **filename** of `ref` (basename, no
  path prefix, e.g. `text.lua`). This is the module / file the
  function lives in.
- **Output port label (right edge)** = the function call form,
  `fn_name()`, e.g. `concat()`, `read_text()`, `prompt_ollama()`.
  This reads as "this wire carries the return value of calling
  fn_name."

So a box that calls `concat` from `libs/text.lua` reads, top to
bottom on the canvas:

```
┌─ text.lua ────────────────┐
│ ● sep      "\n\n"          │
│ ● text_0                   │   concat() ●
│ ● text_1                   │
└────────────────────────────┘
```

(Combined with issue 235, the `sep` literal also shows on the
canvas — included here for illustration of how the canvas reads as
a whole.)

### Why this split

A reader scanning a map mentally parses it as a series of function
calls. The function call form `fn_name()` is the natural way to
read "this is the value of calling fn_name with these inputs." The
filename header sits one layer up: "this function comes from this
module." Together they read like an annotated call site — module
on top, return value on the side.

It also frees the box header from having to be unique. Right now
the default label is the function name, which collides across
boxes that all call the same function. The filename groups by
module — many boxes can share `text.lua` as their header and that
is fine; what distinguishes them is what they compute and how
they are wired.

### Label override

The user-editable `box.label` field remains. If a user sets an
explicit label, it overrides the filename in the header. Use case:
multiple boxes in the same map call `text.concat`, and the user
wants to name them by their role ("join paragraphs", "join CSV
row") rather than by module. The override is intentional and only
applied when set; default behavior is filename-from-ref.

The output port label is not user-editable in this issue — it is
always derived from `fn`. (If a use case appears, add it later.)

### Filename derivation

`ref` is a path like `libs/text.lua` or `maps/foo/src/echo.sh`.
Take the basename (`text.lua`, `echo.sh`). Do not strip the
extension — the extension says which language driver runs, which
is information the user cares about.

For boxes without a `ref` (branch boxes, comparators, the `read`
and `write` primitives from issue 229, iterators from issue 221),
the header shows the **full kind name** in plain text. No
abbreviations.

| kind        | default header text |
|-------------|---------------------|
| `call`      | basename of `ref`   |
| `read`      | `read`              |
| `write`     | `write`             |
| `branch`    | `branch`            |
| `iterator`  | `iterator`          |
| `comparator`| `comparator`        |

The user-set `box.label` override still wins above all of these —
the table is the default for "no override, no ref."

These boxes also skip the `fn()` output label (which only applies
to `call`). Use a per-kind output name instead:

| kind        | output label |
|-------------|--------------|
| `read`      | `contents`   |
| `write`     | `done`       |
| `branch`    | (multiple ports, named by branch) |
| `iterator`  | `item`       |
| `comparator`| `lt` / `eq` / `gt` (three ports) |

## Suggested implementation steps

1. `assets/js/002-boxes.js::draw_box` — header text path:
   - If `box.label` is set and non-empty, use it (current behavior).
   - Else, derive from `box.ref`: basename of the path string.
   - Else (no ref), use `box.kind` as a final fallback.
2. Output port label path: when `box.fn` is set, render
   `${box.fn}()` instead of `output` / `result`. Otherwise leave
   the existing default.
3. `assets/js/004-inspector.js` — the inspector's "label" field
   shows the override value (the explicit user-set label) and its
   placeholder shows what the canvas is using by default. So the
   user sees "currently displayed: text.lua (from ref)" and can
   override.

## Open questions

- **Path vs basename for the header**: a deeply-nested ref like
  `maps/foo/src/sub/echo.sh` becomes just `echo.sh` in the header.
  Is the parent dir worth keeping (`sub/echo.sh`) for disambiguation?
  Probably not by default — the inspector shows the full ref —
  but worth revisiting if collisions cause confusion.
- **Future kinds**: when a new kind is added later, it picks up
  the same convention — full kind name as the default header,
  per-kind output label. Add one row to each table above.

## Relevant files

- `assets/js/002-boxes.js` — box draw routine
- `assets/js/004-inspector.js` — label field + placeholder hint
- `issues/completed/207-source-file-browser-and-port-auto-population.md`
  — ref/fn pairing established here
- `issues/completed/210-llm-route-box.md` — single-output redesign
  this issue's `fn()` label leans on
- `issues/235-literal-value-replaces-port-name-on-canvas.md` — sister
  display change for input ports
