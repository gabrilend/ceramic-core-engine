# langs/bash/spec.c — public surface

Bash language spec. Built as `spec.so`; `dlopen`'d by the pool
runner at startup. Fully implemented (issue 308).

## External symbols

- `lang_spec_t soramech_lang_spec` — the exported spec record.
  - `name` = `"bash"`, `file_ext` = `".sh"`
  - `init` / `teardown` — per-worker server subprocess lifecycle
  - `invoke` — one request/response over the socketpair
  - `native_to_json` / `json_to_native` — wire bridges
  - `compile` — none; shell needs no build step
  - `translate_targets` = `{"lua", "c"}`
  - no `invoke_wrote_native`, which the dispatch reads as "I write
    what I'm asked"

## Out of process, and persistent

This is the only spec whose boxes do not run inside the runner.
Each worker's `init` forks a persistent `bash-server.sh` and keeps
a socketpair to it; `invoke` writes a request and reads a response
over a line protocol. The subprocess lives for the worker's
lifetime.

The alternative — fork a shell per fire — would put process
creation on the hot path of every single box invocation. The
persistent server pays that cost once per worker.

`bash-server.sh` is found relative to the loaded `spec.so` via
`dladdr`, so the spec locates its own sibling script no matter
where the artifact was copied to.

## Environment

- `SORAMECH_BASH_PRESOURCE_FILES=<paths>` — files each worker's
  server sources once at startup, before any box runs. This is
  where shared shell functions go; sourcing them per fire would
  defeat the persistent server.

## Thread safety

Nothing is shared in memory — each worker has its own subprocess,
and each fire is a request on that worker's own socket. The shared
resource is the **filesystem**. Because every box is multi-spawn,
two fires of one box can be mid-write to the same path at once.
Write to distinct paths, or stage and atomically rename. See
`docs/005-writing-boxes.md`.

## Values across wires

Everything is text. Crossing to Lua or C goes through JSON; a
same-language Bash→Bash wire passes the bytes through.

## Build

- `make` here, or `make specs` from the project root. Links
  nothing beyond libc.

## Related

- Issue 308 — the implementation.
- Issue 325 — `translate_targets`.
- `langs/lang-spec.h` — the contract.
- `docs/005-writing-boxes.md` — the Bash box author's quickstart.
