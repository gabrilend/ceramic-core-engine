# 256 — Sampling controls and GBNF-constrained output

## Status

open

## Motivation

The reason to move off Ollama is customization. `llama-server` exposes the full
sampler and — the big one — **GBNF grammars** that force the model's output into
a guaranteed shape. Ollama's client had none of this. This issue turns those
knobs into box-level controls, and makes the grammar path a first-class partner
to the comparator/routing boxes.

## How GBNF works (for the reader)

GBNF ("GGML BNF") is a grammar string handed to `llama-server` in a `grammar`
field. Mechanically: at each generation step the model produces a probability
over the entire vocabulary. With a grammar active, llama.cpp tracks a parser
state ("given what's produced so far, which tokens could legally come next?")
and zeroes the probability of every token that would violate the grammar before
sampling. The model can only ever pick a token that keeps the output on a valid
path, so the result is *provably* valid against the grammar — not "usually
valid," impossible-to-be-otherwise.

Grammars are named rules; `root` is the entry point:

```gbnf
root ::= "yes" | "no"                    # a strict enum
root ::= "-"? [0-9]+ ("." [0-9]+)?       # a decimal number, nothing else
```

This is why it pairs with issue 210's comparator: the executor **halts** when a
box's output is not a strict number. A number-grammar LLM box *guarantees* a
numeric wire value — no parsing, no halts, no fallback.

## Current behavior

After issue 254, `libs/llamacpp.lua` offers `M.query` / `M.chat` with fixed
sampling — whatever `llama-server` defaults to. There is no way to set
temperature or constrain the output shape from a box.

## Intended behavior

`M.query` and `M.chat` accept an **optional trailing options table** so the
existing positional signatures stay drop-in, and boxes can set individual knobs
as literal port values. Recognised keys map straight onto the `/completion`
request fields (and the chat endpoint's equivalents):

- `temperature`, `top_k`, `top_p`, `min_p`, `repeat_penalty` — sampler knobs.
- `n_predict` — max tokens to generate (`max_tokens` on the chat endpoint).
- `grammar` — a GBNF string passed through verbatim.

Any key left nil is omitted from the payload (server default applies). Unknown
keys are an error, not a silent drop — a typo'd knob should be loud.

The manager (issue 255) proxies the request body through to the resolved backend
unchanged, so these fields need no manager-side handling — they ride along to the
per-model `llama-server` as-is. The only manager concern is that the routed model
actually supports them (all do for sampling; grammar needs no special flag).

For the common constrained cases, ship a tiny grammar helper module so box
authors don't hand-write GBNF for the basics:

- a number grammar (feeds a comparator box safely),
- an enum grammar built from a list of allowed strings (feeds routing),
- a strict-JSON grammar.

The helper returns the GBNF string to drop into the `grammar` option. Whether it
lives in `libs/llamacpp.lua` or a companion `libs/gbnf.lua` is an implementation
choice; the enum/number/JSON builders are the required surface either way.

## Suggested implementation steps

1. Extend `M.query` / `M.chat` in `libs/llamacpp.lua` to accept an options table
   and merge recognised keys into the request payload; error on unknown keys.
   Use a whitelist table (key -> payload field) rather than a chain of ifs.
2. Route `grammar` to `/completion`'s `grammar` field; for `M.chat`, use the
   grammar extension the OpenAI-compat endpoint accepts (confirm the exact field
   against the running server's `/props` at build time).
3. Add the grammar builders (number / enum / strict-JSON) and document them in
   the `.info.md`.
4. Add a demo/test: a number-grammar `M.query` box wired into a comparator box,
   proving the output always lands as a strict number and the comparator never
   halts.

## Open design question (worth a human call at implementation time)

Knob delivery: one options **table** argument (compact, but the file browser
sees a single opaque port), or individual optional ports per knob (more ports to
auto-populate, but each is a literal you can set in the inspector like `model`
and `host` are today). The choice interacts with how the file browser
auto-detects ports in issue 207/208. Recommend individual optional ports for the
handful of common knobs and a table only for the long tail.

## Blocks / blocked by

- Blocked by: 254 (extends its client). Benefits from 255 (a live server to
  test grammars against).
- Blocks: nothing; this is a capability layer.

## Related documents

- libs/llamacpp.lua — client being extended (issue 254)
- issues/completed/210-llm-route-box.md — the comparator the number grammar feeds
- assets/js/004-inspector.js — literal port values (issue 208), for knob ports
- assets/js/007-filebrowser.js — port auto-population (issue 207)
