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

## What's NOT here (deferred)

- **Per-worker init invocation.** The pool worker's `worker_main`
  has a hook between TLS setup and the init barrier; this is
  where each spec's `init(worker_idx)` runs and populates the
  worker's per-language handle. Plumbing lands as a follow-on
  within 303.
- **Auto-discovery of langs_dir.** The runner currently passes
  the path explicitly. `/proc/self/exe` resolution and a
  `SORAMECH_LANGS_DIR=` override land with the binary's
  install-target work (within issue 309).

## Related

- Issue 303 — design.
- Issue 301 — pool worker hook.
- Issue 304 — dispatch invokes spec callbacks.
- `langs/lang-spec.h` — the C contract this loads.
