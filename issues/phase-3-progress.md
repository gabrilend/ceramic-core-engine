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
| [311 — the registry dissolved](completed/311-the-registry-dissolved.md) | **complete** | **Parent.** The table telling the engine what every box *is* went away entirely — not shrank, went. One claim in the original draft did not survive, and it is the most useful thing the family produced: a running program was to hold no name and no lookup, and that turned out to be incompatible with a requirement nobody had written down — the engine is agnostic about what runs inside it, so a program must be extendable while it runs. A program that can be handed a description next year has to let it bind to a box compiled today, which means publishing names. So names did not disappear; they moved into the operating system's own symbol table, which every process already carries and nothing here maintains. |
| [311a — boxes addressed by file](completed/311a-boxes-addressed-by-file.md) | **complete** | A map names a box three ways — bare function, basename and function, or path and function — and the path is a more specific way of saying the same thing rather than a rescue. A collision refuses and names both paths. It required undoing an older refusal: the generator forbade two boxes of one name anywhere, which was right while a symbol came from the bare name and is wrong now that it carries the path. The dump writes the short form when it resolves, which is what keeps a grown program's dump reloadable. |
| [311b — placement instead of records](completed/311b-placement-instead-of-records.md) | **complete** | A generated placement function per box writes a station directly, every size a folded constant, and the box record beside it is gone — type, table, parameter arrays, type names, task size, comparison. It did not have to wait on 311d after all: the step assumed one table where there were two, and only the *placement* table is needed to resolve a name. The word did not have to wait either — it turned out to be doing three unrelated jobs, only one of which was the deleted record. |
| [311c — source rides in the binary](completed/311c-source-rides-in-the-binary.md) | **complete** | Each included box source emitted a second time as a C array, and a test compares the carried text against the file on disk byte for byte. Reading *names* back out of it turned out not to be wanted: a refusal names both ends of a wire by position and by size, because a name is not what makes a wire legal — the width is — so a message built around names points at the thing that is not the disagreement. |
| [311d — the map becomes code](completed/311d-the-map-becomes-code.md) | **complete** | A description becomes the calls it describes, at build time and while a program runs, through one pipe: generator, compiler, load. The engine's own way of reading one is deleted and the parser left with it, so no program built with this engine carries a parser. Three routes to one program — read as text, compiled at build, compiled while running — dump identically. Two of the twelve steps were dropped rather than done, both because later work moved the ground under them: trimming what the generator emits was a guess at an answer the linker computes exactly, and deleting the last name table was written to serve a premise extendability had already spent. |

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

## How it came to be this way

These are the turns the design actually took, lifted out of the source
comments where they had been sitting. They describe states the engine is
no longer in, which is why they are here rather than beside the code: a
comment is for what is true now.

### A struct's layout stopped being a table and became two functions

A struct static was turned into bytes by walking a generated table of
field offsets and widths, and the port found that table by searching
every emitted struct for one whose name matched. The generated pair now
reaches each field by name, so no offset is stored or computed anywhere,
and the placement function hands the pair over directly instead of
leaving a search to answer a question it already knew.

### The symbol escape had to be made injective, not merely tidy

The first mangling rule replaced punctuation with underscores and had no
entry for the separator between a file and a function, nor for the
underscore itself. That makes `math.c` and `math_c` produce one symbol,
and the linker's complaint is then about a duplicate symbol rather than
about two files that should have been named differently. Worse, the
design leans on paths to tell two files with one basename apart — so a
symbol built from the short name alone would make writing the path settle
nothing. Every punctuation mark now transcribes to a distinct spelling,
the escape character escapes itself, and anything with no rule becomes
hex: the same reason percent-encoding has to write `%` as `%25`.
