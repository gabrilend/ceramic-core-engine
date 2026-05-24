-- tests/maps/246-lua-shim/translations/echo__msg.lua — issue 246
-- Lua shim for the "msg" input port on the "echo" box.
--
-- The file's top-level expression returns the translation function.
-- The function takes the raw bytes (Lua string) and a boolean for
-- raw_native (true if the bytes came from a same-language native
-- producer; false if from a cross-language producer via JSON).
-- Returns the translated bytes (Lua string) the box's invoke will
-- see in place of the raw bytes.
--
-- For this fixture: prepend "REV-" then reverse the raw bytes.

return function(raw, raw_native)
    return "REV-" .. raw:reverse()
end
