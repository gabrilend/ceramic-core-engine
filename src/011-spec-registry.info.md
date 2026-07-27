# 011-spec-registry.c — public surface

Language spec registry. Scans `langs/<name>/spec.so` files at
startup, `dlopen`s each, reads the exported `soramech_lang_spec`
symbol, and stores the lookup table the rest of phase 3 consults.

## Lifecycle

- `spec_registry_t *spec_registry_load(const char *langs_dir, char **err)`
  — read the directory, open every spec.so. NULL on failure;
  `*err` is malloc'd and the caller frees it.
- `void spec_registry_destroy(spec_registry_t *r)` — dlclose
  every handle, free the registry. Safe on NULL.

## Lookup

- `int spec_registry_size(const spec_registry_t *r)`
- `const lang_spec_t *spec_registry_get(r, "lua")` — by name.
- `const lang_spec_t *spec_registry_for_ext(r, ".sh")` — by file
  extension.
- `const lang_spec_t *spec_registry_at(r, i)` — by index, for
  enumeration during pool init.

## How specs get found and initialised

Both halves ship:

- **Directory discovery.** The runner resolves `langs/` relative
  to the binary through `/proc/self/exe`, so a copied compiled
  artifact loads its own sibling specs rather than the source
  tree's. `SORAMECH_LANGS_DIR=<path>` overrides.
- **Per-worker init.** Each pool worker calls every spec's
  `init(worker_idx)` at the startup barrier, before any task
  dispatches, and holds its own per-language handle from then on.
  This is why a `lua_State` is per worker rather than per box, and
  why the Bash spec's socketpair is per worker. The graph's
  language enumeration lets init skip specs no box in this map
  uses.

## What's NOT here (deferred)

- **Unloading a spec mid-run.** Handles are `dlclose`d only at
  registry destroy. Hot-swapping a language plugin while a map
  runs is not supported.
- **Version negotiation.** A `spec.so` built against an older
  `lang-spec.h` is not detected; the struct layout is assumed to
  match.

## Related

- Issue 303 — design.
- Issue 301 — the pool worker hook that runs per-worker init.
- Issue 304 — dispatch invokes the spec callbacks.
- `langs/lang-spec.h` — the C contract this loads.
- `docs/007-architecture.md` — the language plugin boundary.
