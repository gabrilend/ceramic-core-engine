# 028-generate.lua — the generator, from outside

Runs at build time: box sources in, one generated C file out. After
it runs, adding a box to the engine is exactly "write a C function".

## Invocations

**`luajit 028-generate.lua <output.c> <box.c>...`** — parse every box
source and emit the registry file. On any parse problem: a message
naming file and line, a nonzero exit, and *no output written* — the
emission goes to a temporary name and is moved into place only on
success, so a build can never see a stale or partial registry.

**`luajit 028-generate.lua --describe <box.c>...`** — print what the
parser saw (structs, boxes, compares, with file:line each), for
diagnosing a build by reading the description instead of the
emission.

## The rules it enforces on box sources

- Value types are `typedef struct { ... } name;` — one field per
  declaration, nested structs defined separately and named.
- Every non-static function is a box. Static functions are private
  helpers. `type__compare` functions are three-way orderings —
  validated to be `int type__compare(type a, type b)` — and never
  become boxes.
- Parameter and return types: primitives, typedef'd structs, or
  `const char *` (a borrowed string). Returning a string pointer is
  refused — borrowed memory through a wire has no owner.

## What it emits (all sizes/offsets as sizeof/offsetof — the
compiler computes every number)

- One static shim per box, loading arguments by memcpy (the task's
  value area packs values unaligned; memcpy keeps it defined).
- `registry_boxes[]` / `registry_n_boxes` — name, shim, parameter
  types and sizes, return type and size, exact task size, compare.
- `registry_structs[]` / `registry_n_structs` — field tables with
  offsets, sizes, kinds, nested-table pointers, string lengths.
- Three-way compares for every primitive some box returns, plus
  generic wrappers over author-written struct compares.
