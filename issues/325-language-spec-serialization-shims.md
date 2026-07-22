# 325 — language-spec serialization shims: translate values to the target language

## Status

open · design directive captured 2026-07-21; this settles the
"decision to settle" left open in
[323](323-same-language-fast-path-drops-table-values.md).

## Current behavior

First slice landed 2026-07-22: the declaration layer exists; the
per-pair serialization layer is still the shared JSON machinery.

- The spec interface (`langs/lang-spec.h`) carries
  `translate_targets`: each spec names the languages it declares
  it can serialize values for. All three shipped specs declare
  every other shipped language — JSON is now the *declared shim
  choice* for those pairs instead of an implicit fallback.
- The load-time wire walker fails the load — fatally, naming the
  pair and the fix — on any cross-language edge whose producer
  does not declare the consumer's language. Same-language pairs
  are implicitly declared (identity needs no shim). A user-added
  language with no declarations errors on its first
  cross-language wire, which is the intended onboarding shape:
  the author writes the translation story first.
- The lookup (`spec_declares_target` in the spec registry) is a
  pure function with unit coverage over synthetic and real specs.
  Underneath, bugs 323 and 324 already fixed how tables and
  typed-in constants travel.

Still open — the serialization layer itself: a per-pair serialize
callback the dispatch consults at push time in place of the single
JSON bridge; replacing the same-language brace-sniff with the
declared Lua→Lua shim; per-pair fixtures asserting byte-level
expectations; the writing-boxes doc teaching the declaration.

## Intended behavior

Cross-language transmission is explained *in the language spec
itself*, as serialization, not as commentary: each spec carries
**manually written shims** that translate its values into text in
the form the intended target language consumes. The shim IS the
translation — "how does a Lua table become something Bash can
read" is answered by a small function that serializes it that way,
per producer→consumer language pair.

Consequences:

- A box author returns a value; which wire it crosses and which
  language consumes it never changes what arrives — the pair's
  shim defines the text form, deterministically.
- Generic JSON remains available, but as an explicit shim choice
  for a pair ("Lua→C translates via JSON"), not as an implicit
  fallback the runtime reaches for.
- A wire between languages with no shim for the pair is a loud
  load-time error from the wire walker — never a silent
  best-effort coercion (fallbacks are warnings, warnings are
  errors).
- Same-language pairs may declare identity / by-reference as their
  shim (the ceiling fix of 323 — the whole-program merge — is the
  extreme form of that).

## Suggested implementation steps

1. Extend the language-spec interface (`langs/lang-spec.h`) with a
   translate-to-target surface: given a value and a target
   language name, produce the wire bytes. Start with the three
   shipped languages — nine pairs, most sharing a JSON-shim
   implementation, declared per pair rather than assumed.
2. Teach the load-time wire walker (the same pass that validates
   sentinel capability masks, `src/020-sentinels.c`) to check each
   edge: producer spec must declare a shim for the consumer's
   language, else load fails naming the pair.
3. Replace the same-language fast path's string coercion with the
   pair's declared shim (for Lua→Lua that shim routes tables
   through the encoder — 323's floor fix falls out of this shape).
4. Fixtures: one map per language pair exercising a structured
   value across the wire; assert byte-level expectations.
5. Update `docs/005-writing-boxes.md` and the spec info.md files:
   a user adding a language writes the shims for each language
   they want to interoperate with — the spec is where translation
   is explained, in executable form.

## Related tools / files

- [323](323-same-language-fast-path-drops-table-values.md) — the
  silent table drop this design repairs at the root
- `langs/lang-spec.h` — the spec interface to extend
- `langs/lua/spec.c`, `langs/c/spec.c`, `langs/bash/spec.c` — the
  shipped specs that gain shim tables
- `src/020-sentinels.c` — the capability-mask walker the per-edge
  shim check rides alongside
- [312](completed/312-same-language-wire-fast-path.md) /
  [313](completed/313-research-whole-program-same-language-merge.md)
  — the fast path and merge research this reframes
