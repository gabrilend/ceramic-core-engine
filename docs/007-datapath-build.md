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

## No table at all

The map file says `"math.c:add"` as text, and something has to turn
that into a function pointer. **The generator does, while generating.**

A map is a blueprint for the compilation rather than something a
program parses while it runs, so the generator reads it and emits the
construction calls it describes:

```c
void build_program(map_t *m) {
    map_place(m, 0, place__math_dot_c__add,   PLAIN);
    map_place(m, 1, place__io_dot_c__print,   PLAIN);
    map_wire (m, 0, /*out*/0, /*to*/1, /*port*/0);
    map_set_static(m, 0, /*port*/1, 5);
}
```

**No name survives into the running program**, so there is nothing to
look a name up in. The loader stops being a parser and becomes
generated code — which does not weaken *one way to build a program*,
since it calls the same construction surface a person would.

**Each box gets a placement function**, and every number it writes is a
constant the compiler folded:

```c
static void place__math_dot_c__add(station_t *s) {
    s->box_name = "math.c:add";      /* a literal, for the dump */
    s->call     = add__call;
    s->slots[0].elem_size = sizeof(int);
    s->slots[1].elem_size = sizeof(int);
    s->out_size = sizeof(int);
    s->compare  = int__compare_g;
}
```

**Why nothing else needs a lookup either.** A station copies what it
needs at placement and never consults anything again; the station
header is deliberately free of any reference to a table, which is why a
comparator resolves its comparison *at placement* rather than on the
delivery path. A struct port's field table is written onto the port by
the placement function, so reading `{ 1.5, 2.5, 3.5 }` out of a map
follows a pointer rather than searching by type name. And the name a
station reports is a string literal its own placement function wrote,
not an index into anything.

**There used to be a record per box** — parameter arrays, type-name
strings, the exact task allocation size, a backwards lookup from shim
pointer to name — and every field of it was read once, at placement, by
code that generated code could just as well have written. Issue 311 is
that change.

### Generated symbols escape punctuation, so two files cannot collide

`math.c:add` is not a C identifier. Mangling it naively to
`math_c__add` would make **`math.c` and `math_c` produce the same
symbol**, and the linker would fail with a message about a duplicate
symbol rather than about two files that should have been named
differently.

So punctuation is transcribed, and the escape character escapes itself:
`.` becomes `_dot_`, `_` becomes `_und_`, and the colon becomes a
double underscore. `math.c` gives `math_dot_c`; `math_c` gives
`math_und_c`. Escaping the escape is what makes the scheme injective,
for the same reason percent-encoding has to write `%` as `%25`.

Nothing decodes a symbol back into a name — a name a person reads comes
from the string literal, because these are static functions whose
symbol names may not survive a stripped binary at all.

### The useful property it keeps: the map file never mentions a type

The generator knows the source box's return type and the destination
box's parameter type, both derived from the actual C that will
actually run. If the map also declared types, there would be two
sources of truth that could disagree, and the map would always be the
one that was wrong.

Naming the *file* beside the function is provenance, not a type
declaration, so this still holds.

The same numbers size every ring buffer cell, so a port's cells are
exactly `sizeof` the parameter they feed and a write is a `memcpy` with
no allocation.

### Sizes cannot be computed at runtime, which is the one irreducible thing

`sizeof` is a **compile-time** operator. The compiler evaluates it and
burns a literal into the machine code. A running program has no types
at all — C erases every bit of type information during compilation — so
there is nothing left for `sizeof` to be applied to. You cannot hand a
running program the text `vec3` and get 12 back.

The trick is visible in the generated file's first real line: it
`#include`s the box source **whole**, so the types become visible to
the compiler, and only then writes `sizeof a0`.

So every size comes from exactly one of two places: a `sizeof`
expression compiled in, or a compiler invoked at runtime. There is no
third door — and it is the only thing about the old registry that could
not be avoided.

## What the build includes

**A map is also a manifest.** Having read it to emit the construction
calls, the generator knows exactly which box sources the program needs.
It includes those files whole and emits shims **only** for the
functions the map names. A program using three boxes out of five
hundred no longer carries five hundred shims.

**The linker decides what actually ships.** Built with
`-ffunction-sections -fdata-sections -Wl,--gc-sections`, every function
lands in its own section and the linker discards every section nothing
reaches — computing exact reachability through includes, through
hand-written `extern` declarations, and through function pointers taken
by name. That is every case a source parser would get wrong, and it
costs nothing but build time.

**Following `#include` directives instead would be a heuristic with a
hole in it**, worth naming so nobody re-proposes it: linking resolves
*symbols*, not includes, so a file may call a function it never
included a header for by declaring it by hand.

**And the build now checks every box reference.** A map naming a
function that does not exist, a file that does not exist, or a bare
basename matching two files with no path given — all of it fails at
build time, on the author's machine, naming the map line. Wire checking
does not move; it depends on how stations are actually connected and
stays at load. What moves is *"you named a box that isn't there."*

**Each included source is also emitted as text**, as a C string array,
so the binary carries the C it was made from. That is where error
messages and the dump get type and argument names, now that the engine
carries none — and because the embedded text is by definition the text
that was compiled, a name reported can never come from a source that
has since changed on disk.

## What the generator parses

It does not need to understand C. It needs to recognize, in files
designated as box sources:

- **Function declarations** — name, return type, parameter types in
  order. These become shims and placement functions.
- **Struct definitions** — field names, types, and order. These give
  every value type a size, and a field table that lets a struct
  constant be read out of a map without a parser per type.
- **Compare functions**, found by the `__compare` suffix. These are
  what a comparator calls. The primitives get theirs generated; a
  struct supplies its own.

## What the compiler protects, and what it does not

The inside of every box is fully protected. A C function has exactly
one return type, so a box cannot be sometimes-int-sometimes-float, and
the compiler enforces every type inside it.

The wiring is protected by nothing, because after compilation there is
no type information left to check against. That is why the check moves
to load time: the sizes the compiler folded into the placement
functions are the last surviving trace of the types, and load is the
last place that trace still exists.

## Limitations, stated plainly

- **The build checks that a named box exists; whether it checks a
  *wire* is now an open question rather than a settled no.** It used to
  be a flat no, because the map was not a build input. Now that it is,
  the generator knows both ends of every wire a map draws — and while
  it still cannot compute a size, it can emit a `_Static_assert` that
  makes the compiler compare them. That would move a class of error
  from load to build for every wire written in a file. Wires drawn at
  runtime still need the load-time check, so both would exist. Issue
  311d carries the question.
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
- [009 — Loading](009-datapath-load.md), which describes a runtime parser
  that this replaces with generated code
