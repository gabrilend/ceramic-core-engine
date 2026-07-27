# 007 — Datapath: the build

A program made with this engine is two files that never meet until it
runs. One is C — a list of functions that might be called sometime,
somewhere. The other is a map — structured data saying which of them
are placed where and what feeds what.

The compiler only ever sees the first. This document is about what has
to be manufactured at build time so that the second can be read at
startup and turned into something executable.

## The problem the build solves

A worker holds a task and needs to call the box function it names. In
C, calling a function pointer requires the signature to be written
literally at the call site — the compiler must know what to place in
which register and what comes back. The worker cannot learn that at
runtime.

Every signature *is* known at compile time. But the worker has one call
site, and one line of source cannot be every signature at once.

So the call site has to be per-box, generated, with the station holding
a pointer to the right one.

## The shim

You write an ordinary function. Real types, by value, nothing special:

```c
int add(int a, int b) { return a + b; }
```

The generator reads the file, sees the declaration, and writes out:

```c
/* generated from: int add(int a, int b) */
void add__call(task_t *t) {
    int a = *(int *)t->in[0];
    int b = *(int *)t->in[1];
    int r = add(a, b);
    memcpy(t->out, &r, sizeof r);
}
```

Every generated shim has the same shape:

```c
typedef void (*box_call_t)(task_t *t);
```

Different insides, identical signature. So one table holds all of them,
a station stores one of these pointers, and the whole engine contains
exactly one call site:

```c
task_t *t = pop_task();
t->call(t);
deliver_output(t);
```

That line compiles against one signature, known at compile time, the
same for every box that will ever exist. The knowledge of what `add`
looks like was spent inside `add__call`, at a place where it was a
compile-time constant. By the time the pointer reaches the worker, the
signature has already been consumed and nobody needs to ask.

The box function itself is untouched by any of this. It takes real
types by value and returns one. The byte-poking lives outside it, in
generated code no human writes, and the compiler will almost certainly
inline `add` into its shim so the wrapper costs nothing.

## Why not the alternatives

**Every box takes an array of pointers and casts internally.** No shim
— but every box author then hand-writes casts, which is more unchecked
code, not less, and it gives up passing by value.

**Build the call frame at runtime from a type description.** This is
what libffi does, and it genuinely works. It costs an external
dependency, roughly fifty times the per-call overhead, and hand-written
assembly per architecture — because placing an integer in one register
file and a float in another, and passing a large struct on the stack
while a large *returned* struct displaces every other argument, cannot
be expressed in portable C at all. It is also the same machinery this
project dropped when it dropped the language bridge.

**Generate the shims with a macro.** Works, and was the first proposal.
Rejected in favour of a build-time script, because a generator is one
generalized program that parses things rather than a macro expanded
per box, and it leaves no macros in the source to read around later.

## The registry

The map file says `"add"` as text. Something has to turn that into a
function pointer. The generator emits a table, compiled into the
binary, holding for each box:

- its name, as it appears in a map
- its shim pointer
- the type and `sizeof` of each parameter, in order
- the type and `sizeof` of its return value

This table is the joint between the two halves of the program, and it
has a useful property: **the map file never has to mention a type.**

The loader knows the source box's return type and the destination
box's parameter type, both from the registry, both derived from the
actual C that will actually run. It checks the wire itself. If the map
also declared types, there would be two sources of truth that could
disagree, and the map would always be the one that was wrong.

The same table sizes every ring buffer cell, so a slot's cells are
exactly `sizeof` the parameter they feed and a write is a `memcpy` with
no allocation.

## What the generator parses

It does not need to understand C. It needs to recognize, in files
designated as box sources:

- **Function declarations** — name, return type, parameter types in
  order. These become shims and registry entries.
- **Struct definitions** — field names, types, and order. These give
  every value type a size, and a field table that lets the loader read
  a struct constant out of a map's statics table without a parser per
  type.
- **Compare functions**, found by the `__compare` suffix. These are
  what a comparator calls. The primitives get theirs generated; a
  struct supplies its own.

## What the compiler protects, and what it does not

The inside of every box is fully protected. A C function has exactly
one return type, so a box cannot be sometimes-int-sometimes-float, and
the compiler enforces every type inside it.

The wiring is protected by nothing, because after compilation there is
no type information left to check against. That is why the check moves
to the registry and happens when the map loads. It is the last place
the information still exists.

## Limitations, stated plainly

- **The build cannot check a wire**, because the map is not a build
  input. All wire checking is load-time. This is the price of maps
  being data rather than code, and it was chosen deliberately.
- **Typedefs are transparent.** `typedef int meters` and
  `typedef int seconds` are the same type and will connect happily.
  Distinguishing them means wrapping each in its own struct, which is
  the beginning of reimplementing a much larger type system, and is
  deliberately not done.
- **Nothing checks a hand-written shim.** Bypassing the generator
  restores every unchecked cast it was there to eliminate.

## Related

- [002 — Stations and slots](002-stations-and-slots.md), which stores the shim pointer
- [008 — Map file format](008-map-file-format.md), the other half
- [009 — Loading](009-datapath-load.md), where the registry is consulted
