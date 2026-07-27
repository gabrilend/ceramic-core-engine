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
-- Interface files, wherever they live.
for _, place in ipairs({ "libs", "src", "src/boxes", "scripts" }) do
    for _, name in ipairs(list_dir(DIR .. "/" .. place, "%.info%.md$")) do
        add_page(DIR .. "/" .. place .. "/" .. name,
                 "info-" .. name:gsub("%.info%.md$", ".html"):gsub("%.", "-", 1),
                 title_of(DIR .. "/" .. place .. "/" .. name, name),
                 "Interfaces")
    end
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
nav h2 { color: var(--box); font-size: 12px; letter-spacing: 1px;
  text-transform: uppercase; margin: 18px 0 6px; }
nav a { display: block; color: var(--dim); text-decoration: none;
  padding: 1px 0; white-space: nowrap; overflow: hidden;
  text-overflow: ellipsis; }
nav a:hover { color: var(--wire); }
nav a.here { color: var(--ink); }
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
    ["002-stations-and-slots.md"] = WIDGET_RING,
    ["003-datapath-delivery.md"] = WIDGET_READY,
    ["005-routing.md"] = WIDGET_ITERATOR,
}
-- }}}

-- {{{ local function sidebar_html()
local function sidebar_html(current)
    local out = { '<nav>' }
    out[#out + 1] = ('<h2>minimal soramech</h2>')
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
</div></body></html>
]]):format(escape_html(page.title), sidebar_html(page), body, widget)
    write_file(OUT .. "/" .. page.out, html)
end

-- The front door: index redirects to the table of contents.
write_file(OUT .. "/index.html",
    '<!DOCTYPE html><meta charset="utf-8">' ..
    '<meta http-equiv="refresh" content="0; url=doc-000-table-of-contents.html">')

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

print(("docs-html: %d pages into %s (%d unresolved references)")
    :format(#pages, OUT, #unresolved))
