# 255 — llama-server manager daemon (Ollama, re-implemented on llama.cpp)

## Status

open

## Motivation

Ollama gave the project three conveniences for free: it was always running, it
addressed models by name, and it pulled/loaded them on demand. A bare
`llama-server` gives none of these — it is one process, one model, launched by
hand against one `.gguf` file. This issue rebuilds those conveniences as a small
HTTP **manager** that sits on the LLM host (local or remote), owns every
`llama-server` process, and routes requests to them by model name. The client
(issue 254) only ever talks to this manager.

This is also the answer to "who watches the watcher": the manager is the watcher,
and one mechanism — the start-rendezvous below — covers cold start, duplicate
concurrent starts, and re-spawn after a crash.

## Current behavior

Nothing in the project starts or supervises an inference server. `libs/ollama.lua`
assumed a daemon on `localhost:11434`. There is no model registry, no health
gate, and no routing — a dead or wrong-model server surfaces only as a failed
`curl`.

## Intended behavior

A long-running HTTP service (Lua, reusing the HTTP machinery in
`src/005-http-server.lua`) with the following parts. Because one `llama-server`
serves exactly one model, everything below exists to turn "many named models"
into "the right single-model process, running, reachable."

### Model registry (config)

`config/llamacpp.conf` maps model names to backends. Each entry is one of:

- **managed** — `{ gguf = <path>, ctx_size, gpu_layers, parallel }`. The manager
  spawns a `llama-server` for it on demand.
- **external** — `{ url = <host:port> }`. An already-running server the manager
  only routes to; it never spawns or stops this one.

Plus a top-level `default_model` (used when a request omits `model`) and the
manager's own `host` / `port`. This is where "both managed and external" lives —
per model, not a global switch.

### Lazy per-model spawn

On the first request (or explicit start) for a managed model, the manager picks
a free port, launches `llama-server -m <gguf> -c <ctx_size> -ngl <gpu_layers>
-np <parallel> --host 127.0.0.1 --port <chosen>`, logs to the `tmp/` RAM
directory, and records `name → { pid, port, state }` in its live registry. One
process per model; models not yet requested cost nothing.

### Idempotent start + notify-list rendezvous

This is the core coordination rule. A `POST /start` (and, implicitly, any
generation request for a not-yet-ready model) does:

- If the model's backend is **ready** → respond ready immediately.
- If a spawn is **already in progress** → drop the duplicate spawn silently (do
  **not** launch a second process), and append this requester to the model's
  **waiter list**.
- If **not started** → begin exactly one spawn, append the requester to the
  waiter list.

The manager polls the backend's `GET /health` until it returns ok, then flushes
the whole waiter list at once — every held request completes together. The
simplest notify mechanism is a held (long-poll) HTTP response per waiter: the
waiter list *is* the set of pending responses, flushed on ready. Document the
long-poll timeout so a wedged load surfaces as an error, not an infinite hang.

### Request routing / proxying

The manager exposes the same generation endpoints the client speaks —
`POST /completion` and `POST /v1/chat/completions` — plus `POST /start` and
`GET /health`. On a generation request it reads `model`, resolves it through the
registry (running the rendezvous if needed), then proxies the request to that
backend's port and streams the reply back. Use a dispatch table keyed on the
resolved model → backend, not a chain of ifs.

### Crash handling falls out of the same path

No separate supervisor loop. If a backend's health check fails (crash, OOM
kill), the manager marks that model `not started`. The next request re-enters
the start-rendezvous and spawns a fresh process. Fire-and-forget and
auto-restart become the same code path, triggered lazily by demand.

### Client-side helper

`libs/llamacpp.lua` gains `M.health(host)` (hits the manager's `/health`) so a
box can pre-warm a model or gate on readiness before firing.

## Suggested implementation steps

1. Define the `config/llamacpp.conf` schema (per-model managed/external entries,
   `default_model`, manager `host`/`port`). Readable by the Lua manager.
2. Build the manager HTTP service on `src/005-http-server.lua`: route table for
   `/start`, `/health`, `/completion`, `/v1/chat/completions`.
3. Implement the live registry (`name → {pid, port, state}`) and lazy spawn with
   `tmp/` logging and free-port selection.
4. Implement the start-rendezvous: single-flight spawn + per-model waiter list +
   `/health` poll + flush-on-ready. This is the piece to test hardest — fire
   several concurrent `/start`s for one model and assert exactly one process
   spawns and every caller is released.
5. Implement generation proxying by resolved model.
6. Implement crash re-spawn via the same rendezvous (kill a backend mid-run,
   assert the next request brings it back).
7. Add `M.health` to `libs/llamacpp.lua`; a `scripts/llama-manager.sh` launcher
   with the `${DIR}` header convention starts/stops the manager itself.
8. Test end to end: two managed models + one external entry; confirm routing
   lands each request on the right backend and the external one is never
   spawned or stopped.

## Blocks / blocked by

- Blocked by: 254 (shares the request/response contract and `config/llamacpp.conf`).
- Blocks: full end-to-end use of 256/257 (they need a live, routed backend).

## Related documents

- src/005-http-server.lua — HTTP machinery the manager reuses
- config/editor-server.lua — existing config file, for style reference
- scripts/start-server.sh — existing launcher, for the `${DIR}` + tmp pattern
- libs/llamacpp.lua — client that targets the manager, and `M.health` (issue 254)
- docs/004-runtime.md — where server readiness fits the run lifecycle
