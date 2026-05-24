#!/usr/bin/env luajit
-- scripts/export-llm-transcripts.lua — local transcript exporter.
--
-- What it does, in one sentence: walks every Claude conversation
-- JSONL for this project under ~/.claude/projects/, renders each
-- one at six verbosity levels into per-conversation markdown
-- files under llm-transcripts/v{N}-{name}/, and skips
-- conversations whose source JSONL hasn't changed since the last
-- render.
--
-- Replaces an earlier wrapper around an external exporter that
-- (a) guessed at which project's conversations to read and got it
-- wrong for projects outside /home/ritz/programming/ai-stuff/,
-- (b) re-rendered every conversation top-to-bottom every run, and
-- (c) injected global CLAUDE.md text into the output. This local
-- version reads the right project (the path is encoded directly
-- from $DIR), supports incremental refresh, and never includes
-- ambient context.
--
-- Output layout:
--   llm-transcripts/
--     v0-minimal/    one file per conversation: code blocks only
--     v1-compact/    user prompts + assistant text + tool signatures
--     v2-standard/   v1 + truncated tool results (200-line cap)
--     v3-verbose/    v2 + full tool results
--     v4-complete/   v3 + tool metadata (names, durations if known)
--     v5-raw/        pretty-printed JSONL, one entry per chunk

local DIR = arg and arg[1] or "/mnt/mtwo/programs/sora/soramech"

-- {{{ require dkjson via the project's libs/ path
package.path = DIR .. "/libs/?.lua;" .. package.path
local json = require("dkjson")
-- }}}

-- {{{ paths
-- Compute the Claude conversation directory by encoding the project
-- path the same way Claude does — replace every "/" with "-".
local function claude_project_dir()
    local home = os.getenv("HOME")
    local encoded = DIR:gsub("/", "-")
    return home .. "/.claude/projects/" .. encoded
end

local SRC_DIR = claude_project_dir()
local OUT_DIR = DIR .. "/llm-transcripts"
-- }}}

