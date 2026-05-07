# 109 — Data files: persistent and ephemeral storage

## Status

complete

## Implementation notes

`libs/soramech-data.lua` implements `M.load`, `M.get`, `M.set` using dkjson
for all encode/decode. Field path walks the `fields` wrapper structure used
by all data files. Constant enforcement checks both the file-level flag and
each field's wrapper flag before writing. Writes are atomic via
write-to-.tmp-then-os.rename.

`tests/003-data-test.lua` covers: load table, get numeric, get string, set
round-trip, constant field rejection, file-level constant rejection, missing
field error, missing file error. All 9 tests pass.

`libs/soramech-data.info.md` documents all three public functions with param
tables and error conditions.

The lua driver already exported `libs/` on `LUA_PATH` before the shim runs,
so `require("soramech-data")` works in any lua box function without changes.

## Blockers

- 101 (map directory and tmp/ symlink created there)
- 104 (executor must exist; data file access added to driver interface here)

## Current behavior

No data file access exists. Boxes cannot read or write named data from
within their functions. There is no distinction between persistent and
ephemeral storage in the map directory.

## Intended behavior

A map's data/ directory contains JSON files. Each file is a lua table
serialized with dkjson. A section-level "constant" flag marks individual
fields as read-only. A file-level "constant" flag makes the entire file
read-only.

Boxes access data files through a lua library (libs/soramech-data.lua)
that the driver for lua boxes includes in the lua environment. The library
is not built into the runner — it is imported by the lua driver like any
other library.

  data.get(map_dir, filename, field_path) -> value
  data.set(map_dir, filename, field_path, value) -> ok, err
  data.load(map_dir, filename) -> table, err

data.set refuses writes to constant fields and returns an error string.
The caller decides whether to propagate the error or halt.

Ephemeral data (state during a run, logs, last-run snapshot) goes to
tmp/, which is a symlink to /tmp/<map-name>/. The same library works
with tmp/ as a directory argument — no special case. Tmp files are not
backed up and are expected to disappear on reboot.

## Suggested implementation steps

1. Write libs/soramech-data.lua — implements data.get, data.set, data.load.
   Uses dkjson for all encode/decode. data.set reads the current file,
   modifies the specified field path (dot-separated key), validates the
   constant flag, and writes the file back atomically (write to .tmp then
   rename).
2. Define field_path as a dot-separated key string: "config.model" accesses
   table["config"]["model"]. Supports only string keys (no array indexing
   in v1).
3. Write tests/003-data-test.lua — creates a temp data file, calls
   data.set, reads back with data.get, asserts value matches. Tries to
   write a constant field, asserts error returned.
4. Document in libs/soramech-data.info.md: each public function with
   inputs, outputs, and error conditions.
5. Update drivers/lua.sh to add libs/ to the luajit package path so
   soramech-data.lua is available to any lua box function without
   explicit path manipulation.

## Related documents

- docs/001-architecture.md — data file format and tmp/ symlink
- issues/101 — tmp/ symlink creation
- issues/102 — lua driver (package path updated in step 5)
- issues/104 — executor (data library available to boxes during execution)

## Notes

Atomic writes (write-to-tmp-then-rename) are important for data files
that a running map and the browser editor might both touch. Rename is
atomic on POSIX filesystems; the reader always sees either the old or
the new file, never a partial write.

Constant field validation: when data.set is called on a constant field,
return an error string rather than panicking. The driver should propagate
this to the runner as a non-zero exit (write the error to stderr). The
runner then halts the map with a descriptive message. Prefer hard errors
over silent skips here.

The library design (separate from the runner, imported by the driver) is
intentional. It means the library works identically in a standalone lua
script, in a test, and inside a running map. No runner context is needed
to use it.
