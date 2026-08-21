# 301 — The box-source parser

## Current behavior

Built, as a LuaJIT script in `scripts/` (Lua being the house language
for build tooling). Box sources are designated by location — anything
under `src/boxes/` — so a box cannot exist that the generator does not
see. The parser blanks comments, string contents, and preprocessor
lines while preserving newlines, then stops at every top-level brace
and classifies what led to it: a typedef struct (one field per
declaration, nested bodies refused by name), a function (non-static
becomes a box, static is a private helper, a `__compare` suffix is an
ordering and never a box), or an error naming file and line. Parse
and emit stayed separate: a describe mode prints the parser's
findings without emitting, and the make target `describe` exposes it.
Ground rules the issue left open are now written down in the script's
header: value types must be typedef structs, string returns are
refused (borrowed memory has no owner), parameters must be named.
Proven by shell tests covering braces inside strings, multi-line
declarations, helper exclusion, and errors that name their line.

## Intended behavior

A program that runs as part of the build, reads the files designated as
box sources, and comes away holding everything the rest of phase 3
needs to emit.

**It does not need to understand C.** That distinction is the whole
reason this is tractable — a full C parser needs to track typedefs to
even know whether a construct is a declaration, and that is a large
program. This one recognizes three things in files it is pointed at:

**Function declarations** — name, return type, and parameter types in
order. These become shims and registry entries.

**Struct definitions** — field names, types, and order. These give
every value type a size and a field table, which is what lets a map's
statics table hold a struct constant without a parser per type.

**Compare functions**, recognized by the `__compare` suffix. These are
what a comparator calls.

**What it produces** is an in-memory description handed to issues 302
through 305, which do the emitting. Keeping parse and emit separate
means the parser can be tested against source text alone, and each
emitter can be tested against a hand-built description.

**It fails rather than guesses.** A declaration it cannot parse stops
the build and names the file and line. A box source containing
something surprising is a thing the author needs to know about, and a
parser that quietly skips what it does not understand produces a
registry with a hole in it — which surfaces much later as a box that
cannot be found by name, pointing at the map rather than at the real
cause.

## Suggested implementation steps

1. Decide how box sources are designated — a directory, a naming
   convention, or a list in the build. Whichever it is, it should be
   impossible to write a box that the generator silently does not see.
2. Tokenize far enough to find top-level declarations. Comments and
   strings have to be skipped correctly or a brace inside one will
   throw off everything after it.
3. Extract function declarations into name, return type, parameter list.
4. Extract struct definitions into name and an ordered field list.
5. Recognize compare functions by suffix and associate each with its
   type.
6. Report the description in a readable form on demand, so a build
   problem can be diagnosed by looking at what the parser saw rather
   than at what it emitted.
7. Tests over source text covering: several declarations in one file,
   a struct returned by value, a function returning nothing, a
   declaration spanning several lines, and a brace inside a string
   literal.

## Related

- [007 — The build path](../../docs/007-datapath-build.md)
- Issues 302 through 305 — the emitters this feeds
- Issue 306 — where in the build this runs
