# 604 — Validation that needs the whole map

## Current behavior

Built, as the final pass before the seed, each check its own routine,
failures collected and printed together before one stop — someone
fixing a new map wants the whole list. The rules as landed: an arrow
onto a slot that is not a buffer (the value would have nowhere to
go); a station both written into by arrows and gathered from
(neither pushed nor pulled coherently); a gathered station with
ring-buffer inputs (gathering runs inline and cannot wait); and the
loud-but-not-fatal warning for a buffered station no arrow feeds,
kept a warning because something outside may deliver into it. Two
rules moved earlier than this issue placed them, recorded rather
than hidden: gather cycles are refused edge by edge as wires are
drawn (the same rule, applied at the moment both station numbers are
in hand), and the comparator-without-compare refusal lives at
placement, where the registry row is first consulted. The
comparator slot count cannot disagree by construction, since
placement derives it. Each rule proven by a wrong map dying with the
right words.

## Intended behavior

A final pass over the loaded map, run before the seed. Every failure
here is fatal and names the stations involved.

**Gather cycles.** Two gatherers pointing at each other recurse until
the stack dies, which surfaces as a segfault with no message and no
hint that anything in the map is wrong. Issue 404 built the check that
runs as each connection is made; this is where it is applied across a
map loaded all at once.

**Mixed fan-out.** A port whose destinations include both a gatherer
slot and a ring-buffer slot. Its box would be neither pushed nor pulled
coherently — the pull path assembles its value into a task struct
someone is building, the push path writes into a buffer, and one box
cannot be both.

**A gatherer with a ring-buffer slot.** A station wired into a gatherer
slot must itself have no ring-buffer inputs, because gathering runs
inline and cannot wait for a value to arrive. This is the same fact as
"a gatherer is a station with no ring-buffer slots," checked from the
other end.

**A comparator whose return type has no compare function.** Issue 503
notes this belongs at build time and cannot be there, because the
generator never sees the map. Here is where it lands instead.

**Comparator slot count.** A station declared `c` must have exactly one
more slot than its box has parameters. Since the kind is written rather
than inferred, the two can disagree, and this is the check that catches
it — which is the entire justification for writing the kind.

**Unreachable stations — a warning, not a failure.** A station with
ring-buffer inputs that no arrow points at will never run. Not
necessarily wrong: a map under construction has these. But silently
never running is the thing hardest to notice from the outside, so it is
reported by name.

Per the project's standing rule, a warning is an error that has not
been decided about yet. This one should be loud.

## Suggested implementation steps

1. The validation pass, run after the second pass and before the seed.
2. Each check as its own routine so its failure message is specific.
3. Collect every failure before stopping rather than dying on the
   first — someone fixing a new map wants the whole list.
4. Tests, one per rule, each asserting the right message names the
   right stations.

## Related

- [009 — Loading](../docs/009-datapath-load.md)
- Issue 404 — the cycle walk this applies
- Issue 403 — the gatherer rules being enforced
- Issue 605 — what runs after this passes
