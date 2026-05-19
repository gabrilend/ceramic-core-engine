-- Minimal HTTP server for the SoraMech web editor.
-- Handles file CRUD for map directories. Validates writes against the
-- schema before touching disk. Closes after each response (HTTP/1.0 style).
-- Started with: luajit soramech-server.lua <maps-root> [port]

local DIR = "/mnt/mtwo/programs/sora/soramech"

package.path  = DIR .. "/libs/?.lua;"
             .. DIR .. "/libs/luasocket/share/lua/5.1/?.lua;"
             .. DIR .. "/src/?.lua;"
             .. package.path
package.cpath = DIR .. "/libs/luasocket/lib/lua/5.1/?.so;"
             .. package.cpath

local socket = require("socket")
local json   = require("dkjson")
local schema = require("001-schema")

local M = {}

-- Bundled script dirs come from config/editor-server.lua (issue 238).
-- Set at serve() time; read by the file-browser handlers so every map
-- sees the same default toolbox without per-map setup. Module-level
-- state keeps the dispatch-handler signatures unchanged.
local bundled_dirs = {}

-- {{{ read_file
local function read_file(path)
    local f, err = io.open(path, "r")
    if not f then return nil, err end
    local data = f:read("*a")
    f:close()
    return data
end
-- }}}

-- {{{ write_file_atomic
local function write_file_atomic(path, data)
    local tmp = path .. ".tmp"
    local f, err = io.open(tmp, "w")
    if not f then return false, err end
    f:write(data)
    f:close()
    local ok, rerr = os.rename(tmp, path)
    if not ok then return false, rerr end
    return true
end
-- }}}

-- {{{ parse_json_file
local function parse_json_file(path)
    local raw, err = read_file(path)
    if not raw then return nil, err end
    local obj, _, jerr = json.decode(raw)
    if not obj then return nil, "JSON error: " .. tostring(jerr) end
    return obj
end
-- }}}

