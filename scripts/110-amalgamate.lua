-- 110-amalgamate.lua — the engine's eighteen files become two, once.
--
-- What this is: a one-shot migration tool for issue 901. It reads the
-- numbered engine headers and bodies and writes src/cera.h and
-- src/cera.c, which from then on are the engine's source rather than
-- anything derived. Once issue 904 deletes the numbered files this
-- script has no inputs and is removed; it exists so the move is a
-- repeatable mechanical act during the days when both halves are on
-- disk, rather than a large hand-edit nobody can check.
--
-- How it does it, in general terms: concatenate in dependency order,
-- one section per original file, and take out only the two things that
-- stop making sense in one file — a per-file include guard, and one
-- engine file including another. Every line taken out is replaced by a
-- blank line rather than deleted, so line numbers inside a section are
-- unchanged and the #line directive at each seam makes compiler errors
-- and debugger backtraces point at the original numbered source.
--
-- What it deliberately does not do: rename anything, mark anything
-- static, move any declaration between the header and the body. Those
-- are issues 902 and 903. A step that both moves code and changes it
-- cannot be checked by comparing output, and comparing output is the
-- only proof available for this one.
--
-- Usage: luajit 110-amalgamate.lua [project-dir]

-- {{{ configuration
local DIR = arg[1] or "/mnt/mtwo/programming/ai-playground/minimal-soramech"

-- The headers, in dependency order: each needs the ones above it.
local HEADERS = {
  { path = "libs/011-pool.h",     what = "the pool — worker threads and the task ring" },
  { path = "src/018-station.h",   what = "stations, ports, wires, and the construction surface" },
  { path = "src/026-emitted.h",   what = "what the generator emits, and how a value becomes text" },
  { path = "src/040-mapfile.h",   what = "loading a description, and a map placed inside a map" },
  { path = "src/049-observe.h",   what = "reports, the observer thread, the dump, rewiring" },
  { path = "src/073-latebox.h",   what = "code that arrives after the build" },
  { path = "src/091-stopping.h",  what = "ending a program, and putting one down" },
}

-- The bodies, in their existing numbered order.
local BODIES = {
  { path = "libs/012-pool.c",           what = "the pool",                    undef = { "POOL_INITIAL_CAPACITY" } },
  { path = "src/019-station.c",         what = "the station table" },
  { path = "src/020-delivery.c",        what = "delivery, readiness, routing", undef = { "STATS_MARK", "STATS_CHARGE" } },
  { path = "src/027-emitted-support.c", what = "support for generated code" },
  { path = "src/033-statics.c",         what = "constants, and values from text" },
  { path = "src/042-loader.c",          what = "reading a description" },
  { path = "src/050-observe.c",         what = "reports and the observer",     undef = { "GROWTH_SHOUT_THRESHOLD" } },
  { path = "src/051-dump.c",            what = "a live map written back out" },
  { path = "src/052-rewire.c",          what = "changing a running program" },
  { path = "src/074-latebox.c",         what = "boxes and maps compiled at run time" },
  { path = "src/092-stopping.c",        what = "signals, capture, and the end" },
}

-- The build-time facts (which compiler, where the generator is, the two
-- RAM tiers) are NOT undefined at the end of the section that falls back
-- on them: two sections use them — the one that compiles code at run
-- time and the one that writes a report on the way out — and in one file
-- the second would lose them. They come from the command line in an
-- ordinary build and the fallbacks never fire.
-- }}}

