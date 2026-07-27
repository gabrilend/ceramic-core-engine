#!/usr/bin/env luajit
--[[
028-generate.lua — the build-time generator: box sources in, registry out.

What this is: the program that makes "adding a box" mean "writing a
function". It reads the C files designated as box sources, finds every
function, struct, and compare function in them, and writes one C file
containing a shim per box (the per-box call site), a registry of names
to shims and full type information, a field table per struct, and a
three-way compare per comparable type.

How it does it, in general terms: it does not understand C — it
recognizes exactly three top-level shapes in files whose whole purpose
is to hold them, and stops loudly on anything else. Every size and
offset in its output is a sizeof or offsetof expression, so the
compiler computes the numbers and this script never guesses about
padding or alignment. Output is written to a temporary file and moved
into place only on success, so a failing run can never leave a
half-registry for the build to compile against.

Usage:
  luajit 028-generate.lua <output.c> <box-source.c> [more...]
  luajit 028-generate.lua --describe <box-source.c> [more...]
    (prints what the parser saw, for diagnosing build problems)

Box source ground rules, enforced here, documented once:
  - value types are `typedef struct { ... } name;` — plain struct
    declarations and inline nested struct bodies are refused; define
    nested structs separately and refer to them by name.
  - every non-static function is a box; static functions are private
    helpers; a function named `type__compare` is that type's
    three-way comparison and never becomes a box.
  - parameters and returns may be primitives, typedef'd structs, or
    `const char *` (a borrowed string — the bytes live wherever the
    pointer points, which for statics is the statics table).
]]

-- The project root, derived from this script's own location so the
-- generator runs from any directory; overridable via SORAMECH_DIR.
local DIR = os.getenv("SORAMECH_DIR")
if not DIR then
    local self = arg[0]
    DIR = self:match("^(.*)/scripts/[^/]+$") or "."
end

-- {{{ local function fail()
-- Every parse problem stops the build and names file and line: a
-- parser that skips what it does not understand produces a registry
-- with a hole in it, which surfaces much later pointing at the map
-- instead of the real cause.
local function fail(file, line, message)
    io.stderr:write(("generator: %s:%d: %s\n"):format(file, line or 0, message))
    os.exit(1)
end
-- }}}

-- {{{ local function read_file()
local function read_file(path)
    local f = io.open(path, "r")
    if not f then
        io.stderr:write("generator: cannot open " .. path .. "\n")
        os.exit(1)
    end
    local text = f:read("*a")
    f:close()
    return text
end
-- }}}

