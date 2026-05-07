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

-- {{{ eval_predicate
local function eval_predicate(pred, value)
    -- structured predicates only; no arbitrary lua eval
    local op  = pred.op
    local lit = pred.value

    if op == "eq"       then return value == lit
    elseif op == "lt"   then return value <  lit
    elseif op == "gt"   then return value >  lit
    elseif op == "lte"  then return value <= lit
    elseif op == "gte"  then return value >= lit
    elseif op == "contains" then
        return type(value) == "string" and value:find(lit, 1, true) ~= nil
    elseif op == "matches" then
        return type(value) == "string" and value:match(lit) ~= nil
    end
    return false
end
-- }}}

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
-- the thread pool. Returns outputs table and ok/err.
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

    -- build argument list: file fn arg_count [args...]
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
    -- io.popen doesn't support stderr separation; redirect stderr to a tmp file
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

    -- parse output JSON array from stdout
    local result_arr, _, jerr = json.decode(stdout)
    if not result_arr then
        return nil, "box '" .. box.id .. "': driver stdout is not valid JSON: " ..
            tostring(jerr) .. " (got: " .. tostring(stdout):sub(1, 80) .. ")"
    end
    if type(result_arr) ~= "table" then
        return nil, "box '" .. box.id .. "': driver output must be a JSON array"
    end

    -- map positional results to named output ports
    local outputs = {}
    for i, port in ipairs(box.outputs or {}) do
        local raw = result_arr[i]
        if raw ~= nil then
            -- each element in the outer array is itself a JSON-encoded value
            local val, _, aerr = json.decode(raw)
            outputs[port.name] = aerr and raw or val
        end
    end

    return outputs, nil
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
local function fire_connections(box, outputs, store, queue, boxes, retry_counts)
    -- for branch boxes: evaluate predicates and fire the first matching port
    -- for call boxes: push all wired output values directly
    if box.kind == "branch" then
        local input_val = outputs["_branch_input"]
        local fired = false
        for _, port in ipairs(box.ports or {}) do
            if port.name == "else" then goto next_port end
            if port.predicate and eval_predicate(port.predicate, input_val) then
                -- find and fire connections from this port
                for _, c in ipairs(box.connections or {}) do
                    if c.from_box == box.id and c.from_port == port.name then
                        store[c.to_box .. "." .. c.to_input] = input_val
                        queue[#queue + 1] = c.to_box
                    end
                end
                fired = true
                break
            end
            ::next_port::
        end

        if not fired then
            -- fire "else" port
            for _, c in ipairs(box.connections or {}) do
                if c.from_box == box.id and c.from_port == "else" then
                    store[c.to_box .. "." .. c.to_input] = input_val
                    queue[#queue + 1] = c.to_box
                    fired = true
                end
            end

            if not fired then
                -- unwired else: return signal to re-queue the upstream box
                return "retry"
            end
        end

    else
        -- call box: push each output value to connected input ports
        for _, c in ipairs(box.connections or {}) do
            if c.from_box ~= box.id then goto next_conn end
            local val = outputs[c.from_output]
            store[c.to_box .. "." .. c.to_input] = val
            -- enqueue target if all its inputs are now satisfied
            local target = boxes[c.to_box]
            if target and inputs_satisfied(target, store) then
                queue[#queue + 1] = c.to_box
            end
            ::next_conn::
        end
    end
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

    -- per-box retry counters (for branch box else-retry)
    local retry_counts = {}

    -- seed entry box: if it has inputs, they must come from data files;
    -- for v1, entry boxes with inputs are the user's responsibility to pre-seed
    -- via a data box or by starting with a box that has no inputs
    local entry_box = boxes[entry_id]
    if entry_box and #(entry_box.inputs or {}) == 0 then
        -- no inputs needed; ready immediately
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

        -- skip boxes whose inputs are not yet satisfied (they'll be re-enqueued)
        if not inputs_satisfied(box, store) then
            goto next_iter
        end

        if visited[id] and box.kind ~= "branch" then
            goto next_iter
        end
        visited[id] = true

        local inputs = collect_inputs(box, store)

        -- branch box is handled specially: it doesn't invoke a driver,
        -- it routes based on its single input value
        local outputs, task_err
        if box.kind == "branch" then
            -- the branch input is the value wired to its single input port
            local branch_val = inputs[(box.inputs or {})[1] and box.inputs[1].name or "value"]
            outputs = { _branch_input = branch_val }
            task_err = nil
        else
            outputs, task_err = run_task(box, inputs, drivers, map_dir)
        end

        run_log[id] = {
            inputs  = inputs,
            outputs = outputs or {},
            status  = task_err and "error" or "ok",
            error   = task_err,
        }

        if task_err then
            map_ok  = false
            map_err = task_err
            io.stderr:write("executor: " .. task_err .. "\n")
            break
        end

        -- fire connections and enqueue newly-ready boxes
        local signal = fire_connections(box, outputs, store, queue, boxes, retry_counts)
        if signal == "retry" then
            -- unwired else: find the immediate upstream box and re-queue it
            -- retry_vary is applied in run_task on the next invocation
            local max_retries = box.retry_limit or 3
            retry_counts[id] = (retry_counts[id] or 0) + 1
            if retry_counts[id] > max_retries then
                map_ok  = false
                map_err = "branch box '" .. id .. "' exceeded retry limit (" ..
                    max_retries .. ")"
                break
            end
            -- find the box that wired into this branch box and re-queue it
            for src_id, src_box in pairs(boxes) do
                for _, c in ipairs(src_box.connections or {}) do
                    if c.from_box == src_id and c.to_box == id then
                        visited[src_id] = nil
                        queue[#queue + 1] = src_id
                    end
                end
            end
        end

        ::next_iter::
    end

    -- write last-run snapshot to tmp/
    write_last_run(map_dir, {
        ok     = map_ok,
        error  = map_err,
        boxes  = run_log,
    })

    return map_ok, map_err
end
-- }}}

return M
