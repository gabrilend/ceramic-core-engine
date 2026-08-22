# 005 — Routing

There are three kinds of box. They are identical in every respect
except one: which output port a returned value goes down.

All three pop their inputs the same way, build the same task struct,
and call their shim the same way. The kind is consulted at exactly one
moment — step 1 of delivery — and nowhere else in the engine. It is a
three-entry dispatch table on the way out, not a property that
propagates through the system.

## Plain

One output port. The value goes down it. The port may carry any number
of destinations, and the value is copied to each.

Fan-out is not a kind of box. It is what a single port with several
destinations already does.

## Comparator

Three output ports, meaning less, equal, and greater.

A comparator station has one more input port than its box function has
parameters. That last port holds the value to compare against. It is
not passed to the box function — the comparison happens after the
function returns, on the delivery path — but it participates in the
readiness check like any other port, so the station cannot run until it
holds something. In practice it is nearly always a static port holding
a fixed threshold, and a static port is always full, so it usually has
no effect on readiness at all.

Its type is never declared. It must match the box function's return
type, because that is what it gets compared against, and the emitted file
already knows what that is.

**Comparison is three-way.** The engine calls a function returning
`-1`, `0`, or `1`:

```c
int vec3__compare(vec3 a, vec3 b);
```

The generator emits these automatically for the primitive types. A
struct that wants to be compared supplies its own, found by name. A
comparator whose box returns a type with no compare function fails at
build time and names the type — because comparing raw bytes would
produce an answer, and it would be wrong. Two floats differing only in
sign compare backwards byte-wise.

**There is no operator setting.** Choosing an operator is choosing
which ports to wire:

| Operator | Wire these ports to the same place |
|---|---|
| `<` | less |
| `<=` | less, equal |
| `==` | equal |
| `!=` | less, greater |
| `>=` | equal, greater |
| `>` | greater |

All six fall out of three ports, along with combinations that have no
operator name — sending equal one way and greater another, which no
comparison operator can express. The map says it by where the arrows
go, and the engine needs no setting to read.

## Iterator

Any number of output ports. Each run sends the value down the next one,
wrapping at the end.

The cursor lives on the station, and it is the one piece of memory a
station keeps across invocations. It is safe because of *when* it is
touched: the cursor advances during the readiness check, while the
station's mutex is held, and the port it landed on is written into the
task struct. The box function never sees it. Two tasks assembled a
moment apart therefore get different ports, decided by the enqueuing
thread under the lock, and no two invocations can ever collide over it.

It is not really an exception to "a box cannot remember." It is the
station remembering on the box's behalf, at a moment when only one
thread can be looking.

**An iterator distributes fairly but does not deliver in order.** The
port is chosen when the task is created, not when it finishes. If the
task holding port one takes longer than the one holding port two, port
two receives first. This is correct for what an iterator is for —
spreading work across several destinations — and wrong for anything
that wants ordering. It is a spreader, not a funnel.

## Related

- [002 — Stations and ports](002-stations-and-ports.md), where ports live
- [003 — Delivery](003-datapath-delivery.md), step 1
- [007 — The build path](007-datapath-build.md), where compare functions are found
