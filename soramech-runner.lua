-- SoraMech runner — entry point for FSM execution.
-- Loads a map directory, validates the graph, then executes it synchronously.
-- Run: luajit soramech-runner.lua <map-dir>
-- Output goes to map-dir/tmp/last-run.json; logs to map-dir/tmp/logs/.

local DIR = "/mnt/mtwo/programs/sora/soramech"

package.path = DIR .. "/libs/?.lua;" ..
               DIR .. "/src/?.lua;" ..
               package.path

local loader   = require("003-loader")
local executor = require("004-executor")

-- {{{ main
local function main(args)
    local map_dir = args[1]
    if not map_dir then
        io.stderr:write("usage: luajit soramech-runner.lua <map-dir>\n")
        os.exit(1)
    end

    -- ensure tmp/logs/ exists (tmp/ is the /tmp/<name>/ symlink from create-map.sh)
    os.execute("mkdir -p " .. map_dir .. "/tmp/logs")

    local graph, load_errs = loader.load_map(map_dir)
    if not graph then
        io.stderr:write("runner: load failed:\n")
        for _, e in ipairs(load_errs) do
            io.stderr:write("  " .. e .. "\n")
        end
        os.exit(1)
    end

    local ok, val_errs = loader.validate_graph(graph)
    if not ok then
        io.stderr:write("runner: validation failed:\n")
        for _, e in ipairs(val_errs) do
            io.stderr:write("  " .. e .. "\n")
        end
        os.exit(1)
    end

    local run_ok, run_err = executor.execute(graph, map_dir)
    if not run_ok then
        io.stderr:write("runner: execution failed: " .. tostring(run_err) .. "\n")
        os.exit(1)
    end

    print("runner: OK — " .. map_dir .. "/tmp/last-run.json written")
    os.exit(0)
end
-- }}}

main(arg)
