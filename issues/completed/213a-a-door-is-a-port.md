# 213a — A door is a port, not a station

The entrance half of moving a program's surface off the station and onto
the port. [209a](209a-results-go-where-the-caller-says.md) is the other
half, and they are one design.

## Current behavior

**Built.** The mark lives on an input port and carries the number of the
argument it is.

An argument costs nothing now. It used to cost a whole station running
the identity function — a station struct, a mutex, a ring buffer, and
per value a task, a pool dispatch, a call that returns its argument, a
readiness check and a second delivery.

**Which argument a port is, is stated rather than positional.** A gap or
a repeat is refused at bring-up, neither of which the old scheme could
detect: the order was the order stations happened to sit in the table,
so moving two lines in a sub-map silently swapped two of the parent's
arguments.

**Being an argv slot is derived**: a marked port that something inside
feeds is fed both ways and simply is not one. That replaced the idea of
closing a door when an enclosing map wires in — there is nothing stored,
so nothing can go stale, and no bookkeeping is needed at the moment of
wiring. The same derivation makes the bring-up numbering checks safe
under composition, where one description instantiated twice puts two
ports in the table both marked argument zero.

**A station may hold ports of both kinds.** The old refusal existed
because the mark was on the station and a station is one thing.

## Intended behavior

**The mark lives on an input port.** A port says the outside delivers
here; no station is special, and the identity station disappears along
with its task, its dispatch, and its ring buffer.

**Which argument a port is, is stated rather than positional.** The mark
carries a number, and that number is the argument's identity — chosen by
the author, not derived from where the line sits. Reordering every line
in the file changes nothing. A gap or a duplicate is refused at load,
which the current implicit ordering cannot even detect.

**The number means the same thing whoever supplies it.** Argument one is
argument one whether it arrives from a shell, from a C caller pushing
values, or from an enclosing map's wire — the same way a C function's
first parameter does not care who called it. That is what makes a map
substitutable.

**Being an argument is derived, not stored.** A port is a command-line
argument when it is marked and nothing feeds it. A port that is both
marked and wired is fed both ways and is simply not an argv slot. This
replaces the idea of closing a door when something wires into it: there
is nothing to close, because nothing was stored to go stale, and fan-in
from outside and inside at once stays legal.

**The unfed-port warning is unaffected.** Bring-up warns about a station
with buffered inputs no arrow feeds, and a marked port is exempt — the
warning's own escape clause, unchanged. It is an explicit mark that
buys the exemption, never the mere absence of a wire, so a genuinely
forgotten wire stays distinguishable from a deliberate door.

**A station may hold ports of both kinds.** The old refusal existed
because the mark was on the station and a station is one thing; a port
is a smaller thing and a station with an argument port and a result port
is an ordinary station, not a mistake.

## Suggested implementation steps

1. Move the mark from the station struct to the input port struct, as a
   number: not-a-door, or argument *N*.
2. Replace the station-level designation call with a port-level one, and
   refuse a duplicate or a gap in the argument numbering when a program
   is brought up.
3. Change outside delivery to accept a station-and-port whose port
   carries a mark, rather than any port of a marked station.
4. Change command-line delivery to walk marked ports in argument-number
   order, and to skip a marked port that a wire also feeds.
5. Change the bring-up exemption and the nothing-can-start refusal to
   ask the port rather than the station.
6. Make instantiation carry port marks rather than station marks, which
   removes the stale-mark problem without a rule: an inner port that the
   enclosing map wires is no longer an argument, because being an
   argument is derived.
7. Tests: a map with two argument ports on one station; arguments
   renumbered by editing marks rather than moving lines; a marked port
   with a wire is not an argv slot; a duplicate argument number is
   refused.

## Related

- [213 — The input station](completed/213-the-input-station.md), which
  this replaces the mechanism of
- [209a — Results go where the caller says](209a-results-go-where-the-caller-says.md),
  the other half
- [601b — The dollar sign means the boundary](601b-the-dollar-sign-means-the-boundary.md),
  which spells this in the map file
- [217a — One receipt for a box and a map](217a-one-receipt-for-a-box-and-a-map.md),
  which addresses doors by number against a receipt
- [002 — Stations and ports](../docs/002-stations-and-ports.md)
