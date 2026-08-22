# 100-mapparse.c — the parser, from inside

Interface in `099-mapparse.h.info.md`. **Part of the compiler, not of
the engine** (issue 311d): a running program does not read
descriptions, it hands them to the generator, so nothing links this
into anything anybody runs. Reads text into a description
and constructs nothing — whether the description makes sense is
answered by the calls it becomes; whether it is well-formed dies here
with file and line.
The keyword dispatch: `in` and `out` attach to the open station,
`statics` opens the numbered section (values kept as raw text — the
type that shapes them arrives at binding), and anything else must be
a three-word station line with kind p, c, or i. The kind is written
rather than inferred so a forgotten threshold is an error instead of
a silent demotion. Duplicate names, duplicate entries, trailing
words, and dangling `in`/`out` lines are all refused by name.
