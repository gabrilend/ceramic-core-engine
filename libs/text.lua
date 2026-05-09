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

return M
