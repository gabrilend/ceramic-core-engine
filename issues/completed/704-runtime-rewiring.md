# 704 — Rewiring while it runs

## Current behavior

**Built, and turning out to have been the foundation rather than a late
feature.**

Three things change and none of them is a retreat. **Gather-repoint
goes**, along with the gather cycle walk and the gather source purity
rule, because nothing is pulled
([056](../../docs/implementation-notes/056-no-pull-path.md)). **The
destination-list snapshot goes** — this issue's one acknowledged
retrofit — replaced by an immutable array published in a single atomic
write, so the delivery walk takes no lock at all
([214](214-destinations-without-a-lock.md)). And **adding a station,
which this issue deliberately left out of scope, is now the mechanism
underneath every program's first moment**
([211](../211-growing-the-station-table.md),
[212](../212-one-way-to-build-a-program.md)): the table starts empty and
grows as a program is read, so loading is this capability's first
caller rather than a separate construction path.

That last one is worth sitting with. This issue described itself as
*the feature the whole design had been quietly preparing for* — wires
holding indices, stations never moving, cycles checked when drawn — and
it turned out to be preparing for something larger still. Editing a
running program and building one stopped being two things.

**Two decisions here are now load-bearing everywhere.** That the check
and the change happen under one lock, because two individually legal
edits can be jointly illegal — proven by fifty rounds of exactly that.
And that a refusal returns and names its reason rather than killing the
process, because a loader that dies serves its author while a running
plant that dies for one bad instruction takes the plant down. Under one
construction surface those are the same call, so the reasoning had to
be right the first time.

The remainder describes it as built.

Built. Connect, disconnect, and gather-repoint operate on a running
map, every load-time rule applied per edge — type compatibility by
registry name, buffer-slot destinations, port limits by kind, the
gather source purity rule, and the cycle walk — with the check and
the change under one rewiring lock, proven by fifty rounds of two
threads drawing individually-legal jointly-cyclic edges with exactly
one refused every round. List surgery additionally happens under the
owning station's mutex, and delivery grew a destination-list
snapshot under that same mutex so a walker can never be left holding
a freed wire — the one retrofit this issue's "quietly preparing"
list did not include, recorded for the second pass. A removed wire's
in-flight value delivers down it, indistinguishable from having been
sent a moment earlier, exactly as designed. Refusal behaviour was
decided rather than defaulted: refusals return minus one with the
reason on stderr, because a loader that dies serves its author while
a running engine that dies for one bad control instruction takes the
plant down; the cost — an ignorable return — is weighed in the
report. Adding a station remains out of scope, as this issue drew
it. Proven further by a wire moved mid-run with nothing lost and
behaviour bending at the seam.

## Intended behavior

Connections may be added and removed while the engine is running.

**This is the feature the whole design has been quietly preparing
for.** Wires hold station indices rather than addresses. Stations never
move. The cycle check runs when a connection is made rather than when
it is traversed. Each of those was chosen partly for this, and each is
worth noting here as the debt being redeemed.

**The check and the insertion must happen under one lock.** Two threads
each adding an individually legal edge can produce an illegal pair —
neither closes a cycle on its own, and together they do. The cycle
walk from issue 404 and the append to the destination list are one
operation, not two.

**A wire being removed while a value is in flight is fine.** A value
already inside a task struct is a copy, and the task carries its
destination. The worst case is a value delivered down a wire that was
removed a moment ago, which is indistinguishable from having been
delivered a moment earlier — there is no correct alternative and no
way for an observer to tell.

**Adding a station is a different problem and is not in scope here.**
The station table is allocated once at load; growing it means
reallocating the array, and while every wire holds an index rather than
a pointer — so nothing dangles — every thread reading the table needs
to see the new base. That is a real design question and deserves its
own issue rather than being smuggled in alongside rewiring.

**Every load-time validation rule applies to a runtime edit.** Type
compatibility, gather cycles, mixed fan-out, the gatherer rule. A
rewiring path that skips them is a way to reach a state the loader
would have refused. The rules from issue 604 should be callable per
edge, not only per map.

**What happens on refusal** needs deciding rather than defaulting. The
project prefers a stop to a silent wrong turn, and an illegal rewire is
a programming error, so stopping is defensible. But an engine that dies
because a control socket sent a bad instruction is fragile in a way a
loader is not. Whichever is chosen, it should be chosen deliberately
and the reasoning left in a comment.

## Suggested implementation steps

1. A rewiring lock covering the port destination lists.
2. Connect: validate the edge against every rule from issue 604, walk
   for cycles per issue 404, append — all under that lock.
3. Disconnect: remove a destination from a port's list under the same
   lock.
4. Decide and document the refusal behaviour.
5. A test that a map rewired mid-run changes behaviour from that moment
   and that no value is lost across the change.
6. A test that two threads adding edges which are individually legal
   and jointly cyclic results in exactly one of them being refused.
7. Use issue 703's dump before and after, as the record of what changed.

## Related

- Issue 404 — the cycle check being reused
- Issue 604 — the rules being reused
- Issue 703 — how the resulting map is read back
