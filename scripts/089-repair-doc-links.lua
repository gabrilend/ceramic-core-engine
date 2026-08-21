--[[
089-repair-doc-links.lua — make every link in the written record lead
somewhere.

What this is, in one paragraph for somebody who will never run it: the
documents in this project point at each other constantly, and a link
is a *relative path*, which means it encodes where the file was
standing when somebody wrote it. Move the file and the link is wrong,
and nothing complains — markdown has no compiler. This tool reads
every link in every document, checks whether the file it names is
actually there, and where it is not, finds the one file in the project
with that name and rewrites the path to reach it from where the link
lives now.

**It resolves by name, not by guessing.** Every document filename in
this project is unique, which is what the numbered-filename rule buys:
`210b-the-port-record.md` names exactly one file wherever it has been
moved to. So a broken link is not ambiguous — it says which document
it wanted, and only the route to it went stale.

It refuses to touch two kinds of link, and says so instead:

  - one whose name matches **no** file, which means the document was
    renamed or never written, and a tool that picked a near-match
    would quietly point a reader at the wrong document.
  - one whose name matches **more than one** file, which cannot happen
    while the numbering rule holds and is worth hearing about
    immediately if it ever does.

Anchors (`#section`) and external links are left alone.

Usage:
  luajit 089-repair-doc-links.lua [--apply] [DIR]

  --apply   actually write; without it nothing changes and the report
            says what would have happened
  DIR       project root, if not the one hard-coded below
]]

-- The project root, hard-coded so this works from any directory, and
-- overridable by an argument for the same reason.
local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- Never rewrite the generated site: `make html` builds it from these
-- very files, so a repair there would be undone and would meanwhile
-- disagree with its own source.
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

-- {{{ local function all_documents()
--[[
Every markdown file in the project, as paths relative to the root.
Found with `find` because the shell already does this well.
]]
local function all_documents()
    local out = {}
    local p = assert(io.popen("find " .. shell_quote(DIR) ..
                              " -type f -name '*.md'"))
    for line in p:lines() do
        local rel = line:sub(#DIR + 2)
        if not skipped(rel) then out[#out + 1] = rel end
    end
    p:close()
    table.sort(out)
    return out
end
-- }}}

-- {{{ local function index_by_name()
--[[
Which path each filename belongs to. A name seen twice is recorded as
ambiguous rather than as whichever came last, because silently
choosing one of two documents is the single worst thing this tool
could do.
]]
local function index_by_name(documents)
    local where = {}
    for _, rel in ipairs(documents) do
        local name = rel:match("([^/]+)$")
        if where[name] == nil then
            where[name] = rel
        else
            where[name] = false      -- seen more than once
        end
    end
    return where
end
-- }}}

-- {{{ local function normalise()
--[[
Collapse `a/b/../c` into `a/c`, so that a link's target can be
compared against a real path. A leading `..` that escapes the project
root is kept as-is; it will simply fail to match anything, which is
the correct outcome for a link pointing outside the record.
]]
local function normalise(path)
    local parts = {}
    for piece in path:gmatch("[^/]+") do
        if piece == ".." and #parts > 0 and parts[#parts] ~= ".." then
            parts[#parts] = nil
        elseif piece ~= "." then
            parts[#parts + 1] = piece
        end
    end
    return table.concat(parts, "/")
end
-- }}}

-- {{{ local function route_between()
--[[
The relative path from the directory holding one document to another
document. Climb out of the shared prefix, then descend.
]]
local function route_between(from_rel, to_rel)
    local from_dir = {}
    for piece in from_rel:gmatch("[^/]+") do from_dir[#from_dir + 1] = piece end
    from_dir[#from_dir] = nil                  -- drop the filename

    local to = {}
    for piece in to_rel:gmatch("[^/]+") do to[#to + 1] = piece end

    local shared = 0
    while shared < #from_dir and shared < #to - 1
          and from_dir[shared + 1] == to[shared + 1] do
        shared = shared + 1
    end

    local hops = {}
    for _ = shared + 1, #from_dir do hops[#hops + 1] = ".." end
    for i = shared + 1, #to do hops[#hops + 1] = to[i] end
    return table.concat(hops, "/")
end
-- }}}

-- {{{ main
local apply = false
for _, arg in ipairs({ ... }) do
    if arg == "--apply" then apply = true
    elseif arg:sub(1, 1) == "/" then DIR = arg:gsub("/$", "") end
end

local documents = all_documents()
local where = index_by_name(documents)

local repaired, unfindable, ambiguous, files_touched = 0, 0, 0, 0

for _, rel in ipairs(documents) do
    local dir = rel:match("^(.*)/[^/]+$") or ""
    local f = assert(io.open(DIR .. "/" .. rel, "r"))
    local text = f:read("*a")
    f:close()

    local changes = {}
    for target in text:gmatch("%]%(([^)#%s]+%.md)%)") do
        if not target:find("^https?:") and not changes[target] then
            local full = normalise((dir == "" and "" or dir .. "/") .. target)
            local exists = io.open(DIR .. "/" .. full, "r")
            if exists then
                exists:close()
            else
                local name = target:match("([^/]+)$")
                local home = where[name]
                if home == nil then
                    unfindable = unfindable + 1
                    print(string.format("  ?  %s -> %s (no such document " ..
                                        "anywhere; renamed, or never written)",
                                        rel, target))
                elseif home == false then
                    ambiguous = ambiguous + 1
                    print(string.format("  !  %s -> %s (that name belongs to " ..
                                        "more than one file)", rel, target))
                else
                    changes[target] = route_between(rel, home)
                end
            end
        end
    end

    local n = 0
    for target, fixed in pairs(changes) do
        local pattern = "%]%(" .. target:gsub("([%-%.%+%[%]%(%)%$%^%%%?%*])",
                                              "%%%1") .. "%)"
        local count
        text, count = text:gsub(pattern, "](" .. fixed .. ")")
        n = n + count
        repaired = repaired + count
    end

    if n > 0 then
        files_touched = files_touched + 1
        print(string.format("  %-56s %d link%s", rel, n, n == 1 and "" or "s"))
        if apply then
            local out = assert(io.open(DIR .. "/" .. rel, "w"))
            out:write(text)
            out:close()
        end
    end
end

print("")
print(string.format("%s %d link%s across %d file%s.",
                    apply and "Repaired" or "Would repair",
                    repaired, repaired == 1 and "" or "s",
                    files_touched, files_touched == 1 and "" or "s"))
if unfindable > 0 then
    print(string.format("%d link%s named a document that is not in the " ..
                        "project; left alone.", unfindable,
                        unfindable == 1 and "" or "s"))
end
if ambiguous > 0 then
    print(string.format("%d link%s were ambiguous; left alone. The " ..
                        "numbered-filename rule is supposed to make this " ..
                        "impossible.", ambiguous, ambiguous == 1 and "" or "s"))
end
if not apply then print("Nothing was written. Add --apply to do it.") end
-- }}}
