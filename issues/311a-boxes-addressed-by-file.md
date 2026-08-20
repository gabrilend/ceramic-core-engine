# 311a — Boxes addressed by file

First child of [311](311-the-registry-dissolved.md), and first because
it decides how a box is named and how its generated symbol is spelled.
Everything else in the family is written against both.

## Current behavior

**The escaping is built and proven; the map-file half waits, and the
waiting is structural rather than a matter of effort.**

This issue reads as one change and is two. The **symbol scheme** — how
a box's file and function become one C identifier — is what
[311b](311b-placement-instead-of-records.md) needs in order to emit a
placement function per box, and it stands entirely on its own. The
**map file format** — a station line naming `file:function`, and the
resolution rules behind it — is a generation-time rule, and the
generator does not read maps until
[311d](311d-the-map-becomes-code.md), which waits on the construction
surface, which waits on 311b. Building the format half now would mean
putting it in the runtime loader that 311d deletes.

So the order is: the scheme, then placement functions, then the
construction surface, then the map format arrives with the generator's
map reader.

### What the scheme turned out to need

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
program runs.** [311d](311d-the-map-becomes-code.md) turns a map into
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

1. The map parser — in the generator, which is now its only home —
   accepts `file:function` on a station line, and refuses a bare
   function name with a message saying what to write instead. The old
   form loading into *something* would be worse than it refusing.
2. Resolution: exact path match when the name contains a separator,
   otherwise basename search across the known box sources. One match
   resolves; several refuse and name every candidate.
3. **Done.** The escaping, with a test over a list of awkward names —
   including `math.c`, `math_c`, `math_dot_c`, a name containing both
   characters, a path, and one of this project's own hyphenated
   sources — asserting the property that distinct names give distinct
   legal identifiers, rather than asserting particular spellings.
4. The collision check over the whole box source tree. **Note what the
   decision above did to this step**: two files sharing a basename no
   longer collide at the symbol level, because the symbol carries the
   path, so this is no longer a build-breaking condition to detect. It
   becomes what it always should have been — the question resolution
   asks when a map addresses a basename briefly, and it belongs with
   the resolution in step 2.
5. The dump reads each station's name literal and chooses bare-or-path
   by asking whether the bare form resolves uniquely.
6. A test that a map naming an ambiguous basename refuses and names
   both paths; and a round-trip test on a program containing two boxes
   whose files share a basename, proving the dump wrote paths where it
   had to.

## Open questions

None outstanding.

## Related

- [311 — The registry dissolved](311-the-registry-dissolved.md), the
  parent
- [311b — Placement instead of records](311b-placement-instead-of-records.md),
  which writes the name literal the dump reads
- [311d — The map becomes code](311d-the-map-becomes-code.md), which is
  where resolution actually happens
- [008 — Map file format](../docs/008-map-file-format.md), which gains
  the new station line and the resolution rule
- [703 — The map dump](completed/703-map-dump.md), which has to choose
  a form
- [801 — The workbench in the browser](801-browser-workbench.md), whose
  note already anticipated this: *where a function's home matters, the
  file name is typed beside it*
