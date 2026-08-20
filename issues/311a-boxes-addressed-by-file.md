# 311a — Boxes addressed by file

First child of [311](311-the-registry-dissolved.md), and first because
it decides how a box is named and how its generated symbol is spelled.
Everything else in the family is written against both.

## Current behavior

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
3. The escaping, as its own small pair of functions with a test that
   round-trips a list of awkward names — including `math.c`, `math_c`,
   `math_dot_c`, and a name containing both characters — because a bug
   here surfaces as a duplicate-symbol error hundreds of lines away.
4. The collision check run over the whole box source tree rather than
   only over what a map mentions, so two `math.c` files are caught
   whether or not anyone has referenced them yet.
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
