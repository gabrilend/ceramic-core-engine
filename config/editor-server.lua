-- Editor server config (issue 238).
--
-- Returns a Lua table read at startup by src/006-server-main.lua.
-- Every field is optional; missing fields fall back to the defaults
-- baked into the loader. Editing this file customizes the editor
-- without touching code or the start-server script. The server
-- prints the resolved settings on boot so you can confirm what's in
-- effect.
--
-- Path resolution
--   /abs/path  → used as-is
--   ~/under-home → $HOME expansion
--   relative   → resolved against the project root (the `DIR` constant
--                in src/006-server-main.lua)

return {
  -- TCP port the HTTP server binds to.
  port = 7700,

  -- Directory holding map subdirectories. The editor lists subdirs
  -- of this path in its map picker.
  map_root = "maps",

  -- Bundled script directories — every map sees these in the file
  -- browser without per-map setup. They're merged in alongside the
  -- map's own meta.json src_dirs at runtime, not seeded into each
  -- new map's meta.json. Add your personal library directory here
  -- (absolute path or ~ shorthand) to make it available everywhere.
  -- A map can hide one of these for itself by adding its path to
  -- the meta.json `hidden_bundled_dirs` list (the editor writes
  -- this when you right-click "hide directory" on a bundled entry).
  bundled_script_dirs = {
    "libs",
  },
}
