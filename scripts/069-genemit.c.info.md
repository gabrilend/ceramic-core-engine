# 069-genemit.c — the description becomes C

`ge_build` writes four kinds of output in the order the emitted-shapes
header expects: a three-way comparison per orderable return type, a shim
per box, a field table per struct, and the table of placements itself.
`ge_emit` calls it and puts the result in a file.

They are two because there are two callers wanting two different things.
The build-time generator wants a file on disk. `cerac` wants the text and
never writes it anywhere, because it concatenates the engine in front of
it and hands the whole thing to the compiler down a pipe — and a
generator whose only exit was a filename forced a file to exist, which
was the reason a scratch directory looked unavoidable (issue 910).

`ge_main` writes the `main` a program made of only a description gets.
Nothing is emitted for its arguments, because the engine derives which
marked ports a command line fills and one call hands the whole line over.
Results need the emitter: printing a value means knowing its shape, and
that is the return type of the box at the marked station — a build-time
fact that no longer exists at run time. Each result gets an array to land
in and a printer chosen from its type, and values go out one per line
through the same writers that put a constant into a map file, so what
comes out can be fed back in.

A `const char *` return has no branch there, because the parser refuses a
box that returns one: the bytes live wherever the pointer points and a
value on a wire has no owner to keep them alive. A result can therefore
never be one.

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
never leave yesterday's emission, or half of today's, where a build
can find it.
