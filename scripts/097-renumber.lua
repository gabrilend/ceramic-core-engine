--[[
097-renumber.lua — change where files sit in the reading order without
leaving a single stale reference behind.

What this is, in one paragraph for somebody who will never run it:
every file in this project carries an index, and the indices run
across the whole tree rather than per directory, so the project sorts
into one sequence somebody can read from end to end. Changing that
sequence means renaming files — and a filename in this project is
referenced from source includes, from companion documents, from the
issues, and from the documentation site, so renaming one by hand
leaves stragglers, and a straggler here is a broken build or a dead
link rather than a cosmetic flaw. This tool takes a desired order,
renames everything, and rewrites every reference in one pass.

**The order is an input, not a computation.** Deciding what should be
read after what is a judgement about how the project is best
explained; nothing here sorts, infers, or groups. It is handed a list
and it obeys it. That separation is deliberate: the mechanical half
should be boring enough to trust, so the interesting half can be
argued about on its own.

**A companion follows its source.** `066-gentext.c.info.md` is not a
step in the reading order; it is the same step as `066-gentext.c`, so
it takes whatever number that file takes and is never listed
separately. The same is true of a demo runner's letter suffix — `037a`
runs `037` — which is why suffixes are preserved rather than
renumbered.

**Renaming is staged behind placeholders**, for the reason
079-rename-identifiers.lua stages its renames: if 019 is to become 021
while some existing 021 becomes 019, doing them one after another
turns the first into the second. Every old name becomes a private
placeholder, and only then does every placeholder become a new name.
Nothing is ever renamed twice and the order of the list cannot change
the result.

Usage:
  luajit 097-renumber.lua <order-file> [--apply] [DIR]

  <order-file>  one path per line, relative to the project root, in
                the order they should be read. Blank lines and
                #comments ignored. Every indexed file must appear
                exactly once; anything missing or listed twice is
                refused rather than guessed at.
  --apply       actually rename and rewrite; without it nothing
                changes and the report says what would have happened
  DIR           project root, if not the one hard-coded below
]]

-- The project root, hard-coded so this works from any directory, and
-- overridable by an argument for the same reason.
local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- Where indexed files live. The issues are deliberately absent: they
-- carry a phase-and-sequence number that means something else
-- entirely, and renumbering them by reading order would destroy it.
local ROOTS = { "src", "libs", "tests", "scripts", "docs", "maps", "notes" }

-- Where a reference to a filename may appear. Everything, in other
-- words, except the two derived trees and the transcripts — the site
-- is rebuilt from its sources and the transcripts are a record of what
-- was said rather than a description of what is.
local SKIP = { "docs/HTML/", "src/generated/", "llm%-transcripts/", "%.git/" }

-- {{{ local function shell_quote()
local function shell_quote(s)
    return "'" .. s:gsub("'", "'\\''") .. "'"
end
-- }}}

-- {{{ local function skipped()
local function skipped(path)
    for _, pattern in ipairs(SKIP) do
        if path:find(pattern) then return true end
    end
    return false
end
-- }}}