-- {{{ helpers — file IO
local function read_all(path)
    local f = io.open(path, "rb")
    if not f then return nil end
    local s = f:read("*a")
    f:close()
    return s
end

local function write_all(path, content)
    local f, err = io.open(path, "wb")
    if not f then return nil, err end
    f:write(content)
    f:close()
    return true
end

local function ensure_dir(path)
    os.execute("mkdir -p '" .. path .. "'")
end

local function file_mtime(path)
    local f = io.popen("stat -c %Y '" .. path .. "' 2>/dev/null")
    if not f then return 0 end
    local s = f:read("*l")
    f:close()
    return tonumber(s) or 0
end

local function list_jsonls(dir)
    local f = io.popen("ls '" .. dir .. "'/*.jsonl 2>/dev/null")
    if not f then return {} end
    local result = {}
    for line in f:lines() do
        result[#result + 1] = line
    end
    f:close()
    return result
end

local function basename_no_ext(path, ext)
    local b = path:match("([^/]+)$") or path
    return (b:gsub("%." .. ext .. "$", ""))
end
-- }}}

-- {{{ helpers — content extraction from message entries
-- The JSONL has heterogeneous shapes per entry type:
--   {type="user",      message={content="..." OR [items]}}
--   {type="assistant", message={content=[items]}}
--   {type="permission-mode" / "attachment" / ...}
--
-- Within a message's content list, items have a `type` field:
--   {type="text",        text="..."}
--   {type="tool_use",    name="Bash", input={...}}
--   {type="tool_result", content="..." OR [items]}

local function get_user_text(entry)
    local m = entry.message or {}
    local c = m.content
    if type(c) == "string" then return c end
    if type(c) == "table" then
        local out = {}
        for i = 1, #c do
            local item = c[i]
            if type(item) == "table" and item.type == "text" then
                out[#out + 1] = item.text or ""
            end
        end
        if #out > 0 then return table.concat(out, "\n") end
    end
    return nil
end

local function get_assistant_items(entry)
    local m = entry.message or {}
    return type(m.content) == "table" and m.content or {}
end

local function summarise_tool_call(item)
    local name = item.name or "?"
    local input = item.input or {}
    if name == "Bash" then
        local cmd = input.command or ""
        if #cmd > 120 then cmd = cmd:sub(1, 117) .. "..." end
        return "Bash: " .. cmd
    elseif name == "Edit" or name == "Write" or name == "Read" then
        return name .. ": " .. (input.file_path or "?")
    elseif name == "Agent" or name == "Skill" then
        return name .. ": " .. (input.description or input.skill or "?")
    elseif name == "TaskCreate" or name == "TaskUpdate" then
        return name .. ": " .. (input.description or input.id or "?")
    end
    return name .. " (...)"
end

local function tool_result_text(item)
    local c = item.content
    if type(c) == "string" then return c end
    if type(c) == "table" then
        local out = {}
        for i = 1, #c do
            local sub = c[i]
            if type(sub) == "table" and sub.type == "text" then
                out[#out + 1] = sub.text or ""
            end
        end
        return table.concat(out, "\n")
    end
    return ""
end

-- Pull every fenced code block out of a markdown string.
local function extract_code_blocks(text)
    if not text or text == "" then return {} end
    local blocks = {}
    for code in text:gmatch("```[%w_-]*\n(.-)```") do
        blocks[#blocks + 1] = code
    end
    return blocks
end

-- Truncate a multi-line string to at most N lines, marking the tail.
local function truncate_lines(text, max_lines)
    if not text or text == "" then return text end
    local count = 0
    local cut_at = nil
    for pos in text:gmatch("()\n") do
        count = count + 1
        if count == max_lines then cut_at = pos break end
    end
    if not cut_at then return text end
    return text:sub(1, cut_at) ..
           "... [truncated; " .. max_lines .. "-line cap]"
end
-- }}}

-- {{{ render — one conversation at one verbosity level
-- Returns a string. Each level walks the JSONL entries once and
-- emits a different subset.

local function render_v0_minimal(entries)
    -- Code blocks from assistant text only. The interesting
    -- artifacts when you want to skim "what got written" without
    -- the surrounding discussion.
    local out = { "# Code-only transcript\n" }
    for _, e in ipairs(entries) do
        if e.type == "assistant" then
            for _, item in ipairs(get_assistant_items(e)) do
                if item.type == "text" then
                    for _, code in ipairs(extract_code_blocks(item.text or "")) do
                        out[#out + 1] = "```"
                        out[#out + 1] = code
                        out[#out + 1] = "```\n"
                    end
                end
            end
        end
    end
    return table.concat(out, "\n")
end

local function render_v1_compact(entries)
    -- Conversation outline: user prompts, assistant prose, tool
    -- call signatures. Tool results are omitted.
    local out = { "# Compact transcript\n" }
    for _, e in ipairs(entries) do
        if e.type == "user" then
            local text = get_user_text(e)
            if text and text:match("%S") then
                out[#out + 1] = "## User\n\n" .. text .. "\n"
            end
        elseif e.type == "assistant" then
            local text_chunks = {}
            local tools       = {}
            for _, item in ipairs(get_assistant_items(e)) do
                if item.type == "text" then
                    text_chunks[#text_chunks + 1] = item.text or ""
                elseif item.type == "tool_use" then
                    tools[#tools + 1] = summarise_tool_call(item)
                end
            end
            if #text_chunks > 0 or #tools > 0 then
                out[#out + 1] = "## Assistant\n"
                if #text_chunks > 0 then
                    out[#out + 1] = table.concat(text_chunks, "\n") .. "\n"
                end
                if #tools > 0 then
                    out[#out + 1] = "_Tools:_\n"
                    for _, t in ipairs(tools) do
                        out[#out + 1] = "- `" .. t .. "`"
                    end
                    out[#out + 1] = ""
                end
            end
        end
    end
    return table.concat(out, "\n")
end

local function render_with_tool_results(entries, result_max_lines)
    local out = { "# Transcript\n" }
    for _, e in ipairs(entries) do
        if e.type == "user" then
            local m = e.message or {}
            local c = m.content
            if type(c) == "string" then
                out[#out + 1] = "## User\n\n" .. c .. "\n"
            elseif type(c) == "table" then
                local tool_results = {}
                local text_items   = {}
                for _, item in ipairs(c) do
                    if type(item) == "table" then
                        if item.type == "text" then
                            text_items[#text_items + 1] = item.text or ""
                        elseif item.type == "tool_result" then
                            tool_results[#tool_results + 1] = item
                        end
                    end
                end
                if #text_items > 0 then
                    out[#out + 1] = "## User\n\n" ..
                                    table.concat(text_items, "\n") .. "\n"
                end
                for _, tr in ipairs(tool_results) do
                    local txt = tool_result_text(tr)
                    if result_max_lines and result_max_lines > 0 then
                        txt = truncate_lines(txt, result_max_lines)
                    end
                    out[#out + 1] = "### Tool result\n\n```\n" ..
                                    (txt or "") .. "\n```\n"
                end
            end
        elseif e.type == "assistant" then
            local text_chunks = {}
            local tools       = {}
            for _, item in ipairs(get_assistant_items(e)) do
                if item.type == "text" then
                    text_chunks[#text_chunks + 1] = item.text or ""
                elseif item.type == "tool_use" then
                    tools[#tools + 1] = summarise_tool_call(item)
                end
            end
            if #text_chunks > 0 or #tools > 0 then
                out[#out + 1] = "## Assistant\n"
                if #text_chunks > 0 then
                    out[#out + 1] = table.concat(text_chunks, "\n") .. "\n"
                end
                if #tools > 0 then
                    out[#out + 1] = "_Tools:_\n"
                    for _, t in ipairs(tools) do
                        out[#out + 1] = "- `" .. t .. "`"
                    end
                    out[#out + 1] = ""
                end
            end
        end
    end
    return table.concat(out, "\n")
end

local function render_v2_standard(entries) return render_with_tool_results(entries, 200) end
local function render_v3_verbose(entries)  return render_with_tool_results(entries, 0)   end

local function render_v4_complete(entries)
    -- Everything v3 has, plus session metadata and entry-type tags.
    local out = { "# Complete transcript with metadata\n" }
    for i, e in ipairs(entries) do
        local t = e.type or "?"
        out[#out + 1] = "### [" .. i .. "] type=" .. t
        if e.timestamp then out[#out + 1] = "ts: " .. tostring(e.timestamp) end
        if t == "user" then
            local txt = get_user_text(e)
            if txt then out[#out + 1] = "\n" .. txt .. "\n" end
            local m = e.message or {}
            if type(m.content) == "table" then
                for _, item in ipairs(m.content) do
                    if type(item) == "table" and item.type == "tool_result" then
                        out[#out + 1] = "\n_tool_result:_\n```\n" ..
                                        (tool_result_text(item) or "") .. "\n```\n"
                    end
                end
            end
        elseif t == "assistant" then
            for _, item in ipairs(get_assistant_items(e)) do
                if item.type == "text" then
                    out[#out + 1] = "\n" .. (item.text or "") .. "\n"
                elseif item.type == "tool_use" then
                    out[#out + 1] = "\n_tool_use:_ `" ..
                                    summarise_tool_call(item) .. "`"
                    if item.input then
                        out[#out + 1] = "input:\n```json\n" ..
                                        json.encode(item.input, {indent=true}) ..
                                        "\n```\n"
                    end
                end
            end
        end
        out[#out + 1] = ""
    end
    return table.concat(out, "\n")
end

local function render_v5_raw(entries)
    -- Pretty-printed JSONL: one entry per chunk, properly indented.
    local out = { "# Raw conversation entries\n" }
    for i, e in ipairs(entries) do
        out[#out + 1] = "## [" .. i .. "]\n"
        out[#out + 1] = "```json"
        out[#out + 1] = json.encode(e, {indent = true})
        out[#out + 1] = "```\n"
    end
    return table.concat(out, "\n")
end

local LEVELS = {
    { n = 0, name = "minimal",  render = render_v0_minimal  },
    { n = 1, name = "compact",  render = render_v1_compact  },
    { n = 2, name = "standard", render = render_v2_standard },
    { n = 3, name = "verbose",  render = render_v3_verbose  },
    { n = 4, name = "complete", render = render_v4_complete },
    { n = 5, name = "raw",      render = render_v5_raw      },
}
-- }}}

-- {{{ parse one JSONL file into a list of entry tables
local function parse_jsonl(path)
    local entries = {}
    local f = io.open(path, "rb")
    if not f then return entries end
    for line in f:lines() do
        if line ~= "" then
            local ok, dec = pcall(json.decode, line)
            if ok and type(dec) == "table" then
                entries[#entries + 1] = dec
            end
        end
    end
    f:close()
    return entries
end
-- }}}

-- {{{ main
local function main()
    if not SRC_DIR or SRC_DIR == "" then
        io.stderr:write("error: could not compute claude project dir\n")
        os.exit(1)
    end
    ensure_dir(OUT_DIR)

    local jsonls = list_jsonls(SRC_DIR)
    if #jsonls == 0 then
        io.stderr:write("No JSONL conversations found at " .. SRC_DIR .. "\n")
        return
    end

    io.stderr:write("Exporting " .. #jsonls .. " conversation(s) for " ..
                    DIR .. "\n")
    io.stderr:write("  source : " .. SRC_DIR .. "\n")
    io.stderr:write("  output : " .. OUT_DIR .. "/v{0..5}-*/\n\n")

    for _, lvl in ipairs(LEVELS) do
        ensure_dir(OUT_DIR .. "/v" .. lvl.n .. "-" .. lvl.name)
    end

    local total_rendered = 0
    local total_skipped  = 0
    for _, src_path in ipairs(jsonls) do
        local conv_id = basename_no_ext(src_path, "jsonl")
        local src_m = file_mtime(src_path)

        -- Incremental check: skip only if EVERY level's output is
        -- newer than the source. The first run always renders all
        -- levels for every conversation.
        local need_render = false
        for _, lvl in ipairs(LEVELS) do
            local out_path = OUT_DIR .. "/v" .. lvl.n .. "-" .. lvl.name ..
                             "/" .. conv_id .. ".md"
            if file_mtime(out_path) <= src_m then
                need_render = true
                break
            end
        end
        if not need_render then
            total_skipped = total_skipped + 1
        else
            local entries = parse_jsonl(src_path)
            for _, lvl in ipairs(LEVELS) do
                local out_path = OUT_DIR .. "/v" .. lvl.n .. "-" .. lvl.name ..
                                 "/" .. conv_id .. ".md"
                local body = lvl.render(entries)
                local ok, werr = write_all(out_path, body)
                if not ok then
                    io.stderr:write("  WARN write " .. out_path .. ": " ..
                                    tostring(werr) .. "\n")
                end
            end
            total_rendered = total_rendered + 1
        end
    end

    io.stderr:write("Rendered " .. total_rendered ..
                    " conversation(s), skipped " .. total_skipped ..
                    " unchanged.\n")
end

main()
-- }}}
