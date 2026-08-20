# 068-genparse.c — reading a box source, from inside

Interface in `067-genparse.h.info.md`. Comments, string contents and
whole preprocessor lines are blanked to spaces first, keeping every
newline so byte offsets still map to line numbers — a brace inside a
comment or a string would otherwise throw off everything after it.
Strings are blanked before preprocessor lines, so a `#` inside text
cannot be mistaken for a directive.

Then one pass stops at every top-level `{` and classifies the
declaration that led to it, using only the text since the last `;`,
because complete declarations before this one end with a semicolon.
Three shapes are recognized and everything else stops the build.

A function header is found by scanning **backwards** from the last
`)` to its match, rather than by one pattern, because the name may
follow a star with no space — `char *sneaky` — which no single split
can express. The same backwards-scan finds the trailing identifier in
every other place a name has to be separated from what precedes it.

The refusals are the point. A parser that quietly skips what it does
not understand emits a registry with a hole in it, and the hole
surfaces much later as an error pointing at somebody's map.
