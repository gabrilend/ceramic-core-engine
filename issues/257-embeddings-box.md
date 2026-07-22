# 257 — Embeddings box

## Status

open

## Motivation

`llama-server` exposes an embeddings endpoint that turns text into a vector.
The Ollama client never wrapped this, so the box library has no way to measure
how *similar* two pieces of text are without a full generation call. An
embeddings box opens up similarity-based routing and lookup — cheaper and more
deterministic than asking a model "are these the same?" in prose.

## Current behavior

`libs/llamacpp.lua` (after issue 254) offers generation only — `M.query` and
`M.chat`. There is no embeddings function; similarity has to be faked with a
generation prompt, which is slow and unreliable.

## Intended behavior

`libs/llamacpp.lua` gains a third public function usable as a box source:

- `M.embed(text, host)` — POSTs to `POST /v1/embeddings` with `{ input=text }`,
  returns the embedding as a Lua array of numbers (`resp.data[1].embedding`).

Because a raw vector is awkward to carry on a single wire and to eyeball in the
editor, also ship a small similarity helper so the common downstream use — "how
close are these two texts?" — is one box, not vector plumbing the user has to
assemble:

- `M.similarity(text_a, text_b, host)` — embeds both and returns their cosine
  similarity as a single number in `[-1, 1]`.

The single-number output of `M.similarity` drops straight onto a comparator box
(issue 210): compare against a threshold literal and branch `lt` / `eq` / `gt`.
That gives similarity-based routing with the machinery that already exists.

Server note: a `llama-server` only serves embeddings if it was launched with
`--embedding` and an embedding-capable model. That becomes a per-model flag in
the manager's registry (issue 255): a managed model entry carries an
`embedding = true` flag, and the manager adds `--embedding` when it spawns that
model's backend. The `model` argument routes `M.embed` to that backend the same
way generation routes. Error clearly (not a fallback) if a routed model returns
"embeddings not supported."

## Suggested implementation steps

1. Add `M.embed(text, host)` to `libs/llamacpp.lua`, parsing
   `resp.data[1].embedding`; error to stderr and return nil on a bad shape.
2. Add `M.similarity(text_a, text_b, host)` — two `M.embed` calls plus a cosine
   over the vectors. Keep the cosine as a named local helper.
3. Document both in `libs/llamacpp.lua.info.md`, noting the `--embedding` server
   requirement.
4. Add the `embedding = true` per-model flag to the issue 255 registry schema so
   the manager spawns that model's backend with `--embedding`.
5. Demo/test: a `M.similarity` box into a comparator box, routing two near-
   identical strings down the `gt` branch and two unrelated strings down `lt`.

## Open design question (worth a human call at implementation time)

Where the cosine lives: inside `M.similarity` (convenient, but bakes one metric
in), or as its own tiny box so the raw vectors from `M.embed` can feed other
distance metrics (dot product, euclidean) the user wires up themselves. The
project's separation-of-concerns leaning (data generation vs. data viewing)
argues for exposing `M.embed`'s raw vector and keeping the metric a separate,
swappable box; `M.similarity` can stay as a convenience wrapper on top.

## Blocks / blocked by

- Blocked by: 254 (extends its client). Needs 255's managed mode configured with
  `--embedding` to run end-to-end.
- Blocks: nothing.

## Related documents

- libs/llamacpp.lua — client being extended (issue 254)
- issues/255-llama-server-manager-daemon.md — the `embedding` per-model registry
  flag and request routing
- issues/completed/210-llm-route-box.md — the comparator that consumes the
  similarity number
