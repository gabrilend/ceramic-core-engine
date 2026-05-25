# SoraMech — Editor

The editor is a browser app. It loads a map directory, draws the
boxes on a canvas, lets you wire them, and saves every change
straight to the JSON files on disk. The runner reads those same
files — there's no "save and rebuild" step between editing and
running.

## Starting it

```bash
./scripts/start-server.sh
# default: http://localhost:7700/
```

The server is a small Lua HTTP server (`src/005-http-server.lua`)
that exposes the file CRUD endpoints the editor uses. It serves
the map directory you launched it against.

You can override the port or pick a different map root via
`config/editor.json` or environment variables — see
`scripts/start-server.sh` for the knobs.

## The layout

Three regions:

- **Canvas** (centre) — boxes, wires, ports. Pan with click-drag
  on empty space; zoom with the scroll wheel.
- **Inspector** (right sidebar) — the selected box's editable
  fields. Click a box to populate it; click empty canvas to hide.
- **Map header** (top) — current map name (click to switch maps),
  compile button, and run status.

## Working with boxes

**Create**: right-click an empty spot on the canvas and pick a
kind from the menu. Or middle-click an existing box to
duplicate it at the cursor position.

**Move**: click-drag a box's body.

**Inspect / edit**: click a box. The inspector fills with the
box's fields. Editing any field saves to disk on the next
keystroke (debounced) or on blur.

**Delete**: click a box to select, then press the delete button
at the top of the inspector. The wires attached to it get
cleared automatically.

## Working with wires

**Draw**: click-drag from one box's port dot to another box's
port dot. Source side is the right edge (an output port);
destination side is the left edge (an input port).

**Delete**: click the wire (anywhere along its length) to select
it, then press Delete.

**Self-loops** route around the box body so they stay readable.

## Inspector by box kind

The inspector shows only the fields that apply to the current
box's kind. The shared header (label, id, kind dropdown, delete
button) is always there.

### Call boxes

- **kind**: dropdown lets you swap between `call` / `read` /
  `write` / `map`. Changing kind clears the per-kind fields.
- **routing**: dropdown picks the routing kind. Switching kinds
  severs any outgoing wires whose port names would no longer
  exist under the new kind.
- **ref**: the source file. Click "browse" to pick from the
  attached source directories.
- **fn**: the function name within that file. Click "fn" to
  pick from the parsed function list.
- **inputs**: per-port editable rows. Name comes from the
  parser; you supply the literal value (or leave blank for
  wired input).
- **per-routing-kind controls**: comparator gets a `comparand`
  number input and a `thresholds` text input; iterator /
  randomizer / distributor get an `n_outputs` number;
  weighted gets a comma-separated `weights` text input;
  nonlinearity gets the `range` toggle + `memory` + `k`.

### Read boxes

- **value**: literal text the box emits. When set, the `path`
  input port hides on the canvas.
- **external**: toggle to mark this read box as
  externally-supplied (consumed by the encapsulating sub-map's
  port shape). When on, a binding-kind selector lets you pick
  `named` / `positional` / `numbered` plus the matching name or
  index.

### Write boxes

- **inputs**: `path` (where to write) and `value` (what to
  write). Either can be wired or carry a literal.
- **external**: toggle to mark this write box as
  externally-consumed (the value also surfaces as an output
  port on the encapsulating sub-map). Same binding-kind shape
  as read.

### Map boxes (encapsulated sub-maps)

- **ref**: path to the sub-map directory (relative to the
  current map's directory, or absolute).
- **inputs / outputs**: editable port lists. Each row is a
  port name; +/− buttons add and remove rows. The names need to
  match the externally-supplied read boxes and
  externally-consumed write boxes inside the sub-map.

## The file browser

The bottom of the inspector hosts the file browser when you
click "browse" or "fn" on a call box. It lists source files
across every attached source directory. Click a directory header
to collapse / expand; right-click to hide a directory.

The "+ library dir" button at the top lets you attach an
arbitrary directory as another source root for the current map —
useful for sharing helpers across maps without copying files.

## The map picker

Click the map name in the header to open the map picker. It
lists every map known to the current server, plus a recent-maps
history. Selecting one switches the editor to that map.

Each map row also has an **encap** button. Clicking it adds the
selected map as a `kind: "map"` sub-map box on the
currently-edited map's canvas. The new box's port shape is
derived from the sub-map's externally-marked read and write
boxes, so the wiring is ready as soon as the box appears.

## Compile

The compile button at the top of the editor runs
`scripts/soramech-compile.sh` against the current map. The
compiled artifact lands under `<map>/compiled/` and is portable
— it bundles the language plugins, pre-compiles every C box,
and includes the pool runner binary. Copy or symlink the
directory anywhere on the same machine and it runs the map
standalone (no source tree required).

See [`docs/004-runtime.md`](004-runtime.md) for the runtime
side of compilation, including the reference-counted artifacts
system that keeps in-flight runs safe across rebuilds.

## Keyboard shortcuts

- **Delete**: delete the selected box or wire.
- **Middle-click**: duplicate the box under the cursor at the
  cursor position.
- **Scroll**: zoom.
- **Right-click**: context menu (new box, hide directory in the
  file browser, etc.).
