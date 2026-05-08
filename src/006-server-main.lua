-- SoraMech HTTP server entry point.
-- Thin file CRUD proxy for the web editor. No runner management.
-- Run: luajit src/006-server-main.lua <maps-root> [port]

local DIR = "/mnt/mtwo/programs/sora/soramech"

package.path  = DIR .. "/libs/?.lua;"
             .. DIR .. "/libs/luasocket/share/lua/5.1/?.lua;"
             .. DIR .. "/src/?.lua;"
             .. package.path
package.cpath = DIR .. "/libs/luasocket/lib/lua/5.1/?.so;"
             .. package.cpath

local server_mod = require("005-http-server")

-- {{{ main
local function main(args)
    local maps_root = args[1]
    local port      = tonumber(args[2]) or 7700

    if not maps_root then
        io.stderr:write("usage: luajit src/006-server-main.lua <maps-root> [port]\n")
        os.exit(1)
    end

    -- resolve to absolute path
    local handle = io.popen("realpath " .. maps_root)
    maps_root = handle:read("*l")
    handle:close()

    server_mod.serve(maps_root, port)
end
-- }}}

main(arg)
