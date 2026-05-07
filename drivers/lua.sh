#!/usr/bin/env bash
# Lua/LuaJIT language driver for SoraMech.
# Loads a .lua module, calls the named function with JSON-decoded arguments,
# and writes the return value(s) as a JSON array to stdout.
# Contract: <script> <file-path> <fn-name> <arg-count> [<arg> ...]

DIR="/mnt/mtwo/programs/sora/soramech"

FILE_PATH="${1}"
FN_NAME="${2}"
ARG_COUNT="${3}"
shift 3

if [ -z "${FILE_PATH}" ] || [ -z "${FN_NAME}" ]; then
    echo "lua.sh: missing file-path or fn-name" >&2
    exit 1
fi

if ! command -v luajit > /dev/null 2>&1; then
    echo "lua.sh: luajit not found on PATH" >&2
    exit 1
fi

if [ ! -f "${FILE_PATH}" ]; then
    echo "lua.sh: file not found: ${FILE_PATH}" >&2
    exit 1
fi

# build a JSON array of the raw argument strings for passing into lua
ARGS_JSON="["
SEP=""
for ARG in "$@"; do
    ARGS_JSON="${ARGS_JSON}${SEP}${ARG}"
    SEP=","
done
ARGS_JSON="${ARGS_JSON}]"

# set LUA_PATH so dkjson is findable before the shim runs its first require
export LUA_PATH="${DIR}/libs/?.lua;${LUA_PATH:-;}"

# run luajit with the driver shim inline; the shim loads the module,
# decodes arguments, calls the function, and encodes the result
luajit - "${FILE_PATH}" "${FN_NAME}" "${ARGS_JSON}" << 'LUASHIM'
local json = require("dkjson")

-- read driver arguments
local file_path = arg[1]
local fn_name   = arg[2]
local args_raw  = arg[3]

-- add libs and the module's own directory to the path for any further requires
local script_dir = file_path:match("^(.+)/[^/]+$") or "."
package.path = script_dir .. "/?.lua;" ..
               "/mnt/mtwo/programs/sora/soramech/libs/?.lua;" ..
               package.path

-- decode args array
local args_table, _, jerr = json.decode(args_raw)
if not args_table then
    io.stderr:write("lua.sh: failed to decode args JSON: " .. tostring(jerr) .. "\n")
    os.exit(1)
end

-- load the module
local ok, mod = pcall(dofile, file_path)
if not ok then
    io.stderr:write("lua.sh: error loading " .. file_path .. ": " .. tostring(mod) .. "\n")
    os.exit(1)
end

-- resolve the function
local fn = mod[fn_name]
if type(fn) ~= "function" then
    io.stderr:write("lua.sh: no public function '" .. fn_name .. "' in " .. file_path .. "\n")
    os.exit(1)
end

-- decode each arg from JSON to lua value; strings are passed as-is
local call_args = {}
for i, raw_arg in ipairs(args_table) do
    local val, _, aerr = json.decode(raw_arg)
    if aerr then
        -- not valid JSON — treat as a plain string
        call_args[i] = raw_arg
    else
        call_args[i] = val
    end
end

-- call the function; collect all return values into a table
local results = table.pack(fn(table.unpack(call_args)))

-- encode results as a JSON array (each return value is one element)
local encoded = {}
for i = 1, results.n do
    encoded[i] = json.encode(results[i])
end
print(json.encode(encoded))
LUASHIM
