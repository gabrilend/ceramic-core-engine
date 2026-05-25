# 319e — C bindings, end-to-end test, docs (Bash deferred)

## Rolled back — superseded by phase 4

The C bindings that shipped here (`soramech_create_box`,
`soramech_connect` aliases in `langs/c/soramech.h`) and the
cross-language end-to-end fixture were reverted with the rest of
the runtime-mutation surface. The Bash bindings were never
built; they were deferred at the time, and now stay deferred
inside the phase-4 redesign.

The redesigned approach lives under phase 4. See
[`issues/419-runtime-graph-mutation.md`](419-runtime-graph-mutation.md).
The historical-behavior text below describes what the
rolled-back C bindings did.

## Status
complete (slice 1 — C). Bash bindings deferred to a follow-on.

## Parent issue
Sub-issue of 319. Extends 319d's runtime self-construction
primitives from Lua-only to also include C, with an end-to-end
test fixture and the docs update for `docs/004-ipc-and-threading.md`.

## What lands

- `langs/c/soramech.h` — header user C box code includes to get
  declarations for `soramech_create_box` and `soramech_connect`.
  The names are `#define`'d aliases for the runner's `runtime_*`
  symbols (already exported via `-rdynamic` from 319d).
- `langs/c/Makefile` — adds `-DSORAMECH_LANGS_C_INCLUDE=...` so the
  C spec knows the path to embed in box-compile `-I` flags.
- `langs/c/spec.c` — adds `-I<langs/c>` to every box compile so user
  source can `#include "soramech.h"`.
- `src/018-runtime-builtins.c` — removes the slice-1 "lang must be
  lua" restriction. Any language registered in the spec registry is
  now acceptable. Each language's per-worker initialisation still
  has to have happened on the dispatching worker, which surfaces
  the "worker handle NULL" error if the static graph didn't include
  that language. Workaround documented; auto-init follow-on.
- `tests/maps/319e-c-create/` — end-to-end fixture. A C box uses
  `soramech_create_box` to spawn a downstream C box at runtime,
  uses `soramech_connect` to wire its output to the new box's
  input, returns a marker. The dispatcher pushes through the new
  connection, the dynamically-created C box fires and produces
  output. Verified end-to-end.
- `docs/004-ipc-and-threading.md` — new "Runtime self-construction"
  section documents the contract, the three-layer growable
  machinery, and every slice-1 limit.

## What's deferred

- **Bash bindings.** The bash spec uses a persistent subprocess
  speaking a line-oriented request/response protocol over a Unix
  socketpair (issue 308). Exposing `create_box` / `connect` to
  user bash scripts requires either extending that protocol with
  new request types and threading them back to the C runtime, or
  giving user bash a side-channel to talk to the runner. Neither
  fits cleanly in this slice; deferred to a future sub-issue.
- **Auto-initialisation of specs for runtime-introduced languages.**
  Currently a worker only initialises specs for languages used by
  the static graph. A `create_box` call introducing a new language
  fails at dispatch time. Fix is to have specs auto-init on first
  use; not in this slice.
- **The architectural redesign.** Per parent-issue discussion, the
  correct shape is `create_box` and `connect` as box KINDS (like
  `read` / `write`), not as per-language callbacks. The current
  per-language design is the slice-1 expedient. Refactor planned
  but explicitly out of scope here.

## Validation

- 14 integration tests pass, including the new `319e-c-create`
  fixture and the existing `319d-runtime-create` from the Lua
  side.

## Relevant files

- `langs/c/soramech.h` (new)
- `langs/c/Makefile`
- `langs/c/spec.c`
- `src/018-runtime-builtins.c`
- `tests/maps/319e-c-create/`
- `docs/004-ipc-and-threading.md`
- `scripts/run-tests.sh`
