# 311a — Boxes addressed by file

First child of [311](../311-the-registry-dissolved.md), and first because
it decides how a box is named and how its generated symbol is spelled.
Everything else in the family is written against both.

## Current behavior

**Built, both halves.**

The **symbol scheme** was done first, and everything else is written
against it: a symbol comes from the full path, canonicalised, with
punctuation transcribed into words so that `math.c` and `math_c`
cannot collapse into one identifier and the linker reject the build
with a message about a symbol rather than about two files.

The **map file format** is done now. A station line may name a box
three ways — a bare function name, a basename and a function, or a
path and a function — and the path is **not a fallback**: it is a more
specific way of saying the same thing, so nobody has to guess which
form is the real one and an author who prefers paths everywhere is not
fighting the format.

**Both halves of the resolution share one rule.** It is asked at
generation time, where a name becomes a pointer and where a refusal
reaches somebody who is still looking at the map; and at run time, for
placing by name, where the compiled-in rows and the rows that arrived
while the program ran are searched separately and must agree about
what a name means. They did not agree, briefly, and the symptom was a
dump that could not shorten an address it had just written — which is
why the rule is one function now rather than two loops.

**A collision is fatal and names both paths**, because the author's
fix is to write one of them out in full and they cannot do that
without being told which two files are in question.

**A station carries the whole address**, and the dump writes whichever
form is unambiguous — the bare name when it resolves to the same box,
the address when it does not. That is not tidiness: a box compiled
while a program ran lives at a serial-numbered path in a scratch
directory belonging to *that* process, so writing its address down
would produce a file naming somewhere nothing will be next time. The
bare name is what a later process can act on, and it is what makes a
grown program's dump reloadable.

**What unblocked it** was the generator learning to read maps
([311d](../311d-the-map-becomes-code.md)). This issue said the format
half was waiting on that, and it was right; what it did not say is how
little was left once it arrived.

### What the scheme turned out to need### What the scheme turned out to need

**The escape table in this issue was incomplete, and the gap was
load-bearing.** It transcribed the dot and the underscore so that
`math.c` and `math_c` could not collapse into one symbol — but it had
no rule for the **path separator**, while the design leans on paths to
settle two files sharing a basename. Those two cannot both hold: if a
symbol comes from the basename alone, two `math.c` files in different
directories produce one symbol and the linker rejects the build, so a
path settles nothing.

**Decided: the symbol comes from the full path**, canonicalised before
use so that a map naming a box briefly and one naming it by path
produce the *same* symbol rather than two definitions of one function.

Two more characters had to be faced, and this project's own sources
are why: they are named like `029-demo-boxes.c`, which is a **hyphen**
and a **leading digit**. So the table gained a row for the hyphen, and
every symbol carries a fixed prefix — which handles the leading digit
and keeps generated names clear of anything a box author writes, for
one decision.

| in a name | in a symbol | why |
|---|---|---|
| `.` | `_dot_` | a filename's extension |
| `/` | `_sl_` | the path, which is what settles a shared basename |
| `-` | `_dsh_` | this project's own sources |
| `_` | `_und_` | the escape character, escaping itself |
| `:` | `__` | the separator between file and function |
| anything else | `_xNN_` | hex, so no filename can defeat it |

The last row is the one that makes the scheme **total** rather than
merely adequate for names anybody has thought of. A character with no
rule would otherwise have to be dropped or flattened, and either is
how two files quietly become one symbol.

**Proven by property rather than by spelling**, because a bug here
does not surface here — it surfaces as a duplicate-symbol error
hundreds of lines away in a generated file, naming a mangled
identifier rather than the two source files that should have been told
apart. Forty-eight awkward names, chosen so that every pair collides
under a naive mangling, produce forty-eight distinct symbols, and
every one of them is a legal C identifier.

### What the map file still says

A map's station line names a box by a **bare function name**:

```
adder add p
```

`add` is looked up in the registry at load time, in one global table
holding every function the generator found anywhere under the box
source directory. Two files each defining a function called `read` are
a collision nobody declared and nothing detects — the second one
emitted simply wins, or the build breaks with a duplicate-symbol error
that names a symbol rather than a design mistake.

Nothing in a map says where a function lives, so nothing in a map tells
the build which sources a program actually needs.

## Intended behavior

**A station line names the file and the function.**

```
adder math.c:add p
```

**Provenance belongs in the file a person reads.** A bare name is not
an address; it is a name in a namespace nobody wrote down. Naming the
file makes it an address, and it is the same information a reader wants
anyway when they go looking for what `add` actually does.

**A bare file name is tried first; a path settles ties.** `math.c`
resolves when exactly one box source anywhere in the tree is called
that. If two are, resolution refuses and names **both paths**, and the
author writes one out in full:

```
adder src/boxes/math.c:add p
```

Both forms are legal at any time. **The path is not a fallback**, it is
a more specific way of saying the same thing, so nobody has to guess
which form is "the real one" — and an author who prefers paths
everywhere is not fighting the format.

