-- Synchronous FSM executor for the SoraMech runner.
-- Walks the graph from the entry box, invokes language drivers for each
-- box in dependency order, and threads output values through wires.
-- Each box call is isolated in run_task() — the future thread pool swap point.

local DIR = "/mnt/mtwo/programs/sora/soramech"

package.path = DIR .. "/libs/?.lua;" ..
               DIR .. "/src/?.lua;" ..
               package.path

local json = require("dkjson")
local M    = {}

-- {{{ shell_quote
local function shell_quote(s)
    -- wrap a string in single quotes, escaping any embedded single quotes
    return "'" .. tostring(s):gsub("'", "'\\''") .. "'"
end
-- }}}

-- {{{ driver_path_for
local function driver_path_for(ref, drivers)
    -- binary callers (no extension mapping needed — they are executable directly)
    -- source callers: look up extension in driver table
    local ext = ref:match("%.([^./]+)$")
    if not ext then return ref end  -- no extension: treat as binary
    local driver = drivers["." .. ext]
    return driver
end
-- }}}

-- {{{ run_task
-- The task boundary. In v1: synchronous. Future: hand this signature to
-- the thread pool. Returns a single output value and ok/err.
local function run_task(box, inputs, drivers, map_dir)
    local ref = box.ref
    local fn  = box.fn

    -- locate the driver for this ref
    local driver = driver_path_for(ref, drivers)
    if not driver then
        return nil, "no driver found for ref '" .. ref .. "'"
    end

    -- resolve ref relative to map_dir
    local abs_ref = map_dir .. "/" .. ref

    -- build argument list: each input port's value as a JSON-encoded shell arg
    local args = {}
    for _, port in ipairs(box.inputs or {}) do
        local val = inputs[port.name]
        if val == nil then
            -- nil input becomes JSON null
            args[#args + 1] = shell_quote(json.encode(nil))
        else
            args[#args + 1] = shell_quote(json.encode(val))
        end
    end

    local cmd
    if fn then
        -- source caller: driver file fn arg_count [args...]
        cmd = driver .. " " .. shell_quote(abs_ref) .. " " ..
              shell_quote(fn) .. " " .. #args
        for _, a in ipairs(args) do cmd = cmd .. " " .. a end
    else
        -- binary caller: run ref directly with args
        cmd = abs_ref .. " " .. #args
        for _, a in ipairs(args) do cmd = cmd .. " " .. a end
    end

    -- invoke driver; capture stdout; errors go to stderr (visible in terminal)
    local tmpfile = map_dir .. "/tmp/driver-stderr-" .. box.id .. ".txt"
    local full_cmd = "sh -c " .. shell_quote(cmd .. " 2>" .. tmpfile)
    local ph = io.popen(full_cmd, "r")
    local stdout = ph:read("*a")
    local ok_close = ph:close()

    -- read stderr if any
    local sf = io.open(tmpfile, "r")
    local stderr_text = sf and sf:read("*a") or ""
    if sf then sf:close() end
    os.remove(tmpfile)

    if not ok_close then
        local err_msg = "driver failed for box '" .. box.id .. "'"
        if stderr_text and #stderr_text > 0 then
            err_msg = err_msg .. ": " .. stderr_text:gsub("%s+$", "")
        end
        return nil, err_msg
    end

    -- parse single JSON value from stdout; one output wire per box
    local output_val, _, jerr = json.decode(stdout)
    if jerr then
        return nil, "box '" .. box.id .. "': driver stdout is not valid JSON: " ..
            tostring(jerr) .. " (got: " .. tostring(stdout):sub(1, 80) .. ")"
    end

    return output_val, nil
end
-- }}}

-- {{{ inputs_satisfied
local function inputs_satisfied(box, store)
    -- a box is ready when all its declared input ports have a value in the store
    for _, port in ipairs(box.inputs or {}) do
        if store[box.id .. "." .. port.name] == nil then
            return false
        end
    end
    return true
end
-- }}}

-- {{{ collect_inputs
local function collect_inputs(box, store)
    local inputs = {}
    for _, port in ipairs(box.inputs or {}) do
        inputs[port.name] = store[box.id .. "." .. port.name]
    end
    return inputs
end
-- }}}

-- {{{ fire_connections
-- Routes the single output value through wired connections.
-- If box.comparand is set: compare output against it (must be a number or error).
-- Otherwise: fire all connections whose from_branch is nil.
-- Returns nil on success, or an error string.
local function fire_connections(box, output_val, store, queue, boxes)
    if box.comparand and box.comparand ~= "" then
        -- comparator mode: output must be a number
        local comparand = tonumber(box.comparand)
        if not comparand then
            return "box '" .. box.id .. "': comparand '" ..
                tostring(box.comparand) .. "' is not a valid number"
        end
        local num_val = tonumber(output_val)
        if not num_val then
            return "box '" .. box.id .. "': output value '" ..
                tostring(output_val) .. "' is not a number, but comparator is configured"
        end

        -- determine which branch to fire
        local branch
        if num_val < comparand then
            branch = "lt"
        elseif num_val == comparand then
            branch = "eq"
        else
            branch = "gt"
        end

        for _, c in ipairs(box.connections or {}) do
            if c.from_box ~= box.id then goto next_cmp_conn end
            if c.from_branch == branch then
                store[c.to_box .. "." .. c.to_input] = output_val
                local target = boxes[c.to_box]
                if target and inputs_satisfied(target, store) then
                    queue[#queue + 1] = c.to_box
                end
            end
            ::next_cmp_conn::
        end
    else
        -- single-wire mode: fire connections where from_branch is nil
        for _, c in ipairs(box.connections or {}) do
            if c.from_box ~= box.id then goto next_conn end
            if c.from_branch == nil then
                store[c.to_box .. "." .. c.to_input] = output_val
                local target = boxes[c.to_box]
                if target and inputs_satisfied(target, store) then
                    queue[#queue + 1] = c.to_box
                end
            end
            ::next_conn::
        end
    end
    return nil
end
-- }}}

-- {{{ write_last_run
local function write_last_run(map_dir, run_state)
    local tmp_dir = map_dir .. "/tmp"
    local path    = tmp_dir .. "/last-run.json"
    local f, ferr = io.open(path, "w")
    if not f then
        io.stderr:write("executor: cannot write last-run.json: " .. tostring(ferr) .. "\n")
        return
    end
    f:write(json.encode(run_state, { indent = true }))
    f:close()
end
-- }}}

-- {{{ execute
function M.execute(graph, map_dir)
    local boxes    = graph.boxes
    local drivers  = graph.drivers
    local entry_id = graph.entry

    -- store: keyed by "box_id.port_name" -> value
    local store = {}

    -- pre-seed store with any literal values set on input ports;
    -- wires that fire at runtime will overwrite these, so wires always win
    for box_id, box in pairs(boxes) do
        for _, port in ipairs(box.inputs or {}) do
            if port.value ~= nil and port.value ~= "" then
                -- try JSON decode first; fall back to raw string
                local parsed, _, _ = json.decode(port.value)
                store[box_id .. "." .. port.name] = parsed ~= nil and parsed or port.value
            end
        end
    end

    local queue   = { entry_id }
    local visited = {}  -- boxes that have been run (prevent double-execution)
    local run_log = {}  -- per-box execution records for last-run.json
    local map_ok  = true
    local map_err = nil

    while #queue > 0 do
        -- dequeue
        local id = table.remove(queue, 1)

        local box = boxes[id]
        if not box then
            map_ok  = false
            map_err = "unknown box in queue: '" .. id .. "'"
            break
        end

        -- skip boxes whose inputs are not yet satisfied
        if not inputs_satisfied(box, store) then
            goto next_iter
        end

        if visited[id] then
            goto next_iter
        end
        visited[id] = true

        local inputs = collect_inputs(box, store)

        local output_val, task_err = run_task(box, inputs, drivers, map_dir)

        run_log[id] = {
            inputs = inputs,
            output = output_val,
            status = task_err and "error" or "ok",
            error  = task_err,
        }

        if task_err then
            map_ok  = false
            map_err = task_err
            io.stderr:write("executor: " .. task_err .. "\n")
            break
        end

        -- route output through wired connections
        local conn_err = fire_connections(box, output_val, store, queue, boxes)
        if conn_err then
            map_ok  = false
            map_err = conn_err
            io.stderr:write("executor: " .. conn_err .. "\n")
            break
        end

        ::next_iter::
    end

    -- write last-run snapshot to tmp/
    write_last_run(map_dir, {
        ok    = map_ok,
        error = map_err,
        boxes = run_log,
    })

    return map_ok, map_err
end
-- }}}

return M
