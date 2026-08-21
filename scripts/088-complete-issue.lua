--[[
088-complete-issue.lua — move a finished issue into the completed
record without leaving a single dead link behind.

What this is, in one paragraph for somebody who will never run it: an
issue file lives in issues/ while it is open and in issues/completed/
once it is built. Every other document that mentions it points at it
by a relative path, and so does every link inside it — so moving the
file by hand breaks links in two directions at once, silently, because
markdown has no compiler to complain. This tool performs the move
through git, so both the old path and the new one are in the history,
and then hands the whole project to the link repairer, which finds
every link that now leads nowhere and re-aims it.

**The repair is not this tool's job and deliberately so.** Working out
where a link should point is one problem with one right answer, and
089-repair-doc-links.lua answers it for the whole project rather than
for the two directions a move happens to break. An earlier version of
this file did the repointing itself, in three ordered substitutions
that had to avoid rewriting each other's output; it worked, and it was
a second implementation of something that had to exist anyway. The
first thing the two of them disagreed about was a batch of issues
moving together, where each one's links to the others were repointed
as though the others had stayed put.

It refuses to move a file that is not there, and refuses to overwrite
one already in completed/ — either means somebody has done this
already, and doing it twice would destroy the version carrying the
record.

Usage:
  luajit 088-complete-issue.lua <issue-name> [more...] [--apply] [DIR]

  <issue-name>  the file, with or without the .md — e.g. 210g or
                210g-one-way-to-build-a-station
  --apply       actually move and repair; without it nothing changes
                and the report says what would have happened
  DIR           project root, if not the one hard-coded below
]]

-- The project root, hard-coded so this works from any directory, and
-- overridable by an argument for the same reason.
local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- The repairer, named relative to this file so the pair travels
-- together.
local REPAIRER = "scripts/089-repair-doc-links.lua"

-- {{{ local function shell_quote()
local function shell_quote(s)
    return "'" .. s:gsub("'", "'\\''") .. "'"
end
-- }}}

-- {{{ local function exists()
local function exists(path)
    local f = io.open(path, "r")
    if f then f:close() return true end
    return false
end
-- }}}

-- {{{ local function resolve_issue()
--[[
Turn what the caller typed into the one file it means. A bare number
like `210g` is enough when exactly one open issue starts with it; two
matches is an ambiguity the caller has to settle, because guessing
which issue somebody meant to finish is the one mistake this tool must
never make.
]]
local function resolve_issue(name)
    name = name:gsub("%.md$", "")
    if exists(DIR .. "/issues/" .. name .. ".md") then return name .. ".md" end

    local matches = {}
    local p = io.popen("ls " .. shell_quote(DIR .. "/issues") .. " 2>/dev/null")
    if p then
        for line in p:lines() do
            if line:match("%.md$") and line:sub(1, #name) == name then
                matches[#matches + 1] = line
            end
        end
        p:close()
    end
    if #matches == 1 then return matches[1] end
    if #matches == 0 then
        error("no open issue starts with '" .. name .. "'")
    end
    error("'" .. name .. "' names " .. #matches ..
          " open issues; say which one")
end
-- }}}

-- {{{ main
local names, apply = {}, false
for _, arg in ipairs({ ... }) do
    if arg == "--apply" then
        apply = true
    elseif arg:sub(1, 1) == "/" then
        DIR = arg:gsub("/$", "")
    else
        names[#names + 1] = arg
    end
end

if #names == 0 then
    io.stderr:write("usage: luajit 088-complete-issue.lua <issue> " ..
                    "[more...] [--apply] [DIR]\n")
    os.exit(1)
end

local files = {}
for _, name in ipairs(names) do
    local file = resolve_issue(name)
    if exists(DIR .. "/issues/completed/" .. file) then
        error("issues/completed/" .. file .. " already exists — moving " ..
              "over it would destroy the record it holds")
    end
    files[#files + 1] = file
end

print((apply and "Completing " or "Would complete ") .. #files ..
      " issue" .. (#files == 1 and "" or "s") .. ":")
for _, file in ipairs(files) do
    print("  issues/" .. file .. "  ->  issues/completed/" .. file)
end
print("")

if not apply then
    print("Nothing was moved. Add --apply to do it, which also runs")
    print("the link repairer over the whole project afterwards.")
    os.exit(0)
end

-- Through git rather than around it, so the history holds both paths
-- rather than a delete and an unrelated add.
for _, file in ipairs(files) do
    local rc = os.execute("git -C " .. shell_quote(DIR) .. " mv " ..
                          shell_quote("issues/" .. file) .. " " ..
                          shell_quote("issues/completed/" .. file))
    if rc ~= 0 and rc ~= true then
        error("git could not move issues/" .. file)
    end
end

print("Moved. Repairing every link that now leads nowhere:")
print("")
os.execute("luajit " .. shell_quote(DIR .. "/" .. REPAIRER) ..
           " --apply " .. shell_quote(DIR))
-- }}}
