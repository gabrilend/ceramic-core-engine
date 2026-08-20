# 210h — Optional parameters

Last child of [210](../210-input-port-record.md), and the one that is not
built. This is the record of the refusal, kept in full because the
reasoning constrains everything downstream of it and because the design
it rejects is one somebody will propose again.

## Current behavior

**Every parameter of a box must have a source**, and that is how it
stays. A station is ready only when every port holds a value, and there
is no way for a box to say that one of its inputs is one it can do
without.

After [210b](../210b-the-port-record.md) there is a *none* tag, and after
[210g](../210g-one-way-to-build-a-station.md) a port left in it is caught
at configuration time as an error. **There is no exemption from that
check**, and this issue is the record of why one was considered and
refused.

## What was going to be built

A box would have declared a parameter optional. That parameter's type
would have become a small generated wrapper — a struct carrying the
value with a presence flag beside it — rather than a reserved value
inside the type's own range. Readiness would have treated an
unconfigured port feeding such a parameter as satisfied, handing the
box a wrapper whose flag said absent.

The wrapper rather than a reserved value was the right half of the
design and the reasoning still holds: a box returning an `int` has no
spare number that could honestly mean *deliberately absent*, because
every bit pattern is a quantity somebody might mean. A reserved value
is a convention the author has to learn, remember, and never violate,
and this engine's standing promise is that somebody can bring existing
C unchanged.

## Why it is refused

**Because it would have been the only exemption to the one rule.**

> A station runs when, and only when, every one of its input slots
> holds a value.

Nothing in this engine polls and nothing scans for ready work; the
check is the tail end of a write, and every part of it is simple
because that sentence has no exceptions in it. Readiness answering
*satisfied* for a slot holding nothing is an exception, and it is the
first one. Spending the design's single invariant on a convenience is a
bad trade, and it is a worse trade at engine level than anywhere else,
because everyone downstream inherits the exception permanently and
cannot decline it.

**And C has no optional parameters to be faithful to.** No version of
the language has default arguments or overloading. What C actually
does, and why none of it is this:

| idiom | how it works | why it is not this |
|---|---|---|
| a nullable pointer | `strtol(s, NULL, 10)` | works only because a pointer type has a genuine spare bit pattern — null can never be a valid object address. An `int` has none. |
| a reserved in-range value | `-1` for "no timeout" | costs a number you might have meant; the convention lives in the author's head |
| a zero-initialized options struct | `(struct opts){.timeout = 5}`, unset fields are zero and zero means default | the modern idiom, and it is **a little struct the caller writes**. The language contributes nothing. |
| two separate functions | `foo()` and `foo_with_extra(x)` | no mechanism at all |
| varargs with a terminator | `execl(path, arg, ..., NULL)` | untyped, and dreadful for anything but strings |

The wrapper-with-a-presence-flag is not a C idiom being imported. It is
a feature of other languages that C approximates with the options
struct — which the caller writes by hand, at no cost to the platform.
So an author who wants one writes one, as a struct, exactly as they
would in any other C program. Nothing is taken away from them.

## The case this was actually reaching for, and where it belongs

The motivating example was the demo box that reads a file: it cannot
decline to produce a value, because the task struct has a place waiting
for bytes and no way to say nothing arrived, so a missing file stops
the program.

**That is the output side, and optional parameters would never have
fixed it.** It is a box wanting to produce nothing, not a box wanting
to consume nothing.

The engine already answers it in its own vocabulary. The box returns a
struct carrying a validity flag beside the bytes, and a **comparator**
station downstream — three output ports, choosing one by comparing the
returned value against a threshold in an extra input slot, built and
working since phase 5 — sends success down one wire and failure down
another. **Absence becomes a value that gets routed**, rather than a
hole in readiness. That is the little options struct again, doing the
whole job, with nothing added to the engine.

Which is the general shape of the answer whenever this comes back:
where a box wants to say "nothing this time," it says it in a value and
the graph reads it. This engine already spells every conditional as
wiring, and a second mechanism would give one idea two spellings.

## What refusing it simplifies

- **The type registry gains no entries nobody wrote.** A wrapper type
  per underlying type would have had to be nameable by the dump and
  findable by the reader.
- **The map file needs no way to spell presence or absence in a port.**
  The question of whether `in 1 = 5` could write a present value, and
  what the absent form looked like, dissolves with the feature.
- **The configuration-time check carries no exemption.** A port with no
  source feeding a station is an error, with no second case.
- **The *none* tag keeps meaning one thing**: unconfigured, and a
  station holding one can never run. It never means "deliberately
  absent, which is fine."

## Open questions

None. The two this issue carried — whether a map file could write a
present value into an optional port as ordinary text, and whether a
parameter could be made absent again after being configured — both
dissolved with the feature rather than being answered.

## Related

- [210 — What an input port is](../210-input-port-record.md), the parent,
  whose *none* tag this leaves meaning exactly one thing
- [210b — The port record](../210b-the-port-record.md), whose *none* tag
  this would have reinterpreted for one kind of parameter
- [210g — One way to build a station](../210g-one-way-to-build-a-station.md),
  whose configuration-time check keeps its single unqualified rule
- [005 — Routing](../../docs/005-routing.md), the comparator that carries
  the case this was reaching for
- [058 — Guarantees](../../docs/058-guarantees.md), where the readiness
  rule's lack of exceptions is what this protects
