# 102 — Language driver interface

## Status

completed

## Current behavior

No driver scripts exist. The runner has no mechanism to invoke functions
in any language. There is no defined contract for how a driver receives
arguments or returns values.

## Intended behavior

A drivers/ directory exists alongside soramech-runner.lua containing
three built-in driver scripts: lua.sh, c.sh, and bash.sh. Each follows
the same interface. The runner invokes any driver identically regardless
of language. A user can write a fourth driver for any language by copying
the pattern.

Driver invocation contract:
  <driver-script> <file-path> <fn-name> <arg-count> [<arg> ...]

All args are strings. Structured values are JSON strings. Driver writes
one JSON value (or JSON array for tuples) to stdout on success. Exits
non-zero on failure, error message to stderr.

## Suggested implementation steps

1. Write drivers/lua.sh — sources the target .lua file via luajit,
   calls the named function with decoded JSON args, encodes return value
   to JSON on stdout. Single-value and multi-value (tuple) returns both
   produce a JSON array.
2. Write drivers/c.sh — checks mtime of .c file against cached binary
   in tmp/cache/. Compiles if stale (using gcc or cc, flags configurable
   via env). Invokes the binary with args as argv. Binary must write JSON
   to stdout per the contract.
3. Write drivers/bash.sh — sources the .sh file, calls the named
   function with args as positional params. Function must echo JSON to
   stdout.
4. Write a small test map maps/driver-test/ with one box per driver type
   to verify each driver works end-to-end before the runner is built.
5. Document the driver contract in drivers/README (plain text, short).

## Implementation notes

Three driver scripts exist at `drivers/lua.sh`, `drivers/bash.sh`, and `drivers/c.sh`. All follow the contract: `<driver> <file> <fn> <arg_count> [args...]` and write a single JSON value to stdout. The contract was refined during issue 218 to single-output; `drivers/README` documents the current form. The `maps/driver-test/` map exercises all three drivers before the runner was built.

## Related documents

- docs/003-driver-system.md — full driver system spec
- issues/101 — map scaffold (tmp/cache/ directory created there)
- issues/103 — runner uses these drivers once they exist

## Notes

For lua.sh: luajit must be on PATH. The script should fail with a clear
error if luajit is not found, not silently produce wrong output.

For c.sh: the cache key for compiled binaries is the absolute path of the
source file. A sha1 or md5 of the path is sufficient; the binary is
stored as tmp/cache/<hash>. Stale check: stat both files, compare mtimes.

For bash.sh: functions in a sourced file may have side effects and access
the filesystem. This is expected and correct — bash boxes are intentionally
less constrained than lua or C boxes.

The "no special cases in the runner" constraint means the runner must not
have any if-elseif chain on file extension. All dispatch goes through
the driver table loaded from drivers.json.
