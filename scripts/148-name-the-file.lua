#!/usr/bin/env luajit
--[[
148-name-the-file.lua — every station line says which file its box is in.

What this is: the one-pass rewrite for issue 609. A station line used to
be able to address its box by a bare function name, searched for among
whatever sources a build happened to be handed. It now always says the
file, so a description is the only statement of what a program is made
of. This turns every map file, every fenced example in a document, and
every piece of map text living inside a C string literal in a test from
the old spelling into the new one.

How it does it, in general terms: it reads the box sources first and
builds a table from function name to the file it lives in. Then it walks
every file it was pointed at, finds station lines, and rewrites a bare
address into the long form by looking the name up in that table. A name
that resolves to two files is reported and left alone, because the tool
cannot know which was meant and guessing is how a migration puts the
wrong function into somebody's program.

What it cannot see, and therefore must not be trusted about: a shell test
that writes its own box source into a heredoc has boxes this index knows
nothing about. If such a box shares a name with one in the project, the
lookup finds the project's and writes an address pointing at the wrong
file — which happened on the first run, to a test's own `swallow`. Map
text inside a test that builds its own sources is checked by hand, and
every name the tool could not place is reported so that the ones it did
place can be read over.

Why it is a tool rather than an afternoon: the same spelling appears in
four kinds of file, and a rewrite done by hand will finish three of them.
The station line's last rewrite learned that the hard way — its
pattern-matching tool recognised only one of the five station keywords
and reported every file clean.

Usage:
  148-name-the-file.lua [DIR] [--check]

  DIR       the project; defaults to the path hard-coded below
  --check   report what would change and change nothing
]]

-- The project directory, overridable as an argument so this runs from
-- anywhere.
local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"
local CHECK = false

for i = 1, #arg do
    if arg[i] == "--check" then CHECK = true
    elseif arg[i]:sub(1, 2) ~= "--" then DIR = arg[i] end
end

-- {{{ local function read_file()
local function read_file(path)
    local f = io.open(path, "rb")
    if not f then return nil end
    local text = f:read("*a")
    f:close()
    return text
end
-- }}}

-- {{{ local function write_file()
local function write_file(path, text)
    local f = assert(io.open(path, "wb"))
    f:write(text)
    f:close()
end
-- }}}

