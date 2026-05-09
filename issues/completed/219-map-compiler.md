# 219 — Map compiler

## Status
won't implement (folded into issue 309)

## Resolution

The original 219 specified a "package a map for standalone execution"
step that produces `maps/<name>/compiled/` containing the pool runner
binary, all source files, per-box compiled `.so`s, language spec
`.so`s, and a manifest.

Every dependency of that work lives in phase 3:
- 305 — graph loader (reused to walk the map)
- 307 — C compile callback (produces per-box `.so`)
- 303 — language spec interface (`compile` hook)
- 309 — build system & Makefile orchestration

There is nothing to do in phase 2 — the artifacts being packaged
don't yet exist. The substantive content (compiled directory layout,
manifest format, mtime incremental compile, editor `POST
/maps/<name>/compile` endpoint, `soramech-compile` CLI) is folded
into issue 309 under the "Compile and package a map for standalone
execution" section.

The editor's Compile button (issue 222, complete) is a placeholder
until 309 ships the backing endpoint.

## Relevant files

- `issues/309-build-system.md` — the new home for compile/package
- `issues/completed/222-compile-button-and-assets-directory.md` —
  editor button that will trigger the compile step
