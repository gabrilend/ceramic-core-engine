-- DEPRECATED — retired by issue 254 (llama.cpp client replacing Ollama).
-- New maps should not adopt this library; libs/llamacpp.lua replaces it
-- with the same two public signatures. Known forbidden-fallback: a nil
-- model / host silently becomes DEFAULT_MODEL / DEFAULT_HOST below —
-- part of why this file is being retired rather than extended.
--
-- Ollama API client — shared library and ready-to-use box source.
-- require("ollama") from any Lua function, or copy to a map's src/ directory
-- to use it directly as a call box via the file browser.
-- Uses curl; no extra Lua packages needed beyond dkjson.

local json = require("dkjson")
local M    = {}

local DEFAULT_HOST  = "http://localhost:11434"
local DEFAULT_MODEL = "llama3.2"

-- {{{ shell_quote
local function shell_quote(s)
    return "'" .. tostring(s):gsub("'", "'\\''") .. "'"
end
-- }}}

-- {{{ curl_post
local function curl_post(url, payload)
    local cmd = "curl -s --max-time 120 -X POST " .. shell_quote(url) ..
                " -H " .. shell_quote("Content-Type: application/json") ..
                " -d " .. shell_quote(payload)
    local ph  = io.popen(cmd)
    local out = ph:read("*a")
    ph:close()
    return out
end
-- }}}

-- {{{ M.query
-- Plain text generation. model and host are optional; nil or empty uses defaults.
function M.query(prompt, model, host)
    model = (model ~= nil and model ~= "") and model or DEFAULT_MODEL
    host  = (host  ~= nil and host  ~= "") and host  or DEFAULT_HOST
    local out  = curl_post(host .. "/api/generate", json.encode({
        model  = model,
        prompt = tostring(prompt),
        stream = false,
    }))
    local resp, _, err = json.decode(out)
    if not resp then
        io.stderr:write("ollama.query: bad response: " .. tostring(err) .. "\n")
        return nil
    end
    if resp.error then
        io.stderr:write("ollama.query: " .. tostring(resp.error) .. "\n")
        return nil
    end
    return resp.response
end
-- }}}

-- {{{ M.chat
-- Chat-style query. system prompt is optional; nil or empty omits it.
-- model and host default when nil or empty.
function M.chat(prompt, system, model, host)
    model = (model ~= nil and model ~= "") and model or DEFAULT_MODEL
    host  = (host  ~= nil and host  ~= "") and host  or DEFAULT_HOST
    local messages = {}
    if system ~= nil and system ~= "" then
        messages[#messages + 1] = { role = "system", content = tostring(system) }
    end
    messages[#messages + 1] = { role = "user", content = tostring(prompt) }
    local out  = curl_post(host .. "/api/chat", json.encode({
        model    = model,
        messages = messages,
        stream   = false,
    }))
    local resp, _, err = json.decode(out)
    if not resp then
        io.stderr:write("ollama.chat: bad response: " .. tostring(err) .. "\n")
        return nil
    end
    if resp.error then
        io.stderr:write("ollama.chat: " .. tostring(resp.error) .. "\n")
        return nil
    end
    return resp.message and resp.message.content or nil
end
-- }}}

return M
