# 069-genemit.c — the description becomes C

`ge_emit` writes four kinds of output in the order the registry header
expects: a three-way comparison per orderable return type, a shim per
box, a field table per struct, and the registry itself.

Every size and every offset is emitted as a `sizeof` or `offsetof`
expression rather than as a number, so the compiler computes all of
them and this program never guesses about padding or alignment. That
is guarantee C1, and it is why a box compiled later can wire into a
box compiled earlier — both numbers came from the same place.

Comparisons copy bytes into real typed variables before comparing,
because raw-byte comparison reads negative floats backwards. A struct
return with no author-written comparison gets a null in its row; the
refusal happens at load, when a comparator actually asks, because a
box whose value nobody compares is perfectly legal.

Shim loads are `memcpy` and never pointer casts: the task's value area
packs values back to back, so a double following an int sits
misaligned.

Output is built whole in memory, written to a temporary name, and
moved into place only on success — a generator that dies partway must
never leave yesterday's registry, or half of today's, where a build
can find it.
