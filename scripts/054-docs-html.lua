#!/usr/bin/env luajit
--[[
054-docs-html.lua — the documentation, generated as one linked site.

What this is: the generator for docs/HTML/ (issue 705). It reads the
markdown documentation — docs/, notes/, the issues open and
completed, every .info.md interface file, and the sealed vision — and
emits cross-linked HTML pages sharing one aesthetic, with a table of
contents down the left of every page, so every page is reachable
from every other.

How it does it, in general terms: generated, never maintained — a
set kept in parallel with the markdown would drift, and then there
are two descriptions of the engine and no way to know which is true.
A small markdown subset is converted (headers, fences with basic C
highlighting, tables, emphasis, links); issue numbers and document
references become links wherever they appear; and the pages that
describe moving parts carry small self-contained interactive pieces —
a ring buffer that wraps and grows under sliders, a readiness check
that fires when clicked full, an iterator dealing to uneven
consumers — because several of the machine's parts are far clearer
manipulated than described.

Usage: luajit 054-docs-html.lua [project-root]
]]

local DIR = arg and arg[1] or nil
if not DIR then
    local self = arg and arg[0] or "."
    DIR = self:match("^(.*)/scripts/[^/]+$") or "."
end

local OUT = DIR .. "/docs/HTML"

-- {{{ local function read_file() / write_file()
local function read_file(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local text = f:read("*a")
    f:close()
    return text
end

local function write_file(path, text)
    local f = io.open(path, "w")
    if not f then
        io.stderr:write("docs-html: cannot write " .. path .. "\n")
        os.exit(1)
    end
    f:write(text)
    f:close()
end
-- }}}

-- {{{ local function list_dir()
-- One directory, one level deep. Still used where the *order* of a
-- listing is the point — the numbered documents are a reading order
-- and folding a subdirectory into them would break the sequence.
local function list_dir(path, pattern)
    local names = {}
    local p = io.popen("ls " .. path .. " 2>/dev/null")
    if p then
        for name in p:lines() do
            if not pattern or name:match(pattern) then
                names[#names + 1] = name
            end
        end
        p:close()
    end
    table.sort(names)
    return names
end
-- }}}

-- {{{ local function list_subdirs()
-- The directories directly inside one, so each can become its own
-- section rather than being missed. This replaced a hand-written pass
-- for the one subdirectory that existed at the time, which was the
-- wrong shape: the next one would have been missed the same way, and
-- missed *silently* — the pages simply would not appear and nothing
-- would say so.
local function list_subdirs(path)
    local names = {}
    local p = io.popen("find " .. path .. " -mindepth 1 -maxdepth 1 -type d "
                       .. "2>/dev/null")
    if p then
        for line in p:lines() do
            names[#names + 1] = line:match("([^/]+)$")
        end
        p:close()
    end
    table.sort(names)
    return names
end
-- }}}

-- {{{ local function walk_for()
-- Every file matching a pattern anywhere under a root, as full paths.
-- Used where a file's *location* is incidental — an interface file
-- lives beside the source it describes, and which directory that is
-- should never decide whether it reaches the site.
--
-- The derived and the foreign are skipped: the generated site itself,
-- the RAM scratch tiers, the repository's own metadata, and the
-- conversation logs, which are large, numerous, and not documentation
-- of the engine.
local function walk_for(root, pattern)
    local paths = {}
    local cmd = "find " .. root .. " -type f -name '" .. pattern .. "' "
             .. "-not -path '*/docs/HTML/*' -not -path '*/tmp/*' "
             .. "-not -path '*/.git/*' -not -path '*/llm-transcripts/*' "
             .. "2>/dev/null"
    local p = io.popen(cmd)
    if p then
        for line in p:lines() do paths[#paths + 1] = line end
        p:close()
    end
    table.sort(paths)
    return paths
end
-- }}}

-- {{{ local function heading_for()
-- A directory name as a sidebar heading: hyphens become spaces and
-- the first letter is raised, so `implementation-notes` reads as
-- "Implementation notes" without anybody maintaining a table of
-- special cases.
local function heading_for(dirname)
    local words = dirname:gsub("%-", " ")
    return words:sub(1, 1):upper() .. words:sub(2)
end
-- }}}

-- ------------------------------------------------------------------
-- Collect the pages.
-- ------------------------------------------------------------------

-- {{{ page collection
local pages = {}          -- ordered
local by_source = {}      -- source basename -> page
local by_issue_number = {}-- "704" -> page

local function add_page(src_path, out_name, title, section)
    local page = {
        src = src_path,
        out = out_name,
        title = title,
        section = section,
    }
    pages[#pages + 1] = page
    by_source[src_path:match("([^/]+)$")] = page
    local issue_number = title:match("^(%d%d%d%a?) ")
    if issue_number and (section:match("issue")) then
        by_issue_number[issue_number] = page
    end
    return page
end

local function title_of(path, fallback)
    local text = read_file(path)
    if text then
        local t = text:match("^#%s*([^\n]+)")
        if t then return t end
    end
    return fallback
end

-- The documents, in reading order.
for _, name in ipairs(list_dir(DIR .. "/docs", "%.md$")) do
    add_page(DIR .. "/docs/" .. name, "doc-" .. name:gsub("%.md$", ".html"),
             title_of(DIR .. "/docs/" .. name, name), "The documents")
end
-- Every subdirectory of docs/, each its own section. The top-level
-- listing above stays one level deep on purpose — those documents are
-- a reading order and a subdirectory folded into them would break the
-- sequence — but a subdirectory is a *group*, and groups enrol
-- themselves here rather than being named one at a time.
--
-- The prefix on the output name is the directory's own, so two
-- subdirectories cannot collide with each other or with the documents
-- above them.
for _, sub in ipairs(list_subdirs(DIR .. "/docs")) do
    if sub ~= "HTML" then
        local here = DIR .. "/docs/" .. sub
        for _, name in ipairs(list_dir(here, "%.md$")) do
            add_page(here .. "/" .. name,
                     sub:sub(1, 4) .. "-" .. name:gsub("%.md$", ".html"),
                     title_of(here .. "/" .. name, name),
                     heading_for(sub))
        end
    end
end
-- The sealed vision.
add_page(DIR .. "/vision", "vision.html", "vision (sealed)", "The beginning")
-- Notes.
for _, name in ipairs(list_dir(DIR .. "/notes", "%.md$")) do
    add_page(DIR .. "/notes/" .. name, "note-" .. name:gsub("%.md$", ".html"),
             title_of(DIR .. "/notes/" .. name, name), "Notes")
end
-- Open issues, then completed.
for _, name in ipairs(list_dir(DIR .. "/issues", "%.md$")) do
    add_page(DIR .. "/issues/" .. name, "issue-" .. name:gsub("%.md$", ".html"),
             title_of(DIR .. "/issues/" .. name, name), "Issues, open")
end
for _, name in ipairs(list_dir(DIR .. "/issues/completed", "%.md$")) do
    add_page(DIR .. "/issues/completed/" .. name,
             "done-" .. name:gsub("%.md$", ".html"),
             title_of(DIR .. "/issues/completed/" .. name, name),
             "Issues, completed")
end
-- Interface files, wherever they live — found by walking rather than
-- by naming the places they have lived so far. An interface file sits
-- beside the source it describes, and which directory that is should
-- never be what decides whether it reaches the site.
for _, path in ipairs(walk_for(DIR, "*.info.md")) do
    local name = path:match("([^/]+)$")
    add_page(path,
             "info-" .. name:gsub("%.info%.md$", ".html"):gsub("%.", "-", 1),
             title_of(path, name),
             "Interfaces")
end
-- }}}

-- ------------------------------------------------------------------
-- Markdown, converted.
-- ------------------------------------------------------------------

-- {{{ local function escape_html()
local function escape_html(s)
    s = s:gsub("&", "&amp;")
    s = s:gsub("<", "&lt;")
    s = s:gsub(">", "&gt;")
    return s
end
-- }}}

-- {{{ local function highlight_c()
-- Enough highlighting to make code read as code: keywords, types,
-- comments, strings. Applied to already-escaped text.
local KEYWORDS = {
    "if", "else", "for", "while", "return", "switch", "case", "break",
    "continue", "typedef", "struct", "enum", "static", "const", "extern",
    "sizeof", "void", "int", "char", "long", "short", "float", "double",
    "unsigned", "signed",
}
local KEYWORD_SET = {}
for _, k in ipairs(KEYWORDS) do KEYWORD_SET[k] = true end

local function highlight_c(escaped)
    -- Comments and strings first, spans protected with placeholders.
    local saved = {}
    local function save(class, text)
        saved[#saved + 1] = ('<span class="%s">%s</span>'):format(class, text)
        return "\1" .. #saved .. "\1"
    end
    escaped = escaped:gsub("(/%*.-%*/)", function(c) return save("cm", c) end)
    escaped = escaped:gsub("(//[^\n]*)", function(c) return save("cm", c) end)
    escaped = escaped:gsub("(&quot;.-&quot;)", function(s) return save("st", s) end)
    escaped = escaped:gsub("(\"[^\"\n]*\")", function(s) return save("st", s) end)
    escaped = escaped:gsub("([%a_][%w_]*)", function(word)
        if KEYWORD_SET[word] then
            return '<span class="kw">' .. word .. "</span>"
        end
        return word
    end)
    escaped = escaped:gsub("\1(%d+)\1", function(i)
        return saved[tonumber(i)]
    end)
    return escaped
end
-- }}}

-- {{{ local function linkify()
-- Issue numbers become links wherever they appear; markdown links to
-- other documents become page links. Unresolvable references are
-- reported, never silently rendered as text.
local unresolved = {}

local function link_for_source(name)
    local page = by_source[name]
    return page and page.out or nil
end

local function linkify(html, self_page)
    html = html:gsub("([Ii]ssues?%s)(%d%d%d%a?)", function(prefix, number)
        local page = by_issue_number[number]
        if page then
            return prefix .. ('<a href="%s">%s</a>'):format(page.out, number)
        end
        return prefix .. number
    end)
    html = html:gsub("%[([^%]]+)%]%(([^%)]+)%)", function(text, target)
        if target:match("^https?://") then
            return ('<a href="%s">%s</a>'):format(target, text)
        end
        local name = target:match("([^/]+)$")
        local out = link_for_source(name)
        if out then
            return ('<a href="%s">%s</a>'):format(out, text)
        end
        unresolved[#unresolved + 1] =
            (self_page.src .. " -> " .. target)
        return text
    end)
    return html
end
-- }}}

-- {{{ local function markdown_to_html()
local function markdown_to_html(text, page)
    local out = {}
    local in_code = false
    local in_table = false
    local in_list = false
    local paragraph = {}

    local function flush_paragraph()
        if #paragraph > 0 then
            out[#out + 1] = "<p>" .. table.concat(paragraph, "\n") .. "</p>"
            paragraph = {}
        end
    end
    local function close_list()
        if in_list then out[#out + 1] = "</ul>"; in_list = false end
    end
    local function close_table()
        if in_table then out[#out + 1] = "</table>"; in_table = false end
    end

    local function inline(s)
        s = escape_html(s)
        -- Code spans first, protected from emphasis.
        local saved = {}
        s = s:gsub("`([^`]+)`", function(code)
            saved[#saved + 1] = "<code>" .. code .. "</code>"
            return "\1" .. #saved .. "\1"
        end)
        s = s:gsub("%*%*([^%*]+)%*%*", "<strong>%1</strong>")
        s = s:gsub("%*([%w][^%*\n]-)%*", "<em>%1</em>")
        s = s:gsub("\1(%d+)\1", function(i) return saved[tonumber(i)] end)
        return s
    end

    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        if in_code then
            if line:match("^```") then
                out[#out + 1] = "</pre>"
                in_code = false
            else
                out[#out + 1] = highlight_c(escape_html(line))
            end
        elseif line:match("^```") then
            flush_paragraph(); close_list(); close_table()
            out[#out + 1] = '<pre class="code">'
            in_code = true
        elseif line:match("^#") then
            flush_paragraph(); close_list(); close_table()
            local hashes, title = line:match("^(#+)%s*(.*)")
            local level = math.min(#hashes, 4)
            out[#out + 1] = ("<h%d>%s</h%d>"):format(level, inline(title), level)
        elseif line:match("^%s*|") then
            flush_paragraph(); close_list()
            if line:match("^%s*|[%s%-|]*$") then
                -- the separator row; nothing to emit
            else
                if not in_table then
                    out[#out + 1] = "<table>"
                    in_table = true
                end
                local cells = {}
                for cell in line:gmatch("|([^|]*)") do
                    cells[#cells + 1] = "<td>" .. inline(cell:match("^%s*(.-)%s*$")) .. "</td>"
                end
                if cells[#cells] == "<td></td>" then
                    cells[#cells] = nil
                end
                out[#out + 1] = "<tr>" .. table.concat(cells) .. "</tr>"
            end
        elseif line:match("^%s*[-*]%s+") then
            flush_paragraph(); close_table()
            if not in_list then
                out[#out + 1] = "<ul>"
                in_list = true
            end
            out[#out + 1] = "<li>" .. inline(line:gsub("^%s*[-*]%s+", "")) .. "</li>"
        elseif line:match("^%s*$") then
            flush_paragraph(); close_list(); close_table()
        elseif line:match("^>%s?") then
            flush_paragraph(); close_list(); close_table()
            out[#out + 1] = "<blockquote>" .. inline(line:gsub("^>%s?", "")) .. "</blockquote>"
        else
            paragraph[#paragraph + 1] = inline(line)
        end
    end
    flush_paragraph(); close_list(); close_table()
    return linkify(table.concat(out, "\n"), page)
end
-- }}}

-- ------------------------------------------------------------------
-- The shared shell: style and sidebar.
-- ------------------------------------------------------------------

-- {{{ the stylesheet
local STYLE = [[
:root {
  --bg: #14161a; --panel: #1b1e24; --ink: #d6d9de; --dim: #8b919b;
  --wire: #4f9cf9; --box: #f9b44f; --ok: #67c587; --warn: #e0705f;
  --code-bg: #101215;
}
* { box-sizing: border-box; }
body {
  margin: 0; background: var(--bg); color: var(--ink);
  font: 15px/1.6 Georgia, 'Times New Roman', serif;
}
.wrap { display: flex; min-height: 100vh; }
nav {
  width: 280px; flex-shrink: 0; background: var(--panel);
  padding: 18px; overflow-y: auto; height: 100vh; position: sticky; top: 0;
  border-right: 1px solid #2a2e36;
  font-family: 'DejaVu Sans Mono', Menlo, monospace; font-size: 12px;
}
.text-size {
  position: fixed; right: 14px; bottom: 12px; z-index: 5;
  display: flex; align-items: center; gap: 2px;
  background: var(--panel); border: 1px solid #2a2e36; border-radius: 6px;
  padding: 2px 4px; font-size: 12px;
  font-family: 'DejaVu Sans Mono', Menlo, monospace;
}
.text-size button {
  font: inherit; background: none; border: none; color: var(--dim);
  cursor: pointer; padding: 2px 7px; border-radius: 4px;
}
.text-size button:hover { color: var(--ink); background: #2a2e36; }
#text-size-now { color: var(--box); min-width: 40px; text-align: center; }

nav h2 { color: var(--box); font-size: 12px; letter-spacing: 1px;
  text-transform: uppercase; margin: 18px 0 6px; }
nav a { display: block; color: var(--dim); text-decoration: none;
  padding: 1px 0; white-space: nowrap; overflow: hidden;
  text-overflow: ellipsis; }
nav a:hover { color: var(--wire); }
nav a.here { color: var(--ink); }
/* The one link out of the reading order, marked so it does not read as
 * the first chapter (issue 801). */
nav a.elsewhere { color: var(--box); border-bottom: 1px solid #2a2e36;
  padding-bottom: 10px; margin-bottom: 8px; }
nav a.elsewhere:hover { color: var(--wire); }
main { padding: 34px 44px; max-width: 860px; }
h1, h2, h3, h4 { font-family: 'DejaVu Sans Mono', Menlo, monospace;
  color: var(--box); line-height: 1.3; }
h1 { font-size: 24px; border-bottom: 1px solid #2a2e36; padding-bottom: 10px; }
h2 { font-size: 18px; margin-top: 34px; }
a { color: var(--wire); }
code { background: var(--code-bg); padding: 1px 5px; border-radius: 3px;
  font: 13px 'DejaVu Sans Mono', Menlo, monospace; color: var(--ok); }
pre.code { background: var(--code-bg); padding: 14px; border-radius: 6px;
  overflow-x: auto; font: 13px/1.5 'DejaVu Sans Mono', Menlo, monospace;
  border-left: 3px solid var(--wire); }
pre.code .kw { color: var(--wire); }
pre.code .cm { color: var(--dim); font-style: italic; }
pre.code .st { color: var(--ok); }
table { border-collapse: collapse; margin: 14px 0; }
td { border: 1px solid #2a2e36; padding: 5px 12px; }
tr:first-child td { color: var(--box);
  font-family: 'DejaVu Sans Mono', Menlo, monospace; font-size: 13px; }
blockquote { border-left: 3px solid var(--box); margin: 14px 0;
  padding: 4px 18px; color: var(--box); background: var(--panel); }
.widget { background: var(--panel); border: 1px solid #2a2e36;
  border-radius: 8px; padding: 18px; margin: 26px 0;
  font-family: 'DejaVu Sans Mono', Menlo, monospace; font-size: 13px; }
.widget h3 { margin-top: 0; }
.widget .cells { display: flex; gap: 4px; margin: 10px 0; flex-wrap: wrap; }
.widget .cell { width: 34px; height: 34px; border: 1px solid #3a3f48;
  border-radius: 4px; display: flex; align-items: center;
  justify-content: center; color: var(--dim); cursor: default; }
.widget .cell.full { background: #24405f; color: var(--ink);
  border-color: var(--wire); }
.widget .cell.head { border-color: var(--ok); }
.widget .cell.tail { border-color: var(--box); }
.widget .cell.slot { cursor: pointer; width: 60px; height: 44px; }
.widget button { background: #24405f; color: var(--ink); border: none;
  border-radius: 4px; padding: 6px 12px; cursor: pointer;
  font-family: inherit; }
.widget button:hover { background: #2d517a; }
.widget input[type=range] { width: 160px; }
.widget .note { color: var(--dim); }
.widget .fired { color: var(--ok); }
.widget .bar { height: 14px; background: var(--wire); border-radius: 3px; }
]]
-- }}}

-- {{{ the widgets
local WIDGET_RING = [[
<div class="widget" id="ringwidget">
<h3>the ring buffer, by hand</h3>
<div>capacity <input type="range" id="rw-cap" min="4" max="12" value="6">
<span id="rw-capv">6</span>
&nbsp; <button id="rw-push">write</button>
<button id="rw-pop">pop</button></div>
<div class="cells" id="rw-cells"></div>
<div class="note" id="rw-note">green border marks the head, orange the
tail. fill it and the next write grows the ring — watch the unwrap.</div>
<script>
(function(){
var cap=6, head=0, tail=0, seq=1, held=[];
function draw(){
  var box=document.getElementById('rw-cells'); box.innerHTML='';
  for(var i=0;i<cap;i++){
    var c=document.createElement('div'); c.className='cell';
    var idx=(i-head+cap)%cap;
    if(idx<held.length){ c.className+=' full'; c.textContent=held[idx]; }
    if(i===head) c.className+=' head';
    if(i===tail) c.className+=' tail';
    box.appendChild(c);
  }
  document.getElementById('rw-capv').textContent=cap;
}
document.getElementById('rw-cap').oninput=function(){
  cap=+this.value; if(held.length>cap-1) held=held.slice(0,cap-1);
  head=0; tail=held.length%cap; draw();
};
document.getElementById('rw-push').onclick=function(){
  if(held.length===cap-1){
    cap=cap*2; head=0; tail=held.length;
    document.getElementById('rw-cap').value=Math.min(cap,12);
    document.getElementById('rw-note').textContent=
      'it grew: storage doubled, the wrapped contents were copied '+
      'straight, head reset to zero — and nothing pointing at the '+
      'station noticed anything.';
  }
  held.push(seq++); tail=(head+held.length)%cap; draw();
};
document.getElementById('rw-pop').onclick=function(){
  if(held.length){ held.shift(); head=(head+1)%cap;
    tail=(head+held.length)%cap; }
  draw();
};
draw();
})();
</script>
</div>
]]

local WIDGET_READY = [[
<div class="widget" id="readywidget">
<h3>the readiness check, by click</h3>
<div class="cells">
<div class="cell slot" id="rd-0">slot 0</div>
<div class="cell slot" id="rd-1">slot 1</div>
<div class="cell slot" id="rd-2">slot 2</div>
</div>
<div class="note" id="rd-note">click slots to deliver values. the
station runs when, and only when, every slot holds one — and the
values are claimed on the spot.</div>
<div id="rd-count" class="fired"></div>
<script>
(function(){
var full=[false,false,false], fired=0;
function draw(){
  for(var i=0;i<3;i++){
    var el=document.getElementById('rd-'+i);
    el.className='cell slot'+(full[i]?' full':'');
    el.textContent=full[i]?'value':'slot '+i;
  }
}
function click(i){ return function(){
  full[i]=true;
  if(full[0]&&full[1]&&full[2]){
    fired++; full=[false,false,false];
    document.getElementById('rd-count').textContent=
      'task fired: '+fired+' — the claim emptied every slot at once';
  }
  draw();
};}
for(var i=0;i<3;i++) document.getElementById('rd-'+i).onclick=click(i);
draw();
})();
</script>
</div>
]]

local WIDGET_ITERATOR = [[
<div class="widget" id="iterwidget">
<h3>the iterator: fair counts, unfair order</h3>
<div>slow consumer costs <input type="range" id="it-slow" min="1" max="9" value="6">
<span id="it-slowv">6</span>x
&nbsp; <button id="it-run">send 30 values</button></div>
<div style="margin-top:10px">
<div>consumer 0 (fast) <span class="bar" id="it-b0" style="display:inline-block;width:0"></span> <span id="it-c0"></span></div>
<div>consumer 1 (slow) <span class="bar" id="it-b1" style="display:inline-block;width:0"></span> <span id="it-c1"></span></div>
<div>consumer 2 (fast) <span class="bar" id="it-b2" style="display:inline-block;width:0"></span> <span id="it-c2"></span></div>
</div>
<div class="note" id="it-order"></div>
<script>
(function(){
document.getElementById('it-slow').oninput=function(){
  document.getElementById('it-slowv').textContent=this.value;
};
document.getElementById('it-run').onclick=function(){
  var slow=+document.getElementById('it-slow').value;
  var counts=[0,0,0], arrivals=[], pending=[];
  for(var v=0;v<30;v++){
    var port=v%3;               // the cursor, at enqueue time
    var cost=(port===1)?slow:1; // uneven durations
    pending.push({port:port, done:v*0.1+cost});
  }
  pending.sort(function(a,b){return a.done-b.done;});
  for(var i=0;i<pending.length;i++){
    counts[pending[i].port]++; arrivals.push(pending[i].port);
  }
  for(var p=0;p<3;p++){
    document.getElementById('it-b'+p).style.width=(counts[p]*8)+'px';
    document.getElementById('it-c'+p).textContent=counts[p];
  }
  document.getElementById('it-order').textContent=
    'arrival order: '+arrivals.join('')+' — counts even ('+
    counts.join('/')+'), order scrambled. a spreader, not a funnel.';
};
})();
</script>
</div>
]]

-- Which pages carry which widget, keyed by source basename.
local WIDGETS = {
    ["002-stations-and-ports.md"] = WIDGET_RING,
    ["003-datapath-delivery.md"] = WIDGET_READY,
    ["005-routing.md"] = WIDGET_ITERATOR,
}
-- }}}

-- {{{ local function sidebar_html()
local function sidebar_html(current)
    local out = { '<nav>' }
    out[#out + 1] = ('<h2>minimal soramech</h2>')
    -- **A peer, not a chapter** (issue 801). The workbench sits above
    -- the numbered reading order rather than inside it: a reader
    -- working down that order should not meet a tool where a document
    -- was promised. It is offered here because a door nobody can find
    -- is a door nobody opens.
    out[#out + 1] = '<a href="workbench.html" class="elsewhere">workbench &rarr;</a>'
    local last_section = nil
    for _, page in ipairs(pages) do
        if page.section ~= last_section then
            out[#out + 1] = ("<h2>%s</h2>"):format(escape_html(page.section))
            last_section = page.section
        end
        out[#out + 1] = ('<a href="%s"%s>%s</a>'):format(
            page.out, page == current and ' class="here"' or "",
            escape_html(page.title))
    end
    out[#out + 1] = "</nav>"
    return table.concat(out, "\n")
end
-- }}}

-- ------------------------------------------------------------------
-- Emit.
-- ------------------------------------------------------------------

os.execute("mkdir -p " .. OUT)
write_file(OUT .. "/style.css", STYLE)

for _, page in ipairs(pages) do
    local text = read_file(page.src) or "(missing)"
    local body
    if page.src:match("/vision$") then
        -- The sealed note, rendered as it stands, untouched.
        body = "<h1>the vision, sealed</h1><pre class=\"code\">"
             .. escape_html(text) .. "</pre>"
    else
        body = markdown_to_html(text, page)
    end
    local widget = WIDGETS[page.src:match("([^/]+)$")] or ""
    local html = ([[<!DOCTYPE html>
<html><head><meta charset="utf-8">
<title>%s</title>
<link rel="stylesheet" href="style.css">
</head><body><div class="wrap">
%s
<main>
%s
%s
</main>
</div><script src="keeping-your-place.js"></script></body></html>
]]):format(escape_html(page.title), sidebar_html(page), body, widget)
    write_file(OUT .. "/" .. page.out, html)
end

-- {{{ keeping your place across pages
--
-- Every page here is a whole document, so following a link throws away
-- everything the browser was holding: the index scrolls back to the top
-- and the text goes back to whatever size the browser thinks is right.
-- With a hundred and fifty entries in that index, losing your place in
-- it on every click is most of what makes a reference unpleasant to
-- read.
--
-- Both are kept in the browser's own per-tab storage, so they survive a
-- link and vanish when the tab does. Nothing is written anywhere else.
write_file(OUT .. "/keeping-your-place.js", [==[
/*
 * keeping-your-place.js — the index stays where you left it, and the
 * text stays the size you asked for.
 *
 * Generated by scripts/054-docs-html.lua; edit that.
 */
(function () {
    var nav = document.querySelector("nav");
    var root = document.documentElement;

    /* {{{ the text size */
    /*
     * A browser's own zoom is a browser setting and a page cannot read
     * or keep it, so this is a size of our own that can be. It rides on
     * the root element, which every other size here is written against.
     */
    var STEP = 1.1, MIN = 0.7, MAX = 2.2;
    var size = parseFloat(localStorage.getItem("cera-text-size") || "1") || 1;

    function apply() {
        root.style.fontSize = (size * 100) + "%";
        try { localStorage.setItem("cera-text-size", String(size)); } catch (e) {}
        var say = document.getElementById("text-size-now");
        if (say) say.textContent = Math.round(size * 100) + "%";
    }

    function resize(by) {
        size = Math.min(MAX, Math.max(MIN, by === 0 ? 1 : size * by));
        apply();
    }
    /* }}} */

    /* {{{ the controls */
    var bar = document.createElement("div");
    bar.className = "text-size";
    bar.innerHTML =
        '<button type="button" title="smaller text">&minus;</button>' +
        '<span id="text-size-now">100%</span>' +
        '<button type="button" title="larger text">+</button>' +
        '<button type="button" title="back to the usual size">reset</button>';
    var buttons = bar.getElementsByTagName("button");
    buttons[0].onclick = function () { resize(1 / STEP); };
    buttons[1].onclick = function () { resize(STEP); };
    buttons[2].onclick = function () { resize(0); };
    document.body.appendChild(bar);

    window.addEventListener("keydown", function (e) {
        if (e.target !== document.body) return;
        if (e.key === "+" || e.key === "=") resize(STEP);
        else if (e.key === "-" || e.key === "_") resize(1 / STEP);
        else if (e.key === "0") resize(0);
        else return;
        e.preventDefault();
    });
    apply();
    /* }}} */

    /* {{{ where you were in the index */
    /*
     * Restored before the first paint where possible, so the index does
     * not visibly jump. If there is nothing remembered — a first visit,
     * or a link followed from outside — the page being read is brought
     * into view instead, which is a better guess than the top.
     */
    if (!nav) return;
    var KEY = "cera-nav-scroll";

    var kept = sessionStorage.getItem(KEY);
    if (kept !== null) {
        nav.scrollTop = parseInt(kept, 10) || 0;
    } else {
        var here = nav.querySelector("a.here");
        if (here && here.scrollIntoView)
            here.scrollIntoView({ block: "center" });
    }

    /* A link out of this page is the only moment worth recording: the
     * scroll as it stands when you leave is the place to come back to. */
    nav.addEventListener("click", function () {
        try { sessionStorage.setItem(KEY, String(nav.scrollTop)); } catch (e) {}
    }, true);
    window.addEventListener("pagehide", function () {
        try { sessionStorage.setItem(KEY, String(nav.scrollTop)); } catch (e) {}
    });
    /* }}} */
})();
]==])
-- }}}

-- The front door: index redirects to the table of contents.
write_file(OUT .. "/index.html",
    '<!DOCTYPE html><meta charset="utf-8">' ..
    '<meta http-equiv="refresh" content="0; url=doc-000-table-of-contents.html">')

-- {{{ the workbench, carried in rather than generated
--
-- **A third front door** (issue 801): the site is something to read
-- and the workbench is something to use, and they share a stylesheet
-- and an aesthetic so it is unmistakably one project.
--
-- It is *copied* rather than generated, which is the one thing in this
-- output that is not derived from a document. It is still derived —
-- from the files under workbench/ — so the claim the sweep rests on
-- holds: nothing here is authored in place, and nothing here can be
-- lost by being deleted.
--
-- Copied rather than linked, because a link into the repository would
-- make the published site depend on where it was built.
--
-- Its markup is the page's content only. The skeleton around it — the
-- doctype, the stylesheets, the script — is written here, so that the
-- workbench cannot drift from the site by forgetting to include
-- something the site changed.
local workbench = read_file(DIR .. "/workbench/102-workbench.html")
if workbench then
    write_file(OUT .. "/workbench.css",
               read_file(DIR .. "/workbench/103-workbench.css") or "")
    write_file(OUT .. "/workbench.js",
               read_file(DIR .. "/workbench/104-workbench.js") or "")
    write_file(OUT .. "/workbench.html", ([[
<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<title>workbench — minimal soramech</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="style.css">
<link rel="stylesheet" href="workbench.css">
</head><body>
%s
<script src="workbench.js"></script>
</body></html>
]]):format(workbench))
end
-- }}}

-- {{{ the sweep
--
-- **Anything in the output that no source produces is deleted.**
--
-- The generator only ever wrote, which meant a renamed document left
-- its old page sitting there: stale content, stale links, and a URL
-- that still worked. Found by renaming an issue and having the
-- previous page survive; met again every time a document moved or an
-- issue was completed, each of which had to be cleaned up by hand.
--
-- That is the whole argument for doing it here. Anything a generator
-- does not do automatically is something a person has to remember,
-- and the failure of remembering is silent — a stale page does not
-- announce itself, it just quietly disagrees with the project.
--
-- Deleting is safe because this directory is *entirely* derived:
-- every file in it is written by this run or is left over from an
-- older one. Nothing here is authored, so nothing here can be lost.
local kept = {
    ["index.html"] = true, ["style.css"] = true,
    -- Written by this run but not derived from any document, so the
    -- page list the sweep is built from does not mention them. The
    -- sweep deleted this one the first time, on its next line, which is
    -- the sweep working exactly as intended and worth leaving a note
    -- about: a file this generator writes has to be named here too.
    ["keeping-your-place.js"] = true,
    -- Derived from workbench/ rather than from a document.
    ["workbench.html"] = true, ["workbench.css"] = true,
    ["workbench.js"] = true,
}
for _, page in ipairs(pages) do kept[page.out] = true end

local swept = 0
local listing = io.popen("ls " .. OUT .. " 2>/dev/null")
if listing then
    local strays = {}
    for name in listing:lines() do
        if not kept[name] then strays[#strays + 1] = name end
    end
    listing:close()
    for _, name in ipairs(strays) do
        os.remove(OUT .. "/" .. name)
        swept = swept + 1
    end
end
-- }}}

-- Reachability: every page carries the full sidebar, so reachability
-- is structural; the check verifies the sidebar really lists all.
local sample = read_file(OUT .. "/" .. pages[1].out)
local listed = 0
for _ in sample:gmatch('<nav>.-</nav>') do end
for _, page in ipairs(pages) do
    if sample:find(page.out, 1, true) then listed = listed + 1 end
end
if listed < #pages then
    io.stderr:write(("docs-html: sidebar lists %d of %d pages\n")
        :format(listed, #pages))
    os.exit(1)
end

if #unresolved > 0 then
    io.stderr:write("docs-html: unresolved references:\n")
    for _, u in ipairs(unresolved) do
        io.stderr:write("  " .. u .. "\n")
    end
end

-- Said out loud rather than done quietly: a sweep that removes a page
-- somebody expected to be there should be visible in the build log
-- they are already reading.
print(("docs-html: %d pages into %s (%d unresolved references%s)")
    :format(#pages, OUT, #unresolved,
            swept > 0 and (", %d stale swept"):format(swept) or ""))
