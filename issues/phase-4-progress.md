# Phase 4 progress — the pull path and configuration

Phase 4's goal: the two slot kinds that are not buffers. Statics —
values that are simply always there — and gatherers, the one place
the engine runs backwards so a value can be fresh at the moment it
is used.

| Issue | State | In one line |
|---|---|---|
| 401 — static slots | complete | Always full, never consumed; bytes shaped by the first binder's type. |
| 402 — struct constants | complete | One reader walks field tables and brace text; malformed is fatal at bind. |
| 403 — gatherer slots | in progress | |
| 404 — chains and cycles | in progress | |
| 405 — statics mutation | in progress | |
| 406 — phase 4 demo | not started | |

Notes for the phase: statics and gathering landed as two modules
(033, 034) joined to task construction through two claim calls,
keeping user code and the statics lock forever outside any station's
mutex. The statics text-versus-bytes tension is the phase's main
finding for the report.
