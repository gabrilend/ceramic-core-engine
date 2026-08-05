# 307 — Phase 3 demo: writing a box is writing a function

## Current behavior

Built, shell-driven so the rebuild steps are visible, working on a
scratch copy of the box sources so the project is never touched. The
scenes, in order: one ordinary function appended to a box source
appears in the registry with real types and sizes and not a single
other edit; the add function printed beside the call site the
generator wrote for it; the registry printed whole and then laid
beside the compiler's own sizeof answers, column for column
identical; ints, floats, structs by value, a nested struct with a
string, and a wide unsigned all flowing through the one call site
byte-perfect, with each box's exact task size listed; the occupancy
figure re-measured for the series; and a box whose return type
changes out from under its registry, shown regenerating — with the
no-partial-output guarantee meaning a shim casting to yesterday's
type cannot exist. Mirrored to the shared-memory tier.

Seven scenes now, each opening with a story and reporting in that
story's units, under the standard issue 707 sets: a library whose
catalogue is printed by walking the shelves, a translator nobody
briefed, a parts book written by reading the machine, two surveyors
measuring one field by different methods, a loading door that never
inspects its freight, the same track with a new engine under the
bonnet, and a passport photograph that no longer matches the face. The
shell half and the compiled half speak through matching presenters and
share one report — the compiled half joins the document rather than
starting its own — so the seams do not show.

## Intended behavior

A demo about the seam between the two halves of a program. It should
make one thing obvious: **the only thing an author writes is ordinary
C, and everything the engine needs was derived from it.**

**What it should show, in order of how convincing it is:**

**A box added live.** Write a new function into a box source, rerun the
build, and show it appearing in the registry with its real types and
sizes — without a single other edit anywhere. This is the claim of the
whole phase and it is worth showing rather than asserting.

**The registry, printed.** Every box, its parameter types and sizes,
its return type, its task size. Alongside it, the same sizes obtained
from `sizeof` in compiled code, so the two columns can be compared.
They should be identical, and showing them side by side is the proof
that nothing was hand-typed.

**A generated shim, next to the function it wraps.** Both printed. This
is the one place where showing the code is the point, because the
reader's question is exactly "what did it write for me."

**Type coverage.** The same map run with boxes taking and returning
integers, floating-point numbers, structs by value, nested structs, and
nothing at all — all through one call site in the engine. Report each
one's task size and confirm the values arrive byte-identical.

**A stale-registry attempt.** Change a box's return type, rebuild, and
show the build catching it rather than producing a shim that casts to
the old type. The failure is the feature.

**Reuse from earlier phases.** All of it runs on phase 1's pool and
phase 2's stations, unchanged. Report the same occupancy figure phase 2
reported, so the two demos can be read against each other.

## Suggested implementation steps

1. Drive the demo from a shell script so the rebuild step is visible as
   it happens.
2. Report every number by measuring it.
3. Write results to `tmp/shared-memory/` alongside the screen output.
4. Confirm the root launcher finds it.

## Related

- [007 — The build path](../docs/007-datapath-build.md)
- Issues 301 through 306 — everything being demonstrated
- Issue 208 — the phase 2 demo this builds on
