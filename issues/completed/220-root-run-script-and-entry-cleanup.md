# 220 — Root run script and entry point cleanup

## Status
completed

## Current behavior
The project root contains `soramech-runner.lua` and `soramech-server.lua` —
Lua entry points that users are not expected to invoke directly. The intended
invocation path is through bash scripts in `scripts/`. There is no single
"start working" command at the root.

## Intended behavior
The project root contains exactly one user-facing script: `run`. Running it
starts the HTTP server and opens the editor in a browser — the complete
development environment in one command.

The Lua entry points (`soramech-runner.lua`, `soramech-server.lua`) move to
`src/` since they are part of the source, not user-facing commands. The
`scripts/start-server.sh` and `scripts/open-editor.sh` update their paths
accordingly. The root `run` script calls `scripts/start-server.sh` (background)
then `scripts/open-editor.sh`.

## Suggested implementation steps
1. Move `soramech-server.lua` → `src/006-server-main.lua`; update
   `scripts/start-server.sh` to reference the new path.
2. Move `soramech-runner.lua` → `src/007-runner-main.lua`; note that the
   compiled run script (issue 219) replaces this for user invocation.
3. Write root `run` script: starts the server in the background, waits a
   moment for it to bind, then opens the editor.
4. Delete `soramech-runner.lua` and `soramech-server.lua` from root.

## Relevant files
- `soramech-server.lua` → `src/006-server-main.lua`
- `soramech-runner.lua` → `src/007-runner-main.lua`
- `scripts/start-server.sh` — references server entry point
- `scripts/open-editor.sh` — editor open command
