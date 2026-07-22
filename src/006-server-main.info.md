# 006-server-main.lua — Editor server entry point

Boot program for the SoraMech HTTP server: a thin file-CRUD proxy
that the web editor talks to. It owns no server logic itself — it
loads (or generates) the config file, resolves every path to an
absolute form, and hands the resolved settings to the HTTP server
module (`005-http-server`), whose `serve` call never returns. No
runner management happens here.

Not a `require`-able module; it runs immediately.

## Invocation

    luajit src/006-server-main.lua [maps-root] [port]

Both arguments are optional and, when present, **override** the
config file — the positional form exists so `scripts/start-server.sh`
keeps working unchanged. The maps-root argument is resolved to an
absolute path before use. On boot the program prints the resolved
port, map root, and bundled script directories so what's in effect
is visible in the log.

## Config file — config/editor-server.lua

A Lua file returning a table (issue 238). Every field is optional.

- `port` — TCP port the server binds to. Default 7700.
- `map_root` — directory whose subdirectories appear in the
  editor's map picker. Default `maps`.
- `bundled_script_dirs` — directories every map sees in its file
  browser without per-map setup, merged alongside the map's own
  declared source directories at runtime. Default `{ "libs" }`. A
  map opts out of one entry by listing its path under
  `hidden_bundled_dirs` in its meta.json (the editor writes this
  on right-click "hide directory").

Paths in the config come in three shapes: absolute (used as-is),
home-relative (`~/` expanded against `$HOME`), and everything else
(resolved against the project root).

## Missing or partial config

If the file is absent, the program writes a fully-commented default
template to the expected path and then loads that freshly-written
file — first-time users start from an editable template instead of
a mystery. If the file is present but incomplete, each missing
field takes its value from the same embedded template. Because the
template text is the single source for both the seed file and the
runtime fallbacks, the on-disk file and the defaults can never
drift apart. A config file that fails to load or doesn't return a
table is a hard exit with a message, not a silent fallback.

## Related

- `src/005-http-server.lua` — the routing and CRUD logic this boots.
- Issue 238 — config file design.
- `scripts/start-server.sh` — the usual way to launch this.