-- {{{ local function blank_noise()
-- Replace comments, string/char literal contents, and preprocessor
-- lines with spaces, preserving every newline so positions still map
-- to line numbers. A brace inside a comment or a string would
-- otherwise throw off everything after it.
local function blank_noise(text)
    local out = {}
    local i = 1
    local n = #text
    local function keep_newlines(s)
        return (s:gsub("[^\n]", " "))
    end
    while i <= n do
        local c = text:sub(i, i)
        local two = text:sub(i, i + 1)
        if two == "//" then
            local stop = text:find("\n", i) or (n + 1)
            out[#out + 1] = keep_newlines(text:sub(i, stop - 1))
            i = stop
        elseif two == "/*" then
            local stop = text:find("*/", i + 2, true)
            if not stop then
                fail("(input)", nil, "unterminated block comment")
            end
            out[#out + 1] = keep_newlines(text:sub(i, stop + 1))
            i = stop + 2
        elseif c == '"' or c == "'" then
            local quote = c
            local j = i + 1
            while j <= n do
                local cj = text:sub(j, j)
                if cj == "\\" then
                    j = j + 2
                elseif cj == quote then
                    break
                else
                    j = j + 1
                end
            end
            -- Keep the quotes, blank the contents.
            out[#out + 1] = quote .. keep_newlines(text:sub(i + 1, j - 1)) .. quote
            i = j + 1
        else
            out[#out + 1] = c
            i = i + 1
        end
    end
    local blanked = table.concat(out)
    -- Preprocessor lines whole, after strings are blanked, so a '#'
    -- inside text cannot be mistaken for a directive.
    blanked = blanked:gsub("[^\n]*", function(line)
        if line:match("^%s*#") then return (line:gsub(".", " ")) end
        return line
    end)
    return blanked
end
-- }}}

-- {{{ local function line_of()
local function line_of(text, pos)
    local _, count = text:sub(1, pos):gsub("\n", "")
    return count + 1
end
-- }}}

-- {{{ local function normalize_type()
-- Collapse whitespace and settle pointer spelling, so "const char*"
-- and "const  char  *" are one name. The registry compares types by
-- these strings, so the spelling must be canonical.
local function normalize_type(s)
    s = s:gsub("%s+", " ")
    s = s:gsub("%s*%*%s*", " *")
    s = s:gsub("^%s+", ""):gsub("%s+$", "")
    return s
end
-- }}}

-- What a primitive fundamentally is: the field-kind the statics
-- reader will use, keyed by canonical type name. A dispatch table
-- rather than a chain of comparisons, like everything else here.
local PRIMITIVES = {
    ["char"] = "int",  ["signed char"] = "int",  ["unsigned char"] = "uint",
    ["short"] = "int", ["unsigned short"] = "uint",
    ["int"] = "int",   ["unsigned"] = "uint",    ["unsigned int"] = "uint",
    ["long"] = "int",  ["unsigned long"] = "uint",
    ["long long"] = "int", ["unsigned long long"] = "uint",
    ["float"] = "float", ["double"] = "float",
    ["int32_t"] = "int", ["int64_t"] = "int",
    ["uint32_t"] = "uint", ["uint64_t"] = "uint",
    ["size_t"] = "uint",
}

local STRING_TYPES = {
    ["const char *"] = true,
    ["char *"] = true,
}

-- {{{ local function mangle()
-- A type name as an identifier fragment: spaces to underscores,
-- stars to the word "ptr".
local function mangle(type_name)
    local s = type_name:gsub("%*", "ptr")
    s = s:gsub("%s+", "_")
    return s
end
-- }}}

-- {{{ local function find_matching_brace()
local function find_matching_brace(text, open_pos)
    local depth = 0
    for i = open_pos, #text do
        local c = text:sub(i, i)
        if c == "{" then
            depth = depth + 1
        elseif c == "}" then
            depth = depth - 1
            if depth == 0 then return i end
        end
    end
    return nil
end
-- }}}

-- {{{ local function parse_fields()
local function parse_fields(file, body, body_line)
    if body:find("{", 1, true) then
        fail(file, body_line,
             "nested struct bodies are refused — define the inner struct "
             .. "separately and refer to it by name")
    end
    local fields = {}
    for entry in body:gmatch("[^;]+") do
        if entry:match("%S") then
            if entry:find(",", 1, true) then
                fail(file, body_line,
                     "one field per declaration — 'float x, y;' hides the "
                     .. "field order this table exists to state")
            end
            local array_len = entry:match("%[%s*(%d+)%s*%]%s*$")
            local core = entry:gsub("%[%s*%d*%s*%]%s*$", "")
            local type_part, name = core:match("^%s*(.-)%s*([%w_]+)%s*$")
            if not name or type_part == "" then
                fail(file, body_line,
                     "cannot read struct field: '" .. entry:gsub("%s+", " ") .. "'")
            end
            fields[#fields + 1] = {
                type = normalize_type(type_part),
                name = name,
                array_len = array_len and tonumber(array_len) or nil,
            }
        end
    end
    return fields
end
-- }}}

-- {{{ local function parse_params()
local function parse_params(file, line, inner)
    inner = inner:gsub("^%s+", ""):gsub("%s+$", "")
    if inner == "" or inner == "void" then return {} end
    local params = {}
    for piece in inner:gmatch("[^,]+") do
        local core = piece
        local type_part, name = core:match("^%s*(.-[%s%*])%s*([%w_]+)%s*$")
        if not type_part then
            fail(file, line, "cannot read parameter: '" .. piece .. "' — "
                 .. "parameters must be named")
        end
        params[#params + 1] = {
            type = normalize_type(type_part),
            name = name,
        }
    end
    return params
end
-- }}}

-- {{{ local function parse_file()
-- One pass over a blanked file, stopping at every top-level '{' and
-- classifying the declaration that led to it. Three shapes are
-- recognized; everything else stops the build.
local function parse_file(path, into)
    local raw = read_file(path)
    local text = blank_noise(raw)

    local pos = 1
    local segment_start = 1
    while true do
        local open = text:find("{", pos, true)
        if not open then break end

        local header = text:sub(segment_start, open - 1)
        -- Complete declarations before this one (externs, globals)
        -- end with ';'. Only what follows the last one leads to this
        -- brace.
        local after_semi = header:match(".*;(.*)$") or header
        local line = line_of(text, open)

        local td_tag = after_semi:match("^%s*typedef%s+struct%s*([%w_]*)%s*$")
        local plain_tag = after_semi:match("^%s*struct%s+([%w_]+)%s*$")
        -- A function header: everything up to a trailing balanced
        -- paren group, with the name as the identifier just before
        -- it. Split by hand because the name may follow a star with
        -- no space ("char *sneaky"), which one pattern cannot say.
        local ret_and_name, fn_name, parens
        local head, tail_parens = after_semi:match("^%s*(.-)%s*(%b())%s*$")
        if head then
            local ret_part, name_part = head:match("^(.-)([%w_]+)$")
            if ret_part and ret_part ~= ""
               and ret_part:match("[%s%*]$") then
                ret_and_name = ret_part
                fn_name = name_part
                parens = tail_parens
            end
        end

        local close = find_matching_brace(text, open)
        if not close then
            fail(path, line, "unbalanced braces from here to end of file")
        end

        if td_tag then
            -- typedef struct [tag] { body } name;
            local name_after = text:match("^%s*([%w_]+)%s*;", close + 1)
            if not name_after then
                fail(path, line, "typedef struct with no name after the closing brace")
            end
            into.structs[#into.structs + 1] = {
                name = name_after,
                file = path,
                line = line,
                fields = parse_fields(path, text:sub(open + 1, close - 1), line),
            }
            local semi = text:find(";", close + 1, true)
            pos = semi + 1
            segment_start = pos
        elseif plain_tag then
            fail(path, line, "plain 'struct " .. plain_tag .. "' is refused — "
                 .. "box value types must be 'typedef struct { ... } name;' "
                 .. "so maps can name them")
        elseif fn_name then
            local ret = normalize_type(ret_and_name)
            local is_static = ret:match("^static%s") or ret == "static"
            ret = ret:gsub("^static%s+", ""):gsub("^inline%s+", "")
            local inner = parens:sub(2, -2)
            local compare_type = fn_name:match("^([%w_]+)__compare$")

            if compare_type then
                local params = parse_params(path, line, inner)
                if ret ~= "int" or #params ~= 2
                   or params[1].type ~= compare_type
                   or params[2].type ~= compare_type then
                    fail(path, line, fn_name .. " must be: int "
                         .. compare_type .. "__compare(" .. compare_type
                         .. " a, " .. compare_type .. " b)")
                end
                into.compares[compare_type] = { file = path, line = line }
            elseif is_static then
                -- A private helper: visible to its boxes, invisible
                -- to maps. Deliberately not an error.
            else
                into.boxes[#into.boxes + 1] = {
                    name = fn_name,
                    file = path,
                    line = line,
                    ret = ret,
                    params = parse_params(path, line, inner),
                }
            end
            pos = close + 1
            segment_start = pos
        else
            fail(path, line,
                 "unrecognized declaration before this brace: '"
                 .. after_semi:gsub("%s+", " "):sub(1, 60) .. "' — box sources "
                 .. "hold typedef structs and functions only")
        end
    end
end
-- }}}

-- {{{ local function classify_type()
-- What a type is to the engine: a primitive kind, a known struct, a
-- borrowed string, or a mistake. Returns kind plus struct record.
local function classify_type(t, structs_by_name)
    if PRIMITIVES[t] then return PRIMITIVES[t], nil end
    if STRING_TYPES[t] then return "string_ptr", nil end
    if structs_by_name[t] then return "struct", structs_by_name[t] end
    return nil, nil
end
-- }}}

-- {{{ local function validate()
local function validate(description)
    local by_name = {}
    for _, s in ipairs(description.structs) do
        if by_name[s.name] then
            fail(s.file, s.line, "struct '" .. s.name .. "' defined twice")
        end
        by_name[s.name] = s
    end
    description.structs_by_name = by_name

    for _, s in ipairs(description.structs) do
        for _, f in ipairs(s.fields) do
            if f.array_len then
                if f.type ~= "char" then
                    fail(s.file, s.line, "field '" .. f.name .. "': only char "
                         .. "arrays (fixed strings) are supported as array fields")
                end
                f.kind = "string"
            else
                local kind, nested = classify_type(f.type, by_name)
                if not kind or kind == "string_ptr" then
                    fail(s.file, s.line, "field '" .. f.name .. "' has type '"
                         .. f.type .. "', which the statics reader cannot fill "
                         .. "— use a primitive, a known struct, or a char array")
                end
                f.kind = kind
                f.nested = nested
            end
        end
    end

    local seen = {}
    for _, b in ipairs(description.boxes) do
        if seen[b.name] then
            fail(b.file, b.line, "box '" .. b.name .. "' defined twice")
        end
        seen[b.name] = true
        for _, p in ipairs(b.params) do
            local kind = classify_type(p.type, by_name)
            if not kind then
                fail(b.file, b.line, "box '" .. b.name .. "' parameter '"
                     .. p.name .. "' has unknown type '" .. p.type .. "'")
            end
            p.kind = kind
        end
        if b.ret ~= "void" then
            local kind = classify_type(b.ret, by_name)
            if not kind then
                fail(b.file, b.line, "box '" .. b.name .. "' returns unknown type '"
                     .. b.ret .. "'")
            end
            if kind == "string_ptr" then
                fail(b.file, b.line, "box '" .. b.name .. "' returns a string "
                     .. "pointer — returning borrowed memory through a wire "
                     .. "has no owner; return a struct with a char array instead")
            end
            b.ret_kind = kind
        end
    end
end
-- }}}

-- {{{ local function describe()
-- The parser's findings in readable form, for diagnosing a build by
-- looking at what was seen rather than what was emitted.
local function describe(description)
    for _, s in ipairs(description.structs) do
        print(("struct %s  (%s:%d)"):format(s.name, s.file, s.line))
        for _, f in ipairs(s.fields) do
            print(("  %-12s %s%s"):format(f.name, f.type,
                f.array_len and ("[" .. f.array_len .. "]") or ""))
        end
    end
    for _, b in ipairs(description.boxes) do
        local ps = {}
        for _, p in ipairs(b.params) do ps[#ps + 1] = p.type .. " " .. p.name end
        print(("box %s(%s) -> %s  (%s:%d)"):format(
            b.name, table.concat(ps, ", "), b.ret, b.file, b.line))
    end
    for t, c in pairs(description.compares) do
        print(("compare for %s  (%s:%d)"):format(t, c.file, c.line))
    end
end
-- }}}

-- {{{ local function emit()
local function emit(description, sources, out_path)
    local w = {}
    local function line(s) w[#w + 1] = s end

    line("/* GENERATED by scripts/028-generate.lua — do not edit, do not commit.")
    line(" * Derived entirely from the box sources named below; every size and")
    line(" * offset is a sizeof/offsetof expression the compiler computes. */")
    line("#include <stddef.h>")
    line("#include <string.h>")
    line('#include "026-registry.h"')
    line("")
    line("/* The box sources, included whole: their types become visible, and")
    line(" * the compiler can inline each box into its shim. */")
    for _, src in ipairs(sources) do
        line(('#include "%s"'):format(src))
    end
    line("")

    -- Compare functions for every primitive that some box returns,
    -- and wrappers for every author-written struct compare. Each
    -- copies bytes into real typed variables first: raw-byte
    -- comparison reads negative floats backwards (issue 305).
    local compare_of = {}   -- return type -> generated symbol or nil
    local emitted_cmp = {}
    for _, b in ipairs(description.boxes) do
        local t = b.ret
        if t ~= "void" and not emitted_cmp[t] then
            if PRIMITIVES[t] then
                local sym = mangle(t) .. "__compare_g"
                line(("/* three-way compare for %s, generated (issue 305) */"):format(t))
                line(("static int %s(const void *a, const void *b)"):format(sym))
                line("{")
                line(("    %s x, y;"):format(t))
                line("    memcpy(&x, a, sizeof x);")
                line("    memcpy(&y, b, sizeof y);")
                line("    return (x > y) - (x < y);")
                line("}")
                line("")
                compare_of[t] = sym
                emitted_cmp[t] = true
            elseif description.compares[t] then
                local sym = mangle(t) .. "__compare_g"
                line(("/* wrapper over the author's %s__compare (issue 305) */"):format(t))
                line(("static int %s(const void *a, const void *b)"):format(sym))
                line("{")
                line(("    %s x, y;"):format(t))
                line("    memcpy(&x, a, sizeof x);")
                line("    memcpy(&y, b, sizeof y);")
                line(("    return %s__compare(x, y);"):format(t))
                line("}")
                line("")
                compare_of[t] = sym
                emitted_cmp[t] = true
            end
            -- A struct return with no author compare simply has a
            -- null compare in its registry row; the refusal happens
            -- at load, when a comparator actually asks (issue 604).
        end
    end

    -- Shims: the per-box call site. Loads are memcpy, never pointer
    -- casts — the task's value area packs values back to back, so a
    -- double after an int sits misaligned, and memcpy is how that
    -- stays defined behavior everywhere.
    for _, b in ipairs(description.boxes) do
        line(("/* generated from: %s %s(...) [%s:%d] */"):format(
            b.ret, b.name, b.file, b.line))
        line(("static void %s__call(task_t *t)"):format(b.name))
        line("{")
        line("    (void)t;")
        local args = {}
        for i, p in ipairs(b.params) do
            line(("    %s a%d;"):format(p.type, i - 1))
            line(("    memcpy(&a%d, t->in[%d], sizeof a%d);"):format(i - 1, i - 1, i - 1))
            args[#args + 1] = ("a%d"):format(i - 1)
        end
        if b.ret == "void" then
            line(("    %s(%s);"):format(b.name, table.concat(args, ", ")))
        else
            line(("    %s r = %s(%s);"):format(b.ret, b.name, table.concat(args, ", ")))
            line("    memcpy(t->out, &r, sizeof r);")
        end
        line("}")
        line("")
    end

    -- Field tables (issue 304): one per struct, offsets and sizes
    -- computed by the compiler, nested structs pointing at the
    -- shared struct array declared extern in the registry header.
    local struct_index = {}
    for i, s in ipairs(description.structs) do
        struct_index[s.name] = i - 1
    end
    local KIND_ENUM = {
        int = "FIELD_INT", uint = "FIELD_UINT", float = "FIELD_FLOAT",
        string = "FIELD_STRING", struct = "FIELD_STRUCT",
    }
    for _, s in ipairs(description.structs) do
        line(("static const field_info_t %s__fields[] = {"):format(s.name))
        for _, f in ipairs(s.fields) do
            local nested = f.nested
                and ("&registry_structs[" .. struct_index[f.nested.name] .. "]")
                or "NULL"
            line(("    { \"%s\", (int)offsetof(%s, %s), (int)sizeof(((%s *)0)->%s), %s, %s, %d },")
                :format(f.name, s.name, f.name, s.name, f.name,
                        KIND_ENUM[f.kind], nested, f.array_len or 0))
        end
        line("};")
        line("")
    end

    line("const struct_info_t registry_structs[] = {")
    for _, s in ipairs(description.structs) do
        line(("    { \"%s\", (int)sizeof(%s), %d, %s__fields },"):format(
            s.name, s.name, #s.fields, s.name))
    end
    if #description.structs == 0 then
        line("    { NULL, 0, 0, NULL },")
    end
    line("};")
    line(("const int registry_n_structs = %d;"):format(#description.structs))
    line("")

    -- The registry itself (issue 303). Type names ride along as text
    -- because "4 bytes versus 4 bytes" is not an error message.
    for _, b in ipairs(description.boxes) do
        if #b.params > 0 then
            line(("static const box_param_t %s__params[] = {"):format(b.name))
            for _, p in ipairs(b.params) do
                line(("    { \"%s\", (int)sizeof(%s) },"):format(p.type, p.type))
            end
            line("};")
        end
    end
    line("")
    line("const box_info_t registry_boxes[] = {")
    for _, b in ipairs(description.boxes) do
        local params_ref = #b.params > 0 and (b.name .. "__params") or "NULL"
        local ret_size = b.ret == "void" and "0" or ("(int)sizeof(" .. b.ret .. ")")
        local task_bits = {
            "sizeof(task_t)",
            ("%d * sizeof(void *)"):format(#b.params),
        }
        for _, p in ipairs(b.params) do
            task_bits[#task_bits + 1] = ("sizeof(%s)"):format(p.type)
        end
        if b.ret ~= "void" then
            task_bits[#task_bits + 1] = ("sizeof(%s)"):format(b.ret)
        end
        local cmp = (b.ret ~= "void" and compare_of[b.ret]) or "NULL"
        line(("    { \"%s\", %s__call, %d, %s, \"%s\", %s,"):format(
            b.name, b.name, #b.params, params_ref, b.ret, ret_size))
        line(("      %s, %s },"):format(table.concat(task_bits, " + "), cmp))
    end
    if #description.boxes == 0 then
        line("    { NULL, NULL, 0, NULL, NULL, 0, 0, NULL },")
    end
    line("};")
    line(("const int registry_n_boxes = %d;"):format(#description.boxes))
    line("")

    -- Written whole to a temporary name, moved into place on
    -- success: a failing generator must never leave yesterday's or
    -- half of today's output where the build can find it.
    local tmp = out_path .. ".tmp"
    local f = io.open(tmp, "w")
    if not f then
        io.stderr:write("generator: cannot write " .. tmp .. "\n")
        os.exit(1)
    end
    f:write(table.concat(w, "\n"))
    f:write("\n")
    f:close()
    local ok, err = os.rename(tmp, out_path)
    if not ok then
        io.stderr:write("generator: cannot move output into place: "
                        .. tostring(err) .. "\n")
        os.exit(1)
    end
end
-- }}}

-- {{{ main
local args = { ... }
if arg then args = arg end

local describe_only = args[1] == "--describe"
local out_path = not describe_only and args[1] or nil
local first_source = describe_only and 2 or 2

local sources = {}
for i = first_source, #args do
    sources[#sources + 1] = args[i]
end
if (not describe_only and not out_path) or #sources == 0 then
    io.stderr:write("usage: 028-generate.lua <output.c> <box.c>...\n")
    io.stderr:write("       028-generate.lua --describe <box.c>...\n")
    os.exit(1)
end

local description = { structs = {}, boxes = {}, compares = {} }
for _, src in ipairs(sources) do
    parse_file(src, description)
end
validate(description)

if describe_only then
    describe(description)
else
    emit(description, sources, out_path)
end
-- }}}
