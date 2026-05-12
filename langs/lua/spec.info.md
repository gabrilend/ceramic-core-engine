# langs/lua/spec.c — public surface

Lua language spec. Built as `spec.so`; dlopen'd by the pool runner
at startup. Currently a scaffold — exports the spec symbol with
NULL callbacks so the spec registry can verify the loading
mechanism end-to-end. The real implementation lands with issue 306.

## External symbols

- `lang_spec_t soramech_lang_spec` — the exported spec record.
  - `name`     = `"lua"`
  - `file_ext` = `".lua"`
  - `init`, `teardown`, `compile`, `invoke` — all `NULL` (stub).

## Build

- `make` here, or `make specs` from the project root, produces
  `spec.so`. Requires LuaJIT 2.1 headers (`/usr/include/luajit-2.1`
  by default; override with `LUA_INCLUDE=…`).
