# 105 — HTTP server: file CRUD API

## Status

completed

## Blockers

- 101 (map format must be defined; server validates against schema)

## Current behavior

No server exists. Map files can only be edited by hand on the machine
where they live.

## Intended behavior

soramech-server.lua is a minimal HTTP server backed by luasocket. It
exposes a REST API that the browser uses to read and write map files.
It does nothing else. No runner management, no process spawning, no
streaming.

Started with: `luajit soramech-server.lua <maps-root> [port]`
Default port: 7700

Endpoints:
  GET  /maps                          — list map names (array of strings)
  GET  /maps/<name>/boxes             — list box IDs in this map
  GET  /maps/<name>/boxes/<id>        — read one box file (JSON)
  PUT  /maps/<name>/boxes/<id>        — write one box file (validates schema)
  DELETE /maps/<name>/boxes/<id>      — delete box (hard error if referenced)
  GET  /maps/<name>/data/<filename>   — read a data file
  PUT  /maps/<name>/data/<filename>   — write a data file
  GET  /maps/<name>/drivers           — read drivers.json
  PUT  /maps/<name>/drivers           — write drivers.json
  GET  /maps/<name>/meta              — read meta.json
  PUT  /maps/<name>/meta              — write meta.json

All request and response bodies are JSON. Content-Type: application/json.
CORS headers must be set (Access-Control-Allow-Origin: *) so index.html
served from a file:// URL or a different origin can reach the server.

Error responses: { "error": "message" } with appropriate HTTP status.
400 for schema violations, 404 for missing resources, 409 for constraint
violations (delete of a referenced box), 500 for filesystem errors.

## Suggested implementation steps

1. Write src/005-http-server.lua — top-level server loop using luasocket's
   tcp server primitives. Accept connections, parse HTTP request line and
   headers, dispatch to handler table by method + path pattern.
2. Route table: a dispatch table mapping {method, pattern} -> handler_fn.
   Pattern matching is simple prefix matching on the path segments —
   no regex needed.
3. Each handler receives {method, path_parts, body_string} and returns
   {status, body_string}. The server wraps these in HTTP framing.
4. Validate box writes against src/001-schema.lua before writing to disk.
   Reject with 400 and a descriptive error if validation fails.
5. For DELETE /boxes/<id>: scan all other box files for references to the
   deleted id in their connections list. Hard error with 409 if found;
   list all referencing boxes in the error body.
6. Write a simple smoke test: scripts/test-server.sh — starts the server,
   uses curl to create a box, read it back, update it, delete it. Asserts
   expected responses.

## Implementation notes

`src/005-http-server.lua` implements `serve(maps_root, port)` using luasocket TCP primitives with a dispatch table keyed on method and path pattern. It handles GET/PUT/POST for boxes, data files, meta, and src files, as well as the extra-src-dirs endpoints added in issues 211 and 211a. The entry point is `src/006-server-main.lua`. CORS headers are set on all responses; every request is logged to stdout as a single timestamped line.

## Related documents

- docs/007-architecture.md — endpoint list and error format
- issues/101 — schema validator used in step 4
- issues/106 — browser client that calls these endpoints

## Notes

luasocket's HTTP server support is low-level. The server loop must handle
keep-alive connections correctly, or close after each response. Closing
after each response (HTTP/1.0 style) is simpler and sufficient for the
browser editor's access pattern.

The server should log each request to stdout in a single line:
  [timestamp] METHOD /path -> status (ms)
This makes debugging easy without needing a log file.

The server must not follow symlinks outside the maps-root. Validate that
every resolved file path begins with maps-root before reading or writing.
This is the only security boundary needed since the server is local-only.
