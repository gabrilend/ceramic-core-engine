-- 130-replay-the-page.lua — what the viewer would be showing, without a browser.
--
-- What this is: the page's own rule for "this wire is carrying",
-- applied to a captured event stream, so a claim the drawing makes can
-- be checked against the events it was drawn from.
--
-- How it does it, in general terms: read the warm window out of the
-- page's source rather than restating it here, then walk the stream at
-- the rate the page redraws, keeping when each wire last carried. At
-- every frame it can then answer what the page would have been showing.
--
-- What it is for: a wire lit for longer than the program's own rhythm
-- keeps glowing after it has stopped, and two exits of one comparator
-- end up lit together — a picture of something that cannot happen,
-- since one value goes to one exit. That is invisible by inspection and
-- obvious by replay.
--
-- Usage: luajit 130-replay-the-page.lua <stream> <viewer.js> [station] [exitA] [exitB]

-- {{{ arguments
local stream_path = arg[1]
local source_path = arg[2]
local station     = tonumber(arg[3] or "3")
local exit_a      = tonumber(arg[4] or "0")
local exit_b      = tonumber(arg[5] or "2")

if not stream_path or not source_path then
    io.stderr:write("usage: replay-the-page <stream> <viewer.js> " ..
                    "[station] [exitA] [exitB]\n")
    os.exit(2)
end
-- }}}

-- {{{ local function warm_window(path)
-- The number the page actually uses. Read rather than repeated: two
-- places that must agree is one place too many.
local function warm_window(path)
    local f = assert(io.open(path), "cannot read " .. path)
    local js = f:read("*a")
    f:close()
    local ms = tonumber(js:match("WIRE_WARM_MS%s*=%s*(%d+)"))
    local frame = tonumber(js:match("FRAME_MS%s*=%s*(%d+)")) or 33
    return ms, frame
end
-- }}}

-- {{{ local function deliveries(path)
-- Every value that moved, with the moment and the exit it left by.
local function deliveries(path)
    local out = {}
    for line in io.lines(path) do
        local body = line:match("^data:%s*(.*)$")
        if body and body:find('"kind":"moved"', 1, true) then
            out[#out + 1] = {
                t    = tonumber(body:match('"ns":(%d+)')) / 1e6,
                from = tonumber(body:match('"a":(%d+)')),
                exit = tonumber(body:match('"b":(%d+)')),
            }
        end
    end
    return out
end
-- }}}

local warm, frame = warm_window(source_path)
if not warm then
    print("  the page no longer states a warm window")
    os.exit(1)
end

local moves = deliveries(stream_path)
if #moves < 30 then
    print("  only " .. #moves .. " deliveries seen, which is too few to judge")
    os.exit(1)
end

-- {{{ the replay
local lit, at = {}, 1
local together, frames = 0, 0

for t = moves[1].t, moves[#moves].t, frame do
    while at <= #moves and moves[at].t <= t do
        lit[moves[at].from .. "." .. moves[at].exit] = moves[at].t
        at = at + 1
    end
    frames = frames + 1

    local a = lit[station .. "." .. exit_a]
    local b = lit[station .. "." .. exit_b]
    if a and b and (t - a) < warm and (t - b) < warm then
        together = together + 1
    end
end
-- }}}

local share = together / frames * 100
print(string.format("  %d deliveries replayed through a %dms window",
                    #moves, warm))
print(string.format("  station %d's exits %d and %d shown live together " ..
                    "%.1f%% of the time", station, exit_a, exit_b, share))

if share > 20 then
    print("  which is the light outliving the truth. One value goes to one")
    print("  exit, so two of them lit together is a handover and not a")
    print("  steady state — the window is longer than the program's rhythm.")
    os.exit(1)
end
print("  which is a handover rather than a steady state")
