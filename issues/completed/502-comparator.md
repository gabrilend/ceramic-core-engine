# 502 — The comparator

## Current behavior

Built. Placement by name gives a comparator one port more than its
box has parameters — the threshold, at the end of the ports array,
typed to the box's return value from the registry, exactly where
this issue put it. The shim reads only the real parameters; the
threshold rides the task as its last input and is consulted after
the box returns, on the delivery path. It participates in readiness
like any port: the buffered-threshold test proves a station starving
until its threshold arrives. All three outcomes reach their own
ports; an unwired outcome discards ten values without a whisper. No
operator setting exists anywhere — the operator table in this issue
remains a statement about wiring, and the demo makes it in numbers.
A sink asked to be a comparator, and a comparator whose return type
has no ordering, are both refused at placement naming the fix.

## Intended behavior

Three output ports, meaning less, equal, and greater. The box's return
value is compared against a threshold and goes down exactly one of
them.

**The threshold is an extra input port.** A comparator station has one
more port than its box function has parameters, and that last port
holds the value to compare against.

**It is not passed to the box function.** The comparison happens after
the function returns, on the delivery path. Ports and parameters stop
being one-to-one for this one kind, which is why the kind is written in
the map rather than inferred — see issue 601.

**It participates in the readiness check like any other port.** The
station cannot run until it holds something. In practice it is nearly
always a static port holding a fixed threshold, and a static port is
always full, so it usually has no effect on readiness at all.

**Its type is never declared anywhere.** It must match the box
function's return type, because that is what it gets compared against,
and the registry already knows what that is. This is stricter than a
declaration and impossible to get wrong.

## There is no operator setting

Choosing an operator is choosing which ports to wire:

| Operator | Wire these ports to the same place |
|---|---|
| `<` | less |
| `<=` | less, equal |
| `==` | equal |
| `!=` | less, greater |
| `>=` | equal, greater |
| `>` | greater |

All six fall out of three ports, along with combinations no operator
can name — sending equal one way and greater another. The map says it
by where the arrows go and the engine needs no setting to read.

An earlier design carried an operator on the station plus a lookup
table turning a comparison result into true or false. It was dropped
because three ports already express strictly more.

## Where the extra port lives

At the end of the ports array, not in a field of its own. The shim
receives ports zero through N-1; the routing reads port N. The
readiness check does not care either way — it walks all of them.

This is why the station struct needs no comparator-specific field and
the station table stays a flat array of identical records.

## Suggested implementation steps

1. Station creation allocates the extra port for a comparator, typed to
   match the box's return value from the registry.
2. Fill the comparator row of issue 501's dispatch: read the threshold
   port, compare (issue 503), return port zero, one, or two.
3. Ensure the shim is handed only the real parameters.
4. Extend the construction calls from issue 207 to place a comparator.
5. A test of all three outcomes reaching the right ports.
6. A test of a comparator whose threshold is a buffer rather than a
   static, confirming it blocks until fed — the case that proves the
   extra port really is in the readiness walk.
7. A test that a port left unwired discards its values rather than
   failing.

## Related

- [005 — Routing](../../docs/005-routing.md)
- Issue 501 — the dispatch this fills
- Issue 503 — the comparison itself
- Issue 601 — why the kind is written rather than inferred
