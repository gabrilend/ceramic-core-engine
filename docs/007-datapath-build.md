# 007 — Datapath: the build

A program made with this engine is two files that never meet until it
runs. One is C — functions that might be called sometime. The other is a
map — which of them are placed where and what feeds what.

The compiler only ever sees the first. This document is what has to be
manufactured at build time so that the second becomes something
executable.

## The problem the build solves

A worker holds a task and needs to call the box function it names. In C,
calling a function pointer requires the signature to be written literally
at the call site — the compiler must know what to place in which register
and what comes back. The worker cannot learn that at runtime, and one
line of source cannot be every signature at once.

So the call site has to be per-box, generated, with the station holding a
pointer to the right one.

## The shim

You write an ordinary function. Real types, by value:

```c
int add(int a, int b) { return a + b; }
```

The generator reads the file, sees the declaration, and writes out:

```c
/* generated from: int add(int a, int b) */
void add__call(cera_task_t *t) {
    int a = *(int *)t->in[0];
    int b = *(int *)t->in[1];
    int r = add(a, b);
    memcpy(t->out, &r, sizeof r);
}
```

Every shim has the same shape — `void (*)(cera_task_t *)` — so a station
stores one of these pointers and the whole engine contains exactly one
call site:

```c
cera_task_t *t = pop_task();
t->call(t);
deliver_output(t);
```

The knowledge of what `add` looks like was spent inside `add__call`, at a
place where it was a compile-time constant. By the time the pointer
reaches the worker, the signature has been consumed.

The box function is untouched by any of this. The byte-poking lives
outside it, in generated code, and the compiler will almost certainly
inline `add` into its shim so the wrapper costs nothing.

## Why not the alternatives

**Every box takes an array of pointers and casts internally.** No shim,
but every box author hand-writes casts — more unchecked code, not less —
and it gives up passing by value.

**Build the call frame at runtime from a type description.** What libffi
does. It costs an external dependency, roughly fifty times the per-call
overhead, and hand-written assembly per architecture: placing an integer
in one register file and a float in another, and passing a large struct
on the stack while a large *returned* struct displaces every other
argument, cannot be expressed in portable C. It is also the machinery
this project dropped when it dropped the language bridge.

**Generate the shims with a macro.** The first proposal. A build-time
generator is one program that parses things rather than a macro expanded
per box, and it leaves no macros in the source to read around later.

## A map is compiled, not parsed

The map says `"math.c:add"` as text, and the generator turns that into a
function pointer while generating. A map is a blueprint for the
compilation, so the generator reads it and emits the construction calls
it describes:

```c
void build_program(cera_map_t *m) {
    cera_map_place(m, 0, place__math_dot_c__add,   PLAIN);
    cera_map_place(m, 1, place__io_dot_c__print,   PLAIN);
    cera_map_wire (m, 0, /*out*/0, /*to*/1, /*port*/0);
    map_set_static(m, 0, /*port*/1, 5);
}
```

**No name survives into the running program**, so there is nothing to
look a name up in and no table to hold one. The loader is generated code
calling the same construction surface a person would.

**Each box gets a placement function**, and every number it writes is a
constant the compiler folded:

```c
static void cera_box_src_sl_boxes_sl_math_dot_c__add__place(
        cera_map_t *m, int station, int kind) {
    int extra = (kind == CERA_STATION_COMPARATOR) ? 1 : 0;
    int sizes[] = { (int)sizeof(int), (int)sizeof(int), (int)sizeof(int) };
    cera_map_place(m, station, add__call, kind, 2 + extra, sizes,
              (int)sizeof(int));

    cera_station_t *s = cera_map_station(m, station);
    s->box_name = "add";             /* a literal, for the dump */
    s->in_ports[0].type_name = "int";
    s->in_ports[1].type_name = "int";
    if (extra) {
        s->in_ports[2].type_name = "int";
        s->compare = int__compare_g;
    }
}
```

**Nothing else needs a lookup either.** A station copies what it needs at
placement and never consults anything again — which is why a comparator
resolves its comparison *at placement* rather than on the delivery path,
why a struct port's field table is written onto the port so reading
`{ 1.5, 2.5, 3.5 }` follows a pointer, and why the name a station reports
is a string literal rather than an index.

### Generated symbols escape punctuation, so two files cannot collide

`math.c:add` is not a C identifier. Mangling it to `math_c__add` would
make **`math.c` and `math_c` produce the same symbol**, and the linker
would complain about a duplicate symbol rather than about two files that
should have been named differently.

| in a name | in a symbol | why |
|---|---|---|
| `.` | `_dot_` | a filename's extension |
| `/` | `_sl_` | the path, which settles two files sharing a basename |
| `-` | `_dsh_` | this project's own sources are named like `029-demo-boxes.c` |
| `_` | `_und_` | the escape character, escaping itself |
| `:` | `__` | the separator between file and function |
| anything else | `_xNN_` | hex, so no filename can defeat it |

