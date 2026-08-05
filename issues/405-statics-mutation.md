# 405 — Changing a static value while the program runs

## Current behavior

REOPENED. The locking is right and survives; the reachability is wrong
and is the reason a process can hold only one map.

Built. One mutex over the statics table, taken on every claim and every
write, held for the length of one copy — reads constant, writes rare,
contention nil, exactly the analysis this issue made. The write call is
size-checked against the entry and refuses entries no port has bound,
since their shape is unknown. Proven by four thousand claims racing a
writer alternating a struct between two self-consistent worlds: zero
torn reads, and the last write visible to the next claim.

Reaching it from inside a box landed as a bare-name call against a
process-wide "active map" pointer, because a box receives only values
and has no handle to anything. **That pointer is the singleton.** It
exists for this feature and for one optional timing hook, and nothing
else in the engine needs it.

## Intended behavior

**Changing a static is a call on the port that holds it**, made from
outside the graph, through the same runtime configuration surface that
changes any other property of a station.

Once a static value lives on its input port (issue 401), the write has a
specific address — this station, this port — and needs no table, no
table mutex, and no ambient pointer to a map. It takes the station's own
mutex, which the claim already takes, so the write and the read are
serialized by the lock that was already there.

**The torn-read argument survives intact and is the reason for the
lock.** A struct half-overwritten while a claim copies it yields a value
assembled from two different worlds — fields that were never
simultaneously true. For anything larger than a machine word this is not
theoretical, and it was demonstrated. The only thing that changes is
which mutex does the work.

**A box may no longer write a static.** This is a removal, and it is the
point of the issue.

The back channel worked, and it was always described as deserving the
suspicion a global variable deserves — a box could stash a value and
read it back on its next run, with none of it visible in the wiring, so
a map showing no connection between two stations might still have them
talking. What was not obvious until the packaging survey is what it
cost: reaching a map from inside a box requires a process-wide map
pointer, and a process-wide map pointer means a process can only ever
run one map. A feature the design already distrusted was quietly
charging the whole engine its ability to compose.

A box that needs to affect something later in the run does it the way
everything else does: it returns a value, and the value is wired
somewhere.

**A wire may deliver into a static port, and this is not the back
channel returning.** An arrow's destination is a station and a port; if
that port's live source is static, the arriving value overwrites the
static instead of queueing into a ring buffer.

The distinction from what is being removed is the whole point. The back
channel had a *box* reach out and write a value, with nothing in the
wiring showing it — two stations could be talking with no arrow between
them, so the picture lied. Here the box is untouched: it takes its
arguments, returns one value, remembers nothing, and has no idea what
happens next. The **wire** is what says this value overwrites a static.
That is visible in the box file, visible in the dump, and drawable on a
canvas. *A box still may not write a static;* a wire may deliver into
one.

The semantics follow from what a static already is. Writing one does
not affect readiness, because a static is always full — so an arrow
into a static port changes what the destination reads *next time*
rather than making it run. The write takes the destination station's
mutex, which its claim already takes, so no claim can see a half-
written value. Two arrows into one static port means last-writer-wins,
nondeterministically, and that belongs in the guarantees document as a
stated non-guarantee rather than as a surprise.

**This is also how a constant gets computed rather than written.** A
station that reads a clock, seeded so it runs once, wired into a
downstream station's static port: it runs, the timestamp lands, and
every invocation afterward reads it. "Read once at startup, and
everything after that works from that moment" becomes something drawn
rather than something the engine needs a feature for.

**Writers are otherwise all outside the graph** — a debugger, a control
socket, a person turning a knob, or a parent program configuring a
child. That is what the capability was for, and it needs nothing but
the station's mutex.

## Suggested implementation steps

1. Follow issue 401: the value moves to the port first, or this has
   nowhere to land.
2. The write call, naming a station and a port, size-checked against
   what the port holds, taking the station's mutex.
3. Remove the box-reachable write, the process-wide map pointer it
   needed, and the table mutex that no longer guards anything.
4. Relocate the optional box-timing hook, which is the pointer's only
   other user — the station is reachable from the task, so the shim can
   charge time without an ambient global.
5. Keep the racing test, retargeted: many claims against a writer
   alternating a multi-field struct, now through the station's mutex.
   Zero torn reads, last write visible to the next claim.
6. A test that two maps run in one process at the same time and do not
   see each other's values — the property this whole change buys, and
   the one that cannot be tested at all today.

## Related

- Issue 401 — where the value now lives
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where the singleton was traced back to this feature
- [058 — Guarantees](../docs/058-guarantees.md), whose "only one map per
  process" non-guarantee this deletes
- [008 — Map file format](../docs/008-map-file-format.md)
