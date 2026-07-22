# 259 — code-extraction box: LLM text in, code appended to the main-file-slice

## Status

open · concept captured 2026-07-22; pairs with the llama.cpp
family (254–257). The full pipeline this box completes: input text
describing a thing → an LLM call that attempts to implement it in
code (only code) → the code accumulates onto the file being built.

## Current behavior

No box kind understands "code" as a material. An LLM's response
arrives on a wire as plain text; nothing extracts code from it,
nothing accumulates code into a file being assembled, and a
response that came back with no code in it flows downstream like
any other string — the failure is invisible. Building
"describe a thing, get an implementation appended to a file"
today means hand-writing one Lua box that does everything:
prompt, parse, append, retry.

## Intended behavior

A new box kind — working name `code_extract` — that sits
immediately downstream of an LLM call box (the llama.cpp client,
issue 254, is the intended previous box in the chain):

- **Input port `text`** — the received text, normally the LLM's
  response, wired from the previous box.
- **Input port `include_comments`** — 0 or 1. The box carries a
  checkbox inside it (a default value stored on the box like any
  literal); when a different value arrives on the wire, that value
  is used instead. The two-input-methods ruling (bug 324) gives
  this shape for free: a checkbox with no wire is a referenced
  constant, a wire-fed value overrides per fire.
- **Extraction.** The box scans the text for `[code]:` tool calls
  carrying code inside `code{ }` blocks. Every part of the text
  outside the code blocks is considered a comment.
- **On success** the box appends to the current main-file-slice:
  the extracted code always, and the comment text — rendered as
  comments in the slice's language — only when include_comments
  is true. Its single output emits the appended code so
  downstream boxes can observe, compile, or route it.
- **On a text with no `[code]:` block** the box emits a warning
  event to the run transcript and fires its `retry` branch, which
  the user wires back to the previous box's input — re-running
  the LLM call. This is the vision document's existing rule
  ("for LLM classification, else retries upstream — the user
  wires it") applied to code: the retry is routing, expressed by
  a wire in the recursive network, not a new runtime mechanism.
- **The main-file-slice** is the file being grown: a path
  configured on the box. Appends are atomic and newline-
  terminated, so concurrent slices interleave whole blocks, never
  partial lines.

## Suggested implementation steps

1. **Schema and editor surface.** `src/001-schema.lua` accepts the
   new kind with the two ports, the include-comments checkbox
   default, and the slice path; the loader gains the kind enum;
   the editor gets the kind in its dropdown, a checkbox control,
   a slice-path field, and a fresh KIND_COLOR entry.
2. **The splitter.** A small parser that walks the text once and
   yields an ordered list of (comment | code) segments from the
   `[code]:` / `code{ }` grammar. Settle the exact delimiter rules
   here first — nesting, unterminated blocks (treat as comment and
   count as "no code found"), multiple blocks per response
   (append in order).
3. **Comment rendering.** Comments are prefixed per the slice's
   language, resolved from the file extension through the language
   specs' declared extensions — the specs already know which
   extension is whose.
4. **Routing.** Two named branches, `code` and `retry`, on the
   comparator-branch machinery (`from_branch`); the retry fire
   also writes a new transcript event so the lap is visible in
   the log, warning included.
5. **The append primitive.** Decide: teach the write box an
   `append` flag and compose (extractor emits, append-writer
   lands), or let this box own its file append. The composed
   shape is smaller and keeps disk-writing in one kind; the owned
   shape keeps the box self-contained. Lean composed.
6. **Fixtures.** A canned "LLM response" via read box → extractor
   → assert the slice's bytes with the checkbox on and off; a
   no-code response looping the retry branch through a counter
   guard, asserting the warning event and the bounded re-ask.
7. **GBNF tie-in.** Issue 256's grammar-constrained output can
   force the `[code]:` shape at generation time, making the retry
   lap rare instead of routine — note the pairing in both issues.

## Related tools / files

- [254](254-llamacpp-client-library-replacing-ollama.md) — the
  LLM call box this sits behind (the "previous box")
- [256](256-sampling-controls-and-gbnf-constrained-output.md) —
  grammar constraint that makes code-only responses enforceable
- [324](completed/324-multi-fire-boxes-consume-their-literal-inputs.md)
  — the input-methods ruling the checkbox default rides on
- notes/vision — the "else retries upstream (user wires it)" rule
- `src/001-schema.lua`, `src/010-graph-loader.c`,
  `src/012-dispatch.c` — where box kinds live
- `docs/002-map-model.md` — box kinds and routing to update
