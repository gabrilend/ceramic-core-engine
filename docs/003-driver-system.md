# SoraMech — Language Driver System

## What a driver is

A driver is a shell script. It knows how to invoke a function in one
language. The runner knows nothing about any language — it looks up the
file extension in drivers.json, finds the driver script, and calls it.

Lua and C ship with pre-written driver scripts. They are not special
cases in the runner. They use exactly the same interface a user-written
driver uses. If you want to call a Haskell function or a Rust function,
write a driver script and add the extension to drivers.json.

## Driver contract

The runner invokes a driver as:

  <driver-script> <file-path> <fn-name> <arg-count> [<arg> ...]

Arguments are always strings. Structured values (tables, arrays) are
passed as JSON strings and must be decoded by the driver before use.

The driver must:
  - Exit 0 on success, non-zero on failure
  - Print a single JSON value to stdout on success: either a JSON array
    (for multi-output / tuple returns) or a single JSON value (for
    single-output functions)
  - Print an error message to stderr on failure (runner captures and logs
    it)

Nothing else. No side channels. The runner reads stdout, parses JSON,
and maps the array positions to the box's declared output port names in
declaration order.

## Shell script vs. binary caller

Two kinds of refs exist in a box file:

  "ref": "src/strings.lua",  "fn": "trim"
      → source caller. runner looks up ".lua" in drivers.json and
        invokes the driver with file + fn + args.

  "ref": "src/trim.sh"       (no "fn")
      → binary caller. runner invokes the script directly with just
        the args. The script is its own driver. Stdout is parsed as
        JSON as with a normal driver.

Binary callers include compiled binaries too: if the ref ends in a
binary extension or has no extension, the runner execs it directly.

## Built-in driver scripts

Shipped in a global drivers/ directory alongside soramech-runner.lua.
Maps can override any driver by defining the same extension in their
local drivers.json pointing at a map-local script.

### drivers/lua.sh

Invokes a single function from a Lua file using luajit. The function
must be a module-level export (returned from the file or set in a
package). Args are decoded from JSON; return values are encoded to JSON.

### drivers/c.sh

Compiles the .c file if its mtime is newer than the cached binary in
tmp/cache/. The cache key is the source file's absolute path hashed.
Invokes the compiled binary with the args. The binary must follow the
driver output contract (JSON to stdout).

The binary is expected to expose a `main`-style entry that reads args
from argv and writes JSON to stdout. A small C header (shipped with
soramech) provides helpers for this.

### drivers/bash.sh

Invokes a bash function defined in the .sh file by sourcing the file
and calling the function with the args as positional parameters.
The function must echo a JSON value (or array) to stdout.

## User-defined drivers

Add the file extension and path to the map's drivers.json:

  { ".rs": "drivers/rust.sh" }

Write drivers/rust.sh. It receives the same arguments as any built-in
driver. Compile, interpret, or delegate however the language requires.
The runner doesn't care.

## Driver lookup order

  1. Map-local drivers.json (maps/<name>/drivers.json)
  2. Global drivers.json (alongside soramech-runner.lua)
  3. Hard error if extension not found — no fallback, no silent skip

## Passing structured values between boxes

All inter-box values are JSON at the transport layer. Inside a Lua box,
dkjson decodes input strings to Lua tables. The driver re-encodes return
values to JSON. Inside a C box, the shipped helper header decodes argv
JSON strings to C structs and encodes results back.

This means structured values cross language boundaries cleanly: a Lua
table produced by one box can be consumed by a C function in the next
box, because both sides see JSON at the wire level.
