-- SoraMech HTTP server entry point.
-- Thin file CRUD proxy for the web editor. No runner management.
-- Run: luajit src/006-server-main.lua [maps-root] [port]
-- Both args optional; defaults come from config/editor-server.lua.

local DIR = "/mnt/mtwo/programs/sora/soramech"

package.path  = DIR .. "/libs/?.lua;"
             .. DIR .. "/libs/luasocket/share/lua/5.1/?.lua;"
             .. DIR .. "/src/?.lua;"
             .. package.path
package.cpath = DIR .. "/libs/luasocket/lib/lua/5.1/?.so;"
             .. package.cpath

local server_mod = require("005-http-server")

-- {{{ DEFAULT_CONFIG_TEXT
-- The full text of the default config file. Used both as the runtime
-- field-level fallback (any missing key takes its value from here)
-- AND as the seed written to disk when the file is absent. Keeping
-- the template here means the on-disk file and the runtime defaults
-- can never drift — they're the same source.
local DEFAULT_CONFIG_TEXT = [[
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
]]
-- }}}

-- {{{ resolve_path
-- Three path shapes the config can carry (issue 238):
--   absolute (/x) → returned as-is
--   home-relative (~/x) → $HOME expansion
--   project-relative (x) → prefixed with DIR so users can refer to
--                          in-tree dirs by name without writing the
--                          absolute mount-point
local function resolve_path(p, project_root)
    if type(p) ~= "string" or p == "" then return nil end
    if p:sub(1, 1) == "/" then return p end
    if p:sub(1, 2) == "~/" then
        local home = os.getenv("HOME") or ""
        return home .. p:sub(2)
    end
    return project_root .. "/" .. p
end
-- }}}

-- {{{ file_exists
local function file_exists(path)
    local f = io.open(path, "r")
    if not f then return false end
    f:close()
    return true
end
-- }}}

-- {{{ generate_default_config
-- Writes DEFAULT_CONFIG_TEXT to `path`, creating the parent dir if
-- needed. Called from load_server_config when the file is missing
-- so first-time users start from an editable template rather than
-- a "where do I customize this?" mystery. The message prints to
-- stdout so the action is visible in the boot log.
local function generate_default_config(path)
    print("generating config file at " .. path .. " ...")
    local parent = path:match("^(.*)/[^/]+$")
    if parent then os.execute("mkdir -p '" .. parent .. "'") end
    local f, err = io.open(path, "w")
    if not f then
        io.stderr:write("could not write config file: " .. tostring(err) .. "\n")
        os.exit(1)
    end
    f:write(DEFAULT_CONFIG_TEXT)
    f:close()
end
-- }}}

-- {{{ load_server_config
-- Two paths through this function (issue 238):
--   1. Config file present → dofile it; any missing field falls back
--      to the value embedded in DEFAULT_CONFIG_TEXT (parsed once at
--      startup) so partial configs keep working.
--   2. Config file absent → generate the default template at the
--      target path, then load the freshly-written file. The
--      hard-wired defaults are NOT a separate code path — they live
--      in DEFAULT_CONFIG_TEXT and are read back from disk after
--      generation, so the on-disk file is always the source of
--      truth on the next boot.
-- All paths are resolved here so downstream code sees absolute
-- paths only.
local function load_server_config(path, project_root)
    if not file_exists(path) then
        generate_default_config(path)
    end

    local ok, user = pcall(dofile, path)
    if not ok or type(user) ~= "table" then
        io.stderr:write("config file at " .. path .. " did not return a table: "
            .. tostring(user) .. "\n")
        os.exit(1)
    end

    -- Parse the embedded template separately so we can use it for
    -- field-level fallbacks. dostring via load() — runs in-memory,
    -- no second disk read.
    local defaults_fn = load(DEFAULT_CONFIG_TEXT, "default-config", "t")
    local defaults    = defaults_fn and defaults_fn() or {}
    for k, v in pairs(defaults) do
        if user[k] == nil then user[k] = v end
    end

    user.map_root = resolve_path(user.map_root, project_root)
    local resolved_dirs = {}
    for _, d in ipairs(user.bundled_script_dirs or {}) do
        local r = resolve_path(d, project_root)
        if r then resolved_dirs[#resolved_dirs + 1] = r end
    end
    user.bundled_script_dirs = resolved_dirs
    return user
end
-- }}}

-- {{{ main
local function main(args)
    local config = load_server_config(DIR .. "/config/editor-server.lua", DIR)

    -- CLI args override config so the existing start-server.sh script
    -- (which passes positional maps-root + port) keeps working.
    local maps_root = args[1] or config.map_root
    local port      = tonumber(args[2]) or config.port

    if not maps_root then
        io.stderr:write("usage: luajit src/006-server-main.lua [maps-root] [port]\n")
        os.exit(1)
    end

    -- resolve to absolute path
    local handle = io.popen("realpath " .. maps_root)
    maps_root = handle:read("*l")
    handle:close()

    print("soramech-server config:")
    print("  port:                " .. port)
    print("  map_root:            " .. maps_root)
    print("  bundled_script_dirs:")
    for _, d in ipairs(config.bundled_script_dirs) do
        print("    - " .. d)
    end

    server_mod.serve({
        maps_root    = maps_root,
        port         = port,
        bundled_dirs = config.bundled_script_dirs,
    })
end
-- }}}

main(arg)
