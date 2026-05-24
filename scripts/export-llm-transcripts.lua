#!/usr/bin/env luajit
-- scripts/export-llm-transcripts.lua — local transcript exporter.
--
-- What it does, in one sentence: walks every Claude conversation
-- JSONL for this project, renders each one at six verbosity levels
-- into per-conversation markdown files, sets each file's mtime to
-- the conversation's last-message timestamp (and atime to its
-- first-message timestamp), and skips conversations whose source
-- JSONL hasn't changed since the last render.
--
-- File naming: `<slug>-<short-id>.md` where the slug is derived
-- from the first user message (truncated, lower-cased, hyphen-
-- separated) and short-id is the first 8 chars of the conversation
-- UUID. This gives human-readable filenames you can scan in `ls`
-- without losing per-conversation uniqueness.
--
-- Per-level context inclusion (a small frontmatter at the top of
-- each rendered file):
--   v0..v2  — conversation metadata only (id, slug, timestamps)
--   v3      — also includes the project's CLAUDE.md if present
--   v4..v5  — also includes the global CLAUDE.md AND the project's
--             auto-memory files (~/.claude/projects/<id>/memory/)
--
-- The included CLAUDE.md content is what's on disk NOW, not a
-- historical snapshot — the JSONL doesn't carry CLAUDE.md content,
-- and Claude Code doesn't keep per-conversation CLAUDE.md
-- snapshots. A note in the frontmatter calls this out so a reader
-- doesn't assume what's shown is exactly what the conversation saw.
--
-- Timestamp note: mtime and atime are settable post-creation;
-- birth time (`btime` / statx) is not, so it stays as the file's
-- actual creation moment. Most file browsers and tools key off
-- mtime, so the last-message-timestamp anchor is the practical
-- one. atime carries the first-message timestamp for tools that
-- prefer "when did this conversation start?"
--
-- Replaces an earlier wrapper around an external exporter that
-- guessed at which project's conversations to read and got it
-- wrong for projects outside /home/ritz/programming/ai-stuff/.

local DIR = arg and arg[1] or "/mnt/mtwo/programs/sora/soramech"

-- {{{ require dkjson via the project's libs/ path
package.path = DIR .. "/libs/?.lua;" .. package.path
local json = require("dkjson")
-- }}}

