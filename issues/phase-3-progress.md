# Phase 3 progress — the build path

Phase 3's goal: the generator, and the end of hand-written glue.
Writing a box becomes writing a function; everything the engine needs
to call it by name is derived from the C itself.

**What is being added is a change of when, not of what.** The generator
runs at build time and produces a fixed table, so the set of boxes a
program can place is frozen before it starts. The open issues loosen
that in order: the generator becomes a C program rather than a Lua
script, so it can be called by something other than a build; types stop
being compared by name and start being compared by width, so a box
compiled later cannot silently disagree about a struct; and then a
box's source can be handed to a running program.

**And then the table goes away entirely.** Issue 311 asks what the
registry is actually for and finds exactly one answer that survives: a
size can only be computed by a compiler. Everything else was avoidable.
The record per box becomes a generated placement function holding
folded constants; the box source rides along in the binary as text;
and the map file stops being something a program parses and becomes a
blueprint the generator turns into construction calls, so **no name
survives into a running program at all**. What is left is a tool that
runs an unfamiliar map by compiling it, which is how that capability
survives the table being deleted.

| Issue | State | In one line |
|---|---|---|
| 301 — box-source parser | complete | Lua script; three shapes recognized, everything else stops the build with file:line. |
| 302 — shim emission | complete | One generated call site per box, memcpy loads, exact task sizes. |
| 303 — registry emission | **extended** | Name to shim to full type story; the table becomes growable. |
| 304 — struct field tables | **finally consulted** | Offsets from offsetof, kinds per field. Emitted for shape comparison, which was designed and not taken; consulted instead by the statics reader and the writer that turns bytes back into text. |
| 305 — compare functions | complete | Primitives generated, author orderings wrapped, availability in the registry. |
| 306 — build integration | complete | Boxes discovered, registry regenerated on change, failure emits nothing. |
| 307 — phase 3 demo | complete | A box added live, the registry against sizeof, every shape through one call site. |
| [308 — the generator, in C](completed/308-generator-in-c.md) | **complete** | LuaJIT is off the build path: a program built with this engine needs a C compiler and nothing else. Parity with the retired script was proven byte for byte — 259 lines of registry and thirteen refusal messages — and the comparison retired with the thing it compared against. |
| [309 — types compared by width](completed/309-types-by-width.md) | **complete** | A wire is legal when both sides count the same bytes. Identical layouts under different names now connect, which is what it buys; same-width types of different layouts also connect, which is what it costs, recorded as its own non-guarantee. Names ride along in the message, with both widths beside them. |
| [310 — boxes compiled at runtime](completed/310-boxes-compiled-at-runtime.md) | **complete** | Source in, shim out, row added — proven eight ways, including a dump of a grown program reloading in a fresh process, and a box unloaded while the pool was busy. |
| [311 — the registry dissolved](311-the-registry-dissolved.md) | open | **Parent.** The table goes away entirely — not shrinks, goes. A running program holds no name and no lookup, because the generator resolves every name while generating. |
| [311a — boxes addressed by file](311a-boxes-addressed-by-file.md) | **symbol scheme done**, map format waits | A map names `file:function`. Bare basenames resolve, paths settle ties, and generated symbols escape punctuation so `math.c` and `math_c` cannot collide. |
| [311b — placement instead of records](311b-placement-instead-of-records.md) | **placement routed**, deletions wait | A generated placement function per box writes a station directly, every size a folded constant. Hand placement turns out to be the primitive. A struct port is handed its field table, so the engine resolves no struct by name. And the refusal path stopped fetching type spellings, because a refused wire now names both ends by position and by size — one fewer reason for the record to exist. What is left is the deletions, which wait on 311d. |
| [311c — source rides in the binary](completed/311c-source-rides-in-the-binary.md) | **complete** | Each included box source emitted a second time as a C array, and a test compares the carried text against the file on disk byte for byte. Reading *names* back out of it turned out not to be wanted: a refusal names both ends of a wire by position and by size, because a name is not what makes a wire legal — the width is — so a message built around names points at the thing that is not the disagreement. |
| [311d — the map becomes code](311d-the-map-becomes-code.md) | **capability built**, deletions wait | The generator reads the descriptions the build is told about and emits a function that builds each one — every box name resolved on the author's machine into a direct call to that box's placement function, so a misspelled box fails the build rather than somebody else's startup. A map read as text and the same map compiled into calls dump identically. What has not happened is everything that *removes* something: the engine still carries the parser, every box still gets a shim, and the linker is not yet told to discard what nothing reaches. |

## What the phase established

**Every size and offset is a `sizeof` or `offsetof` expression the
compiler computes.** The generator never guesses a number. That is the
rule the whole engine rests on, and it is why a box compiled at runtime
still needs a compiler rather than just a signature: a signature gives
names, and names are not what the engine runs on.

**Type names ride along for the error message, not for the engine.**
"Four bytes versus four bytes" is not a sentence anybody can act on.
That reasoning holds even as names stop being what a wire is *checked*
against — a wire is now checked by **width**, and the name is still
reported, with both widths beside it, so a person can see why two
types disagree rather than only that they do.

**Loads and stores inside a shim are memory copies rather than pointer
casts**, because the task's value area packs values back to back and a
double following an int sits unaligned. The issue's own example code
used casts and would have been wrong.

**Box sources are designated by location**, so a box cannot exist that
the generator does not see. And **a failing generator writes nothing**,
so the build can never compile against yesterday's registry — a
guarantee that has to survive into runtime compilation, where a failed
generation must likewise leave nothing loadable.

**The field tables were built and then not used for anything.** Four
phases later they answer two problems the moment did not require:
turning brace text into bytes, and turning bytes back into text. They
were also the whole answer to comparing types by shape, which was
designed in full and then not taken — width comparison won on being one
integer against another. Emitting more than the moment required was
still the right call; two of the three uses arrived.

Notes for the phase: parser and emitters are one script with distinct
stages (parse → validate → emit), sharing the pattern of the earlier
phases — one mechanism, proven aspect by aspect. That separation is
what makes the translation to C tractable, since the stages can be
moved one at a time.
