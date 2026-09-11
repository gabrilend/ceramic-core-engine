#!/usr/bin/env luajit
--[[
141-station-lines.lua — move a station's kind to the front of its line.

What this is: a map written before issue 608 says what kind of station
it is with a single letter at the far end of the line, and names its box
function as a bare word that looks like every other word there. This
reads such a file and writes the current form — the kind as the line's
first word, spelled out, and the box function in brackets.

    station under_ten keep c          ->  comparator under_ten (keep)
    station split route.c:spread i @2 ->  iterator split (route.c:spread) @2
    station feed cycle_thirty p       ->  station feed (cycle_thirty)

How it does it, in general terms: one pass, one line at a time, with no
state carried between lines — a station line is self-contained, so the
rewrite is local. Every other line is copied through untouched, and a
line already in the current form does not match the old shape at all,
which is what makes the tool safe to run twice.

It handles map text wherever it lives. A line in a .map file is migrated
directly; a line that is a C string literal — which is how every test in
this project writes a map — is unwrapped, migrated, and wrapped again,
with whatever followed the closing quote left where it was. A fenced
example inside a markdown document is an ordinary line and migrates like
one.

Options:
  --check    say what would change and write nothing; exit 1 if anything
             would. For a build or a hook that wants to know whether a
             file is current without altering it.
  --quiet    write the files and say nothing.

Usage:
  141-station-lines.lua [--check] [--quiet] <file> [<file> ...]
  141-station-lines.lua [--check] [--quiet]     (every map under maps/)
]]

local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- {{{ local function kind_keyword()
-- The letter a map used to end a station line with, as the word that
-- now begins one. Anything else is not a kind and the caller gives up.
local function kind_keyword(letter)
    if letter == "c" then return "comparator" end
    if letter == "i" then return "iterator"   end
    if letter == "p" then return "station"    end
    return nil
end
-- }}}

-- {{{ local function migrate_text()
-- One logical map line, with no quoting around it. Returns the current
-- form, or nil when the line is not a station line in the old shape —
-- which includes every line already migrated, since the current form
-- has two words after its keyword rather than three.
--
-- The comment is split off first and put back afterwards, so a station
-- line carrying a trailing `#` keeps it. A station line never holds a
-- quoted string, so a plain split on `#` is safe here in a way it is
-- not in the map reader itself.
local function migrate_text(text)
    local code, comment = text:match("^([^#]*)(.*)$")
    if not code then return nil end

    local lead, name, box, letter, tail =
        code:match("^(%s*)station%s+(%S+)%s+(%S+)%s+([pci])%s*(.-)%s*$")
    if not lead then return nil end

    -- `%S+` backtracks, so `station a b printer` can match with the
    -- letter `p` and `rinter` left over. Only an iterator's position
    -- may follow the kind, so anything else means this was never a
    -- station line in the old shape.
    if tail ~= "" and not tail:match("^@%d+$") then return nil end

    local keyword = kind_keyword(letter)
    if not keyword then return nil end

    local out = lead .. keyword .. " " .. name .. " (" .. box .. ")"
    if tail ~= "" then out = out .. " " .. tail end
    return out .. comment
end
-- }}}

-- {{{ local function migrate_line()
-- One physical line from a file of any kind. A C string literal is
-- unwrapped and rewrapped; anything else is migrated where it stands.
-- Returns nil when the line does not change.
local function migrate_line(line)
    -- A literal ending in an escaped newline, which is how a map line
    -- inside a C test is written.
    local lead, body, rest = line:match('^(%s*)"(.-)\\n"(.*)$')
    if body then
        local moved = migrate_text(body)
        if not moved then return nil end
        return string.format('%s"%s\\n"%s', lead, moved, rest)
    end

    -- A literal with no newline in it, which is how the last line of a
    -- map written as one string sometimes appears.
    lead, body, rest = line:match('^(%s*)"(.-)"(.*)$')
    if body then
        local moved = migrate_text(body)
        if not moved then return nil end
        return string.format('%s"%s"%s', lead, moved, rest)
    end

    return migrate_text(line)
end
-- }}}

-- {{{ local function migrate_file()
-- Reads one file, migrates every line that needs it, and writes it back
-- unless checking. Returns how many lines changed.
local function migrate_file(path, check)
    local f = io.open(path, "r")
    if not f then
        io.stderr:write("station-lines: cannot read " .. path .. "\n")
        return 0, false
    end
    local text = f:read("*a")
    f:close()

    local changed = 0
    local out = {}
    -- Kept without the trailing newline so a file that does not end in
    -- one is not given one.
    local ends_with_newline = text:sub(-1) == "\n"
    for line in (text .. (ends_with_newline and "" or "\n")):gmatch("([^\n]*)\n") do
        local moved = migrate_line(line)
        if moved then
            changed = changed + 1
            out[#out + 1] = moved
        else
            out[#out + 1] = line
        end
    end

    if changed > 0 and not check then
        local w = io.open(path, "w")
        if not w then
            io.stderr:write("station-lines: cannot write " .. path .. "\n")
            return changed, false
        end
        w:write(table.concat(out, "\n"))
        if ends_with_newline then w:write("\n") end
        w:close()
    end
    return changed, true
end
-- }}}

-- {{{ local function maps_under()
-- Every .map file under the project's maps directory, for the form of
-- the command that names no files.
local function maps_under(root)
    local paths = {}
    local p = io.popen("find " .. root .. "/maps -name '*.map' -type f")
    if p then
        for line in p:lines() do paths[#paths + 1] = line end
        p:close()
    end
    table.sort(paths)
    return paths
end
-- }}}

-- {{{ the command line
local check, quiet = false, false
local files = {}
for i = 1, #arg do
    if arg[i] == "--check" then check = true
    elseif arg[i] == "--quiet" then quiet = true
    else files[#files + 1] = arg[i] end
end
if #files == 0 then files = maps_under(DIR) end

local total, ok = 0, true
for _, path in ipairs(files) do
    local changed, wrote = migrate_file(path, check)
    ok = ok and wrote
    total = total + changed
    if changed > 0 and not quiet then
        print(string.format("%s %s: %d station line%s",
                            check and "would change" or "changed",
                            path, changed, changed == 1 and "" or "s"))
    end
end

if not ok then os.exit(2) end
if check and total > 0 then os.exit(1) end
if not quiet and total == 0 then print("every station line is already current") end
-- }}}
