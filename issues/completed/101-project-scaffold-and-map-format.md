# 101 — Project scaffold and map directory format

## Status

completed

## Current behavior

The project directory exists but contains no source files, no library
symlinks, and no example map. There is no defined on-disk format for map
directories, box files, data files, or driver configuration.

## Intended behavior

The repository contains:
  - Symlinks in libs/ pointing at dkjson and luasocket from the shared
    library location
  - A well-defined map directory format, documented and validated by a
    small lua schema checker
  - An example map in maps/hello/ that exercises one lua box and one data
    file
  - A create-map.lua utility that scaffolds a new map directory with all
    required files and a tmp/ symlink
  - A validate-map.lua utility that checks a map directory for format
    errors and missing references

## Suggested implementation steps

1. Symlink dkjson.lua and luasocket into libs/
2. Write src/001-schema.lua — defines the expected structure of box files,
   meta.json, and drivers.json as lua tables. Used by both the runner and
   the server to validate inputs before writing.
3. Write src/002-validate-map.lua — loads all box files in a map directory,
   checks schema, validates that every connection's to_box exists, validates
   that branch boxes have "else" handled or declared. Prints errors and exits
   non-zero if invalid.
4. Write scripts/create-map.sh — takes a map name and maps-root path,
   creates the directory structure, writes empty meta.json and drivers.json,
   creates the tmp/ symlink to /tmp/<map-name>/.
5. Create maps/hello/ as an example:
   - One box calling src/hello.lua / hello()
   - meta.json marking it as the entry box
   - A data file data/state.json with one mutable field
6. Write src/hello.lua with a public hello() function that reads a name
   from its input and returns a greeting string.

## Implementation notes

The map scaffold is created by `scripts/create-map.sh`, which writes the full directory tree (boxes/, data/, src/, tmp/ symlink) and seeds meta.json and drivers.json. The on-disk format — box files, data files, and driver config — is defined in `src/001-schema.lua` and documented in `docs/001-architecture.md`. The `maps/hello/` example map exercises a lua box and a data file end-to-end.

## Related documents

- docs/001-architecture.md — full map directory layout spec
- docs/003-driver-system.md — driver contract referenced by validate-map

## Notes

The tmp/ symlink must be created with an absolute path target. The
create-map utility must handle the case where /tmp/<map-name>/ does not
yet exist and create it.

validate-map.lua is the source of truth for format correctness — any
time the architecture doc and the validator disagree, fix the validator
and update the doc.
