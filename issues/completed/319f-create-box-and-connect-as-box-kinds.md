# 319f — `create_box` and `connect` as language-agnostic box kinds

## Status
complete

## Parent issue
Design-correction follow-on to 319d/319e. Per parent-issue
review feedback, the per-language wrappers added in 319d (Lua
binding) and 319e (C binding) are an architectural smell — they
add work per implementer rather than per primitive. The right
shape for these primitives is **box kinds**, the same way `read`
and `write` are language-agnostic kinds the runtime owns and
every language uses by wiring values in and out.

## Current behavior

Two new box kinds, both language-agnostic, both implemented in
`src/012-dispatch.c` (sibling to `do_write_box`):

- **`kind: "create_box"`** — single input port named `spec`,
  carrying the JSON box-schema text for the new box. The box's
  output is the new box's id string. Implementation:
  `do_create_box_box` reads the input, calls
  `runtime_create_box` (the same C function the per-language
  wrappers call), writes the id to the output buffer.

- **`kind: "connect"`** — single input port named `connection`,
  carrying one connection-entry JSON object. The box's output is
  the literal `"true"`. Implementation: `do_connect_box` mirrors
  `do_create_box_box`'s shape.

The dispatch's per-task TLS (set by `dispatch_action` in 319d)
makes `runtime_create_box` and `runtime_connect` reach the
active graph + slot store + spec registry without needing them
threaded through the new box-kind `do_*` signatures.

## Why box kinds instead of per-language callbacks

Per the strategem `strategems/contracts-must-fit-every-implementer.md`,
a contract that requires per-language wrappers is one the
awkward implementer (Bash) can't satisfy without shims. The
per-language wrappers from 319d/319e:

- worked cleanly for Lua (registered as `soramech.create_box` /
  `soramech.connect`)
- worked cleanly for C (header alias macros over the runtime_*
  symbols)
- were genuinely hard for Bash (needs line-protocol extension
  back to the runner — the gap that made 319e defer Bash)

The box-kind shape collapses all three cases: every language can
write a value to an output wire; every language can consume a
value from an input wire. Routing the value through a special
box kind that knows what to do is the universal mechanism. Bash
gets self-construction for free now — its box function just
writes the spec string, no protocol extension needed.

## Compatibility with the per-language wrappers

The Lua and C wrappers from 319d/319e are not removed. Existing
maps that use them continue to work; the box-kind path is the
new idiomatic way. Per the strategem `redesigns-change-the-
surface-too.md`, the deprecation of the per-language wrappers
is acknowledged here as the right direction — both paths reach
the same `runtime_create_box` / `runtime_connect` C functions,
so behaviour is identical. The on-ramp for new users and the
documented examples should be the box-kind path; the per-language
wrappers are grandfathered.

## Validation

- 18 integration tests pass (one new fixture
  `tests/maps/319-box-kind-create/` exercises the create_box kind
  end-to-end: a read box emits a spec, a create_box-kind box
  consumes it, the new box's id `auto_00000000` appears in
  captured outputs).
- The slot-store unit suite (23 passing) and the rest of the
  per-component suites all still pass.

## Relevant files

- `src/010-graph-loader.h` — `BOX_CREATE_BOX`, `BOX_CONNECT`
  added to `box_kind_t`.
- `src/010-graph-loader.c` — parse `"create_box"` / `"connect"`
  from JSON `kind` field; include these kinds in the
  "runs as a task" filter used by entry-box detection and
  read-predecessor-list building.
- `src/012-dispatch.c` — `do_create_box_box` /
  `do_connect_box` implementations; cases added to
  `dispatch_action`'s switch.
- `tests/maps/319-box-kind-create/` — end-to-end fixture.
- `scripts/run-tests.sh` — invokes the fixture.

## Deferred / follow-on

- The per-language Lua/C wrappers from 319d/319e are still
  present. A future cleanup could remove them in favour of the
  box-kind path exclusively. Until then, both work.
- Bash maps that want to use self-construction can now drop a
  `create_box` kind box on the canvas; no Bash-spec changes
  needed.