-- {{{ list_dir_json
local function list_dir_json(dir)
    local handle = io.popen("ls " .. dir .. "/*.json 2>/dev/null")
    if not handle then return {} end
    local listing = handle:read("*a")
    handle:close()
    local files = {}
    for path in listing:gmatch("[^\n]+") do
        files[#files + 1] = path
    end
    return files
end
-- }}}

-- {{{ list_subdirs
local function list_subdirs(dir)
    local handle = io.popen("ls -d " .. dir .. "/*/ 2>/dev/null")
    if not handle then return {} end
    local listing = handle:read("*a")
    handle:close()
    local names = {}
    for path in listing:gmatch("[^\n]+") do
        local name = path:match("/([^/]+)/$")
        if name then names[#names + 1] = name end
    end
    return names
end
-- }}}

-- {{{ safe_path
-- Returns the resolved path if it is within root, nil otherwise.
-- Prevents path traversal outside the maps root.
local function safe_path(root, ...)
    local parts = { root }
    for _, p in ipairs({...}) do
        -- reject any component containing ".."
        if tostring(p):find("%.%.", 1, true) then return nil end
        parts[#parts + 1] = p
    end
    return table.concat(parts, "/")
end
-- }}}

-- {{{ find_references
-- Scan all box files in a map for any connection referencing the given box id.
local function find_references(map_dir, box_id)
    local refs = {}
    local files = list_dir_json(map_dir .. "/boxes")
    for _, path in ipairs(files) do
        local box, _ = parse_json_file(path)
        if box and box.id ~= box_id then
            for _, c in ipairs(box.connections or {}) do
                if c.to_box == box_id or c.from_box == box_id then
                    refs[#refs + 1] = box.id
                    break
                end
            end
        end
    end
    return refs
end
-- }}}

-- {{{ respond
local function respond(client, status, body, content_type)
    content_type = content_type or "application/json"
    local response = table.concat({
        "HTTP/1.0 " .. status,
        "Content-Type: " .. content_type,
        "Content-Length: " .. #body,
        "Access-Control-Allow-Origin: *",
        "Access-Control-Allow-Methods: GET, PUT, DELETE, OPTIONS",
        "Access-Control-Allow-Headers: Content-Type",
        "Connection: close",
        "",
        body,
    }, "\r\n")
    client:send(response)
end
-- }}}

-- {{{ json_ok
local function json_ok(client, data)
    respond(client, "200 OK", json.encode(data))
end
-- }}}

-- {{{ json_err
local function json_err(client, status, msg)
    respond(client, status, json.encode({ error = msg }))
end
-- }}}

-- {{{ parse_query
-- Decodes a URL query string into a key→value table.
local function parse_query(query_str)
    local params = {}
    for k, v in (query_str or ""):gmatch("([^=&]+)=([^&]*)") do
        v = v:gsub("+", " "):gsub("%%(%x%x)", function(h)
            return string.char(tonumber(h, 16))
        end)
        params[k] = v
    end
    return params
end
-- }}}

-- {{{ parse_request
local function parse_request(client)
    local line, err = client:receive("*l")
    if not line then return nil, err end

    local method, raw_path, _ = line:match("^(%u+) ([^ ]+) HTTP")
    if not method then return nil, "bad request line: " .. line end

    -- split query string from path before building parts
    local path       = raw_path:match("^([^?]*)") or raw_path
    local query_str  = raw_path:match("^[^?]*%?(.*)") or ""

    -- read headers
    local headers = {}
    local content_length = 0
    while true do
        local hline = client:receive("*l")
        if not hline or hline == "" then break end
        local k, v = hline:match("^([^:]+):%s*(.+)")
        if k then
            headers[k:lower()] = v
            if k:lower() == "content-length" then
                content_length = tonumber(v) or 0
            end
        end
    end

    -- read body if present
    local body = ""
    if content_length > 0 then
        body = client:receive(content_length) or ""
    end

    -- split clean path into segments
    local parts = {}
    for seg in path:gmatch("[^/]+") do parts[#parts + 1] = seg end

    return { method = method, path = path, query = parse_query(query_str),
             parts = parts, body = body }
end
-- }}}

-- {{{ handle_options
local function handle_options(client)
    respond(client, "200 OK", "", "text/plain")
end
-- }}}

-- ============================================================
-- Route handlers
-- ============================================================

-- {{{ handle_list_maps
local function handle_list_maps(client, req, maps_root)
    local names = list_subdirs(maps_root)
    json_ok(client, names)
end
-- }}}

-- {{{ handle_list_boxes
local function handle_list_boxes(client, req, maps_root)
    local map_name = req.parts[2]
    local boxes_dir = safe_path(maps_root, map_name, "boxes")
    if not boxes_dir then return json_err(client, "400 Bad Request", "invalid path") end

    local files = list_dir_json(boxes_dir)
    local ids = {}
    for _, f in ipairs(files) do
        local id = f:match("/([^/]+)%.json$")
        if id then ids[#ids + 1] = id end
    end
    json_ok(client, ids)
end
-- }}}

-- {{{ handle_get_box
local function handle_get_box(client, req, maps_root)
    local map_name = req.parts[2]
    local box_id   = req.parts[4]
    local path = safe_path(maps_root, map_name, "boxes", box_id .. ".json")
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    local raw, err = read_file(path)
    if not raw then return json_err(client, "404 Not Found", "box not found: " .. tostring(err)) end
    respond(client, "200 OK", raw)
end
-- }}}

-- {{{ handle_put_box
local function handle_put_box(client, req, maps_root)
    local map_name = req.parts[2]
    local box_id   = req.parts[4]
    local path = safe_path(maps_root, map_name, "boxes", box_id .. ".json")
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    local box, _, jerr = json.decode(req.body)
    if not box then
        return json_err(client, "400 Bad Request", "invalid JSON: " .. tostring(jerr))
    end

    local errs = schema.validate_box(box)
    if #errs > 0 then
        return json_err(client, "400 Bad Request", "schema error: " .. table.concat(errs, "; "))
    end

    local ok, werr = write_file_atomic(path, req.body)
    if not ok then return json_err(client, "500 Internal Server Error", tostring(werr)) end
    json_ok(client, { ok = true })
end
-- }}}

-- {{{ handle_delete_box
local function handle_delete_box(client, req, maps_root)
    local map_name = req.parts[2]
    local box_id   = req.parts[4]
    local map_dir  = safe_path(maps_root, map_name)
    local path     = safe_path(maps_root, map_name, "boxes", box_id .. ".json")
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    -- check for references in other boxes before deleting
    local refs = find_references(map_dir, box_id)
    if #refs > 0 then
        return json_err(client, "409 Conflict",
            "box '" .. box_id .. "' is referenced by: " .. table.concat(refs, ", "))
    end

    local ok = os.remove(path)
    if not ok then return json_err(client, "404 Not Found", "box not found") end
    json_ok(client, { ok = true })
end
-- }}}

-- {{{ handle_list_src
local function handle_list_src(client, req, maps_root)
    local map_name = req.parts[2]
    local src_dir  = safe_path(maps_root, map_name, "src")
    if not src_dir then return json_err(client, "400 Bad Request", "invalid path") end

    local handle = io.popen("ls " .. src_dir .. "/ 2>/dev/null")
    if not handle then return json_ok(client, {}) end
    local listing = handle:read("*a")
    handle:close()
    local files = {}
    for name in listing:gmatch("[^\n]+") do
        if name ~= "" then files[#files + 1] = name end
    end
    json_ok(client, files)
end
-- }}}

-- {{{ canonical_path
-- Strips symlink redirection so two paths that point at the same
-- directory compare equal even when written with different prefixes
-- (e.g. /home/ritz/... via a symlink to /mnt/mtwo/...). Used to
-- decide whether an entry in meta.src_dirs is the implicit map src/.
-- One shell-out per call; called at most ~N times per listing where
-- N is the number of dirs in the map's meta, which stays small.
local function canonical_path(p)
    if not p or p == "" then return p end
    local h = io.popen("realpath -m '" .. p .. "' 2>/dev/null")
    if not h then return p end
    local r = h:read("*l")
    h:close()
    return r or p
end
-- }}}

-- {{{ resolve_src_dirs
-- Returns the unified browseable-dir list for a map. Every entry is
-- {path, kind} with one of three kinds (issue 238):
--   default — the map's own src/ directory, always present
--   added   — a dir the user attached to this map via "+ library dir"
--   bundled — an editor-wide default from config/editor-server.lua
-- The split between default and added isn't about how they behave at
-- runtime (both live in meta.src_dirs) — it's so the file browser
-- can mark "ships with the map" entries differently from "user
-- opted in" entries. The implicit src/ path is recognized by
-- comparing canonical (realpath'd) forms so a symlinked map root
-- matches a /mtwo/-rooted server config.
-- Migration: if meta.json has no src_dirs, synthesizes from the
-- implicit src/ + the older extra_src_dirs field.
local function resolve_src_dirs(meta, maps_root, map_name)
    local out = {}
    local implicit_src      = maps_root .. "/" .. map_name .. "/src"
    local implicit_src_real = canonical_path(implicit_src)
    if meta.src_dirs then
        for _, d in ipairs(meta.src_dirs) do
            local kind = (canonical_path(d) == implicit_src_real) and "default" or "added"
            out[#out + 1] = { path = d, kind = kind }
        end
    else
        out[#out + 1] = { path = implicit_src, kind = "default" }
        for _, d in ipairs(meta.extra_src_dirs or {}) do
            out[#out + 1] = { path = d, kind = "added" }
        end
    end
    -- Per-map opt-out for bundled dirs: a map can list paths in
    -- meta.hidden_bundled_dirs to skip them. Same hide gesture the
    -- user does for map-owned dirs, surfaced through the editor's
    -- right-click menu so the three kinds feel identical to use.
    local hidden = {}
    for _, p in ipairs(meta.hidden_bundled_dirs or {}) do hidden[p] = true end
    for _, d in ipairs(bundled_dirs) do
        if not hidden[d] then
            out[#out + 1] = { path = d, kind = "bundled" }
        end
    end
    return out
end
-- }}}

-- {{{ handle_list_extrasrc
-- Returns [{index, label, path, files:[]}] for all src_dirs entries.
-- src/ is no longer special — it is simply the first entry in src_dirs.
local function handle_list_extrasrc(client, req, maps_root)
    local map_name = req.parts[2]
    local meta_path = safe_path(maps_root, map_name, "meta.json")
    if not meta_path then return json_err(client, "400 Bad Request", "invalid path") end

    local meta = {}
    local raw, _ = read_file(meta_path)
    if raw then
        local decoded, _, _ = json.decode(raw)
        if decoded then meta = decoded end
    end

    local src_dirs = resolve_src_dirs(meta, maps_root, map_name)
    local result = {}
    for i, entry in ipairs(src_dirs) do
        local handle = io.popen("ls " .. entry.path .. "/ 2>/dev/null")
        local files = {}
        if handle then
            local listing = handle:read("*a")
            handle:close()
            for name in listing:gmatch("[^\n]+") do
                if name ~= "" then files[#files + 1] = name end
            end
        end
        local label = entry.path:match("([^/]+)$") or entry.path
        result[#result + 1] = {
            index = i - 1, label = label,
            path = entry.path, kind = entry.kind, files = files,
        }
    end
    json_ok(client, result)
end
-- }}}

-- {{{ handle_get_extrasrc
-- Returns the raw text of a file from src_dirs[index].
local function handle_get_extrasrc(client, req, maps_root)
    local map_name  = req.parts[2]
    local dir_index = tonumber(req.parts[4])
    local file_name = req.parts[5]
    if not dir_index or not file_name then
        return json_err(client, "400 Bad Request", "missing dir index or filename")
    end
    if file_name:find("%.%.", 1, true) or file_name:find("/", 1, true) then
        return json_err(client, "400 Bad Request", "invalid filename")
    end

    local meta_path = safe_path(maps_root, map_name, "meta.json")
    if not meta_path then return json_err(client, "400 Bad Request", "invalid path") end

    local raw, _ = read_file(meta_path)
    if not raw then return json_err(client, "404 Not Found", "meta.json not found") end
    local meta, _, _ = json.decode(raw)
    if not meta then return json_err(client, "500 Internal Server Error", "bad meta.json") end

    local src_dirs = resolve_src_dirs(meta, maps_root, map_name)
    local entry = src_dirs[dir_index + 1]  -- lua 1-indexed
    if not entry then return json_err(client, "404 Not Found", "no src dir at index " .. dir_index) end

    local file_path = entry.path .. "/" .. file_name
    local content, ferr = read_file(file_path)
    if not content then return json_err(client, "404 Not Found", tostring(ferr)) end
    respond(client, "200 OK", content, "text/plain")
end
-- }}}

-- {{{ handle_get_src
local function handle_get_src(client, req, maps_root)
    local map_name  = req.parts[2]
    local file_name = req.parts[4]
    local path = safe_path(maps_root, map_name, "src", file_name)
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    local raw, err = read_file(path)
    if not raw then return json_err(client, "404 Not Found", tostring(err)) end
    respond(client, "200 OK", raw, "text/plain")
end
-- }}}

-- {{{ handle_get_data
local function handle_get_data(client, req, maps_root)
    local map_name  = req.parts[2]
    local file_name = req.parts[4]
    local path = safe_path(maps_root, map_name, "data", file_name .. ".json")
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    local raw, err = read_file(path)
    if not raw then return json_err(client, "404 Not Found", tostring(err)) end
    respond(client, "200 OK", raw)
end
-- }}}

-- {{{ handle_put_data
local function handle_put_data(client, req, maps_root)
    local map_name  = req.parts[2]
    local file_name = req.parts[4]
    local path = safe_path(maps_root, map_name, "data", file_name .. ".json")
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    -- validate it's at least valid JSON
    local _, _, jerr = json.decode(req.body)
    if jerr then
        return json_err(client, "400 Bad Request", "invalid JSON: " .. tostring(jerr))
    end

    local ok, werr = write_file_atomic(path, req.body)
    if not ok then return json_err(client, "500 Internal Server Error", tostring(werr)) end
    json_ok(client, { ok = true })
end
-- }}}

-- {{{ handle_get_simple
-- Handles GET for single-file resources: drivers.json, meta.json
local function handle_get_simple(client, req, maps_root, filename)
    local map_name = req.parts[2]
    local path = safe_path(maps_root, map_name, filename)
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    local raw, err = read_file(path)
    if not raw then return json_err(client, "404 Not Found", tostring(err)) end
    respond(client, "200 OK", raw)
end
-- }}}

-- {{{ handle_put_simple
local function handle_put_simple(client, req, maps_root, filename, validator)
    local map_name = req.parts[2]
    local path = safe_path(maps_root, map_name, filename)
    if not path then return json_err(client, "400 Bad Request", "invalid path") end

    local obj, _, jerr = json.decode(req.body)
    if not obj then
        return json_err(client, "400 Bad Request", "invalid JSON: " .. tostring(jerr))
    end

    if validator then
        local errs = validator(obj)
        if #errs > 0 then
            return json_err(client, "400 Bad Request", table.concat(errs, "; "))
        end
    end

    local ok, werr = write_file_atomic(path, req.body)
    if not ok then return json_err(client, "500 Internal Server Error", tostring(werr)) end
    json_ok(client, { ok = true })
end
-- }}}

-- {{{ handle_list_dirs
-- Lists immediate (non-hidden) subdirectories of an absolute path.
-- Query param: path — the absolute directory path to list.
local function handle_list_dirs(client, req, maps_root)
    local path = (req.query or {})["path"]
    if not path or path == "" then
        return json_err(client, "400 Bad Request", "missing 'path' query parameter")
    end
    if path:sub(1, 1) ~= "/" then
        return json_err(client, "400 Bad Request", "path must be absolute")
    end
    if path:find("%.%.", 1, true) then
        return json_err(client, "400 Bad Request", "path traversal not allowed")
    end
    -- normalize: strip trailing slashes; restore bare "/" for root
    path = path:gsub("/+$", "")
    if path == "" then path = "/" end

    -- shell-quote the path so spaces and special chars are safe
    local quoted = "'" .. path:gsub("'", "'\\''") .. "'"
    local handle = io.popen("ls -d " .. quoted .. "/*/ 2>/dev/null")
    local dirs = {}
    if handle then
        local listing = handle:read("*a")
        handle:close()
        for entry in listing:gmatch("[^\n]+") do
            -- extract the final directory name from the full path
            local name = entry:match("/([^/]+)/?$")
            if name then dirs[#dirs + 1] = name end
        end
    end

    -- Source files in the same directory (issue 225). The picker uses
    -- this to confirm "yes, this is the directory I meant" without the
    -- user having to commit and re-render the main file list. Filtered
    -- by the small set of language extensions phase 2 understands;
    -- phase 3 will replace the hard-coded list with whatever the
    -- language specs declare via their `file_ext` field.
    local files = {}
    local f_handle = io.popen(
        "find " .. quoted .. " -maxdepth 1 -type f " ..
        "\\( -name '*.lua' -o -name '*.c' -o -name '*.sh' \\) " ..
        "2>/dev/null")
    if f_handle then
        local listing = f_handle:read("*a")
        f_handle:close()
        for entry in listing:gmatch("[^\n]+") do
            local name = entry:match("/([^/]+)$")
            if name then files[#files + 1] = name end
        end
        table.sort(files)
    end

    json_ok(client, { path = path, dirs = dirs, files = files })
end
-- }}}

-- {{{ dispatch
local function dispatch(client, req, maps_root)
    local m = req.method
    local p = req.parts

    -- OPTIONS preflight
    if m == "OPTIONS" then return handle_options(client) end

    -- GET /fs/dirs?path=...
    if m == "GET" and #p == 2 and p[1] == "fs" and p[2] == "dirs" then
        return handle_list_dirs(client, req, maps_root)
    end

    -- GET /maps
    if m == "GET" and #p == 1 and p[1] == "maps" then
        return handle_list_maps(client, req, maps_root)
    end

    -- GET /maps/<name>/boxes
    if m == "GET" and #p == 3 and p[1] == "maps" and p[3] == "boxes" then
        return handle_list_boxes(client, req, maps_root)
    end

    -- GET/PUT/DELETE /maps/<name>/boxes/<id>
    if #p == 4 and p[1] == "maps" and p[3] == "boxes" then
        if m == "GET"    then return handle_get_box(client, req, maps_root) end
        if m == "PUT"    then return handle_put_box(client, req, maps_root) end
        if m == "DELETE" then return handle_delete_box(client, req, maps_root) end
    end

    -- GET /maps/<name>/src
    if m == "GET" and #p == 3 and p[1] == "maps" and p[3] == "src" then
        return handle_list_src(client, req, maps_root)
    end

    -- GET /maps/<name>/src/<file>
    if m == "GET" and #p == 4 and p[1] == "maps" and p[3] == "src" then
        return handle_get_src(client, req, maps_root)
    end

    -- GET /maps/<name>/extrasrc  (list all extra dirs + their files)
    if m == "GET" and #p == 3 and p[1] == "maps" and p[3] == "extrasrc" then
        return handle_list_extrasrc(client, req, maps_root)
    end

    -- GET /maps/<name>/extrasrc/<dir_index>/<file>
    if m == "GET" and #p == 5 and p[1] == "maps" and p[3] == "extrasrc" then
        return handle_get_extrasrc(client, req, maps_root)
    end

    -- GET/PUT /maps/<name>/data/<filename>
    if #p == 4 and p[1] == "maps" and p[3] == "data" then
        if m == "GET" then return handle_get_data(client, req, maps_root) end
        if m == "PUT" then return handle_put_data(client, req, maps_root) end
    end

    -- GET/PUT /maps/<name>/drivers
    if #p == 3 and p[1] == "maps" and p[3] == "drivers" then
        if m == "GET" then return handle_get_simple(client, req, maps_root, "drivers.json") end
        if m == "PUT" then return handle_put_simple(client, req, maps_root, "drivers.json", schema.validate_drivers) end
    end

    -- GET/PUT /maps/<name>/meta
    if #p == 3 and p[1] == "maps" and p[3] == "meta" then
        if m == "GET" then return handle_get_simple(client, req, maps_root, "meta.json") end
        if m == "PUT" then return handle_put_simple(client, req, maps_root, "meta.json", schema.validate_meta) end
    end

    -- GET / and GET /js/* and GET /*.html — serve static assets.
    -- Also serves /langs/<name>/<file> from the project's langs/
    -- directory so the editor can load per-language lexers (issue 223)
    -- alongside the phase 3 runtime specs that share that tree.
    if m == "GET" then
        local rel = req.path == "/" and "/index.html" or req.path
        -- only allow paths that don't escape the asset / langs roots
        if not rel:find("%.%.", 1, true) then
            local file_path
            if rel:sub(1, 7) == "/langs/" then
                file_path = DIR .. rel
            else
                file_path = DIR .. "/assets" .. rel
            end
            local raw, _ = read_file(file_path)
            if raw then
                local ext = rel:match("%.([^./]+)$") or ""
                local mime = ({
                    html = "text/html",
                    js   = "application/javascript",
                    css  = "text/css",
                })[ext] or "text/plain"
                respond(client, "200 OK", raw, mime)
                return
            end
        end
    end

    json_err(client, "404 Not Found", "no route for " .. m .. " " .. req.path)
end
-- }}}

-- {{{ serve
-- opts: { maps_root = string, port = number, bundled_dirs = {string} }
-- The bundled_dirs list (resolved absolute paths from issue 238's
-- config file) is captured into module state so the dispatch handlers
-- can read it without threading an extra arg through every signature.
function M.serve(opts)
    local maps_root = opts.maps_root
    local port      = opts.port
    bundled_dirs    = opts.bundled_dirs or {}

    local server, err = socket.bind("*", port)
    if not server then
        io.stderr:write("server: cannot bind to port " .. port .. ": " .. tostring(err) .. "\n")
        os.exit(1)
    end
    server:settimeout(1)  -- 1s accept timeout so we can handle signals cleanly

    print("soramech-server listening on :" .. port .. " (maps: " .. maps_root .. ")")

    while true do
        local client, cerr = server:accept()
        if client then
            client:settimeout(5)
            local t_start = socket.gettime()

            local req, rerr = parse_request(client)
            if req then
                local ok_disp, derr = pcall(dispatch, client, req, maps_root)
                if not ok_disp then
                    pcall(json_err, client, "500 Internal Server Error", tostring(derr))
                end
                local elapsed = math.floor((socket.gettime() - t_start) * 1000)
                print(string.format("[%.0f] %s %s (%dms)",
                    os.time(), req.method, req.path, elapsed))
            else
                pcall(json_err, client, "400 Bad Request", tostring(rerr))
            end

            client:close()
        end
        -- cerr is "timeout" on accept timeout — that's normal, keep looping
    end
end
-- }}}

return M
