-- Text manipulation — shared library and ready-to-use box source.
-- require("text") from any Lua function, or copy to a map's src/ to use
-- it directly as a call box via the file browser.
-- No external dependencies.

local M = {}

-- {{{ M.concat
-- Join N text values with a separator. The signature is variadic so the
-- box can grow its `text` input without modifying this source — see
-- issue 217 part B for the editor-side variadic-input mechanics.
function M.concat(sep, ...)
    return table.concat({ ... }, sep or "")
end
-- }}}

-- {{{ M.split
-- Split a text into parts on a literal (non-pattern) separator. Returns
-- a Lua array of the parts; the driver shim encodes it as JSON for
-- transport on the single output wire. The downstream box decodes the
-- array — there is no fan-out into multiple wires (that would require
-- knowing the part count at design time, which we don't).
--
-- Plain-string match: the separator is escaped before being passed to
-- string.find so users don't have to think about Lua patterns.
function M.split(text, sep)
    if text == nil then return {} end
    if sep == nil or sep == "" then
        -- splitting on empty separator: return a single-element array
        -- with the whole input. Avoids an infinite loop and matches
        -- common library behavior for split-on-empty.
        return { text }
    end

    local parts = {}
    local plain = true   -- treat sep as a literal string, not a pattern
    local pos   = 1

    while true do
        local s, e = string.find(text, sep, pos, plain)
        if s == nil then
            parts[#parts + 1] = string.sub(text, pos)
            break
        end
        parts[#parts + 1] = string.sub(text, pos, s - 1)
        pos = e + 1
    end

    return parts
end
-- }}}

-- Issue 237: the rest of this file is the bundled string-manipulation
-- toolbox. All matching is PLAIN (literal) — never Lua-pattern — so
-- find/prefix/suffix can carry punctuation without escaping. A user
-- who wants real pattern matching can call string.gsub directly via
-- a regular call box.

-- {{{ M.upper
function M.upper(text)
    return string.upper(text or "")
end
-- }}}

-- {{{ M.lower
function M.lower(text)
    return string.lower(text or "")
end
-- }}}

-- {{{ M.trim
-- Strip leading and trailing whitespace. `%s` is Lua's whitespace
-- class — spaces, tabs, newlines, etc.
function M.trim(text)
    if text == nil then return "" end
    local s = text:gsub("^%s+", ""):gsub("%s+$", "")
    return s
end
-- }}}

-- {{{ replace_n
-- Internal helper used by M.replace and M.replace_first. Walks the
-- text with string.find(..., plain=true) and emits an interleaved
-- list of slice + replacement chunks. Avoids string.gsub so we don't
-- have to worry about pattern-metacharacter escaping in either the
-- search needle or the replacement (which would otherwise need
-- separate escape passes — `%(` in needle, `%%` in repl).
-- `limit` is the maximum number of replacements (nil = unlimited).
local function replace_n(text, find, repl, limit)
    if text == nil or find == nil or find == "" then return text or "" end
    repl = repl or ""
    local out  = {}
    local pos  = 1
    local hits = 0
    while true do
        if limit and hits >= limit then
            out[#out + 1] = text:sub(pos)
            break
        end
        local s, e = text:find(find, pos, true)
        if s == nil then
            out[#out + 1] = text:sub(pos)
            break
        end
        out[#out + 1] = text:sub(pos, s - 1)
        out[#out + 1] = repl
        pos  = e + 1
        hits = hits + 1
    end
    return table.concat(out)
end
-- }}}

-- {{{ M.replace
-- Replace every occurrence of `find` with `repl` in `text`.
function M.replace(text, find, repl)
    return replace_n(text, find, repl, nil)
end
-- }}}

-- {{{ M.replace_first
-- Same as M.replace but stops after one replacement. A separate
-- function instead of a flag on replace because the editor has no
-- boolean-port primitive yet — toggling between two function names
-- is clearer than typing "true"/"false" into a third input.
function M.replace_first(text, find, repl)
    return replace_n(text, find, repl, 1)
end
-- }}}

-- {{{ M.contains
-- True if `needle` appears anywhere in `text`. Empty needle returns
-- false on the principle that "contains nothing" is a meaningless
-- question — a user wiring contains() into a guard almost never
-- wants an empty needle to short-circuit truth.
function M.contains(text, needle)
    if text == nil or needle == nil or needle == "" then return false end
    return text:find(needle, 1, true) ~= nil
end
-- }}}

-- {{{ M.starts_with
function M.starts_with(text, prefix)
    if text == nil or prefix == nil then return false end
    if prefix == "" then return true end   -- every string starts with empty
    return text:sub(1, #prefix) == prefix
end
-- }}}

-- {{{ M.ends_with
function M.ends_with(text, suffix)
    if text == nil or suffix == nil then return false end
    if suffix == "" then return true end   -- every string ends with empty
    return text:sub(-#suffix) == suffix
end
-- }}}

-- {{{ M.length
function M.length(text)
    if text == nil then return 0 end
    return #text
end
-- }}}

-- {{{ M.substring
-- Substring starting at `start` (1-indexed) of length `len`. If `len`
-- is omitted, takes everything from `start` to the end. Out-of-range
-- positions follow string.sub semantics (clamped, not errored).
function M.substring(text, start, len)
    if text == nil then return "" end
    start = tonumber(start) or 1
    len   = tonumber(len)
    if len == nil then return text:sub(start) end
    return text:sub(start, start + len - 1)
end
-- }}}

return M