-- {{{ local function read_lines(path)
local function read_lines(path)
  local f = assert(io.open(path, "r"), "cannot read " .. path)
  local lines = {}
  for line in f:lines() do lines[#lines + 1] = line end
  f:close()
  return lines
end
-- }}}

-- {{{ local function strip(lines, drop_guard)
-- Blank out the lines that stop making sense in one file, keeping the
-- line count exactly, so #line stays honest.
--
-- Two kinds go: an engine file including another engine file (every
-- header is above every body now), and — for headers only — the file's
-- own include guard, whose two opening lines and one closing #endif are
-- replaced by the single guard cera.h carries.
local function strip(lines, drop_guard)
  local guard_name = nil
  local last_endif = nil

  for i, line in ipairs(lines) do
    if line:match('^%s*#%s*include%s+"%d%d%d%-[%w%-]+%.h"') then
      lines[i] = ""
    end
    if drop_guard then
      local ifndef = line:match("^%s*#%s*ifndef%s+(SORA_%w+_H)%s*$")
      if ifndef and not guard_name then
        guard_name = ifndef
        lines[i] = ""
      elseif guard_name and line:match("^%s*#%s*define%s+" .. guard_name .. "%s*$") then
        lines[i] = ""
      end
      if line:match("^%s*#%s*endif") then last_endif = i end
    end
  end

  -- The guard's closing #endif is the last one in the file.
  if drop_guard and last_endif then lines[last_endif] = "" end
  return lines
end
-- }}}

-- {{{ local function banner(out, number, name, what)
local function banner(out, number, name, what)
  out[#out + 1] = ""
  out[#out + 1] = "/* " .. string.rep("=", 66)
  out[#out + 1] = " *"
  out[#out + 1] = " * " .. number .. " — " .. what
  out[#out + 1] = " *"
  out[#out + 1] = " * Was " .. name .. ". The number is this section's position in the"
  out[#out + 1] = " * reading order, which is the only thing the filename ever said."
  out[#out + 1] = " * " .. string.rep("=", 66) .. " */"
end
-- }}}

-- {{{ local function emit_section(out, entry, drop_guard)
local function emit_section(out, entry, drop_guard)
  local number, name = entry.path:match("([%d]+)%-([%w%-%.]+)$")
  banner(out, number, entry.path, entry.what)
  out[#out + 1] = string.format('#line 1 "%s/%s"', DIR, entry.path)

  local lines = strip(read_lines(DIR .. "/" .. entry.path), drop_guard)
  for _, line in ipairs(lines) do out[#out + 1] = line end

  -- A #define died at the end of its file; in one file it would run to
  -- the bottom. Each section takes its private macros with it.
  if entry.undef then
    out[#out + 1] = ""
    out[#out + 1] = "/* " .. number .. "'s private macros end with " .. number .. ". */"
    for _, m in ipairs(entry.undef) do out[#out + 1] = "#undef " .. m end
  end
  return number, name
end
-- }}}

-- {{{ local function write_file(path, lines)
local function write_file(path, lines)
  local f = assert(io.open(path, "w"), "cannot write " .. path)
  f:write(table.concat(lines, "\n"))
  f:write("\n")
  f:close()
  print(string.format("wrote %s (%d lines)", path, #lines))
end
-- }}}

-- {{{ the header
local h = {}
h[#h + 1] = "/*"
h[#h + 1] = " * cera.h — everything a program built with this engine may call."
h[#h + 1] = " *"
h[#h + 1] = " * What this is: the engine's whole interface, in one file. Take this"
h[#h + 1] = " * and cera.c beside it and you have the engine; there is nothing"
h[#h + 1] = " * else to install and no include path to configure beyond the"
h[#h + 1] = " * directory these two sit in."
h[#h + 1] = " *"
h[#h + 1] = " * How it is arranged: seven sections, in dependency order, each one"
h[#h + 1] = " * formerly a numbered header. The numbers are kept because they are"
h[#h + 1] = " * this project's reading order and the sections are still a story"
h[#h + 1] = " * meant to be read in sequence — the pool knows nothing of stations,"
h[#h + 1] = " * stations know nothing of the generator, and so on upward."
h[#h + 1] = " *"
h[#h + 1] = " * What it does not yet do is distinguish what a consumer may call"
h[#h + 1] = " * from how the engine talks to itself. Every declaration here was in"
h[#h + 1] = " * a header because one engine file needed to reach another, and"
h[#h + 1] = " * narrowing that to a real public surface is issue 902."
h[#h + 1] = " *"
h[#h + 1] = " * GENERATED ONCE from the numbered headers by scripts/110-amalgamate.lua"
h[#h + 1] = " * (issue 901) and edited by hand from then on. The script is removed"
h[#h + 1] = " * when issue 904 deletes the files it read."
h[#h + 1] = " */"
h[#h + 1] = "#ifndef CERA_H"
h[#h + 1] = "#define CERA_H"

for _, entry in ipairs(HEADERS) do emit_section(h, entry, true) end

h[#h + 1] = ""
h[#h + 1] = "#endif /* CERA_H */"
write_file(DIR .. "/src/cera.h", h)
-- }}}

-- {{{ the body
local c = {}
c[#c + 1] = "/*"
c[#c + 1] = " * cera.c — the engine, entire."
c[#c + 1] = " *"
c[#c + 1] = " * What this is: one translation unit holding every part of the"
c[#c + 1] = " * runtime — the thread pool, the station table, the delivery path,"
c[#c + 1] = " * constants, the reader, the reports, the parts that change a"
c[#c + 1] = " * running program, the parts that compile new code into one, and"
c[#c + 1] = " * the parts that end one."
c[#c + 1] = " *"
c[#c + 1] = " * Why one file rather than eleven. A function in the same"
c[#c + 1] = " * translation unit as its callers can be static, and a static"
c[#c + 1] = " * function is not a linker symbol at all. Eleven files meant every"
c[#c + 1] = " * joint between them had to be a global name, so a host program"
c[#c + 1] = " * linking this engine inherited about forty ordinary English words"
c[#c + 1] = " * it never asked for. One file makes private the default and public"
c[#c + 1] = " * a deliberate act — the act being a declaration in cera.h."
c[#c + 1] = " *"
c[#c + 1] = " * How it is arranged: eleven sections in the project's reading"
c[#c + 1] = " * order, each formerly a numbered file, each opening with a banner"
c[#c + 1] = " * naming what it was. A #line directive at every seam keeps compiler"
c[#c + 1] = " * errors and debugger backtraces pointing at the original source."
c[#c + 1] = " *"
c[#c + 1] = " * GENERATED ONCE from the numbered bodies by scripts/110-amalgamate.lua"
c[#c + 1] = " * (issue 901) and edited by hand from then on."
c[#c + 1] = " */"
c[#c + 1] = '#include "cera.h"'

for _, entry in ipairs(BODIES) do emit_section(c, entry, false) end

write_file(DIR .. "/src/cera.c", c)
-- }}}
