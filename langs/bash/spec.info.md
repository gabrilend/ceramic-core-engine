# langs/bash/spec.c — public surface

Bash language spec. Built as `spec.so`; dlopen'd by the pool runner
at startup. Currently a scaffold — the real implementation (Unix
domain socket connection per worker, length-prefix framing,
persistent bash-server.sh subprocess) lands with issue 308.

## External symbols

- `lang_spec_t soramech_lang_spec` — the exported spec record.
  - `name`     = `"bash"`
  - `file_ext` = `".sh"`
  - `init`, `teardown`, `compile`, `invoke` — all `NULL` (stub).

## Build

- `make` here, or `make specs` from the project root. Links nothing
  beyond libc.
