# ollama.lua

> **DEPRECATED.** Scheduled for replacement by `libs/llamacpp.lua`
> per issue 254 (issues/254-llamacpp-client-library-replacing-ollama.md).
> The replacement keeps both public signatures, so existing call
> boxes keep their port layout — but **new maps should not adopt
> this library**.

Ollama API client. `require("ollama")` from any Lua function, or
copy into a map's `src/` and use it directly as a call box via the
file browser. Transport is `curl` shelled out through `io.popen`
with a 120-second cap and shell-quoted arguments; the only Lua
dependency is dkjson. Both functions block until the model
finishes (streaming is off).

## Known deficiency — silent defaults

When the model or host argument is nil or empty, the library
silently substitutes a hard-coded default model (`llama3.2`) and
default host (`http://localhost:11434`). This is a fallback
pattern the project forbids — fallbacks are warnings, warnings are
errors — and it is part of the motivation for retirement: the
replacement moves the name-to-model mapping into the LLM manager's
config (issue 255) instead of baking a model name into a library.

Errors also soft-fail: a malformed response or an error field from
the daemon writes one line to stderr and returns nil on the output
wire rather than halting the box.

## M.query(prompt, model, host) → string|nil

Plain text generation against the daemon's generate endpoint.

| param  | type   | description                                        |
|--------|--------|----------------------------------------------------|
| prompt | string | the text to complete (any value is tostring'd)     |
| model  | string | optional — nil/empty silently uses the default     |
| host   | string | optional — nil/empty silently uses the default     |

Returns: the model's generated text, or nil on any error (details
on stderr).

## M.chat(prompt, system, model, host) → string|nil

Chat-style query against the daemon's chat endpoint. Builds a
messages array: an optional leading system message (omitted when
nil or empty), then the user prompt.

| param  | type   | description                                        |
|--------|--------|----------------------------------------------------|
| prompt | string | the user message                                   |
| system | string | optional system prompt — nil/empty omits it        |
| model  | string | optional — nil/empty silently uses the default     |
| host   | string | optional — nil/empty silently uses the default     |

Returns: the assistant message's text content, or nil on any error
(details on stderr).

## Related

- Issue 254 — the llama.cpp client that retires this library.
- Issue 255 — the LLM manager daemon the replacement talks to.
