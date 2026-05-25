# 421 — Unify connections-array mutation through the 420 box-swap path

## Status
open · phase 4 · sub-issue of [419](419-runtime-graph-mutation.md).
Waits on [420](420-utility-box-kind.md) landing first; design is
straightforward once the box-swap mechanism exists. Renumbered
from 321 when runtime graph mutation moved to phase 4 as part of
the phase-3 release-candidate rollback.

## Current behavior

Issue 319d added the ability to attach new outgoing connections
to an existing box at runtime via `runtime_connect`. The way it
does this today is a special-case atomic dance directly on the
box's `connections` field: allocate a new connections array,
copy the old entries, append the new entry, atomic-store the
pointer, then atomic-store the new count. Readers that walk
the connections array use the order "read count first, then
pointer" so they always observe either the old pair or the new
pair, never a torn mix.

This works because the connections array is append-only —
runtime_connect only ever adds, never removes or reorders.
That's the reason the staggered-publish pattern doesn't break
existing readers.

Once issue 320 lands, the runtime has a more general mechanism:
build a fully replacement box struct, push its pointer to the
target's control queue, let the next worker visit do the
atomic graph-slot swap. This mechanism is strictly more
powerful than the connections-only dance — it can change
anything about the box, not just append to one field.

## Intended behavior

Retire the special-case copy-and-publish on the connections
field. Re-implement `runtime_connect` as a thin wrapper over
`runtime_apply_box_spec`:

1. Look up the current box for the target id.
2. Build a new box struct that is a copy of the current one,
   with the new connection appended to its connections array.
3. Call `runtime_apply_box_spec(target_id, new_spec_json)` (or
   the underlying push-pointer-to-control-queue equivalent).

The atomic discipline collapses to one path. The connections
field on `box_t` can become a plain pointer + plain int (no
`_Atomic` qualifiers needed) because the only way to mutate it
is via the box-swap mechanism, which never modifies a live box
in place.

## Why bother

- **One mutation discipline instead of two.** Every change to
  a box's identity — adding a connection, changing the function
  pointer, renaming a port, deleting the box — uses the same
  build-new-and-swap pattern. Less code to maintain, fewer
  edge cases to learn.
- **The connections-only dance is structurally fragile.** It
  relies on append-only semantics that nobody enforces at the
  type level. A future change that wanted to remove or reorder
  connections would silently introduce torn-read bugs. Removing
  the path removes the trap.
- **Smaller box_t.** The `_Atomic` qualifiers on
  `n_connections` and `connections` can come off. Plain int +
  plain pointer is fine because mutation only happens during
  the build-new step, before the new box is published.

## Suggested implementation steps

1. **Re-implement `runtime_connect` as a wrapper over
   `runtime_apply_box_spec`.** Caller still sees the same
   signature; internals route through the box-swap path.
2. **Remove the `_Atomic` qualifiers on `n_connections` and
   `connections` in `src/010-graph-loader.h`.** Plain types
   suffice now.
3. **Remove the copy-and-publish helper in
   `src/010-graph-loader.c`** that runtime_connect used to
   call.
4. **Verify the existing 319d fixture (`319d-runtime-create`,
   `319e-c-create`, etc.) still passes under the new path.**
   The behavioural contract for runtime_connect is unchanged;
   the internal mechanism changes.

## Relevant files

- `src/010-graph-loader.{c,h}` — drop atomic qualifiers; drop
  copy-and-publish helper.
- `src/018-runtime-builtins.{c,h}` — rewrite runtime_connect
  to call into runtime_apply_box_spec.
- Existing 319d/319e/319f fixtures act as the regression
  guard; no new fixtures needed.

## Open questions

- **Does the language bridges need any change?** Lua's
  `soramech.connect`, Bash's `soramech_connect`, C's
  `soramech_connect` all currently call straight into
  runtime_connect. If runtime_connect's signature stays the
  same, no bridge changes. Confirm during implementation.
