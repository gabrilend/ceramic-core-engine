# 308 — The generator, in C

## Current behavior

**Done.** The generator is a standalone C program —
`scripts/070-generate.c` and the three pieces under it — compiled by
the build before it is run. It depends on nothing the engine provides,
so the build compiles it, runs it, and compiles everything else.
**A program built with this engine needs a C compiler and nothing
else.**

**Parity was proven rather than asserted, and then retired.** Both
generators were run over the same sources and their output compared
byte for byte: 259 lines of emitted registry identical apart from the
first line, which names the generator that wrote it; describe mode
identical; and thirteen malformed sources each refused by both with
**the same message**. The comparison lived in
`tests/072-test-generator-parity.sh`, said in its own header that it
could not outlive the thing it compared against, and went with it —
the Lua generator was renamed to mark it deprecated for one commit and
removed in the next, so it appears in the record once as something
deliberately put down.

**Two behaviours changed, both deliberately, both improvements:**

- **The exit code.** The Lua generator exited 1 for everything. The C
  one uses the project's own codes (issue 106): 65 for a malformed
  source, 71 for a resource it cannot get. The command-line test only
  ever checked for non-zero, so nothing depended on the old value.
- **An unterminated block comment now names its file.** The Lua
  version reported `(input):0` for that one case, because the routine
  that blanks comments had not been told which file it was reading.

**What the translation actually cost**, since the issue predicted it:
the parsing is the small half. The support machinery underneath — an
arena, a string that grows, an array that grows, and the handful of
string operations Lua supplies for free — is `065-gentext.h` and
`066-gentext.c`, and it has its own test, because everything above it
assumes it is correct and a bug in it surfaces as garbled C hundreds
of lines away.

**One piece of the parser is worth knowing about.** A function header
is recognized by scanning **backwards** from the last `)` to its
match, rather than by a pattern. The name may follow a star with no
space — `char *sneaky` — which no single split can express, and the
Lua version had already resorted to splitting it by hand for the same
reason.

**Out of scope and still true: the HTML documentation generator stays
in Lua.** It is project tooling, not on the path a consumer walks, and
the distinction is now stated in the Makefile's own header, in the
packaging note, and in the front door's info file: building the engine
needs a C compiler; regenerating our documentation needs LuaJIT.

**Why it mattered**, kept because it is the reason the work was done:
a consumer's build inherits our tooling choices, and every inherited
dependency is a reason their build fails on a machine we never saw.
Depending on an interpreter that nothing at run time ever used was
acceptable for a project that only builds itself and not for anything
handed to other people —
[057](../../docs/implementation-notes/057-packaging.md) is where that
was first measured.
## Intended behavior

**A program built with this engine needs a C compiler and nothing else.**

The generator becomes a small, standalone C program with no dependency
on the engine it feeds — it reads C source as text and writes C source
as text, so it can be compiled by the same compiler that will compile
its output, before anything else in the build exists.

There is no bootstrap problem to solve, because the generator does not
depend on anything it generates. The build gains one stage: compile the
generator, run it, compile everything else. A consumer's build does the
same, with their own box sources.

**Behavior is unchanged.** This is a translation, not a redesign. The
emitted file should be equivalent to what the Lua generator emits from
the same input, the describe mode should report the same findings, and
a malformed source should still stop the build naming the file — a
generator that writes nothing on failure is the guarantee that keeps a
build from compiling against a stale registry, and it survives intact.

**What the translation costs.** Lua supplied growable strings, hash
lookup, and pattern matching; C supplies none of these. The honest
majority of this work is not parsing — it is the few hundred lines of
support underneath it: a growable output buffer, an arena so nothing
has to be freed individually, and a string scanner. Writing those
deliberately, as their own readable pieces, is the difference between a
translation and a thicket.

**Out of scope: the documentation generator.** The HTML site generator
stays in Lua. It is project tooling, not on the path a consumer walks to
build a program, and a documentation build that needs LuaJIT costs an
outside consumer nothing. This issue is about the build path only, and
the distinction should be stated wherever the dependency is declared:
building the engine needs a C compiler; regenerating our documentation
needs LuaJIT.

## Suggested implementation steps

1. **Support machinery first, as its own readable unit.** The growable
   byte buffer, the arena, and the string scanner. Each with a test,
   because everything above them assumes they are correct and a bug
   here surfaces as garbled C hundreds of lines away.
2. **The declaration parser**, matching what the existing parser
   accepts — return type, name, parameters with types, declarations
   spanning several lines, braces inside string literals, helper
   functions that are not boxes. The existing generator's test suite
   already names these cases; they are the specification.
3. **The describe mode**, before any emission. It prints what the parser
   found, which makes every later step debuggable by reading the
   parser's findings rather than the emitted C.
4. **Emission**, in the order the existing generator emits: comparison
   functions, shims, box records, struct field tables. Every size and
   offset stays a `sizeof` or `offsetof` expression — the compiler
   computes them, and the generator must continue never to guess.
5. **Parity, held by a test.** Run both generators over the same box
   sources and compare their output. This test is deliberately
   temporary: it cannot outlive the Lua generator it compares against.
6. **Build integration.** The generator compiles into the RAM-backed
   build tier like everything else, and its output is rebuilt when a box
   source, or the generator's own source, is newer.
7. **Retire the Lua generator** once parity holds, along with the parity
   test. Per the project's convention it is renamed to mark it deprecated
   for one commit before removal, so it appears in the record once as a
   thing that was deliberately retired rather than vanishing.
8. **Declare the dependency where a consumer will read it** — building
   needs a C compiler; nothing else.

## Related

- [301 — Box source parser](301-box-source-parser.md), whose
  accepted grammar this must match exactly
- [302 — Shim emission](302-shim-emission.md),
  [303 — Registry emission](303-registry-emission.md),
  [304 — Struct field tables](304-struct-field-tables.md),
  [305 — Compare functions](305-compare-functions.md) — the
  four kinds of output, unchanged
- [306 — Build integration](306-build-integration.md), which
  gains a compile stage ahead of the generate stage
- [007 — The build path](../../docs/007-datapath-build.md), the document
  this must keep true
- [057 — Packaging](../../docs/implementation-notes/057-packaging.md),
  where the dependency was named as an obstacle
