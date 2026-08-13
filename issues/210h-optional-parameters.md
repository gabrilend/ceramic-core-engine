# 210h — Optional parameters

Last child of [210](210-input-port-record.md), and the capstone: the
only one that changes what a box author writes rather than what the
engine does underneath them.

## Current behavior

Every parameter of a box must have a source. A station is ready only
when every port holds a value, and there is no way for a box to say
that one of its inputs is one it can do without.

**Absence has nowhere to live.** A box returning an `int` has no spare
value in that type's range that could honestly mean "deliberately
absent" — every bit pattern is a number somebody might mean. The demo
box that reads a file already runs into this from the other side: it
cannot decline to produce a value, because the task struct has a place
waiting for bytes and no way to say nothing arrived, so a missing file
stops the program.

After [210b](210b-the-port-record.md) there is a *none* tag, and after
[210g](210g-one-way-to-build-a-station.md) a port left in it is caught
at configuration time as an error. This issue is what makes it
sometimes not one.

## Intended behavior

**A box may declare a parameter optional.**

**An optional parameter's type is a small generated wrapper carrying a
presence flag beside the value.** Not a sentinel. The C signature
*looks* optional rather than being an ordinary parameter that might
secretly mean something unusual for one of its values — which matters
because the engine's standing promise is that somebody can bring
existing C unchanged, and a sentinel is a convention the author has to
learn, remember, and never violate.

**The check belongs at configuration time.** A non-optional parameter
left facing an unconfigured port is caught when the port is
configured, naming the station and the port while a person is still
there to read it, rather than on the first task built minutes into a
run. That check is added by
[210g](210g-one-way-to-build-a-station.md); what this issue adds is
the declaration that exempts a parameter from it.

**The shim keeps a cheap assertion as a backstop and never has to make
a decision.** The hot path carries no check that can only fail because
of a configuration error made much earlier. The assertion is there for
the case where the configuration-time check was somehow bypassed, and
it costs a comparison rather than a branch that decides anything.

**Readiness treats an optional parameter's unconfigured port as
satisfied**, handing the box a wrapper whose flag says absent. That is
the whole runtime behaviour: no new port kind, no new dispatch row,
one more way for a port to answer "yes" to the question readiness
already asks.

## Suggested implementation steps

1. The declaration: how a box author marks a parameter optional in the
   source the generator reads.
2. The generated wrapper type per optional parameter type, and the
   generator's handling of both the declaration and the wrapper.
3. Readiness answering "satisfied" for an unconfigured port feeding an
   optional parameter.
4. The shim's backstop assertion.
5. The configuration-time check learns the exemption.
6. A test that a box with an optional parameter runs with that port
   unconfigured, receives absence, and runs again with the port
   configured, receiving presence — the same station, both ways,
   without being rebuilt.
7. A test that a non-optional parameter facing an unconfigured port is
   still refused at configuration time.

## Open questions

- The wrapper is a generated type per underlying type, which means the
  registry gains entries nobody wrote and the dump has to be able to
  name them. Whether a map file can write a *present* value into an
  optional port as ordinary text — and what the absent form looks like
  — is unsettled, and it is the same question
  [210b](210b-the-port-record.md) has about spelling an unconfigured
  port.
- Whether an optional parameter may be *made* absent again after being
  configured, or whether absence is only ever the state it started in.
  The first is more honest given that ports are convertible; the
  second avoids a box seeing a value disappear between two runs.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210b — The port record](210b-the-port-record.md), whose *none* tag
  this reinterprets for one kind of parameter
- [210g — One way to build a station](210g-one-way-to-build-a-station.md),
  whose configuration-time check this teaches an exception
- [302 — Shim emission](completed/302-shim-emission.md), which grows
  the backstop assertion
- [303 — Registry emission](completed/303-registry-emission.md), which
  must carry the wrapper types
- [309 — Types compared by width](309-types-by-width.md), which decides
  what makes two types the same and therefore what a wrapper is
