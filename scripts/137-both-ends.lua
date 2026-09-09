#!/usr/bin/env luajit
--[[
137-both-ends.lua — write the receiving end of every wire in a map file.

What this is: a map written before wires were declared at both ends
says only where values *go*. This reads the arrows it has, works out
what each station is fed by, and writes those facts back into the file
as `in` lines — so an old map becomes a current one without anybody
retyping it.

How it does it, in general terms: one pass collects every `out N -
dest.port` line and files it under the station it points at. A second
pass rewrites the file, and whenever a station line goes by, the `in`
lines that station is owed are written out directly beneath it. Lines
that already exist are left exactly as they are, including any `in`
line the file already had — a port holding a constant is not a port
being fed, and this never touches one.

It is safe to run twice: a wire whose receiving end is already written
down is not written again.

Options:
  --check    say what would change and write nothing; exit 1 if
             anything would. For a build or a hook that wants to know
             whether a map is current without altering it.
  --c        the named files are C sources holding map text as string
             literals, which is how every test in this project writes
             a map. Each run of consecutive literal lines is one map,
             migrated in place, with whatever followed the closing
             quote — a comma, the arguments of a printf — left where
             it was.
  --quiet    write the files and say nothing.

Usage:
  137-both-ends.lua [--check] [--quiet] <map-file> [<map-file> ...]
  137-both-ends.lua [--check] [--quiet]            (every map under maps/)
  137-both-ends.lua --c [--check] <c-source> [<c-source> ...]
]]

local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- {{{ local function station_of()
-- The station name on a `station` line, or nil for any other line.
local function station_of(line)
    return line:match("^%s*station%s+([^%s]+)")
end
-- }}}

-- {{{ local function arrow_of()
-- The three numbers and one name an `out` line carries: which port of
-- this station, which station it points at, and which port there.
local function arrow_of(line)
    local from_port, dest = line:match("^%s*out%s+(%d+)%s+%-%s+([^%s]+)")
    if not from_port then return nil end
    local to_station, to_port = dest:match("^(.*)%.(%d+)$")
    if not to_station then return nil end
    return tonumber(from_port), to_station, tonumber(to_port)
end
-- }}}

-- {{{ local function existing_source_of()
-- The source an `in` line already names, so a file that is already
-- current is left alone. Returns the port, the source station and the
-- source port, or nil when the line is any other kind of `in`.
local function existing_source_of(line)
    local port, source = line:match("^%s*in%s+(%d+)%s+%-%s+([^%s]+)")
    if not port then return nil end
    local from_station, from_port = source:match("^(.*)%.(%d+)$")
    if not from_station then return nil end
    return tonumber(port), from_station, tonumber(from_port)
end
-- }}}

