# langs/c/spec.c — public surface

C language spec. Built as `spec.so`; dlopen'd by the pool runner at
startup. Currently a scaffold — the real implementation (gcc
-shared -fPIC compile callback, dlopen cache, generated typed
wrapper) lands with issue 307.

## External symbols

- `lang_spec_t soramech_lang_spec` — the exported spec record.
  - `name`     = `"c"`
  - `file_ext` = `".c"`
  - `init`, `teardown`, `compile`, `invoke` — all `NULL` (stub).

## Build

- `make` here, or `make specs` from the project root. Links `-ldl`.
