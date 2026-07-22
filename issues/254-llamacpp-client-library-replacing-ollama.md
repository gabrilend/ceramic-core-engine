# 254 — llama.cpp client library (replaces the Ollama client)

## Status

open · **Ollama is formally deprecated.** `libs/ollama.lua` now
carries a deprecation banner (and its companion
`libs/ollama.lua.info.md` opens with one); this issue is the
replacement path. The infrastructure change is deliberately minor:
the same two public function signatures, so existing call boxes
keep their port layout — only endpoints, payloads, parsing, and
defaults change.

## Motivation

llama.cpp's `llama-server` is faster and far more configurable than Ollama
(direct sampling control, GBNF-constrained output). This issue replaces the
Ollama client outright — there is no dual-backend abstraction. `libs/ollama.lua`
is retired and `libs/llamacpp.lua` becomes the single LLM client the box library
offers.

The client does not talk to a raw `llama-server`. It talks to the soramech LLM
**manager** (issue 255) — a small HTTP service that owns model lifecycle and
routes each request to the right per-model backend. That is what keeps `model` a
meaningful argument (see below) and what lets a box left otherwise-blank "just
work" the way it did against Ollama's daemon.

## Current behavior

`libs/ollama.lua` is the only LLM client. It exposes two public functions used
as a box source (copy into a map's `src/`, the file browser auto-populates
ports):

- `M.query(prompt, model, host)` — POSTs to Ollama `/api/generate`, returns the
  `response` field.
- `M.chat(prompt, system, model, host)` — POSTs to Ollama `/api/chat`, returns
  `message.content`.

Both shell out through `curl` via `io.popen` (`curl_post` / `shell_quote`),
default `host` to `http://localhost:11434`, and default `model` to the literal
name `llama3.2`, which Ollama's daemon resolves against models it has pulled.

That nil-argument defaulting is a silent-fallback pattern the project
forbids (fallbacks are warnings, warnings are errors) — part of why
this library is deprecated rather than extended. The replacement
moves the name→model mapping into the manager's config (issue 255)
instead of baking a model name into a library.

## Intended behavior

A new `libs/llamacpp.lua` exposes the **same two public function signatures** so
existing call boxes keep their port layout unchanged (the file browser derives
ports from the signature — see the insight in the phase-2 notes). Only the
endpoints, request payloads, response parsing, and defaults change:

- `M.query(prompt, model, host)` — POSTs to `POST /completion` with
  `{ prompt, model, stream=false }`, returns the `content` field of the reply.
- `M.chat(prompt, system, model, host)` — POSTs to `POST /v1/chat/completions`
  (OpenAI-compatible) with a `messages` array (optional leading `system`
  message) and `model`, returns `choices[1].message.content`.

Defaults and the `model` argument:

- `host` defaults to the **manager** address (issue 255), not a raw server —
  `http://localhost:8080` unless overridden by `config/llamacpp.conf`.
- `model` is the **routing key**. The manager holds a registry of
  `model name → backend`; the client sends the name and the manager picks or
  spawns the matching `llama-server`. When `model` is nil/empty the field is
  omitted and the manager applies its configured `default_model`. There is no
  hard-coded `llama3.2` in the client — the mapping from names to `.gguf` files
  lives in the manager's config, not baked into a library.

Cold-start behaviour is transparent to the box: if the requested model's backend
is not up yet, the manager runs the start-rendezvous (issue 255) and holds the
request until the backend is ready, so `M.query` simply takes longer on the
first call rather than failing. A `M.health(host)` helper (added in 255) lets a
box pre-warm or check readiness explicitly.

Error handling stays strict (no fallbacks, per project policy): a non-JSON
reply, an `error` field, or a missing `content` / `choices` path writes to
`stderr` and returns nil, exactly as `ollama.lua` does today.

## Suggested implementation steps

1. Write `libs/llamacpp.lua`, reusing the `shell_quote` / `curl_post` pattern
   from `libs/ollama.lua` (curl via `io.popen`, `--max-time`, POST with
   `Content-Type: application/json`). Raise `--max-time` enough to cover a cold
   start held open by the manager.
2. Implement `M.query`: build the `/completion` payload (`prompt`, `model`),
   parse `resp.content`.
3. Implement `M.chat`: build `messages` (optional system), POST to
   `/v1/chat/completions`, parse `resp.choices[1].message.content`. Guard every
   nested access and error to stderr on a bad shape.
4. Ship `libs/llamacpp.lua.info.md` documenting the public API of `M.query` and
   `M.chat` (and `M.health`, once 255 adds it), matching `libs/text.lua.info.md`.
5. Retire `libs/ollama.lua`: remove it and grep the tree for `ollama`, `11434`,
   `api/generate`, `api/chat` — update any map `src/` copies, docs, and demo
   references. Record what was found in this issue's final notes.
6. Add a "Superseded by 254" pointer to completed issue 209 and to issue 210's
   Ollama mentions (adding to an issue is permitted; deleting is not).
7. Confirm `require("llamacpp")` resolves — `drivers/lua.sh` already puts
   `${DIR}/libs/?.lua` on `LUA_PATH`, so no driver change should be needed.

## Blocks / blocked by

- Blocks: 256 (sampling + GBNF) and 257 (embeddings) extend this client.
- Blocked by: 255 for the real routing/lifecycle contract. The client can be
  written and unit-tested against a single hand-started `llama-server` first
  (same endpoints), then gains name-routing for free once the manager is up —
  the request shape is identical either way.

## Related documents

- libs/ollama.lua — the client being replaced (`M.query`, `M.chat`, `curl_post`)
- libs/text.lua.info.md — reference format for the new `.info.md`
- issues/255-llama-server-manager-daemon.md — the manager the client targets
- drivers/lua.sh — sets `LUA_PATH` so `require("llamacpp")` works
- issues/completed/209-ollama-query-library.md — the superseded original
- docs/005-writing-boxes.md — how a Lua file becomes a box source
