# 210g — One way to build a station

Seventh child of [210](210-input-port-record.md). Everything above
made a port describable; this makes there be exactly one way to say
it.

## Current behavior

**Two ways exist to create a station, and they can bind different
things.**

Placement by name looks the box up in the registry and copies each
parameter's type name onto the corresponding port. Hand placement
takes an array of element sizes and no type names at all.

Binding a static needs the type, because a static in a map file is
*text* and turning `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into
bytes means knowing the field layout. So **a hand-placed station
cannot bind a static** — and the reason is not a design limit. It is
that phase 2's scaffolding was written before the registry existed and
was never given the argument afterwards.

The result is two contracts where there should be one, differing in
what programs they can express, with the difference undocumented and
discovered by trying.

Configuring a port is likewise scattered: binding a static, drawing a
wire, and converting a tag are separate calls with separate shapes,
some of which die on refusal and some of which return a code.

## Intended behavior

**Configuring a port is a single operation naming a station, a port, a
source, and a value.** The loader calls it while reading a file; a
debugger, a control socket, or a workbench calls the same one on a
running program. There is one description of what it means to give a
port a source, and it is executable.

**Hand placement stops being a second contract that can bind fewer
things than the first.** It takes the type names the registry already
has, or it stops existing. The second option is worth weighing
seriously: the tests that use it predate the registry, and if every
one of them can place by name instead, then keeping a weaker door open
is keeping a way to build a program that the loader cannot.

**A program built by calling the surface directly and one read from a
file are the same program.** Not equivalent — the same, provably, by
dumping both and comparing bytes. That is the test that makes the
claim mean something, and it is the reason this issue exists as more
than tidying: it is what lets [212](212-one-way-to-build-a-program.md)
say that reading a file is a sequence of ordinary operations rather
than a privileged path.

**Refusal follows one policy.** The surface returns a refusal that
travels upward and accumulates rather than dying where it happens,
because the loader's rule is to collect every failure in a file and
present them together. [212](212-one-way-to-build-a-program.md) owns
that policy and overturns the one rewiring chose; this issue supplies
the surface it applies to.

**The configuration-time check for an unconfigured port.** A parameter
left facing a *none* port is a configuration error, and it should be
caught at configuration time — which names the station and the port
while a person is still there to read it — rather than on the first
task built minutes into a run.

**The check has no exceptions and never will.**
[210h](completed/210h-optional-parameters.md) proposed one — a
parameter a box declares it can do without — and was refused, because
it would have been the only exemption to the rule that a station runs
when every one of its slots holds a value. So the check here is
unqualified: a port with no source is an error, full stop.

## Suggested implementation steps

1. Hand placement stays and becomes the primitive: a generated
   placement function per box writes a station directly, and by-name
   placement is a table lookup that finds one and calls it. Built in
   [311b](311b-placement-instead-of-records.md); this issue is its
   caller rather than its author.
2. One port-configuration operation, with the existing static-binding
   and tag-conversion calls becoming cases of it.
3. The loader becomes its first caller, losing whatever it does today
   that the surface does not offer.
4. Runtime editing becomes its second caller.
5. The configuration-time check for a *none* port, unqualified — every
   parameter needs a source, with no exemption.
6. A test that a station with an unconfigured port never becomes
   ready, and becomes ready the moment that port is given a source.
7. A test that a program read from a file and one built by calling the
   configuration surface directly produce identical dumps.

## Open questions

**Answered:**

- *Does hand placement survive at all, or does everything place by
  name?* **It survives, and it turns out to be the primitive rather
  than the alternative.**

  The question looked like a choice between two doors into the engine
  and it is not.
  [311b](311b-placement-instead-of-records.md) has the generator emit a
  **placement function** per box — a function that writes a station's
  shim pointer, slot sizes, return size, and comparison directly, with
  every number a `sizeof` the compiler folded. A placement function
  *is* hand placement, written by the generator instead of by a person.
  By-name placement is a two-column table lookup that finds one and
  calls it.

  So there is one door with a typed front and a raw back, and the front
  is built out of the back. "One way to build a station" stays true;
  what it has is one construction path reachable two ways, rather than
  two paths that must be kept in agreement.

  **And phase 2's tests keep testing only the station table.** They
  call the raw form, which is what they were always doing, and never
  construct a box table to do it. The worry that a station-table test
  would have to drag in a neighbouring subsystem — and thereby start
  failing for two reasons — does not arise.

## Related

- [210 — What an input port is](210-input-port-record.md), the parent
- [210b — The port record](completed/210b-the-port-record.md), whose *none* tag
  this checks for
- [210f — Changing what a port is](210f-changing-what-a-port-is.md),
  whose conversion becomes a case of this surface
- [210h — Optional parameters](completed/210h-optional-parameters.md),
  refused, which is what leaves the check added here unqualified
- [212 — One way to build a program](212-one-way-to-build-a-program.md),
  which stands on this and owns the refusal policy
- [207 — Hand-built maps](completed/207-hand-built-maps.md), the second
  contract in question
- [602 — The loader's first pass](completed/602-loader-first-pass.md),
  which becomes a caller rather than a builder
