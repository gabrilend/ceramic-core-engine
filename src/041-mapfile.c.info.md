# 041-mapfile.c — the parser, from inside

Interface in `040-mapfile.h.info.md`. Reads text into a description
and constructs nothing — whether the map makes sense is the loader's
question; whether it is well-formed dies here with file and line.
The keyword dispatch: `in` and `out` attach to the open station,
`statics` opens the numbered section (values kept as raw text — the
type that shapes them arrives at binding), and anything else must be
a three-word station line with kind p, c, or i. The kind is written
rather than inferred so a forgotten threshold is an error instead of
a silent demotion. Duplicate names, duplicate entries, trailing
words, and dangling `in`/`out` lines are all refused by name.