-- {{{ paths
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

-- {{{ helpers — slugify
-- Turn arbitrary text into a filename-safe ASCII slug. Lowercases,
-- replaces every non-alphanumeric run with a single hyphen, trims
-- leading/trailing hyphens, caps length so the resulting filename
-- doesn't exceed common filesystem limits (255 chars is typical;
-- 40 leaves room for the short-id suffix and extension).
local function slugify(text, max_len)
    if not text or text == "" then return "untitled" end
    -- Strip ASCII control chars and replace anything non-alnum with -.
    local s = text:lower()
    s = s:gsub("[^a-z0-9]+", "-")
    s = s:gsub("^%-+", ""):gsub("%-+$", "")
    if s == "" then return "untitled" end
    if max_len and #s > max_len then
        s = s:sub(1, max_len):gsub("%-+$", "")
        if s == "" then s = "untitled" end
    end
    return s
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

local function extract_code_blocks(text)
    if not text or text == "" then return {} end
    local blocks = {}
    for code in text:gmatch("```[%w_-]*\n(.-)```") do
        blocks[#blocks + 1] = code
    end
    return blocks
end

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

-- {{{ helpers — auto-included context (CLAUDE.md, memory)
-- Tiered by verbosity per the export feature spec. The content is
-- read from disk at export time; the JSONL itself doesn't carry
-- it, and there's no historical-snapshot mechanism, so the included
-- content is "current" content and may differ from what the
-- conversation actually saw. The frontmatter calls this out.

local function project_claude_md()
    return read_all(DIR .. "/CLAUDE.md")
end

local function global_claude_md()
    return read_all(os.getenv("HOME") .. "/.claude/CLAUDE.md")
end

local function project_memory_files()
    local memdir = SRC_DIR .. "/memory"
    local f = io.popen("ls '" .. memdir .. "'/*.md 2>/dev/null")
    if not f then return {} end
    local files = {}
    for line in f:lines() do
        local content = read_all(line)
        if content then
            files[#files + 1] = {
                name    = line:match("([^/]+)$") or line,
                content = content,
            }
        end
    end
    f:close()
    return files
end

local function frontmatter(meta, level)
    local out = {
        "<!--",
        "Conversation transcript — generated by",
        "scripts/export-llm-transcripts.lua.",
        "",
        "id          : " .. meta.id,
        "slug        : " .. meta.slug,
        "first prompt: " .. (meta.first_user or "(none)"),
        "first ts    : " .. (meta.first_ts or "(unknown)"),
        "last ts     : " .. (meta.last_ts  or "(unknown)"),
        "entry count : " .. meta.n_entries,
        "verbosity   : v" .. level.n .. " " .. level.name,
        "-->",
        "",
    }

    -- v3+ : project CLAUDE.md (if it exists on disk now).
    if level.n >= 3 then
        local pcm = project_claude_md()
        if pcm then
            out[#out + 1] = "## Project CLAUDE.md\n"
            out[#out + 1] = "_Content read from disk at export time; may not"
                         .. " reflect exactly what the conversation saw._\n"
            out[#out + 1] = "```markdown"
            out[#out + 1] = pcm
            out[#out + 1] = "```\n"
        end
    end

    -- v4+ : global CLAUDE.md AND project memory files.
    if level.n >= 4 then
        local gcm = global_claude_md()
        if gcm then
            out[#out + 1] = "## Global CLAUDE.md (~/.claude/CLAUDE.md)\n"
            out[#out + 1] = "_Content read from disk at export time._\n"
            out[#out + 1] = "```markdown"
            out[#out + 1] = gcm
            out[#out + 1] = "```\n"
        end
        local mems = project_memory_files()
        if #mems > 0 then
            out[#out + 1] = "## Project auto-memory ("
                         .. SRC_DIR .. "/memory/)\n"
            out[#out + 1] = "_Snapshot at export time._\n"
            for _, m in ipairs(mems) do
                out[#out + 1] = "### " .. m.name .. "\n"
                out[#out + 1] = "```markdown"
                out[#out + 1] = m.content
                out[#out + 1] = "```\n"
            end
        end
    end

    out[#out + 1] = "---\n"
    return table.concat(out, "\n")
end
-- }}}

-- {{{ render — one conversation at one verbosity level
local function render_v0_minimal(entries)
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

-- {{{ parse one JSONL file → (entries, meta)
-- meta carries: id, slug, first_user (text), first_ts, last_ts,
-- n_entries. Used both for the filename slug and the per-file
-- frontmatter rendering.
local function parse_jsonl(path, conv_id)
    local entries = {}
    local first_user = nil
    local first_ts   = nil
    local last_ts    = nil

    local f = io.open(path, "rb")
    if not f then
        return entries, { id = conv_id, slug = "unreadable" }
    end
    for line in f:lines() do
        if line ~= "" then
            local ok, dec = pcall(json.decode, line)
            if ok and type(dec) == "table" then
                entries[#entries + 1] = dec
                if dec.timestamp then
                    if not first_ts then first_ts = dec.timestamp end
                    last_ts = dec.timestamp
                end
                if (not first_user) and dec.type == "user" then
                    local t = get_user_text(dec)
                    if t and t:match("%S") then
                        first_user = t
                    end
                end
            end
        end
    end
    f:close()

    local slug = slugify(first_user or "untitled", 40)
    return entries, {
        id          = conv_id,
        short_id    = conv_id:sub(1, 8),
        slug        = slug,
        first_user  = first_user,
        first_ts    = first_ts,
        last_ts     = last_ts,
        n_entries   = #entries,
    }
end
-- }}}

-- {{{ set_times — anchor atime / mtime to conversation timestamps
-- Uses `touch -d <iso>` which accepts ISO 8601. atime = first
-- message timestamp; mtime = last message timestamp. btime is
-- not settable post-creation by standard tools; documented in
-- the file-level comment.
local function set_times(path, first_ts, last_ts)
    if last_ts then
        os.execute(string.format(
            "touch -m -d '%s' '%s' 2>/dev/null", last_ts, path))
    end
    if first_ts then
        os.execute(string.format(
            "touch -a -d '%s' '%s' 2>/dev/null", first_ts, path))
    end
end
-- }}}

-- {{{ state file — incremental tracking that survives mtime anchoring
-- Because we anchor each output's mtime to the conversation's
-- last-message timestamp (user-visible feature), the output's
-- mtime intentionally diverges from the source JSONL's mtime —
-- the JSONL is often touched after its last timestamped entry
-- (timestamp-less entries, write buffering, etc.). So we can't
-- use the output's own mtime for the skip check. Instead, a single
-- state file at OUT_DIR/.export-state records the source mtime at
-- the moment of last successful render per conversation; the skip
-- check compares the live source mtime against that record.
local STATE_PATH = OUT_DIR .. "/.export-state"

local function load_state()
    local s = read_all(STATE_PATH)
    local state = {}
    if not s then return state end
    for line in s:gmatch("[^\n]+") do
        local id, ts = line:match("^([^=]+)=(%d+)$")
        if id and ts then state[id] = tonumber(ts) end
    end
    return state
end

local function save_state(state)
    local rows = {}
    for id, ts in pairs(state) do
        rows[#rows + 1] = id .. "=" .. tostring(ts)
    end
    table.sort(rows)
    write_all(STATE_PATH, table.concat(rows, "\n") .. "\n")
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

    local state = load_state()
    local total_rendered = 0
    local total_skipped  = 0
    for _, src_path in ipairs(jsonls) do
        local conv_id = basename_no_ext(src_path, "jsonl")
        local src_m = file_mtime(src_path)

        -- Incremental skip check via the state file. The output
        -- file's own mtime can't be used because we deliberately
        -- backdate it to the conversation's last-message timestamp.
        if state[conv_id] and state[conv_id] >= src_m then
            total_skipped = total_skipped + 1
            goto continue
        end

        -- Parse and render.
        do
        local entries, meta = parse_jsonl(src_path, conv_id)

        local base_name = meta.slug .. "-" .. meta.short_id .. ".md"
        local need_render = true
            for _, lvl in ipairs(LEVELS) do
                local out_path = OUT_DIR .. "/v" .. lvl.n .. "-" .. lvl.name ..
                                 "/" .. base_name
                -- One-time migration: if a previous-version file
                -- named <conv-id>.md exists in this level dir,
                -- remove it so we don't leave orphans behind.
                local old_path = OUT_DIR .. "/v" .. lvl.n .. "-" .. lvl.name ..
                                 "/" .. conv_id .. ".md"
                if file_mtime(old_path) > 0 then
                    os.remove(old_path)
                end

                local body = frontmatter(meta, lvl) .. "\n" ..
                             lvl.render(entries)
                local ok, werr = write_all(out_path, body)
                if not ok then
                    io.stderr:write("  WARN write " .. out_path .. ": " ..
                                    tostring(werr) .. "\n")
                else
                    set_times(out_path, meta.first_ts, meta.last_ts)
                end
            end
            total_rendered = total_rendered + 1
            state[conv_id] = src_m
        end
        ::continue::
    end

    save_state(state)
    io.stderr:write("Rendered " .. total_rendered ..
                    " conversation(s), skipped " .. total_skipped ..
                    " unchanged.\n")
end

main()
-- }}}
