# Phase 3 progress — the build path

Phase 3's goal: the generator, and the end of hand-written glue.
Writing a box becomes writing a function; everything the engine needs
to call it by name is derived from the C itself.

| Issue | State | In one line |
|---|---|---|
| 301 — box-source parser | complete | Lua script; three shapes recognized, everything else stops the build with file:line. |
| 302 — shim emission | in progress | |
| 303 — registry emission | in progress | |
| 304 — struct field tables | in progress | |
| 305 — compare functions | in progress | |
| 306 — build integration | in progress | |
| 307 — phase 3 demo | not started | |

Notes for the phase: parser and emitters are one script with distinct
stages (parse → validate → emit), sharing the pattern of the earlier
phases — one mechanism, proven aspect by aspect.
