-- 113-refold.lua — one fold per definition, so the engine reads like a header.
--
-- What this is: the tool that maintains the vim fold markers in
-- src/cera.c. Collapsed, the file is a list of what it defines, in
-- order; opening one fold expands that definition and its comment and
-- nothing else. Section banners are left outside every fold so they
-- stay visible as headings when everything is shut.
--
-- How it does it, in general terms: throw away every marker that is
-- there and put fresh ones in. A definition runs from the top of the
-- comment block above it to the brace that closes it in column one; the
-- fold is named for what the definition declares. Because it discards
-- rather than repairs, running it twice does the same thing as running
-- it once, and a fold that has drifted out of step with the code cannot
-- survive a run.
--
-- Usage: luajit 113-refold.lua [project-dir] [--check]
--   --check reports what it would do and writes nothing.

-- {{{ configuration
local DIR = nil
local CHECK = false
for _, a in ipairs(arg) do
    if a == "--check" then CHECK = true else DIR = a end
end
DIR = DIR or "/mnt/mtwo/programming/ai-playground/minimal-soramech"
local FILE = DIR .. "/src/cera.c"
-- }}}

-- {{{ local function read_lines(path)
local function read_lines(path)
    local f = assert(io.open(path, "r"), "cannot read " .. path)
    local t = {}
    for line in f:lines() do t[#t + 1] = line end
    f:close()
    return t
end
-- }}}

-- {{{ local function is_comment(line)
local function is_comment(line)
    return line:match("^%s*/%*") or line:match("^%s*%*") or line:match("^%s*//")
end
-- }}}

-- {{{ local function is_banner(line)
-- A section banner opens with a rule of equals signs. Banners are the
-- headings of the eleven sections and stay outside every fold.
local function is_banner(line)
    return line:match("^/%* ==========") ~= nil
end
-- }}}

-- {{{ local function names_in(lines, first, last)
-- What a definition declares, for the fold's label. Takes every
-- identifier that is followed by an open paren at the start of a line,
-- plus the name of a file-scope variable when there is no function.
local function names_in(lines, first, last)
    local found, seen = {}, {}
    for i = first, last do
        local l = lines[i]
        if not is_comment(l) and l:sub(1, 1):match("[%a_]") then
            local n = l:match("^[%w_%s%*]-([%a_][%w_]*)%s*%(")
            if n and not seen[n] and n ~= "if" and n ~= "for" and n ~= "while"
               and n ~= "switch" and n ~= "return" and n ~= "sizeof" then
                seen[n] = true
                found[#found + 1] = n .. "()"
            end
        end
    end
    if #found > 0 then return table.concat(found, " / ") end

    -- A type rather than a call. The name a reader wants is the one the
    -- typedef ends with, so look for the closing line first and fall
    -- back to the tag the definition opens with.
    for i = last, first, -1 do
        local n = lines[i]:match("^}%s*([%a_][%w_]*)%s*;")
        if n then return "type " .. n end
    end
    for i = first, last do
        local l = lines[i]
        local n = l:match("^typedef%s+struct%s+([%a_][%w_]*)")
                    or l:match("^struct%s+([%a_][%w_]*)")
                    or l:match("^union%s+([%a_][%w_]*)")
                    or l:match("^enum%s+([%a_][%w_]*)")
        if n then return "struct " .. n end
    end

    -- A file-scope variable.
    for i = first, last do
        local l = lines[i]
        if not is_comment(l) and l:sub(1, 1):match("[%a_]") then
            local n = l:match("([%a_][%w_]*)%s*%[") or l:match("([%a_][%w_]*)%s*=")
                        or l:match("([%a_][%w_]*)%s*;")
            if n then return n end
        end
    end
    return "..." 
end
-- }}}

local src = read_lines(FILE)

-- {{{ strip every existing marker
-- A marker line is one whose entire content is a fold comment; a marker
-- inside a labelled comment keeps its comment and loses the braces.
local body = {}
for _, l in ipairs(src) do
    if l:match("^%s*/%*%s*{{{.*%*/%s*$") or l:match("^%s*/%*%s*}}}%s*%*/%s*$") then
        -- drop the line entirely
    else
        l = l:gsub("{{{", ""):gsub("}}}", "")
        body[#body + 1] = l
    end
end
-- }}}

-- {{{ find the definitions
-- A unit starts at a line in column one that is neither comment,
-- directive nor blank. It ends at the brace in column one that closes
-- it, or at the first line ending in a semicolon when it has no body.
local units = {}
local i = 1
while i <= #body do
    local l = body[i]
    if l ~= "" and not is_comment(l) and not l:match("^#") and l:sub(1, 1):match("[%a_]") then
        local first, last, has_body = i, i, false
        for j = i, #body do
            local s = body[j]
            if s:find("{", 1, true) then has_body = true end
            if has_body then
                if s:match("^}") then last = j break end
            else
                if s:match(";%s*$") then last = j break end
            end
            last = j
        end
        -- walk back over the comment block directly above
        local top = first
        while top > 1 and is_comment(body[top - 1]) and not is_banner(body[top - 1]) do
            top = top - 1
        end
        units[#units + 1] = { top = top, first = first, last = last }
        i = last + 1
    else
        i = i + 1
    end
end
-- }}}

-- {{{ write the folds back in
local out, at, u = {}, 1, 1
while at <= #body do
    if u <= #units and units[u].top == at then
        local unit = units[u]
        out[#out + 1] = "/* {{{ " .. names_in(body, unit.first, unit.last) .. " */"
        for k = unit.top, unit.last do out[#out + 1] = body[k] end
        out[#out + 1] = "/* }}} */"
        at = unit.last + 1
        u = u + 1
    else
        out[#out + 1] = body[at]
        at = at + 1
    end
end
-- }}}

print(string.format("%d definitions folded in %s", #units, FILE))
if CHECK then
    print("--check: nothing written")
else
    local f = assert(io.open(FILE, "w"))
    f:write(table.concat(out, "\n"))
    f:write("\n")
    f:close()
    print(string.format("wrote %d lines (was %d)", #out, #src))
end