`math.c` gives `math_dot_c`; `math_c` gives `math_und_c`. Escaping the
escape is what makes the scheme injective, for the same reason
percent-encoding writes `%` as `%25`. The last row makes it **total**: a
character with no rule would have to be dropped or flattened, and either
is how two files quietly become one symbol.

**The symbol comes from the whole path**, settled to one spelling first,
so naming a box briefly and naming it in full produce the same symbol.

**Every symbol carries a fixed prefix**, which keeps generated names
clear of anything a box author writes and means a file whose name begins
with a digit, as every source here does, still produces a legal
identifier.

Nothing decodes a symbol back into a name — a name a person reads comes
from the string literal, because these are static functions whose symbol
names may not survive a stripped binary.

### The map file never mentions a type

The generator knows both ends' types from the actual C that will run. If
the map also declared types there would be two sources of truth that
could disagree, and the map would always be the wrong one. Naming the
*file* beside the function is provenance, not a type declaration.

The same numbers size every ring buffer slot, so a port's slots are
exactly `sizeof` the parameter they feed and a write is a `memcpy` with
no allocation.

### Sizes cannot be computed at runtime

`sizeof` is a **compile-time** operator: the compiler evaluates it and
burns a literal into the machine code. A running program has no types at
all — C erases every bit of type information during compilation — so
there is nothing left to apply it to. You cannot hand a running program
the text `vec3` and get 12 back.

The trick is in the generated file's first real line: it `#include`s the
box source **whole**, so the types become visible to the compiler, and
only then writes `sizeof a0`. Every size therefore comes from one of two
places — a `sizeof` compiled in, or a compiler invoked at runtime. There
is no third door.

## What the build includes

**The linker decides what actually ships.** With
`-ffunction-sections -fdata-sections -Wl,--gc-sections`, every function
lands in its own section and the linker discards every section nothing
reaches — computing exact reachability through includes, through
hand-written `extern` declarations, and through function pointers taken
by name. That is every case a source parser would get wrong.

**Following `#include` directives instead would be a heuristic with a
hole in it**, worth naming so nobody re-proposes it: linking resolves
*symbols*, not includes, so a file may call a function it never included
a header for.

**What the linker may discard is decided by what the program exports**,
and the two settings pull against each other. A symbol in the dynamic
table cannot be collected, because code compiled later may look it up by
name — so `-rdynamic` quietly makes the whole binary a root. The engine's
public surface is named instead, in
[src/098-engine-surface.syms](../src/098-engine-surface.syms.info.md),
and handed over as `--dynamic-list`.

**The build checks every box reference.** A map naming a function that
does not exist, a file that does not exist, or a bare basename matching
two files with no path given — all of it fails at build time, naming the
map line. Wire checking stays at load, because it depends on how stations
are actually connected.

**Each included source is also emitted as text**, as a C string array, so
the binary carries the C it was made from. That is where error messages
and the dump get type and argument names — and because the embedded text
is by definition the text that was compiled, a name reported can never
come from a source that has since changed on disk.

## What the generator parses

It does not need to understand C. In files designated as box sources it
recognizes:

- **Function declarations** — name, return type, parameter types in
  order. These become shims and placement functions.
- **Struct definitions** — field names, types, and order. These give
  every value type a size, and a field table that lets a struct constant
  be read out of a map without a parser per type.
- **Compare functions**, found by the `__compare` suffix. The primitives
  get theirs generated; a struct supplies its own.

## What the compiler protects, and what it does not

The inside of every box is fully protected: a C function has exactly one
return type, so a box cannot be sometimes-int-sometimes-float.

The wiring is protected by nothing, because after compilation there is no
type information left to check against. That is why the check moves to
load time — the sizes the compiler folded into the placement functions
are the last surviving trace of the types.

## Limitations, stated plainly

- **The build checks that a named box exists. It does not check a wire,
  and deliberately does not.** The generator could: it sees both ends of
  every wire a map draws, and a `_Static_assert` would compare their
  widths. What decides against it is what a map is for at build time —
  the build reads a map to learn **which functions to compile in**, and
  nothing more. Whether the shape is complete is a run-time question,
  because **a half-wired map is a legitimate map**, and a build that
  refused an unfinished one would refuse exactly the program somebody is
  in the middle of writing.
- **Typedefs are transparent.** `typedef int meters` and
  `typedef int seconds` are the same type and will connect happily.
  Distinguishing them means wrapping each in its own struct, which is the
  beginning of reimplementing a much larger type system.
- **Nothing checks a hand-written shim.** Bypassing the generator
  restores every unchecked cast it was there to eliminate.

## Related

- [002 — Stations and ports](002-stations-and-ports.md), which stores the
  shim pointer
- [008 — Map file format](008-map-file-format.md), the other half
- [009 — Loading](009-datapath-load.md), what the emitted build function
  is called by
