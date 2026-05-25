#!/usr/bin/env luajit
-- scripts/normalise-jsonl.lua — drop non-deterministic fields
-- from a JSONL transcript so two runs of the same map produce
-- identical output suitable for diff-against-expected fixture
-- verification.
--
-- What this does, in one paragraph: each input line is parsed as
-- JSON, the fields that vary across runs (timestamps, durations,
-- worker indices, slot ids, task ids — anything timing or
-- thread-id dependent) are removed, the remaining keys are
-- re-emitted in alphabetical order, and then every line is
-- sorted lexicographically against every other line. Two runs of
-- the same map differing only in worker assignment / wall-clock
-- timing will normalise to byte-identical output.
--
-- Usage:
--   normalise-jsonl.lua < last-run.jsonl > normalised.jsonl
--   normalise-jsonl.lua last-run.jsonl > normalised.jsonl
--   normalise-jsonl.lua --dir DIR last-run.jsonl
--
-- Per the project convention: hard-coded ${DIR} path with an
-- override argument; all paths inside are computed relative to
-- ${DIR}.

-- {{{ DIR resolution + argv parsing
local DIR = "/mnt/mtwo/programs/sora/soramech"
local input_path = nil
local i = 1
while i <= #arg do
    local a = arg[i]
    if a == "--dir" then
        DIR = arg[i + 1]
        i = i + 2
    else
        input_path = a
        i = i + 1
    end
end
-- }}}

package.path = DIR .. "/libs/?.lua;" .. package.path
local json = require("dkjson")

-- {{{ Fields stripped from every event.
-- Each one varies across runs of the same map and would defeat a
-- byte-equal diff:
--   ts          — wall-clock seconds
--   duration_us — measured elapsed time
--   worker_idx  — which pool worker fired the task (changes per
--                 schedule)
--   slot_id     — the slot store assigns sequentially; growth /
--                 ordering depends on registration order, which
--                 can race
--   task_id     — sequential dispatch counter; differs when
--                 spawn order interleaves differently
local STRIP_FIELDS = {
    ts          = true,
    duration_us = true,
    worker_idx  = true,
    slot_id     = true,
    task_id     = true,
    -- n_workers depends on the machine's CPU count (or
    -- SORAMECH_WORKERS env); fixture tests are run on different
    -- hardware so the worker count would flake even when the
    -- map's behaviour is identical. Tests that genuinely care
    -- about worker count should pin it via SORAMECH_WORKERS and
    -- inspect the un-normalised log.
    n_workers   = true,
}
-- }}}

-- {{{ read_input() — slurp from file or stdin
local function read_input(path)
    local fp
    if path then
        fp = io.open(path, "r")
        if not fp then
            io.stderr:write("normalise-jsonl: cannot open '", path, "'\n")
            os.exit(1)
        end
    else
        fp = io.stdin
    end
    local body = fp:read("*all")
    if path then fp:close() end
    return body
end
-- }}}

-- {{{ normalise_object() — strip + sort keys in place
-- Returns (sorted_keys_array, n_keys) so the caller can drive
-- dkjson's keyorder serialisation parameter. Recurses into nested
-- objects so a field like {"old": {...}, "new": {...}} also gets
-- sorted-key emission on every level.
local function normalise_object(obj)
    if type(obj) ~= "table" then return nil, 0 end
    for k in pairs(obj) do
        if STRIP_FIELDS[k] then
            obj[k] = nil
        end
    end
    local keys = {}
    for k, v in pairs(obj) do
        keys[#keys + 1] = k
        -- Recurse into nested objects (not arrays — arrays are
        -- already positionally ordered).
        if type(v) == "table" then
            local is_array = (#v > 0)
            if not is_array then
                normalise_object(v)
            end
        end
    end
    table.sort(keys)
    return keys, #keys
end
-- }}}

-- {{{ main
local body = read_input(input_path)

-- Split by newline; allow trailing blank line.
local lines = {}
for line in body:gmatch("[^\n]+") do
    lines[#lines + 1] = line
end

local out = {}
for idx, line in ipairs(lines) do
    local obj, _, err = json.decode(line, 1, nil)
    if err then
        io.stderr:write("normalise-jsonl: line ", tostring(idx),
                        " parse error: ", tostring(err), "\n")
        os.exit(1)
    end
    local keys = normalise_object(obj)
    -- Pass keyorder so dkjson emits fields in alphabetical order.
    -- A line that originally read {"ts":1.2,"event":"task_start"}
    -- normalises to {"event":"task_start"} regardless of source
    -- ordering.
    out[#out + 1] = json.encode(obj, { keyorder = keys })
end

-- Sort lines lexicographically. The combination "alpha-keys per
-- object + alpha-sort across lines" makes the result invariant
-- under both within-object reordering AND across-line reordering.
-- A run that fires task_start then task_end vs. one that fires
-- the same two events with slot_alloc interleaved will normalise
-- to the same byte sequence after the slot_alloc field (with
-- slot_id stripped) folds into its sorted spot.
table.sort(out)

for _, line in ipairs(out) do
    io.write(line, "\n")
end
-- }}}
