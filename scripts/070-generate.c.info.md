# 070-generate.c — the generator's front door

`generate <output.c> <box-source.c>...` writes the registry;
`generate --describe <box-source.c>...` prints what the parser saw,
for diagnosing a build by looking at the findings rather than at the
emission.

It parses every source, validates once, and then either describes or
emits. That is the whole of it; the work is next door.

**Why it is C rather than a script.** A program built with this engine
should need a C compiler and nothing else. This was a LuaJIT script,
which meant every consumer inherited a build dependency on an
interpreter that nothing at run time ever used. The generator depends
on nothing the engine provides, so it compiles before anything else in
the build exists and there is no bootstrap problem to solve.

Regenerating this project's own HTML documentation still needs LuaJIT.
That is project tooling, not on the path a consumer walks to build a
program.
