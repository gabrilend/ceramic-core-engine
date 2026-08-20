# 056 — Why there is no pull path

The engine had two ways a value could reach a box. It was **pushed** —
an upstream station ran, and delivery carried its result into a ring
buffer. Or it was **pulled** — the value did not exist until somebody
needed it, at which point the upstream box was run on the spot to
produce one.

There is now one way. **Everything is pushed.** This document is the
record of why, because the pull path was a real capability with real
users and removing it costs something that should be named rather than
discovered.

---

## What pulling was for

[004](../004-datapath-statics.md) opened by saying it: *some values
should be fresh at the moment they are used, not fresh at the moment
they were produced.* A file's contents, a clock, a configuration value
somebody turns while the program runs. Push a clock reading into a ring
buffer and the box downstream reads the time the clock ran, which under
a backlog is not the time anybody cared about.

That is the whole of it. Everything else the pull path carried —
gatherer ports, chains, the cycle check, the inline-execution
exception, the rule that a gathered station may have no ring buffers —
existed to make that one sentence true.

---

## The three timings, and why none of them was the answer

*When* a pulled value gets pulled is not obvious, and each answer is
better than the others for some shape of program.

**At enqueue.** The thread that completed the claim fills the gathered
ports before the task reaches the queue. This is what was built. The
value is fresh as of *scheduling*, which is a different sentence from
the one the pull path exists to make true — between the pull and the
box running sits the whole queue. It also puts an arbitrary amount of
somebody else's program in the middle of a delivery walk, at exactly
the moment the engine most wants to be handing work out.

**At pickup.** The worker that is about to run the box fills the ports
first, on its own stack. Value age is as close to zero as the engine
can make it, which is the literal reading of the promise. It costs an
outer task struct and a generic shim to keep the pool from learning
that tasks have phases, it keeps a box running on a thread that never
picked up a task for it, and a worker can disappear into an arbitrarily
deep chain of user code.

**By poking.** When readiness finds a pulled port empty, enqueue the
upstream station as an ordinary task and let its result arrive by
ordinary delivery. This needs no machinery at all — no shim, no outer
struct, no inline execution, and every box runs from the pool. It was
the best of the three and it is the one that broke.

---

## The counting problem, which is what actually settled it

A pulled value arriving by delivery lands in a **buffer**, and a buffer
is a queue of values whose depth has to stay in step with the depths of
the station's other ports. Nothing keeps it in step.

Take a station with two ring ports and one pulled port. A value arrives
at the first ring port: readiness fails, so the pulled port's upstream
is poked. A value arrives at the second ring port: readiness fails
again, because the pull has not landed yet, so the upstream is poked
again. Two pulls come back. The station runs **once**, consuming one
value from each ring port and one pulled value. The second pulled value
is stranded — and it is stale, and the next invocation will use it.

The repair is a count of pulls outstanding, so a poke only happens when
the station is genuinely short. That works. It is also one integer per
port whose only job is to paper over a mismatch that exists because a
pull was made to look like a push.

**The two honest shapes are these.** If a pulled value is produced as
part of building or running an invocation, there is exactly one per
invocation and nothing needs counting, because the value and the
invocation are made by the same act — but then the pull runs inline,
with all that costs. If a pulled value arrives by delivery, it queues,
and queues need their depths reconciled. There is no third shape.

---

## What replaced it

**A static port is a slot, and writing one is an event.**

Two kinds of input port remain:

| | how it is read | how it is written | gates readiness |
|---|---|---|---|
| **ring** | consumed — one value taken per invocation | queued behind whatever is already there | **yes** — an empty one stops the station running |
| **static** | peeked — every invocation reads the same value | replaces what was there | no — always full |

And **an invocation happens when any input changes and every ring port
holds a value.** A delivery into a ring port triggers a readiness
check, as it always has. A write into a static port triggers the same
check, which is the new part.

That one addition is what makes the pull path unnecessary. A station
whose value was previously gathered now simply *writes* into the static
port that reads it, and the write triggers whoever depends on it. What
was a pull becomes a push arriving at a slot instead of at a queue.

**A static write cannot make something run that could not run anyway.**
The readiness check it triggers is the ordinary one: if the station has
a ring port and that port is empty, the answer is no, because there is
no value there and the engine will not invent one. A station with no
ring ports at all is always ready, so writing any of its statics runs
it. The rule needs no special case; it is the same check reached from a
new direction.

### What this gives that pulling did not

**Shared recalculation.** A chain of stations wired through static
ports is a recalculation graph — change the value at the top and it
propagates down once, and every consumer reads the same result. Under
pulling, two consumers of one upstream chain each pulled it and it ran
twice. Computing a thing once and reading it twice is the better
default.

**Seeding stops being a rule.** Binding a static from the file is a
write, performed while the program is being constructed, and its effect
lands when construction finishes. There is no sweep to run, no
condition to evaluate, and no mark to keep — the writes that built the
program are what start it.

**Nine things stop existing:** the gatherer port kind, the gather
module, the inline chain walk, the gather cycle check, the rule that a
gathered station may have no ring buffers, the rule that a station
cannot be both pushed-to and gathered-from, the one exception to *a box
only runs when a worker picks it up from the pool*, the requirement
that a gathered box be safe on several threads at once, and every
version of the question this document was originally written to answer.

---

## What it costs

**Fresh-at-use is gone.** Not deferred, not weakened — there is no
longer any mechanism that produces a value at the instant it is
consumed. A static holds whatever was last written into it, and every
invocation between two writes reads the same thing.

The replacement is to read the world *inside the box that needs it*. A
box that wants the current time calls for the current time. This is a
real loss of expressiveness at the graph level, traded for the removal
of an entire path through the engine, and it is only a good trade
because of the next paragraph.

**This engine is not for timing-critical work, and that is now
explicit.** Values move through queues, workers take them up in
whatever order they become free, and ordering across stations was never
promised. A design where a value could be one hop older than it might
have been is inside the tolerance the rest of the engine already asks
for. Anything that needs a value to match the instant it is used should
not have been reaching for a dataflow graph to get it.

**A value recomputes only when something upstream of it changes, never
because somebody read it.** For a recalculation graph that is exactly
right. For something that should be re-read per use — a file changing
underneath you, a counter, a socket — the drawing has to say what makes
it re-read, and the honest way to say that is a wire back from whatever
consumes it. That will look like a cycle, because it is one.

**A ring port with a backlog is drained by static writes.** Each write
runs the readiness check, and if the ring port has values waiting, one
of them is consumed. That is real work on real values rather than an
error, but it means a fast writer against a deep backlog accelerates
consumption, which is a coupling worth knowing about.

---

## Related

- [004 — Statics and recalculation](../004-datapath-statics.md), which
  described the pull path and now describes what replaced it
- [003 — Delivery](../003-datapath-delivery.md), the claim and the task
  build, which a write into a static port now also reaches
- [006 — Scheduling](../006-datapath-scheduling.md), the pool's three
  unconditional steps — which never had to change for any of this, and
  that is worth noticing
- [058 — Guarantees](../058-guarantees.md), where fresh-at-use stops
  being a promise and becomes a stated non-goal
- Issues 403 and 404 built the pull path; issue 407 designed a better
  timing for it; all three are superseded by this and say so
