# 603 — The loader, second pass: arrows and types

## Current behavior

**Built, and surviving as an ordering rather than as a pass.**

The two passes existed for one reason: a station may be wired to one
declared later in the file, so arrows cannot resolve until every
station exists. That reason is permanent and the machinery around it is
not. Under [212](../212-one-way-to-build-a-program.md) the whole
procedure is *create every station, then draw every wire, then let the
construction writes land* — so "resolve names after every station
exists" is all that is left of this, and it is a step in a sequence
rather than a different kind of pass.

Gather input lines resolved here and no longer exist
([056](../../docs/implementation-notes/056-no-pull-path.md)), taking
the cycle check that ran as each one was bound.

Two things outlive it. **Type-check a wire at the first moment both
ends are known** — which is per edge, not per program, and is what let
runtime rewiring reuse the same check when a wire is drawn on a running
program. And the message: naming both stations, the port, and both
type names is why *"head → wrong.1: box returns int, slot takes
double"* is actionable, and it is the standard the shape-based
comparison in [309](../309-types-by-width.md) has to meet or beat.

The remainder describes it as built.

Built. Every output line resolves its destination by name — forward
references included, the reason the passes exist — checks the slot
exists, and type-checks the wire by name at the first moment both
ends are known: the deliverable message reads exactly as this issue
asked, "head -> wrong.1: box returns int, slot takes double", both
stations, the slot, both type names. Gather input lines resolve here
too (their sources are names), type-checked the same way, bound
through the cycle check. Destinations append in file order, which
the eventual dump round-trip leans on. The name table is discarded
when loading ends. Proven by the forward-reference map loading and
by refusals for the missing station, the out-of-range slot, and the
mismatched wire, each with its message checked word for word.

## Intended behavior

Walk the output lines. Every station now exists and can be found by
name, so each arrow can be resolved and appended to its port's
destination list as a pair of numbers: which station, which slot.

**This is also where every wire is type-checked**, because it is the
first moment both ends are known.

The registry knows the source box's return type and the destination
box's parameter type, both derived from the C that will actually run.
The check needs nothing from the file — which is exactly why the file
carries no types. A map that declared them would be a second source of
truth able to disagree with the first, and it would always be the one
that was wrong, because the compiler enforces the C and nothing
enforces the file.

**The error message is the deliverable here.** A wire mismatch should
read as something like *"adder → printer.0: box returns int, slot takes
float"* — both station names, the port and slot, and both type names.
This is the error a person will hit most often after a misspelled box
name, and the difference between a good message and a bad one is the
difference between a five-second fix and an afternoon.

Type *names* are carried in the registry alongside sizes for exactly
this reason. Sizes alone cannot tell an `int` from a `float`, and
"4 bytes versus 4 bytes" is not a message.

**Typedefs are transparent.** Two names for the same underlying type
connect happily, so a wire carrying a count into a slot expecting a
duration passes. Distinguishing them means wrapping each in its own
struct, which is the beginning of reimplementing a much larger type
system and is deliberately not done. Worth a comment at the check, so
the next reader knows it was a decision.

## Suggested implementation steps

1. Walk every output line, resolving station names to indices through
   the first pass's lookup table.
2. Resolve the destination slot index and confirm it exists.
3. Compare the source box's return type against the destination
   parameter's type, by name.
4. Append the destination to the port's list, creating the port if this
   is the first arrow on it.
5. Discard the name lookup table.
6. Tests: a matching wire loads; a mismatched one fails with both names
   in the message; an arrow to a station that does not exist fails
   naming it; an arrow to a slot index out of range fails naming the
   station and index; a port with several destinations produces them
   all in order.

## Related

- [009 — Loading](../docs/009-datapath-load.md)
- Issue 303 — the registry this reads
- Issue 602 — the first pass
- Issue 604 — the checks that need the whole map
