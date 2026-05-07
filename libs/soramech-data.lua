-- Data file access library for SoraMech map boxes.
-- Reads and writes JSON data files in a map's data/ or tmp/ directory.
-- Enforces constant flags; all writes are atomic (write-tmp-then-rename).

local json = require("dkjson")
local M    = {}

-- {{{ load_file
-- Reads and JSON-decodes a data file; returns table, err.
local function load_file(path)
    local f, ferr = io.open(path, "r")
    if not f then return nil, "cannot open " .. path .. ": " .. tostring(ferr) end
    local raw = f:read("*a")
    f:close()
    local obj, _, jerr = json.decode(raw)
    if not obj then return nil, "JSON decode error in " .. path .. ": " .. tostring(jerr) end
    return obj, nil
end
-- }}}

-- {{{ write_file
-- Atomically encodes obj as JSON and writes it to path.
local function write_file(path, obj)
    local tmp = path .. ".tmp"
    local f, ferr = io.open(tmp, "w")
    if not f then return nil, "cannot write " .. tmp .. ": " .. tostring(ferr) end
    f:write(json.encode(obj, { indent = true }))
    f:close()
    local ok, rerr = os.rename(tmp, path)
    if not ok then return nil, "rename failed for " .. path .. ": " .. tostring(rerr) end
    return true, nil
end
-- }}}

-- {{{ resolve_path
-- Builds the full filesystem path for a data file.
-- dir is map_dir/data or map_dir/tmp; filename is e.g. "state.json".
local function resolve_path(dir, filename)
    -- strip any path separators from filename to prevent traversal
    local safe = filename:gsub("[/\\]", "")
    return dir .. "/" .. safe
end
-- }}}

-- {{{ get_field
-- Walks a dot-separated field_path into a nested table; returns value or nil.
local function get_field(obj, field_path)
    local node = obj
    for key in field_path:gmatch("[^.]+") do
        if type(node) ~= "table" then return nil end
        -- data files store values inside {constant, value} wrappers at each leaf
        if node[key] ~= nil then
            node = node[key]
        elseif node.fields and node.fields[key] ~= nil then
            node = node.fields[key]
        else
            return nil
        end
    end
    -- unwrap leaf {constant, value} if present
    if type(node) == "table" and node.value ~= nil then
        return node.value
    end
    return node
end
-- }}}

-- {{{ is_constant
-- Returns true if the field at field_path is marked constant.
local function is_constant(obj, field_path)
    if obj.constant then return true end
    local node = obj
    local keys = {}
    for key in field_path:gmatch("[^.]+") do keys[#keys + 1] = key end
    for i, key in ipairs(keys) do
        if type(node) ~= "table" then return false end
        local next_node = nil
        if node[key] ~= nil then
            next_node = node[key]
        elseif node.fields and node.fields[key] ~= nil then
            next_node = node.fields[key]
        else
            return false
        end
        -- check constant on the intermediate or leaf node
        if type(next_node) == "table" and next_node.constant then
            return true
        end
        node = next_node
    end
    return false
end
-- }}}

-- {{{ set_field
-- Writes value to the field at field_path inside obj, creating intermediates.
-- Returns ok, err. Does not touch the file itself.
local function set_field(obj, field_path, value)
    local keys = {}
    for key in field_path:gmatch("[^.]+") do keys[#keys + 1] = key end
    if #keys == 0 then return nil, "empty field_path" end

    -- walk to the parent of the leaf
    local node = obj
    for i = 1, #keys - 1 do
        local key = keys[i]
        if node.fields and node.fields[key] ~= nil then
            node = node.fields[key]
        elseif node[key] ~= nil then
            node = node[key]
        else
            return nil, "field path '" .. field_path .. "' not found at '" .. key .. "'"
        end
    end

    local leaf_key = keys[#keys]
    -- write into the wrapper table if it exists, else set directly
    local target = nil
    if node.fields and node.fields[leaf_key] ~= nil then
        target = node.fields[leaf_key]
    elseif node[leaf_key] ~= nil then
        target = node[leaf_key]
    end

    if type(target) == "table" and target.value ~= nil then
        target.value = value
    elseif node.fields then
        node.fields[leaf_key] = { constant = false, value = value }
    else
        node[leaf_key] = value
    end
    return true, nil
end
-- }}}

-- {{{ M.load
-- Loads an entire data file and returns the decoded table.
-- dir is the directory containing the file (e.g. map_dir .. "/data").
function M.load(dir, filename)
    local path = resolve_path(dir, filename)
    return load_file(path)
end
-- }}}

-- {{{ M.get
-- Reads a single field from a data file by dot-separated field_path.
-- Returns the value (unwrapped from {constant, value} wrapper if present).
function M.get(dir, filename, field_path)
    local path = resolve_path(dir, filename)
    local obj, err = load_file(path)
    if not obj then return nil, err end
    local val = get_field(obj, field_path)
    if val == nil then
        return nil, "field '" .. field_path .. "' not found in " .. filename
    end
    return val, nil
end
-- }}}

-- {{{ M.set
-- Writes a single field in a data file; refuses writes to constant fields.
-- Returns true on success, nil + error string on failure.
function M.set(dir, filename, field_path, value)
    local path = resolve_path(dir, filename)
    local obj, err = load_file(path)
    if not obj then return nil, err end

    if is_constant(obj, field_path) then
        return nil, "field '" .. field_path .. "' in " .. filename .. " is constant (read-only)"
    end

    local ok, serr = set_field(obj, field_path, value)
    if not ok then return nil, serr end

    return write_file(path, obj)
end
-- }}}

return M
