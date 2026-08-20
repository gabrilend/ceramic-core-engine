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
| 304 — struct field tables | **finally consulted** | Offsets from offsetof, kinds per field — everything shape comparison needs, never asked until now. |
| 305 — compare functions | complete | Primitives generated, author orderings wrapped, availability in the registry. |
| 306 — build integration | complete | Boxes discovered, registry regenerated on change, failure emits nothing. |
| 307 — phase 3 demo | complete | A box added live, the registry against sizeof, every shape through one call site. |
| 308 — the generator, in C | open | Removes LuaJIT from the build path, and makes the parser callable at runtime. |
| 309 — types compared by shape | open | Four ints are four ints; identical layouts wire together, identical names do not. |
| 310 — boxes compiled at runtime | open | Source in, shim out, registry row added — needs both of the above. |
| [311 — the registry dissolved](311-the-registry-dissolved.md) | open | **Parent.** The table goes away entirely — not shrinks, goes. A running program holds no name and no lookup, because the generator resolves every name while generating. |
| [311a — boxes addressed by file](311a-boxes-addressed-by-file.md) | open | A map names `file:function`. Bare basenames resolve, paths settle ties, and generated symbols escape punctuation so `math.c` and `math_c` cannot collide. |
| [311b — placement instead of records](311b-placement-instead-of-records.md) | open | A generated placement function per box writes a station directly, every size a folded constant. Hand placement turns out to be the primitive. |
| [311c — source rides in the binary](311c-source-rides-in-the-binary.md) | open | Each included box source emitted as a C array, so one file carries code, numbers, and text. |
| [311d — the map becomes code](311d-the-map-becomes-code.md) | open | The generator turns a map into the construction calls it describes, so no name survives into a running program. The build also learns what to include; the linker discards the rest. |
| [311e — running an arbitrary map](311e-running-an-arbitrary-map.md) | open | A tool that runs any map by compiling it rather than interpreting it — which is how opening a map you did not build survives the table being deleted. |

## What the phase established

**Every size and offset is a `sizeof` or `offsetof` expression the
compiler computes.** The generator never guesses a number. That is the
rule the whole engine rests on, and it is why a box compiled at runtime
still needs a compiler rather than just a signature: a signature gives
names, and names are not what the engine runs on.

**Type names ride along for the error message, not for the engine.**
"Four bytes versus four bytes" is not a sentence anybody can act on.
That reasoning holds even as names stop being what a wire is *checked*
against — under shape comparison the name is still reported, it is just
no longer the thing decided upon.

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
phases later they turn out to be the answer to two separate problems:
comparing types by shape, and turning bytes back into text. Emitting
more than the moment required was the right call.

Notes for the phase: parser and emitters are one script with distinct
stages (parse → validate → emit), sharing the pattern of the earlier
phases — one mechanism, proven aspect by aspect. That separation is
what makes the translation to C tractable, since the stages can be
moved one at a time.
