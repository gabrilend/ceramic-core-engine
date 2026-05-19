# 238 — Editor HTTP server config file (default bundled-script locations)

## Status
open

## Current behavior

The editor's HTTP server (`src/005-http-server.lua`, issue 105) is
launched with map-relative paths discovered at startup time. The
file browser (issue 211, 211a) sees the map's own `src/` directory
plus the unified pipeline dirs the runner sets up, but the set of
"bundled / default" library locations the editor offers is
hard-coded in the server / file-browser logic.

If a user wants to drop in their own personal library of bundled
scripts (e.g. their own `text-extras.lua`, their own
`my-team-helpers/` directory) and have the editor's file browser
treat them as first-class default refs, there is no per-deployment
way to point the server at additional directories. They would have
to edit the server source.

There is also no record of the editor's runtime configuration —
ports, default map dir, asset dir, the bundled lib dirs — in a
config file the user can read or version. The defaults live in
code.

## Intended behavior

Introduce a config file the editor's HTTP server reads at startup
to learn:

- **Server port** (default `7474` or whichever current default).
- **Map root directory** (default `maps/`).
- **Bundled script locations** — a list of directory paths the
  file browser exposes as default-available refs, distinct from
  the current map's own `src/`. Default list includes `libs/`.
- **Asset directory** for the editor (default `assets/`).
- **Log / tmp directory** if relevant (per the project's tmp
  symlink convention).

Each entry has a sensible default; missing entries fall back to
those defaults. The user can override any subset by editing the
file.

### File location and format

Path: `config/editor-server.lua` (returns a Lua table) or
`config/editor-server.json`. Lean toward the Lua table — every
other config in this project is Lua, and a Lua return-table lets
the user compute paths if needed (e.g. `os.getenv("HOME") .. "/my-libs"`).

Shape:

```lua
return {
  port = 7474,
  map_root = "maps",
  asset_dir = "assets",
  bundled_script_dirs = {
    "libs",           -- the in-tree default
    -- "~/.soramech/my-libs",  -- example user override
  },
  tmp_dir = "tmp",
}
```

A startup helper in `src/006-server-main.lua` loads the config,
fills in defaults, and passes the resolved table down to the HTTP
server module. The server prints the resolved config on boot so
the user can confirm what's in effect.

### File browser surfacing

The file browser (issue 211 + 211a) currently lists the map's own
`src/` and the pipeline-resolved bundled dirs. With this config in
place, the bundled list comes from `bundled_script_dirs` directly.
A second pane (or expandable section per dir) labels each entry by
its source directory so the user can tell their own bundle apart
from the in-tree `libs/`.

### Why a file, not CLI flags

Two reasons:
1. Reproducibility — the config file checks into a personal dotfile
   or repo, and the editor starts up the same way every time.
2. Discoverability — a user reading their `config/` directory sees
   what's customizable. CLI flags are invisible until the user runs
   `--help`.

CLI flags can layer on top if needed (a `--config-file` override),
but the file is the primary mechanism.

### Defaults vs explicit

The server still works with no config file present — every field
has a default and the file is optional. Creating the file is a
"customize me" gesture, not a setup requirement.

## Open questions

- **Per-map config overrides**: a map could ship its own
  `config/editor-server.lua` that adds its own bundled dirs. Useful?
  Probably yes long-term, but defer until a real use case appears.
  The global config is enough for now.
- **Path resolution**: bundled dirs as absolute paths, project-
  relative, or both? Both. If the path starts with `/` or `~`,
  treat as absolute (after `~` expansion); otherwise resolve
  against the project root. Document this in a comment at the top
  of the config file.
- **Hot reload**: changes to the config file requiring a server
  restart vs picked up on next request? Restart is simpler and
  matches the existing model. Hot reload is a separate issue if
  ever wanted.

## Suggested implementation steps

1. New file `config/editor-server.lua` (or a documented sample
   under `config/editor-server.example.lua`) with the schema above
   and the defaults filled in.
2. `src/006-server-main.lua` — `load_server_config(path)`:
   - `pcall(dofile, path)` — fall back to empty table if missing.
   - Merge over a defaults table.
   - `~` expansion on string paths.
   - Print resolved config on stdout at boot.
3. `src/005-http-server.lua` — accept the resolved config table as
   its constructor argument. Use `bundled_script_dirs` in the
   `handle_list_src` family so the file browser sees them.
4. `assets/js/007-filebrowser.js` — render bundled dirs as a
   labeled second pane, separate from the current map's `src/`.
5. Document the file in `docs/`: a new short section in
   `docs/005-language-specs.md` (or its own page) describing the
   config schema and the path resolution rules.

## Relevant files

- `src/005-http-server.lua` — server module to parameterize
- `src/006-server-main.lua` — boot point that loads config
- `assets/js/007-filebrowser.js` — UI surface for bundled dirs
- `issues/completed/105-http-server-file-crud.md` — original server
- `issues/completed/211-file-browser-library-dirs.md` — current
  library-dir discovery this issue parameterizes
- `issues/completed/211a-unify-src-dirs-pipeline.md` — pipeline
  unification that influences how bundled paths get resolved
- `issues/237-string-manipulation-library-expansion.md` — paired
  issue; this config file makes "what counts as bundled" pluggable
- `docs/000-table-of-contents.md` — new doc page to add when the
  config schema doc is written
