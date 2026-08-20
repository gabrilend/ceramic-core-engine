--[[
079-rename-identifiers.lua — change what things are called, everywhere
at once, without the renaming stepping on itself.

What this is, in one paragraph for somebody who will never run it:
renaming a word that appears six hundred times across a codebase is
not six hundred small edits, because two of those renames can collide
— if A must become B while some existing B must become C, doing them
one after another turns the original A into C. This tool takes a list
of old-name-to-new-name pairs, substitutes every old name for a
private placeholder first, and only then substitutes the placeholders
for the new names. Nothing is ever renamed twice, and the order the
pairs are written in cannot change the result.

How it decides what to touch: whole identifiers only. `port` inside
`report` is not the word `port`, and a tool that thought otherwise
would quietly turn a project's reporting into something else. The
match requires a non-identifier character on both sides.

It reports what it did per name and per file, and refuses to write
anything at all in dry-run mode, which is how it should always be run
first.

Usage:
  luajit 079-rename-identifiers.lua <mapping-file> [--apply] [DIR]

  <mapping-file>  lines of `old new`, blank lines and #comments ignored
  --apply         actually write; without it, nothing is modified
  DIR             project root, if not the one hard-coded below
]]

-- The project root, hard-coded so this works from any directory, and
-- overridable by an argument for the same reason.
local DIR = "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- Where a rename is allowed to reach by default. Prose counts: a
-- document using the old word is as wrong as source using it, and
-- worse, because a reader believes documents.
--
-- Narrow it with --roots when code and prose need different mappings,
-- which is usual. A field renamed from `ports` to `out_ports` is right
-- in a struct and absurd in a sentence, so the two passes are run with
-- two mappings rather than one pass with a compromise.
local ROOTS = { "src", "libs", "tests", "docs", "issues", "scripts", "notes" }
local EXTENSIONS = { "%.c$", "%.h$", "%.lua$", "%.md$", "%.map$" }

-- Never rewrite the generated tree or the published site: one is
-- rebuilt from its sources by the build, the other by `make html`.
-- Rewriting a derived file makes it disagree with what derives it.
local SKIP = { "src/generated/", "docs/HTML/", "/tmp/", "llm%-transcripts/" }

-- {{{ local function read_mapping()
local function read_mapping(path)
    local pairs_out, seen = {}, {}
    local f = assert(io.open(path, "r"), "cannot read mapping: " .. path)
    local lineno = 0
    for line in f:lines() do
        lineno = lineno + 1
        local body = line:gsub("#.*$", ""):gsub("^%s+", ""):gsub("%s+$", "")
        if body ~= "" then
            local old, new = body:match("^(%S+)%s+(%S+)$")
            if not old then
                error(("%s:%d: want `old new`, got: %s"):format(path, lineno, body))
            end
            -- A name appearing twice on the left means the mapping
            -- disagrees with itself, and the tool must not pick one.
            if seen[old] then
                error(("%s:%d: %s is renamed twice"):format(path, lineno, old))
            end
            seen[old] = true
            pairs_out[#pairs_out + 1] = { old = old, new = new }
        end
    end
    f:close()
    return pairs_out
end
-- }}}

-- {{{ local function wanted(path)
local function wanted(path)
    for _, skip in ipairs(SKIP) do
        if path:find(skip) then return false end
    end
    for _, ext in ipairs(EXTENSIONS) do
        if path:find(ext) then return true end
    end
    return false
end
-- }}}

-- {{{ local function collect(root)
local function collect(root, out)
    -- find rather than a Lua directory walk: LuaJIT has no directory
    -- reading in its standard library, and shelling out to one tool is
    -- less machinery than binding to another.
    local p = io.popen(("find %q -type f 2>/dev/null"):format(root))
    for path in p:lines() do
        if wanted(path) then out[#out + 1] = path end
    end
    p:close()
    return out
end
-- }}}

-- {{{ local function escape_pattern()
local function escape_pattern(s)
    return (s:gsub("[%^%$%(%)%%%.%[%]%*%+%-%?]", "%%%0"))
end
-- }}}

-- {{{ local function rename_text()
--
-- The two passes are the whole point. Pass one replaces every old name
-- with a placeholder that cannot occur in real source; pass two
-- replaces placeholders with new names. Because no new name is ever
-- visible while old names are still being matched, a pair whose new
-- name equals another pair's old name is harmless.
--
local function rename_text(text, mapping, counts)
    for i, m in ipairs(mapping) do
        local mark = ("\1RN%d\2"):format(i)
        local n
        text, n = text:gsub("%f[%w_]" .. escape_pattern(m.old) .. "%f[^%w_]", mark)
        counts[m.old] = (counts[m.old] or 0) + n
    end
    for i, m in ipairs(mapping) do
        local mark = ("\1RN%d\2"):format(i)
        text = text:gsub(mark, (m.new:gsub("%%", "%%%%")))
    end
    return text
end
-- }}}

-- {{{ local function main()
local function main()
    local mapping_path, apply, list_path = nil, false, nil
    for _, a in ipairs(arg) do
        local roots = a:match("^%-%-roots=(.+)$")
        local files = a:match("^%-%-files=(.+)$")
        if a == "--apply" then apply = true
        elseif roots then
            ROOTS = {}
            for r in roots:gmatch("[^,]+") do ROOTS[#ROOTS + 1] = r end
        elseif files then list_path = files
        elseif not mapping_path then mapping_path = a
        else DIR = a end
    end
    if not mapping_path then
        error("usage: 079-rename-identifiers.lua <mapping-file> [--apply] [DIR]")
    end

    local mapping = read_mapping(mapping_path)
    local files = {}
    if list_path then
        -- An explicit list, for when a corpus is written in more than
        -- one vocabulary and the pairs that are right for half of it
        -- are wrong for the other half. Walking a root would apply one
        -- mapping to both halves, which corrupts the half that was
        -- already correct — and does so invisibly, because the result
        -- is well-formed prose that says the wrong thing.
        local f = assert(io.open(list_path, "r"), "cannot read list: " .. list_path)
        for line in f:lines() do
            local p = line:gsub("^%s+", ""):gsub("%s+$", "")
            if p ~= "" and not p:find("^#") then
                files[#files + 1] = p:find("^/") and p or (DIR .. "/" .. p)
            end
        end
        f:close()
    else
        for _, r in ipairs(ROOTS) do collect(DIR .. "/" .. r, files) end
    end

    local counts, touched, total = {}, 0, 0
    for _, path in ipairs(files) do
        local f = assert(io.open(path, "r"))
        local before = f:read("*a")
        f:close()

        local per_file = {}
        local after = rename_text(before, mapping, per_file)

        local n = 0
        for _, c in pairs(per_file) do n = n + c end
        for k, c in pairs(per_file) do counts[k] = (counts[k] or 0) + c end

        if n > 0 then
            touched = touched + 1
            total = total + n
            print(("%5d  %s"):format(n, path:sub(#DIR + 2)))
            if apply then
                f = assert(io.open(path, "w"))
                f:write(after)
                f:close()
            end
        end
    end

    print("")
    for _, m in ipairs(mapping) do
        -- A pair matching nothing is reported rather than passed over:
        -- it usually means the name was misspelled in the mapping, and
        -- a silent zero is how a rename half-happens.
        local c = counts[m.old] or 0
        print(("%5d  %s -> %s%s"):format(c, m.old, m.new,
              c == 0 and "   <-- MATCHED NOTHING" or ""))
    end
    print(("\n%d replacements across %d files%s")
          :format(total, touched, apply and "" or "  (dry run — nothing written)"))
end
-- }}}

main()
