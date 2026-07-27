# 302 — Shim emission

## Current behavior

Shims are written by hand (issue 207). Each one contains casts that
nothing checks, and each is a promise a human made about types.

## Intended behavior

One generated shim per box, emitted into a build artifact that nobody
edits.

## The problem this solves

A worker holds a task and must call the box function it names. In C,
calling a function pointer requires the signature to be written
literally at the call site — the compiler must know what to place in
which register and what comes back. The worker cannot learn that at
runtime.

Every signature *is* known at compile time. But the worker has one call
site, and one line of source cannot be every signature at once.

So the call site has to be per box, generated, with the station holding
a pointer to the right one. That is all a shim is: the per-box call
site. By the time the pointer reaches the worker, the signature has
already been consumed and nobody needs to ask.

**Every shim has the same signature** — it takes a task and returns
nothing. Different insides, identical outside. So one table holds all
of them and the whole engine contains exactly one call site.

**The box function is untouched.** It takes real types by value and
returns one. The byte-copying lives outside it, in generated code no
human writes, and the compiler will almost certainly inline the box
into its shim so the wrapper costs nothing at runtime.

## Why not the alternatives

**Every box takes an array of pointers and casts internally.** No shim
— but every box author then hand-writes casts, which is more unchecked
code rather than less, and it gives up passing arguments by value.

**Build the call frame at runtime from a type description.** This is
what libffi does and it genuinely works. It costs an external
dependency, roughly fifty times the per-call overhead, and hand-written
assembly per architecture — because placing an integer in one register
file and a float in another, and passing a large struct on the stack
while a large returned struct displaces every other argument, cannot be
expressed in portable C at all.

**A macro instead of a generator.** Works, and was the first proposal.
Rejected because a generator is one generalized program that parses
things rather than a macro expanded per box, and it leaves no macros in
the source to read around later.

## Suggested implementation steps

1. Take the parsed description from issue 301 and emit one shim per box
   into a generated C file, each carrying a comment naming the
   declaration it came from.
2. Emit the shim type and the table that holds them.
3. Emit each box's exact task size, so issue 206's allocation is sized
   per box rather than maximally.
4. Delete the hand-written shims from issue 207 and update that issue's
   current-behavior section to say they are gone.
5. A test that a generated shim called with a hand-built task produces
   the same result as calling the box function directly — for a box
   taking two of one type, one taking two different types, one
   returning a struct, and one returning nothing.

## Related

- [007 — The build path](../docs/007-datapath-build.md)
- Issue 301 — the description this consumes
- Issue 207 — the hand-written shims this deletes
