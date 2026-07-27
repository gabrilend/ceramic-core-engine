# 001-schema.lua — Public API

Schema validators for SoraMech map file types. Every function
returns an array of error strings; empty means valid. Used by the
editor's server before any file is written to disk, so a malformed
box never reaches the filesystem.

This is the **author-side** check. The C loader
(`src/010-graph-loader.c`) validates independently at run time and
is the authority on what will actually load; this module's job is
to catch mistakes at save time, in the editor, where the user can
still see what they typed.

## M.validate_box(box: table) -> errors: string[]

Validates a parsed box file table: `id`, `label`, `kind`, and the
kind-specific fields.

- **`call`** — requires `ref` and a `routing` declaration; `fn` is
  optional. Declares `inputs`; output is single and unnamed.
- **`read`** — an inline `value` or a `path`.
- **`write`** — the `path` and `value` input ports.
- **`map`** — an encapsulated sub-map: `ref` to the sub-map
  directory plus the `inputs` / `outputs` port shape it exposes.
  This is the documented exception to one-output-per-box.

Connections are checked for a well-formed `from_branch`: absent,
one of the comparator branches `lt` / `eq` / `gt`, or `out_<n>`
for the round-robin kinds. Encapsulated map boxes reuse the
`from_branch` slot to name an output port.

## M.check_input_bindings(boxes: table) -> errors: string[]

The graph-level check that `validate_box` cannot do alone, because
it needs every box at once: each non-optional input port must be
satisfied. A port qualifies if it is wired from a producer, or
carries an inline literal, or has a read-box predecessor (read
boxes are pulled on demand, so a port fed only by one is
guaranteed a value), or is marked `optional`. Anything else is an
error — SoraMech does not fire a box with a missing required
input.

## M.validate_meta(meta: table) -> errors: string[]

Validates a parsed `meta.json`. Requires `name`; `description` is
optional.

Also currently requires `entry_box_id`, which **selects nothing** —
the runtime derives its entry set from topology. Issue 206 (entry
box designation) removes the requirement; this is the check that
change deletes.

## M.validate_drivers(drivers: table) -> errors: string[]

Validates a parsed `drivers.json`: each key a dot-prefixed
extension string, each value a path to a driver script.

`drivers.json` is a **phase 2 artifact**. The C pool runner never
reads it — it resolves languages through the `langs/*/spec.so`
plugins. Only the synchronous Lua executor uses driver scripts.

## Related

- `docs/002-map-model.md` — the format this validates.
- `src/010-graph-loader.c` — the independent run-time validation.
- `src/002-validate-map.lua` — the standalone CLI validator, which
  is stranded on the pre-233 dialect; issue 103.
