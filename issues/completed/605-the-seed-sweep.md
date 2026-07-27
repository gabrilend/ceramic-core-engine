# 605 — The seed sweep

## Current behavior

Built, as the last step of loading, before the workers are released
— the pool exists so the seed has somewhere to push, the workers are
parked so the termination rule's outside-pusher clause stays true.
The sweep enqueues every station with no ring-buffer inputs that
nobody gathers from, through the very task construction delivery
uses — one way a task comes into existence, not two. Each seeded
station is announced by name and the count is kept on the map;
nothing seeded is fatal with the reason spelled out, because exiting
successfully having done nothing is what the termination check would
otherwise correctly and uselessly report. One reading of the rule
sharpened during the build: "whose output feeds a ring buffer"
became "not gathered from", so that an input-less sink — which has
no output at all — still runs once for its effect; the first-pass
report weighs the wording. The comment at the sweep says what a
future reader will come looking for: the engine never scans for
work, and this is the one exception. Proven by the linear map
seeding exactly its head, the gathered source rightly unseeded, and
the unstartable map refused with its message.

## Intended behavior

The station table is swept exactly once, here, and never again.

**Enqueue every station that has no ring-buffer inputs and whose output
feeds a ring buffer.**

Both halves of that sentence are load-bearing.

**No ring-buffer inputs** means the station has nothing that can ever
be written into it, so delivery will never discover it. If it is going
to run at all, it must be started here.

**Whose output feeds a ring buffer** excludes gatherers. A gatherer's
value goes into a task struct that someone is assembling right now, not
into a slot — that is the entire difference between the push path and
the pull path. At startup nobody is assembling anything, so a gatherer
enqueued as a task would run, produce a value, walk its destinations
looking for a buffer to write into, and find a gatherer slot, which by
definition has none. The value would have nowhere to go.

**This is the only time anything iterates the station table.** From
here on, every station is reached by index, through a wire. Worth
stating in a comment at the sweep, because a future reader looking for
"where does the engine scan for ready work" needs to find the answer
"it does not, and here is the one exception and why."

**The sweep happens before the workers are released.** This matters for
termination: issue 104's rule depends on nothing outside the pool
pushing tasks after startup. Seeding while the workers are still parked
at their barrier keeps that true.

**A map with nothing to seed is an error.** It cannot ever run, and
saying so at startup is better than exiting successfully having done
nothing — which is what the termination check would otherwise report,
correctly and uselessly.

## Suggested implementation steps

1. The sweep, run after validation and before the workers are
   released.
2. Build and enqueue a task per qualifying station, using the same
   construction path delivery uses, so there is one way a task comes
   into existence rather than two.
3. Report the count seeded; a map that seeds one station when the
   author expected ten is a map with a wiring mistake.
4. Fail if nothing qualifies, naming the reason.
5. A test that a linear map seeds exactly its head.
6. A test that a gatherer is never seeded even though its readiness
   check passes vacuously — the case this issue's second condition
   exists for.
7. A test that a map whose only input-less station feeds a gatherer
   slot fails to seed and says so.

## Related

- [009 — Loading](../docs/009-datapath-load.md)
- Issue 104 — the termination rule this must not violate
- Issue 403 — why gatherers are excluded