-- {{{ local function read_lines()
local function read_lines(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local lines = {}
    for line in f:lines() do lines[#lines + 1] = line end
    f:close()
    return lines
end
-- }}}

-- {{{ local function wires_into()
-- Every wire the file draws, filed under the station it arrives at.
-- The key is the receiving station's name; the value is a list of
-- {port, from, from_port}, in the order the arrows appeared, which is
-- the one order a file can be said to have.
local function wires_into(lines)
    local into, at = {}, nil
    for _, line in ipairs(lines) do
        local name = station_of(line)
        if name then at = name end
        local from_port, to_station, to_port = arrow_of(line)
        if from_port and at then
            into[to_station] = into[to_station] or {}
            table.insert(into[to_station],
                         { port = to_port, from = at, from_port = from_port })
        end
    end
    return into
end
-- }}}

-- {{{ local function already_written()
-- The wires each station already declares from its own side, so
-- running this twice adds nothing the second time. Keyed by the same
-- three facts the arrow carries, joined into one string.
local function already_written(lines)
    local known, at = {}, nil
    for _, line in ipairs(lines) do
        local name = station_of(line)
        if name then at = name end
        local port, from, from_port = existing_source_of(line)
        if port and at then
            known[at .. "|" .. port .. "|" .. from .. "|" .. from_port] = true
        end
    end
    return known
end
-- }}}

-- {{{ local function rewrite()
-- The file with each station's missing `in` lines written directly
-- beneath its station line, where a reader expects a station's own
-- facts to be. Returns the new lines and how many were added.
local function rewrite(lines)
    local into = wires_into(lines)
    local known = already_written(lines)
    local out, added = {}, 0

    for _, line in ipairs(lines) do
        out[#out + 1] = line
        local name = station_of(line)
        if name and into[name] then
            -- The indentation of the line below, so an added line
            -- looks like the ones it sits among rather than like
            -- something a tool put there.
            for _, w in ipairs(into[name]) do
                local key = name .. "|" .. w.port .. "|"
                            .. w.from .. "|" .. w.from_port
                if not known[key] then
                    out[#out + 1] = string.format("  in %d - %s.%d",
                                                  w.port, w.from, w.from_port)
                    known[key] = true
                    added = added + 1
                end
            end
        end
    end
    return out, added
end
-- }}}

-- {{{ local function c_map_line()
-- A line of a C source that is one line of a map written as a string
-- literal, which is how every test in this project writes one:
--
--     "station adder add p\n"
--     "  out 0 - printer.0\n",
--     result_path);
--
-- Returns the indentation, the map text inside the quotes, and
-- whatever followed the closing quote — or nil when the line is not
-- one of these. The trailing part is kept verbatim because the last
-- line of a block often carries a comma and the arguments of a printf.
local function c_map_line(line)
    local lead, body, tail = line:match('^(%s*)"(.-)\\n"(.*)$')
    if not body then return nil end
    return lead, body, tail
end
-- }}}

-- {{{ local function is_filler()
-- A line that sits *inside* a map literal without being part of the
-- map: a blank line, or a C comment explaining the station below it.
-- These do not end a map, and treating them as if they did split one
-- test's map in half and left the generator complaining about a wire
-- whose other end was three lines further down.
local function is_filler(line)
    local trimmed = line:match("^%s*(.-)%s*$")
    return trimmed == ""
           or trimmed:sub(1, 2) == "/*"
           or trimmed:sub(1, 2) == "//"
           or trimmed:sub(1, 1) == "*"
end
-- }}}

-- {{{ local function rewrite_c()
-- The same migration, applied to every run of consecutive map-text
-- lines in a C source. A run is one map: the lines of a map literal
-- sit together, and anything that is not one of them ends it.
--
-- **No existing line is ever rewritten**, only inserted between. The
-- first attempt decoded each literal, migrated, and re-encoded, which
-- promptly doubled the backslashes of a line that already spelled an
-- escaped quote. A tool that has to re-encode what it did not need to
-- change is a tool with a second chance to be wrong; this one only
-- ever writes the lines it invents, and those hold nothing a C
-- compiler cares about.
local function rewrite_c(lines)
    local out, added, i = {}, 0, 1

    while i <= #lines do
        local lead = c_map_line(lines[i])
        if not lead then
            out[#out + 1] = lines[i]
            i = i + 1
        else
            -- The run, gathered as line numbers so filler can sit
            -- inside it. A comment between two stations belongs to the
            -- map it explains, so it must not end one.
            local span, last_map, first, indent = {}, 0, i, lead
            while i <= #lines do
                if c_map_line(lines[i]) then
                    span[#span + 1] = i
                    last_map = #span
                elseif is_filler(lines[i]) then
                    span[#span + 1] = i
                else
                    break
                end
                i = i + 1
            end
            -- Filler after the last map line was never inside the map;
            -- give it back, so the next pass sees it as ordinary code.
            for k = #span, last_map + 1, -1 do
                span[k] = nil
            end
            i = first + (#span > 0 and span[#span] - first + 1 or 0)

            local block = {}
            for _, at in ipairs(span) do
                local _, body = c_map_line(lines[at])
                block[#block + 1] = body or ""
            end

            local into = wires_into(block)
            local known = already_written(block)

            for k = 1, #span do
                local original = lines[span[k]]
                local _, _, tail = c_map_line(original)
                tail = tail or ""

                local inserts = {}
                local name = station_of(block[k])
                if name and into[name] then
                    for _, w in ipairs(into[name]) do
                        local key = name .. "|" .. w.port .. "|"
                                    .. w.from .. "|" .. w.from_port
                        if not known[key] then
                            inserts[#inserts + 1] = string.format(
                                '%s"  in %d - %s.%d\\n"',
                                indent, w.port, w.from, w.from_port)
                            known[key] = true
                            added = added + 1
                        end
                    end
                end

                if #inserts > 0 and tail ~= "" then
                    -- The last line of a literal often carries the comma
                    -- and arguments that close a printf. Those belong
                    -- after the *last* line, so they move — by cutting
                    -- exactly the tail's characters off the end, which
                    -- leaves the quoted part untouched.
                    out[#out + 1] = original:sub(1, #original - #tail)
                    for j, line in ipairs(inserts) do
                        out[#out + 1] = line
                                        .. (j == #inserts and tail or "")
                    end
                else
                    out[#out + 1] = original
                    for _, line in ipairs(inserts) do
                        out[#out + 1] = line
                    end
                end
            end
        end
    end
    return out, added
end
-- }}}

-- {{{ local function maps_under()
-- Every map in the project, when no file was named.
local function maps_under(dir)
    local found = {}
    local pipe = io.popen("find '" .. dir .. "/maps' -name '*.map' | sort")
    if not pipe then return found end
    for path in pipe:lines() do found[#found + 1] = path end
    pipe:close()
    return found
end
-- }}}

-- {{{ main
local check, quiet, c_mode, paths = false, false, false, {}
for i = 1, #arg do
    if arg[i] == "--check" then check = true
    elseif arg[i] == "--quiet" then quiet = true
    elseif arg[i] == "--c" then c_mode = true
    else paths[#paths + 1] = arg[i] end
end
if #paths == 0 then paths = maps_under(DIR) end

local behind = 0
for _, path in ipairs(paths) do
    local lines = read_lines(path)
    if not lines then
        io.stderr:write("both-ends: cannot read " .. path .. "\n")
        os.exit(1)
    end

    local fixed, added
    if c_mode then
        fixed, added = rewrite_c(lines)
    else
        fixed, added = rewrite(lines)
    end
    if added > 0 then
        behind = behind + 1
        if check then
            if not quiet then
                print(string.format("%s is missing %d receiving end%s",
                                    path, added, added == 1 and "" or "s"))
            end
        else
            local f = io.open(path, "w")
            if not f then
                io.stderr:write("both-ends: cannot write " .. path .. "\n")
                os.exit(1)
            end
            f:write(table.concat(fixed, "\n"), "\n")
            f:close()
            if not quiet then
                print(string.format("%s: wrote %d receiving end%s",
                                    path, added, added == 1 and "" or "s"))
            end
        end
    end
end

if check and behind > 0 then os.exit(1) end
os.exit(0)
-- }}}
