# 070-generate.c — the generator's front door

`generate <output.c> <box-source.c>...` writes the emitted C file;
`generate --describe <box-source.c>...` prints what the parser saw,
for diagnosing a build by looking at the findings rather than at the
emission.

It parses every source, validates once, and then either describes or
emits. That is the whole of it; the work is next door.

Two modes do something other than read boxes:

- `generate --map-boxes <map>` prints the box names a description
  references, one per line, unresolved. Whoever asked knows what they
  hold and this program does not.
- `generate --embed <output.c> <file>...` turns arbitrary files into a
  table of C string literals. This is stage two of the build that
  produces `cerac`, and the files it is pointed at are the engine's own
  header, body and export list (issue 910). It is a mode here rather
  than a program of its own because the escaping already lives in this
  build tool, and a second tool with a second copy of it would be a
  second definition of the same rule.

**Why it is C rather than a script.** A program built with this engine
should need a C compiler and nothing else. This was a LuaJIT script,
which meant every consumer inherited a build dependency on an
interpreter that nothing at run time ever used. The generator depends
on nothing the engine provides, so it compiles before anything else in
the build exists and there is no bootstrap problem to solve.

Regenerating this project's own HTML documentation still needs LuaJIT.
That is project tooling, not on the path a consumer walks to build a
program.
