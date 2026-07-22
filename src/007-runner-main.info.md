# 007-runner-main.lua — Interpreter runner entry point

Command-line program that loads one map and executes it
synchronously, start to finish, in a single process. This is the
**interpreter** path — the compiler (issue 219) generates
standalone output that replaces it for deployment.

Not a `require`-able module; it runs immediately.

## Invocation

    luajit src/007-runner-main.lua <map-dir>

One required argument: the map directory. Missing argument prints
usage and exits 1.

## What a run does, in order

1. **Repair the scratch space.** The map's `tmp/` is a symlink into
   the system temp tier (created by the map-creation script). If the
   machine rebooted since, the symlink's target is gone — the runner
   re-creates the target directory and ensures `tmp/logs/` exists
   before anything tries to write there.
2. **Load** the map through the loader module (`003-loader`). Load
   errors are listed one per line on stderr; exit 1.
3. **Validate** the loaded graph through the same module.
   Validation errors are listed the same way; exit 1.
4. **Execute** via the synchronous executor (`004-executor`), which
   drives boxes in dependency order. First failure halts the run;
   the error prints to stderr; exit 1.

On success the runner prints one OK line naming the result file and
exits 0.

## Outputs

- `<map-dir>/tmp/last-run.json` — per-box inputs, outputs, status,
  and errors, written by the executor.
- `<map-dir>/tmp/logs/` — per-box log files.

## Related

- `src/003-loader.lua` — map loading and graph validation.
- `src/004-executor.info.md` — the execution model and result shape.
- Issue 219 — the compiler that supersedes this path for deployment.
