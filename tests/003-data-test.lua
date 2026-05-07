-- Test suite for libs/soramech-data.lua.
-- Creates a temporary data file, exercises get/set/load, and asserts
-- that constant fields reject writes. Exits non-zero on any failure.

local DIR = "/mnt/mtwo/programs/sora/soramech"
package.path = DIR .. "/libs/?.lua;" .. package.path

local data = require("soramech-data")
local json = require("dkjson")

-- {{{ local function fail
local function fail(msg)
    io.stderr:write("[FAIL] " .. msg .. "\n")
    os.exit(1)
end
-- }}}

-- {{{ local function pass
local function pass(msg)
    io.write("[PASS] " .. msg .. "\n")
end
-- }}}

-- {{{ local function write_fixture
-- Writes a known data file into a temp directory for the test.
local function write_fixture(dir, filename, obj)
    local path = dir .. "/" .. filename
    local f = io.open(path, "w")
    if not f then fail("cannot create fixture: " .. path) end
    f:write(json.encode(obj, { indent = true }))
    f:close()
end
-- }}}

-- {{{ local function setup_tmp
-- Creates a unique temp dir under /tmp for this test run.
local function setup_tmp()
    local tmpdir = "/tmp/soramech-data-test-" .. tostring(os.time())
    os.execute("mkdir -p " .. tmpdir)
    return tmpdir
end
-- }}}

-- set up temp dir and fixture
local tmp = setup_tmp()
local fixture = {
    constant = false,
    fields = {
        score   = { constant = false, value = 42 },
        name    = { constant = false, value = "player1" },
        locked  = { constant = true,  value = "do-not-change" },
    }
}
write_fixture(tmp, "state.json", fixture)

-- ── test: load returns the full table ──────────────────────────────────────
local obj, err = data.load(tmp, "state.json")
if not obj then fail("load returned error: " .. tostring(err)) end
if type(obj) ~= "table" then fail("load did not return a table") end
pass("load returns table")

-- ── test: get reads a numeric field ────────────────────────────────────────
local score, gerr = data.get(tmp, "state.json", "score")
if gerr then fail("get score: " .. gerr) end
if score ~= 42 then fail("get score expected 42, got " .. tostring(score)) end
pass("get numeric field")

-- ── test: get reads a string field ─────────────────────────────────────────
local name, nerr = data.get(tmp, "state.json", "name")
if nerr then fail("get name: " .. nerr) end
if name ~= "player1" then fail("get name expected 'player1', got " .. tostring(name)) end
pass("get string field")

-- ── test: set writes a new value and it round-trips through get ─────────────
local ok, serr = data.set(tmp, "state.json", "score", 99)
if not ok then fail("set score: " .. tostring(serr)) end
local new_score, rgerr = data.get(tmp, "state.json", "score")
if rgerr then fail("get after set: " .. rgerr) end
if new_score ~= 99 then fail("set score: expected 99, got " .. tostring(new_score)) end
pass("set and round-trip get")

-- ── test: set on a constant field returns an error ──────────────────────────
local bad_ok, bad_err = data.set(tmp, "state.json", "locked", "changed")
if bad_ok then fail("set on constant field should fail but returned ok") end
if not bad_err then fail("set on constant field returned no error string") end
pass("set on constant field returns error")

-- ── test: constant value is unchanged after failed write ───────────────────
local locked_val, lerr = data.get(tmp, "state.json", "locked")
if lerr then fail("get locked after refused write: " .. lerr) end
if locked_val ~= "do-not-change" then
    fail("constant field mutated: got " .. tostring(locked_val))
end
pass("constant field unchanged after refused write")

-- ── test: file-level constant blocks all writes ────────────────────────────
write_fixture(tmp, "readonly.json", {
    constant = true,
    fields = { x = { constant = false, value = 1 } }
})
local fok, ferr = data.set(tmp, "readonly.json", "x", 2)
if fok then fail("set on file-level-constant should fail but returned ok") end
if not ferr then fail("set on file-level-constant returned no error") end
pass("file-level constant blocks write")

-- ── test: get on missing field returns error ────────────────────────────────
local mv, merr = data.get(tmp, "state.json", "nonexistent")
if mv ~= nil then fail("get missing field should return nil, got " .. tostring(mv)) end
if not merr then fail("get missing field returned no error") end
pass("get on missing field returns error")

-- ── test: load on missing file returns error ────────────────────────────────
local _, loaderr = data.load(tmp, "nope.json")
if not loaderr then fail("load on missing file should return error") end
pass("load on missing file returns error")

io.write("\nAll data tests passed.\n")
os.exit(0)
