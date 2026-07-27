# 427 — `create_box` and `connect` runtime built-ins, Lua bindings (rolled-back prior attempt)

## Rolled back — superseded by phase 4

The runtime built-ins this issue introduced
(`runtime_create_box`, `runtime_connect`, the Lua bridges
`soramech.create_box` and `soramech.connect`, the dispatch
thread-local active context) all shipped and were then reverted
before the phase 3 release candidate. The design committed to a
shape — graph mutation as a language-bridge function, id-as-input,
three separate code paths — that the broader redesign walks back
from.

The redesigned approach uses a single box-kind for create /
reconfigure / delete with wire endpoints as the targeting
language. See
[`issues/419-runtime-graph-mutation.md`](419-runtime-graph-mutation.md)
for the new architectural ground rules. The historical-behavior
text below describes what the rolled-back implementation did.

## Status
rolled back · phase 4 · renumbered from 319d during the
phase-4 consolidation. Historical implementation status was
"complete (slice 1)."

## Parent issue
Sub-issue of 319. Lands the headline self-construction feature
for Lua call boxes with plain routing. C and Bash bindings,
non-call kinds, comparator / iterator / etc. routing, and
encapsulated map outputs all roll up under 319e or follow-on
slices.

## Current behavior

Two runtime functions exposed to language specs (Lua first):

- **`soramech.create_box(table)` → string** — instantiates a new
  box at runtime. The `table` argument mirrors the on-disk box
  JSON schema; anything writable as `boxes/<id>.json` is acceptable.
  Internally: serializes the Lua table to JSON via the same
  `encode_value` machinery used for cross-language wire
  serialization (issue 312); hands the JSON to the C runtime API
  which parses, allocates a box record, allocates input slots
  (319b), and appends to the graph's box index via `graph_add_box`.
  Returns the new box's id (auto-generated via 319c if the spec
  didn't supply one).
- **`soramech.connect(table, ...)`** — variadic. Each table is
  one entry shaped like a `connections[]` entry on disk
  (`{from_box, from_branch, to_box, to_input}`). Appends to the
  producer's atomic `connections` array via `box_add_connection`.

Both hard-crash via `luaL_error` on any failure (Q3 resolution).

Plumbing: a thread-local `runtime_active_context` is set by
`dispatch_action` before each spec invoke and cleared after.
The runtime builtins read the TLS to find the active graph,
slot store, and spec registry — no changes to the spec's
invoke signature were required.

## What lands in this slice

- `src/010-graph-loader.{h,c}` — `box_t *boxes` becomes
  `_Atomic(box_t **)` with copy-and-publish growth via
  `graph_add_box`; box records are now individual mallocs so
  their addresses are stable across pointer-array growth.
  Per-box `connections` becomes `_Atomic(connection_t *)` with
  the same atomic-publish + stale-list pattern via
  `box_add_connection`.
- `src/017-box-id.h/c` — already-shipped 319c utility, reused.
- `src/018-runtime-builtins.{h,c}` — the `runtime_create_box`
  and `runtime_connect` C functions plus the TLS active-context
  setter/getter pair.
- `src/012-dispatch.c` — sets and clears the TLS context around
  each spec invoke. Also: dispatch's per-box arrays
  (`box_ever_spawned`, `last_outputs`, `last_output_sizes`) get
  oversized by `RUNTIME_BOX_HEADROOM = 4096` entries at init
  so runtime-created boxes have room to participate. Proper
  growable arrays are a follow-on.
- `src/008-pool-runner.c` — `print_outputs` now iterates over
  `graph_n_boxes(ctx->graph)` (which reflects runtime additions)
  rather than `ctx->n_boxes` (capacity).
- `langs/lua/spec.c` — `lua_soramech_create_box` /
  `lua_soramech_connect` wrappers + `register_soramech_builtins`
  called from `lua_init`.
- `langs/lua/Makefile` — adds `-I$(DIR)/src` so the lua spec can
  include the runtime-builtins header.
- `Makefile` — adds `-rdynamic` to `LDFLAGS` so dlopen'd spec.so
  binaries can call back into the runner's `runtime_*` symbols.
- `tests/maps/319d-runtime-create/` — end-to-end fixture: a Lua
  box uses `soramech.create_box` to spawn an `echo_dyn` box,
  uses `soramech.connect` to wire to it, then returns a value
  that pushes through the new connection. The dynamically-
  created box fires and produces output.

## Restrictions in this slice

- **Lua only.** C and Bash specs don't yet expose `soramech.*`.
  Their bindings land in 319e.
- **Call boxes only.** `create_box` rejects `kind != "call"`.
  Read / write / encapsulated-map kinds land later.
- **Plain routing only.** Comparator / iterator / randomizer /
  weighted / distributor routing on dynamically-created boxes
  rejected.
- **Runtime-headroom cap.** Dispatch's per-box arrays support up
  to 4096 dynamically-created boxes. Beyond that, spawn checks
  silently fail. The cap is dispatch-internal; raise it or land
  growable arrays when needed.
- **Connection mutation is not race-safe — and its old safety
  argument is void.** This restriction previously read that
  `connect` is safe when called from the spec of the producer
  being wired, "since the single-spawn CAS guard means no other
  worker is reading that box's connections concurrently." That
  guard is being removed: every box is multi-spawn,
  unconditionally (see issue 304, task dispatch layer). A box
  whose spec is calling `connect` on itself may be running on
  several other workers at the same instant, each of them walking
  the very `connections[]` array being mutated — so the case this
  issue called "the common case" and "safe" is now the racy one.

  Nothing about the calling-from-a-different-box case changes; it
  was never safe. What changes is that there is no longer a safe
  case to contrast it with. Runtime connection mutation needs its
  own synchronisation before this slice can claim safety at all —
  a per-box connections lock, a read-copy-update swap of the
  array, or confining mutation to a quiescence point. This is now
  a blocker on the slice, not a documented caveat.

  The `spawned[]` per-box array that 304 deletes is also one of
  the runtime-headroom arrays the cap above refers to; re-check
  that cap's arithmetic once it is gone.

## Validation

- 13 integration tests pass, including the new
  `319d-runtime-create` fixture that exercises the full path
  end-to-end: trigger Lua box's function calls `create_box`,
  calls `connect`, returns a value; the dispatcher pushes
  through the new connection; the dynamically-created `echo_dyn`
  box fires and its output is captured.
- The slot store's 23 unit tests (including the 319b growth
  test) still pass.

## Relevant files

- `src/010-graph-loader.{h,c}`
- `src/012-dispatch.c`
- `src/008-pool-runner.c`
- `src/018-runtime-builtins.{h,c}` (new)
- `langs/lua/spec.c`
- `langs/lua/Makefile`
- `Makefile`
- `tests/maps/319d-runtime-create/`
- `scripts/run-tests.sh`
