-- Regression test for the driver-test map.
-- Verifies all three driver boxes (lua, bash, C) run and produce correct output.
-- Guards against the silent-skip bug where entry box inputs are unsatisfied.

local DIR = "/mnt/mtwo/programs/sora/soramech"
package.path = DIR .. "/libs/?.lua;" .. package.path

local json   = require("dkjson")
local loader = dofile(DIR .. "/src/003-loader.lua")
local exec   = dofile(DIR .. "/src/004-executor.lua")

-- {{{ local function fail
local function fail(msg)
    io.stderr:write("[FAIL] " .. msg .. "\n")
    os.exit(1)
end
-- }}}

-- {{{ local function pass
local function pass(msg)
    io.write("[PASS] " .. msg .. "\n")
end
-- }}}

-- {{{ local function read_json
local function read_json(path)
    local f = io.open(path, "r")
    if not f then fail("cannot open: " .. path) end
    local raw = f:read("*a"); f:close()
    local obj, _, err = json.decode(raw)
    if not obj then fail("JSON error in " .. path .. ": " .. tostring(err)) end
    return obj
end
-- }}}

local map_dir = DIR .. "/maps/driver-test"

-- load and validate graph
local graph, errs = loader.load_map(map_dir)
if not graph then
    fail("load_map failed: " .. table.concat(errs or {}, "; "))
end
local ok, verrs = loader.validate_graph(graph)
if not ok then
    fail("validate_graph failed: " .. table.concat(verrs or {}, "; "))
end
pass("graph loads and validates")

-- run the map
local run_ok, run_err = exec.execute(graph, map_dir)
if not run_ok then fail("execute failed: " .. tostring(run_err)) end
pass("execute returned ok")

-- read last-run.json
local run_log = read_json(map_dir .. "/tmp/last-run.json")
if not run_log.ok then fail("last-run.json reports not ok: " .. tostring(run_log.error)) end

-- verify all three boxes ran
local boxes = run_log.boxes
if type(boxes) ~= "table" then fail("last-run.json boxes is not a table") end

if not boxes["lua-box"] then fail("lua-box did not run") end
if not boxes["bash-box"] then fail("bash-box did not run") end
if not boxes["c-box"]    then fail("c-box did not run") end
pass("all three driver boxes ran")

-- verify output chain: 3+5=8, stringified: 8, STRINGIFIED: 8
local sum = boxes["lua-box"].outputs and boxes["lua-box"].outputs.sum
if sum ~= 8 then fail("lua-box sum expected 8, got " .. tostring(sum)) end
pass("lua-box output: sum=8")

local text = boxes["bash-box"].outputs and boxes["bash-box"].outputs.text
if text ~= "stringified: 8" then
    fail("bash-box text expected 'stringified: 8', got " .. tostring(text))
end
pass("bash-box output: text='stringified: 8'")

local result = boxes["c-box"].outputs and boxes["c-box"].outputs.result
if result ~= "STRINGIFIED: 8" then
    fail("c-box result expected 'STRINGIFIED: 8', got " .. tostring(result))
end
pass("c-box output: result='STRINGIFIED: 8'")

io.write("\nAll driver-test regression tests passed.\n")
os.exit(0)