The basename-first rule carries the cost of being convenient, and it is
worth naming: box source basenames are a **flat global namespace**, and
two files called `math.c` in different directories cannot both be
addressed briefly. That matches what the project already does — the
file index counter runs across the whole tree rather than per
directory, so a single global ordering of filenames is already the
model here.

**A collision is fatal**, naming both paths and saying which map line
asked. It is not a warning and it does not pick one.

**Resolution happens when the generator reads the map, not when a
program runs.** [311d](../311d-the-map-becomes-code.md) turns a map into
construction calls, so a name becomes a pointer at generation time and
no name survives into the running program. Everything in this issue is
therefore a *generation-time* rule, and every refusal it describes
happens while somebody is still looking at the map.

**The dump writes whichever form is unambiguous** — the bare name when
it resolves uniquely, the full path when it does not — reading the
string literal each station carries.

## Escaping, so two files cannot generate one symbol

A generated symbol has to be a C identifier, and `math.c:add` is not
one. Mangling it naively to `math_c__add` reintroduces exactly the
collision this issue exists to remove: **`math.c` and `math_c` both
become `math_c`**, the linker sees two definitions of one function, and
the build fails with a message about a symbol rather than about two
files that should have been named differently.

**So punctuation is transcribed into words, and the escape character
escapes itself:**

| in a name | in a symbol |
|---|---|
| `.` | `_dot_` |
| `_` | `_und_` |
| `:` | `__` (the separator between file and function) |

| file | symbol prefix |
|---|---|
| `math.c` | `math_dot_c` |
| `math_c` | `math_und_c` |
| `math_dot_c` | `math_und_dot_und_c` |
| `math.dot.c` | `math_dot_dot_dot_c` |

**Escaping the escape is what makes it reversible**, and it is the same
reason percent-encoding has to write `%` as `%25`. Without the `_und_`
rule the scheme would merely move the collision rather than remove it.
That last row looks alarming and decodes cleanly, scanning left to
right: `math` + `_dot_` + `dot` + `_dot_` + `c`.

**The scheme's job is uniqueness, not recovery.** Nothing decodes a
symbol back into a name at runtime — a name a person reads comes from
the string literal a placement function writes onto its station
([311b](311b-placement-instead-of-records.md)), because these are
static functions whose symbol names may not survive a stripped binary
at all. Two mechanisms, two jobs, neither doing the other's badly.

## Suggested implementation steps

1. **Done, and the refusal in this step was overturned by this
   issue's own later text.** The step wanted a bare function name
   refused with a message saying what to write instead. The Intended
   behaviour above says plainly that **both forms are legal at any
   time** and that the path is not a fallback, and that is what was
   built: a bare name resolves when it is unambiguous, and nobody has
   to guess which form is the real one.
2. **Done**, and in two places that share one function — at
   generation time, where a name becomes a pointer and a refusal
   reaches somebody still looking at the map, and at run time for
   placing by name. They must agree about what a name means and
   briefly did not; the symptom was a dump that could not shorten an
   address it had just written.
3. **Done.** The escaping, with a test over a list of awkward names —
   including `math.c`, `math_c`, `math_dot_c`, a name containing both
   characters, a path, and one of this project's own hyphenated
   sources — asserting the property that distinct names give distinct
   legal identifiers, rather than asserting particular spellings.
4. **Done, and it required undoing an older refusal.** The generator
   refused two boxes of one name anywhere in the tree, which was right
   while a symbol came from the bare function name — two of them
   produced one identifier and the linker rejected the build with a
   message about a symbol rather than about two files that should have
   been told apart.

   A symbol carries the path now, so two such boxes are two functions
   and refusing them forbids something the C has no problem with. What
   the repetition costs is that the *bare* name stops being an
   address, and that question belongs to resolution — where somebody
   is looking at the map line — rather than to the build, which has no
   map in front of it. Same file is still a redefinition, because C
   says so.
5. **Done.** The dump reads each station's address and writes the bare
   name when it resolves to the same box.

   It is not tidiness, which is worth recording because it looks like
   it. A box compiled while a program ran lives at a serial-numbered
   path in a scratch directory belonging to *that* process, so writing
   its address down produces a file naming somewhere nothing will be
   next time. The bare name is what a later process can act on, and it
   is what keeps a grown program's dump reloadable.
6. **Done.** Two sources in different directories each defining one
   function of the same name, and a map asking for it briefly: the
   generator refuses, names both paths, and writes no output. The same
   two sources with the line written out in full build, and the
   generated code calls the placement function belonging to the file
   that was named.

## Open questions

None outstanding.

## Related

- [311 — The registry dissolved](../311-the-registry-dissolved.md), the
  parent
- [311b — Placement instead of records](311b-placement-instead-of-records.md),
  which writes the name literal the dump reads
- [311d — The map becomes code](../311d-the-map-becomes-code.md), which is
  where resolution actually happens
- [008 — Map file format](../../docs/008-map-file-format.md), which gains
  the new station line and the resolution rule
- [703 — The map dump](703-map-dump.md), which has to choose
  a form
- [801 — The workbench in the browser](../801-browser-workbench.md), whose
  note already anticipated this: *where a function's home matters, the
  file name is typed beside it*
