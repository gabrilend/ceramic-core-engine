# Phase 6 progress — the map file

Phase 6's goal: the capstone. A program becomes a directory of C
functions and a text file, and changing the shape of the program
never touches the C.

| Issue | State | In one line |
|---|---|---|
| 601 — map file parser | complete | Keyword dispatch, kind written not inferred, comments added to the format. |
| 602 — loader first pass | complete | Stations from the registry; statics bound; gathers deferred to pass two. |
| 603 — loader second pass | complete | Arrows resolved and type-checked by name; the message is the deliverable. |
| 604 — load-time validation | complete | Whole-map rules collected and printed together; warnings loud, not fatal. |
| 605 — seed sweep | in progress | |
| 606 — phase 6 demo | not started | |

Notes for the phase: parser and loader are separate files (read
versus build); the gather input line resolves in the second pass
rather than the first, because its source is a name and names may
point forward — a wrinkle in issue 602's split the report records.
