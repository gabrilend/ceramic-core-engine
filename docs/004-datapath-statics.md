# 004 — Datapath: statics and recalculation

Delivery ([003](003-datapath-delivery.md)) is a push: a box finishes and
its value is carried forward into whoever was waiting for it. This
document is about the other kind of input — the one that is **not**
waiting, because it already has a value and always did.

Everything in this engine is pushed. There is no path that runs
backwards, nothing that reaches upstream at the moment a value is wanted,
and no box that runs anywhere but on a worker that picked up a task for
it. [056](implementation-notes/056-no-pull-path.md) is why.

A **ring** port is a stream: values pile up and get taken one at a time,
and an empty one stops the station running. A **static** port is a slot:
one value sits in it, reading does not disturb it, and it never gates
readiness. That is the whole distinction — the field-by-field shape of
both is [002](002-stations-and-ports.md) — and everything below follows
from it.

## Where a static's value comes from

Three places, and they are the same operation arriving from different
directions.

**From the map file, at construction.** An input line carrying `= value`
gives that port its constant, and binding it copies the value into the
port that reads it. The text becomes bytes by walking the field table the
generator emitted for that type, so the compiler decides every offset and
nothing is guessed.

**From outside the graph, while it runs.** A debugger, a control socket,
a person turning a knob, or a parent program configuring a child. The
write names a station and a port, is size-checked against what that port
holds, and takes the station's own mutex — the same lock the claim takes,
so no invocation can see a half-written value.

**From a wire.** An arrow whose destination port holds a static
**overwrites** that static rather than queueing into a ring buffer. The
box producing the value is untouched by this: it takes its arguments,
returns one value, and has no idea what happens next. The *wire* is what
says the value lands in a slot, which keeps the capability visible — in
the file, in the dump, and drawable — rather than being a box reaching
sideways into something no arrow connects it to.

**This is also how a station remembers.** An arrow from a station's own
output back into its own static input port makes that port hold whatever
the last run returned, and every run afterwards reads it. A counter is
that arrow. The box stays a function of its arguments, the memory belongs
to the placement rather than to the function — one box at three stations
is three memories — and the whole of it is one line in the map file.

Two arrows into one static port is last-writer-wins, nondeterministically.

## A write is an event

**Writing a static runs the readiness check on the station that holds
it.** That is one addition rather than a subsystem.

A value delivered into a ring port has always triggered that check, and a
value written into a static port does too. So a chain of stations wired
through static ports behaves like a recalculation graph: change the value
at the top, and it propagates down through every station that reads it,
once, with every consumer seeing the same result.

**A write cannot make something run that could not run anyway.** The
check it triggers is the ordinary one. If the station has a ring port and
that port is empty, the answer is no — there is no value there and the
engine will not invent one. A station with **no** ring ports is always
ready, so writing any of its statics runs it.

One consequence to know: if a ring port has a **backlog**, each static
write consumes one of the waiting values, because each write completes an
input set. That is real work on real values rather than a mistake, but a
fast writer against a deep queue will drain it faster than the producer
alone would have.

## What starts a program

**The writes that build it.** Binding a static from the file is a write
performed during construction, and its effect lands when construction
finishes: every station whose inputs are then satisfied has a task.

A station with only statics is satisfied immediately, so it runs. Its
output goes wherever its arrows point, and the program unfolds from
there. There is no separate sweep, no rule about which stations deserve a
first task, and nothing to remember between runs.

## What this costs

**A value is fresh as of the last write, not as of use**, and **a value
recomputes only when something upstream changes**, never because somebody
read it. Both are recorded as non-goals in
[058](058-guarantees.md) along with what they buy. The short version: a
box that needs the current time asks for it inside the box, and anything
that genuinely should be re-read per use needs a wire back from whatever
consumes it, which will look like a loop because it is one.

## Related

- [002 — Stations and ports](002-stations-and-ports.md), the record a
  port is
- [003 — Delivery](003-datapath-delivery.md), which has one more way to
  arrive
- [056 — Why there is no pull path](implementation-notes/056-no-pull-path.md),
  the reasoning, the three timings that were considered, and the counting
  problem that settled it
- [008 — Map file format](008-map-file-format.md), where statics are
  written and where arrows are drawn
