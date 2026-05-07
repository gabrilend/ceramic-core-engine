# 005-http-server.lua — Public API

Minimal HTTP/1.0 server for the SoraMech web editor.

## M.serve(maps_root: string, port: number)

Binds to the given port and serves map file CRUD indefinitely.
One connection per request; CORS headers on every response.
Logs each request to stdout as: [timestamp] METHOD /path (Nms)

Routes:
  GET  /maps                    -> string[]  (map names)
  GET  /maps/<n>/boxes          -> string[]  (box ids)
  GET  /maps/<n>/boxes/<id>     -> box JSON
  PUT  /maps/<n>/boxes/<id>     <- box JSON  (validates schema)
  DEL  /maps/<n>/boxes/<id>     -> {ok}      (409 if referenced)
  GET  /maps/<n>/data/<f>       -> data JSON
  PUT  /maps/<n>/data/<f>       <- data JSON
  GET  /maps/<n>/drivers        -> drivers JSON
  PUT  /maps/<n>/drivers        <- drivers JSON
  GET  /maps/<n>/meta           -> meta JSON
  PUT  /maps/<n>/meta           <- meta JSON (validates schema)
