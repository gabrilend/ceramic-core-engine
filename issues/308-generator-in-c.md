# 308 — The generator, in C

## Current behavior

The generator is a Lua script run by LuaJIT. It reads the box sources,
parses their function declarations, and emits one C file holding a shim
per box, a record per box, a field table per struct, and a three-way
comparison per comparable type. The build runs it before compiling
anything, and rebuilds its output whenever a box source or the generator
itself is newer.

This works, and it is the reason a map file can say a box's name in text
and reach a compiled C function. It also means **anyone who builds a
program with this engine must have LuaJIT installed**, even though
nothing Lua runs once the program is running. The engine is C; the road
to compiling it is not.

That is acceptable for a project that only builds itself. It is not
acceptable for something handed to other people, which is what
[057](../docs/implementation-notes/057-packaging.md) describes: a
consumer's build inherits our tooling choices, and every inherited
dependency is a reason their build fails on a machine we never saw.

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

- [301 — Box source parser](completed/301-box-source-parser.md), whose
  accepted grammar this must match exactly
- [302 — Shim emission](completed/302-shim-emission.md),
  [303 — Registry emission](completed/303-registry-emission.md),
  [304 — Struct field tables](completed/304-struct-field-tables.md),
  [305 — Compare functions](completed/305-compare-functions.md) — the
  four kinds of output, unchanged
- [306 — Build integration](completed/306-build-integration.md), which
  gains a compile stage ahead of the generate stage
- [007 — The build path](../docs/007-datapath-build.md), the document
  this must keep true
- [057 — Packaging](../docs/implementation-notes/057-packaging.md),
  where the dependency was named as an obstacle
