# 026-emitted.h — the shapes the generator emits, from outside

The joint between compiled C and text maps: for every box, its name,
its shim, and its full type story — all derived from the C that will
actually run, which is why maps never need to mention a type.

## Data structures

**box_param** — `{ type_name: text, size: int }`, one per parameter,
in declaration order.

**the box record — deleted (issue 311b).** It held a name, a shim
pointer, a parameter count, an array of parameter type names and
sizes, a return type name and size, the exact task allocation, and a
comparison function.

**It was read exactly once, at placement, and never again.** A station
holds its own shim, its own slot sizes, its own return size and its
own comparison, and the station header is deliberately free of any
reference back — so the record existed only to be read at the one
moment generated code could just as well do the writing. That is what
a placement function does.

Every number in it was a `sizeof` the compiler folded. They did not
become guesses by moving: they moved from a table into a function,
where the compiler folds them into immediates and they are not stored
at all. Guarantee C1 is untouched.


**field_info / struct_info** — per-struct field tables: name, offset
(offsetof), size, kind (int/uint/float/string/struct), nested table
pointer, string length. What lets the statics reader turn brace text
into bytes without a parser per type.

## Functions

**box_place_find(name) → placement row or null** — the lookup every
map name goes through, and now the only one. Compiled-in rows first,
then anything that arrived after the program started: a box the
program was built with wins over one added afterwards under the same
name, so bringing in new code can never quietly replace something a
map already depends on.

That ordering rule was the record's before it was this one's. The
record is gone, and this is the whole of by-name anything.


**struct_find(type name) → struct_info or null**

**emitted_print(stream)** — every box and struct, sizes and all,
for reading what was emitted.

**map_place_box(map, station, box name, kind)** — placement by name
with sizes drawn from the emitted file; comparators get their extra
threshold port here, typed to the box's return. Aborts loudly on an
unknown name — the most common map mistake there is.

**box_source_text(path) → the C it was compiled from, or null** — by
full path or by basename, the two ways a box is addressed. Asks what
the build compiled in first, then what has arrived since, so a program
that has been handed code can still say what all of it is made of
rather than only what it was born with. The build wins a tie, because
the answer should describe the program rather than the last thing that
happened to it.

It copies nothing: what comes back points into the binary, or into a
loaded object, and lives as long as that does.

Two things read it — writing a grown program back out as something
that can be built again, and refusing to compile a source that is
already here under the same path with the same bytes.

**map_build_find(path) → the compiled form of one description** — the
same two ways of naming, for the same reason.
