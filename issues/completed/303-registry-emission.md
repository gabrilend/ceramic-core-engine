# 303 — The registry

## Current behavior

Built. The generated file defines a table of every box — name, shim
pointer, each parameter's type name and size, return type and size,
exact task size, and the compare function for its return type where
one exists — with every size a sizeof expression the compiler
computes. Type names ride along as text precisely for error
messages, since four bytes versus four bytes is not a message. The
hand-written support file provides lookup by name, a printout of the
whole table, and placement-by-name, which is the moment phase 2's
hand-supplied element sizes became registry lookups: a station placed
by the text "add" runs the generated shim with sizes correct by
construction, proven by a live map in the generator test. The
misspelled-name message names the name and where box sources live.

## Intended behavior

A table compiled into the binary, emitted by the generator, holding for
every box:

- its name, as it will appear in a map
- its shim pointer
- the type and `sizeof` of each parameter, in order
- the type and `sizeof` of its return value
- its exact task size

**This is the joint between the two halves of a program.** The C is
compiled and knows nothing about maps; a map is text and knows nothing
about code. The registry is the only place both are described, and it
is derived entirely from the C, which is what actually runs.

## The property that makes it worth building

**A map file never has to mention a type.**

The loader knows the source box's return type and the destination
box's parameter type, both from here, both derived from the real C. It
checks every wire itself. If the map also declared types there would be
two sources of truth able to disagree, and the map would always be the
one that was wrong — the compiler enforces the C, and nothing enforces
the file.

The same table sizes every ring buffer slot, so a port's slots are
exactly `sizeof` the parameter they feed and a write is a `memcpy` with
no allocation. In phase 2 those sizes were typed in by hand; from here
they are correct by construction.

## What this does not protect

The inside of every box is fully protected by the compiler — a C
function has exactly one return type, so a box cannot be
sometimes-one-thing-sometimes-another.

The wiring is protected by nothing until load time, because after
compilation there is no type information left to check against. The
registry is the last place it still exists. That is a deliberate
consequence of maps being data rather than code, and it means every
wire error is a startup failure rather than a build failure.

**Typedefs are transparent.** Two names for `int` are the same type and
will connect happily. Distinguishing them means wrapping each in its
own struct, which is the beginning of reimplementing a much larger type
system, and is deliberately not done.

## Suggested implementation steps

1. Emit the registry as a generated C file with a lookup by name.
2. Include the type name as text alongside each size — the size alone
   cannot tell an `int` from a `float`, and the error message is worth
   far more than the bytes.
3. Replace phase 2's hand-supplied element sizes with registry lookups.
4. A way to print the registry, so a build problem can be diagnosed by
   reading what was emitted.
5. A test that every box in the source appears exactly once, with sizes
   matching `sizeof` of the real types.

## Related

- [007 — The build path](../../docs/007-datapath-build.md)
- Issue 301 — the description this consumes
- Issue 603 — the load-time wire check that reads this