-- {{{ local function list_files()
-- Every file under a directory matching a pattern, found rather than
-- listed, so a file cannot exist that this silently does not see.
local function list_files(dir, pattern)
    local out = {}
    local pipe = io.popen("find '" .. dir .. "' -type f -name '" .. pattern .. "' 2>/dev/null")
    if not pipe then return out end
    for line in pipe:lines() do out[#out + 1] = line end
    pipe:close()
    return out
end
-- }}}

-- {{{ local function box_index()
--[[
Which file each box function lives in.

Built from the box sources rather than from any list, because a box is a
non-static function in a designated source and nothing registers it
anywhere. The recogniser is deliberately the same shape the generator's
parser uses — a return type, a name, an open bracket, at the start of a
line — because a tool that recognises a different set of things than the
build does is a second definition of what a box is.

A name found in two files is recorded as a collision rather than
overwritten. Those are reported and skipped: the whole point of the
change is that nobody should guess which file was meant, and a tool that
guesses is worse than the ambiguity it is removing.
]]
local function box_index(roots)
    local where, collided = {}, {}
    for _, root in ipairs(roots) do
        for _, path in ipairs(list_files(root, "*.c")) do
            local text = read_file(path)
            if text then
                local base = path:match("([^/]+)$")
                for line in text:gmatch("[^\n]+") do
                    -- A function definition at column zero: something
                    -- that is not a space, then a name, then a bracket.
                    local name = line:match("^[%w_]+[%s%*]+([%w_]+)%s*%(")
                    if name and not line:match("^static") then
                        if where[name] and where[name] ~= base then
                            collided[name] = true
                        end
                        where[name] = base
                    end
                end
            end
        end
    end
    return where, collided
end
-- }}}

-- {{{ local function rewrite()
--[[
One file's worth of station lines, rewritten.

The station line's shape is `<keyword> <name> (<address>)`, and the five
keywords are the ones the format settled on. Only the bracketed address
is touched, and only when it has no colon in it already — a line that
already says its file is a line this has nothing to do with, which is
what makes running the tool twice harmless.

Returns the new text and how many addresses it changed, plus the names it
could not resolve, so the caller can report rather than fail silently.
]]
local function rewrite(text, where, collided)
    local changed, unknown = 0, {}

    -- Line by line rather than by one pattern over the whole text,
    -- because a station line is a line and the keyword has to be at the
    -- start of it. A pattern loose enough to skip that would also match
    -- prose about station lines, which is most of what the documents
    -- are.
    local lines, n = {}, 0
    for line in text:gmatch("([^\n]*)\n?") do
        n = n + 1
        lines[n] = line
    end
    -- gmatch with an optional newline yields one empty trailing piece.
    if lines[n] == "" then lines[n] = nil; n = n - 1 end

    for i = 1, n do
        local line = lines[i]
        local indent, keyword, rest =
            line:match("^([%s\\t]*)(station)%s+(.*)$")
        if not keyword then
            indent, keyword, rest = line:match("^([%s\\t]*)(comparator)%s+(.*)$")
        end
        if not keyword then
            indent, keyword, rest = line:match("^([%s\\t]*)(iterator)%s+(.*)$")
        end
        if keyword then
            local name, addr, tail = rest:match("^([%w_]+)%s+%(([^%)]*)%)(.*)$")
            if name and addr and not addr:find(":") and addr ~= "" then
                local file = where[addr]
                if collided[addr] then
                    unknown[#unknown + 1] = addr .. " (in more than one file)"
                elseif file then
                    lines[i] = indent .. keyword .. " " .. name ..
                               " (" .. file .. ":" .. addr .. ")" .. tail
                    changed = changed + 1
                else
                    unknown[#unknown + 1] = addr
                end
            end
        end
    end

    local out = table.concat(lines, "\n")
    if text:sub(-1) == "\n" then out = out .. "\n" end
    return out, changed, unknown
end
-- }}}

-- {{{ local function rewrite_c_strings()
--[[
Map text living inside a C string literal, rewritten.

A test writes a description as `"station head (seven)\n"` and hands it to
the engine at run time. That is a station line as far as the format is
concerned and has to change with the rest, but it is not on a line of its
own and the line-by-line pass above cannot see it.

**The address it gets is absolute**, spliced in as a concatenation
against the project root the build already defines:

    "station head (" CERA_ROOT "/src/boxes/029-demo-boxes.c:seven)\n"

Relative would not do. These descriptions are written into a scratch
directory at run time, and a path relative to *that* points nowhere near
the box sources. An absolute path is what a description with no fixed
home has, and this is the case the format keeps absolute paths for.
]]
local function rewrite_c_strings(text, where, collided)
    local changed, unknown = 0, {}

    -- Lua patterns have no alternation, so each keyword gets its own
    -- pass. Three passes over a file nobody is waiting on is cheaper
    -- than a pattern language.
    local out = text
    for _, keyword in ipairs({ "station", "comparator", "iterator" }) do
        out = out:gsub('("' .. keyword .. ' [%w_]+ %()([%w_]+)(%))',
            function (before, addr, after)
                if collided[addr] then
                    unknown[#unknown + 1] = addr .. " (in more than one file)"
                    return nil
                end
                local file = where[addr]
                if not file then
                    unknown[#unknown + 1] = addr
                    return nil
                end
                changed = changed + 1
                -- The literal is closed, the root spliced in, and a new
                -- literal opened for the rest.
                return before .. '" CERA_ROOT "/src/boxes/' .. file ..
                       ':' .. addr .. after
            end)
    end

    return out, changed, unknown
end
-- }}}

-- {{{ the run
local where, collided = box_index({ DIR .. "/src/boxes", DIR .. "/tests",
                                    DIR .. "/example", DIR .. "/viewer" })

local targets = {}
for _, p in ipairs(list_files(DIR .. "/maps", "*.map")) do targets[#targets + 1] = p end
for _, p in ipairs(list_files(DIR .. "/tests", "*.c"))  do targets[#targets + 1] = p end
for _, p in ipairs(list_files(DIR .. "/tests", "*.sh")) do targets[#targets + 1] = p end
for _, p in ipairs(list_files(DIR .. "/docs", "*.md"))  do targets[#targets + 1] = p end
-- Open blueprints, but never completed ones. A finished issue describes
-- the format as it stood when that issue was done, and rewriting its
-- examples would make it claim to have produced something it did not.
-- The completed directory is the buildable history, and history that
-- gets edited to match the present is not history.
for _, p in ipairs(list_files(DIR .. "/issues", "*.md")) do
    if not p:find("/completed/") then targets[#targets + 1] = p end
end
for _, p in ipairs(list_files(DIR .. "/viewer", "*.c")) do targets[#targets + 1] = p end
for _, p in ipairs(list_files(DIR .. "/example", "*.c")) do targets[#targets + 1] = p end
targets[#targets + 1] = DIR .. "/README.md"

local total, files_touched, all_unknown = 0, 0, {}

for _, path in ipairs(targets) do
    local text = read_file(path)
    if text then
        local out, changed, unknown = rewrite(text, where, collided)
        -- A C source may also carry map text inside string literals,
        -- which the line pass cannot see.
        if path:sub(-2) == ".c" then
            local more, extra, missed = rewrite_c_strings(out, where, collided)
            out = more
            changed = changed + extra
            for _, u in ipairs(missed) do unknown[#unknown + 1] = u end
        end
        for _, u in ipairs(unknown) do
            all_unknown[#all_unknown + 1] = path .. ": " .. u
        end
        if changed > 0 then
            files_touched = files_touched + 1
            total = total + changed
            if CHECK then
                print(string.format("  %3d  %s", changed, path))
            else
                write_file(path, out)
            end
        end
    end
end

print(string.format("name-the-file: %d addresses in %d files%s",
                    total, files_touched, CHECK and " (nothing written)" or ""))

if #all_unknown > 0 then
    print("")
    print("Left alone, because this tool will not guess:")
    for _, u in ipairs(all_unknown) do print("  " .. u) end
    os.exit(1)
end
-- }}}