-- {{{ local function find_files()
--[[
Every file under the given roots, as paths relative to the project
root. Used twice: once to find what carries an index, and once to find
everything that might mention one.
]]
local function find_files(roots)
    local out = {}
    for _, root in ipairs(roots) do
        local p = io.popen("find " .. shell_quote(DIR .. "/" .. root) ..
                           " -type f 2>/dev/null")
        if p then
            for line in p:lines() do
                local rel = line:sub(#DIR + 2)
                if not skipped(rel) then out[#out + 1] = rel end
            end
            p:close()
        end
    end
    table.sort(out)
    return out
end
-- }}}

-- {{{ local function split_index()
--[[
An indexed filename in its three parts: the number, any letter suffix,
and the rest. Returns nil for a name that carries no index.

  "019-station.c"        -> 19, "",  "station.c"
  "037a-run-demo.sh"     -> 37, "a", "run-demo.sh"
]]
local function split_index(name)
    local number, suffix, rest = name:match("^(%d%d%d)(%a?)%-(.+)$")
    if not number then return nil end
    return tonumber(number), suffix, rest
end
-- }}}

-- {{{ local function is_companion()
--[[
A companion document takes its source's number and is never listed
separately, because it is not a step in the reading order — it is the
same step as the file it describes.
]]
local function is_companion(name)
    return name:match("%.info%.md$") ~= nil
end
-- }}}

-- {{{ local function read_order()
local function read_order(path)
    local wanted, seen = {}, {}
    local f = assert(io.open(path, "r"), "cannot read order file: " .. path)
    local lineno = 0
    for line in f:lines() do
        lineno = lineno + 1
        local body = line:gsub("#.*$", ""):gsub("^%s+", ""):gsub("%s+$", "")
        if body ~= "" then
            if seen[body] then
                error(path .. ":" .. lineno .. ": '" .. body ..
                      "' is listed twice; a file has one place in the order")
            end
            seen[body] = true
            wanted[#wanted + 1] = body
        end
    end
    f:close()
    return wanted
end
-- }}}

-- {{{ local function check_stragglers()
--[[
**Every indexed filename mentioned anywhere, that is not there.**

This is the check that makes renaming safe to do at all. A filename in
this project is referenced from source includes, companion documents,
issues, and prose, so a rename that misses one leaves a broken build
or a dead link — and both of those are found late and blamed on
something else.

It reads every text file, picks out anything shaped like an indexed
filename, and asks whether that file exists. Anything that does not is
a straggler: either a rename that missed a mention, or a file that was
deleted while something kept pointing at it.

Safe to run at any time, and worth running after any rename whoever
did it did by hand.
]]
local function check_stragglers()
    -- every file the project has, by basename
    local present = {}
    --[[
    The issues are included here even though they are not renumbered —
    their numbers mean a phase and a sequence, not a reading order.
    What matters for this check is only whether a name somebody wrote
    down is a file that exists.
    ]]
    local everything = { "src", "libs", "tests", "scripts", "docs", "maps",
                         "notes", "issues" }
    for _, rel in ipairs(find_files(everything)) do
        present[rel:match("([^/]+)$")] = true
    end

    local documents = find_files({ "src", "libs", "tests", "scripts", "docs",
                                   "maps", "notes", "issues" })
    local found = 0
    for _, rel in ipairs(documents) do
        local f = io.open(DIR .. "/" .. rel, "r")
        if f then
            local text = f:read("*a")
            f:close()
            local said = {}
            for name in text:gmatch("(%d%d%d%a?%-[%w%-]+%.%a[%w%.]*)") do
                --[[
                A name captured at the end of a sentence brings the
                full stop with it, and a `.html` name belongs to the
                generated site rather than to the tree. Neither is a
                straggler, and reporting them would bury the ones that
                are.
                ]]
                name = name:gsub("%.$", "")
                if not name:match("%.html$")
                   and not present[name] and not said[name] then
                    said[name] = true
                    found = found + 1
                    print(string.format(
                        "  %-46s mentions %s, which is not in the tree",
                        rel, name))
                end
            end
        end
    end

    print("")
    if found == 0 then
        print("Every indexed filename mentioned in the project is a file " ..
              "that exists.")
    else
        print(found .. " mention" .. (found == 1 and "" or "s") ..
              " of a file that is not there.")
    end
    return found
end
-- }}}

-- {{{ main
local order_path, apply, check_only = nil, false, false
for _, arg in ipairs({ ... }) do
    if arg == "--apply" then apply = true
    elseif arg == "--check" then check_only = true
    elseif arg:sub(1, 1) == "/" and arg:find("%.txt$") == nil then DIR = arg:gsub("/$", "")
    else order_path = arg end
end

if check_only then
    os.exit(check_stragglers() == 0 and 0 or 1)
end

if not order_path then
    io.stderr:write("usage: luajit 097-renumber.lua <order-file> " ..
                    "[--apply] [DIR]\n")
    io.stderr:write("       luajit 097-renumber.lua --check [DIR]\n")
    os.exit(1)
end

-- What carries an index today, and where each one lives.
local indexed, home = {}, {}
for _, rel in ipairs(find_files(ROOTS)) do
    local name = rel:match("([^/]+)$")
    if split_index(name) and not is_companion(name) then
        indexed[#indexed + 1] = rel
        home[rel] = name
    end
end

local wanted = read_order(order_path)

-- Every indexed file must appear exactly once. Anything missing would
-- silently keep its old number and land wherever that put it, which is
-- the opposite of an order somebody decided.
local listed = {}
for _, rel in ipairs(wanted) do listed[rel] = true end
local missing, unknown = {}, {}
for _, rel in ipairs(indexed) do
    if not listed[rel] then missing[#missing + 1] = rel end
end
local known = {}
for _, rel in ipairs(indexed) do known[rel] = true end
for _, rel in ipairs(wanted) do
    if not known[rel] then unknown[#unknown + 1] = rel end
end

if #missing > 0 or #unknown > 0 then
    for _, rel in ipairs(missing) do
        io.stderr:write("not in the order file: " .. rel .. "\n")
    end
    for _, rel in ipairs(unknown) do
        io.stderr:write("in the order file but not in the tree: " ..
                        rel .. "\n")
    end
    io.stderr:write("\nEvery indexed file must appear exactly once. " ..
                    "Nothing was changed.\n")
    os.exit(1)
end

-- The renames, as old basename to new basename. Companions follow.
local renames, in_order = {}, {}
for position, rel in ipairs(wanted) do
    local dir = rel:match("^(.*)/[^/]+$")
    local name = rel:match("([^/]+)$")
    local _, suffix, rest = split_index(name)
    local fresh = string.format("%03d%s-%s", position - 1, suffix, rest)
    if fresh ~= name then
        renames[name] = fresh
        in_order[#in_order + 1] = { dir = dir, from = name, to = fresh }
        -- The companion, if there is one, takes the same number.
        local companion = name .. ".info.md"
        local probe = io.open(DIR .. "/" .. dir .. "/" .. companion, "r")
        if probe then
            probe:close()
            renames[companion] = fresh .. ".info.md"
            in_order[#in_order + 1] = { dir = dir, from = companion,
                                        to = fresh .. ".info.md" }
        end
    end
end

print((apply and "Renumbering " or "Would renumber ") .. #in_order ..
      " file" .. (#in_order == 1 and "" or "s") ..
      " out of " .. #indexed .. " indexed:")
print("")
for _, r in ipairs(in_order) do
    print(string.format("  %s/%s  ->  %s", r.dir, r.from, r.to))
end
print("")

if #in_order == 0 then
    print("The tree is already in that order. Nothing to do.")
    os.exit(0)
end

-- Every reference, staged behind placeholders so that a name becoming
-- another name cannot be renamed twice.
local documents = find_files({ "src", "libs", "tests", "scripts", "docs",
                               "maps", "notes", "issues" })
local touched, total = 0, 0
for _, rel in ipairs(documents) do
    local f = io.open(DIR .. "/" .. rel, "r")
    if f then
        local text = f:read("*a")
        f:close()
        local before = text
        local n = 0
        local marks = {}
        local index = 0
        for from, to in pairs(renames) do
            index = index + 1
            local mark = "\1" .. index .. "\2"
            marks[mark] = to
            local pattern = from:gsub("([%-%.%+%[%]%(%)%$%^%%%?%*])", "%%%1")
            local count
            text, count = text:gsub(pattern, mark)
            n = n + count
        end
        for mark, to in pairs(marks) do
            text = text:gsub(mark, to)
        end
        if text ~= before then
            touched = touched + 1
            total = total + n
            print(string.format("  %-58s %d mention%s", rel, n,
                                n == 1 and "" or "s"))
            if apply then
                local out = assert(io.open(DIR .. "/" .. rel, "w"))
                out:write(text)
                out:close()
            end
        end
    end
end

if apply then
    -- Through git, so both names are in the history, and via a
    -- temporary name so a swap cannot collide with itself.
    for _, r in ipairs(in_order) do
        os.execute("git -C " .. shell_quote(DIR) .. " mv " ..
                   shell_quote(r.dir .. "/" .. r.from) .. " " ..
                   shell_quote(r.dir .. "/." .. r.to .. ".renumbering"))
    end
    for _, r in ipairs(in_order) do
        os.execute("git -C " .. shell_quote(DIR) .. " mv " ..
                   shell_quote(r.dir .. "/." .. r.to .. ".renumbering") .. " " ..
                   shell_quote(r.dir .. "/" .. r.to))
    end
    local counter = assert(io.open(DIR .. "/.file-index-counter", "w"))
    counter:write(string.format("%03d\n", #wanted - 1))
    counter:close()
end

print("")
print(string.format("%s %d mention%s across %d file%s.",
                    apply and "Rewrote" or "Would rewrite",
                    total, total == 1 and "" or "s",
                    touched, touched == 1 and "" or "s"))
if not apply then
    print("Nothing was renamed or written. Add --apply to do it,")
    print("then run the test suite and the link repairer.")
end
-- }}}
