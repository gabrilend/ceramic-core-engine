# 325 — language-spec serialization shims: translate values to the target language

## Status

complete · design directive captured 2026-07-21 (settling the
"decision to settle" left open in
[323](completed/323-same-language-fast-path-drops-table-values.md));
landed across three slices, 2026-07-22. The per-pair custom wire
form is a reserved extension point, not deferred work — see the
completion notes.

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

Still open — the serialization layer itself, with the design now
grounded in how the dispatch actually works (recon 2026-07-22):

- **A push-time per-pair callback is premature.** Serialization
  happens *inside* invoke (the per-call output-format flag picks
  native or JSON), and the value leaves the language's working
  store the moment invoke returns — there is nothing left to
  serialize at push time. Retrofitting value retention across the
  call is major surgery, and every shipped pair's declared shim
  is "JSON via my encoder" anyway, so the callback would change
  no bytes today. Build it when a pair genuinely wants non-JSON
  wire bytes; the declaration layer (slice 1) already reserves
  the seat.
- **Second slice landed 2026-07-22 — invoke reports the form it
  actually wrote.** The spec interface gains an optional
  per-handle accessor (`invoke_wrote_native`): the dispatch reads
  it right after a successful invoke, on the same worker thread,
  and routes the push by the ACTUAL form instead of the form it
  asked for. Lua — the one shipped spec whose invoke diverges
  (table-as-JSON on a native ask, bug 323) — records the form at
  each invoke exit in its state registry; C and Bash omit the
  accessor, which means "I write what I'm asked", still true.
  Tables now land on the JSON ring of dual-ring slots where the
  per-cell tag says parse-me — no guessing on that path. The
  brace-sniff narrowed to its one remaining home, single-ring
  cells (the tagged pop rings of multi-fire ports, whose
  per-port classification is static and cannot mark an
  individual cell), and its comment now states it as that ring
  family's decode rule. Pinned by a spec-level unit test (table
  ask-native → JSON bytes + report 0; primitive → report 1).
- **Third slice landed 2026-07-22 — every pair pinned, and the
  contract taught.** The `tests/maps/325-pair-matrix` fixture
  runs nine chains in one map — three same-language, six cross —
  with every consumer box named for its pair, so a failing
  assertion names the pair. The Lua consumers report the received
  type as well as the value, pinning decode fidelity: a
  cross-language JSON number arrives as a real Lua number; native
  raw bytes arrive as a string. `docs/005-writing-boxes.md` now
  teaches both contract duties to language authors: declare your
  translation story (`translate_targets`), and report what you
  actually wrote (`invoke_wrote_native`) if your invoke can ever
  diverge from the asked form.

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

## Completion notes (2026-07-22)

What landed, across three slices and three commits:

1. **Declarations** — `translate_targets` on the spec struct; all
   three shipped specs declare each other; the wire walker makes
   an undeclared cross-language pair a fatal load error naming
   the pair (unlike the neighboring sentinel check, which warns,
   because a pair mismatch is certain rather than possible). Pure
   lookup helper in the spec registry, unit-tested on synthetic
   and real specs.
2. **Truthful routing** — the optional `invoke_wrote_native`
   accessor; the dispatch routes each push by the form actually
   written. Lua records its form at every invoke exit (tables ride
   native asks as JSON); C and Bash omit the accessor, meaning
   "I write what I'm asked," which is true. The brace-sniff
   narrowed to single-ring cells and is documented as that ring
   family's decode rule.
3. **Pins and teaching** — the nine-pair matrix fixture and the
   writing-boxes documentation described in Current behavior.

Reserved extension point (not deferred work): a per-pair custom
serialize callback at push time. The recon that reshaped this
issue showed values leave the language's working store when
invoke returns, so per-pair custom forms need value retention —
build that when a pair genuinely wants non-JSON wire bytes; the
declaration layer already reserves its seat.

Known softness, recorded for a future tightening: a bare Bash
string crosses cross-language wires unquoted (the
pass-through-or-string-wrap choice of issue 317 passes it
through), so it is not valid JSON and consumers accept it via
the parse-failure fallback. The matrix pins the current bytes;
if Bash output ever gets string-wrapped, the `bash_to_lua` /
`bash_to_c` rows are the ones that will speak up.

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
