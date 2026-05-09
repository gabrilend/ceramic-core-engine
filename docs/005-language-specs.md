# SoraMech — Writing a Language Spec

## Status
Stub. Filled out as the phase 3 specs are implemented (issues
306–308). For the formal contract, see issue 303. For the existing
overview, see `docs/003-driver-system.md`.

## What this doc will cover (when complete)

A walk-through of writing a new language spec, intended for a user
adding Python, Rust, Haskell, or any language not shipped by default.
Sections to fill in once the reference specs are written:

- **The contract.** What `lang_spec_t` requires you to provide
  (`init`, `teardown`, `compile`, `invoke`). Pointer to issue 303.
- **In-process vs out-of-process.** Lua and C run in the worker
  thread directly; Bash talks to a persistent subprocess over a
  socket. Which model fits your language depends on whether it has
  an embeddable runtime. Pointer to `docs/004-ipc-and-threading.md`.
- **Walkthrough: the Lua spec.** The simplest reference. Covers
  per-worker `lua_State`, registry-based file caching, typed input
  marshalling, return-value serialization. Pointer to issue 306.
- **Walkthrough: the C spec.** Adds compile-time wrapper generation
  for arbitrary user functions. Pointer to issue 307.
- **Walkthrough: the Bash spec.** The out-of-process model. Wire
  protocol, framed length-prefix messages, persistent subprocess
  lifecycle. Pointer to issue 308.
- **Adding a new language.** Directory layout, Makefile, the bare
  minimum to link against the spec interface, dropping the result
  into `langs/<name>/`.
- **Thread safety.** Per-worker state isolation. The C spec's
  `static` and `globals` race story (issue 307); cooperative
  patterns for languages with mutable global state.
- **Error handling.** Hard-crash policy (issue 303). What
  language-level errors look like at the spec boundary. How to
  catch them without losing context.

## Why a stub now

Issues 303, 306, 307, and 308 reference this document. Creating a
placeholder establishes the file path and the table of contents
above so cross-references resolve. The full content is written
incrementally as the reference specs are implemented and there is
real code to walk through.

## Relevant issues

- 303 — language runtime spec (the contract)
- 306 — Lua language spec
- 307 — C language spec
- 308 — Bash language spec
- `docs/003-driver-system.md` — phase 2 driver overview, phase 3
  spec overview
- `docs/004-ipc-and-threading.md` — IPC options and threading
  roadmap
