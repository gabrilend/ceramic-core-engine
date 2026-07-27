# 304 — Struct field tables

## Current behavior

The registry knows every struct's total size, which is enough to
allocate a ring buffer cell and copy bytes into it. It does not know
what is inside one.

## Intended behavior

For every struct used as a box's parameter or return value, emit a
table describing its fields: each one's offset, its size, and whether
it is an integer, a floating-point number, a string, or a nested
struct.

**What this is for.** A map's statics table holds constants written as
text — a threshold, a file path, or a structured value written with
braces. Turning `{ 5, 2.0, { 0, 0, 0 }, "hey there", 2 }` into bytes
requires knowing where each number lands.

The alternative is a parser emitted per struct type. The field table is
smaller: one generalized reader walks a table, rather than a function
generated for every shape. It is also the version that matches how the
rest of the generator works — a table of data driving one routine,
instead of code stamped out per case.

**Nested structs are entries pointing at another table**, walked
recursively. This is what lets a static hold something genuinely
structured rather than only a flat row of numbers.

**The table carries no type names, only shapes.** Which type a static
entry is read as is decided by the slot claiming it, and the slot's
type comes from the box function's parameter position. The statics
table itself never says. That is the same rule that keeps types out of
the wiring, applied one level down: one source of truth, and it is the
C.

A consequence worth knowing: two slots of different types may reference
the same statics entry and each read it their own way.

## Suggested implementation steps

1. Extend the parsed description from issue 301 to carry each struct's
   ordered field list with computed offsets. Offsets must come from the
   compiler rather than from arithmetic in the generator — padding and
   alignment are the compiler's business, and a generator that computes
   them itself will eventually disagree with it.
2. Emit one field table per struct, with nested structs referring to
   the table of the type they contain.
3. Emit the lookup from type name to field table.
4. The reader that walks a field table and a text value together,
   filling bytes. It belongs with the statics table in issue 402, but
   the table it walks is emitted here.
5. Tests over a flat struct, a struct containing a nested struct, a
   struct containing a string, and a struct whose padding is
   non-obvious — assert the bytes produced match a compiled initializer
   of the same value.

## Related

- [007 — The build path](../docs/007-datapath-build.md)
- [008 — Map file format](../docs/008-map-file-format.md), the statics table
- Issue 402 — the reader that uses these
