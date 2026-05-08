# 209 — Ollama query library

## Status

completed

## Current behavior

No built-in way to send prompts to a local Ollama instance from a map box.

## Intended behavior

A shared Lua library at `libs/ollama.lua` exposes two public functions:

- `M.query(prompt, model, host)` — plain text generation via /api/generate
- `M.chat(prompt, system, model, host)` — chat completion with optional system
  prompt via /api/chat

`model` and `host` default to `llama3.2` and `http://localhost:11434` when nil
or empty string. This lets the user set them as literal port values in the
inspector and leave them blank to use the defaults.

The file doubles as a box source: copy it to any map's src/ directory and the
file browser will detect both functions and auto-populate ports. The model,
host, and system inputs are natural candidates for literal port values; prompt
is typically wired from an upstream box.

Uses `curl` via `io.popen` — no additional Lua packages needed.

## Suggested implementation steps

1. Write `libs/ollama.lua` with `M.query` and `M.chat`.
2. To use: copy to `maps/<name>/src/ollama.lua`, create a call box, browse to
   it, pick the function, set model/host/system as literal port values.

## Implementation notes

`libs/ollama.lua` implements `M.query(prompt, model, host)` and `M.chat(prompt, system, model, host)`, both defaulting to llama3.2 and http://localhost:11434. All HTTP is done via `curl` through `io.popen` — no extra Lua packages required. A companion `libs/ollama.lua.info.md` lists the public API. To use in a map, copy the file to the map's `src/` directory; the file browser will detect both functions and auto-populate the port list.

## Related documents

- libs/ollama.lua — implementation
- assets/js/004-inspector.js — literal port values (issue 208)
- drivers/lua.sh — sets libs/ on package.path so require("ollama") works
