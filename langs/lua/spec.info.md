# langs/lua/spec.c — public surface

Lua language spec. Built as `spec.so`; `dlopen`'d by the pool
runner at startup. Fully implemented (issue 306).

## External symbols

- `lang_spec_t soramech_lang_spec` — the exported spec record.
  - `name` = `"lua"`, `file_ext` = `".lua"`
  - `init` / `teardown` — per-worker `lua_State` lifecycle
  - `invoke` — call the box's function
  - `native_to_json` / `json_to_native` / `translate` — the wire
    bridges
  - `compile` — none; Lua needs no build step
  - `translate_targets` = `{"c", "bash"}`
  - `invoke_wrote_native` — implemented; see below
  - sentinel emit / reconstruct masks — `$ref` and `$lang_opaque`

## Per-worker state

`init` builds one `lua_State` **per worker**, not per box. Every
box that runs on a given worker shares that state, so module
imports and closures persist across calls within a run — that is
the intended benefit and it is why Lua boxes are cheap to fire
repeatedly.

The trap is the other side of the same fact. Because every box is
multi-spawn, fires of one box land on different workers and
therefore touch different states. Module-level mutable data does
not accumulate the way it looks like it should: a counter at
module scope counts once per worker, reading low and
non-deterministically. This is not a data race — each state is
touched by one thread — it is a split brain. Box authors who need
a real shared counter must reach outside Lua for it.
`docs/005-writing-boxes.md` carries the full contract.

## The one spec whose output can diverge from the ask

The dispatch asks each invoke for either native bytes or JSON,
based on how the loader classified the outgoing edge. Lua is the
only shipped spec that can answer with a different form than it
was asked for: a Lua **table** has no native byte representation,
so a table on a native ask goes out as JSON regardless.

`invoke_wrote_native` is how it says so. Lua records the form at
each invoke exit; the dispatch reads it right after the call, on
the same worker thread, and routes the push by what was actually
written rather than what was requested. C and Bash omit the
accessor, which the dispatch reads as "I write what I'm asked."

That divergence was bug 323, and the accessor is issue 325's
second slice. Without it, tables landed on the wrong ring and
consumers got bytes they could not parse.

## Values across wires

Strings pass through unchanged. Numbers format as their JSON
representation. Tables serialise to JSON. Functions become a
`$lang_opaque` sentinel that a consuming Lua worker can
reconstruct in its own state — and that any other language will
refuse, by design.

## Build

- `make` here, or `make specs` from the project root, produces
  `spec.so`. Requires LuaJIT 2.1 headers
  (`/usr/include/luajit-2.1` by default; override with
  `LUA_INCLUDE=…`).

## Related

- Issue 306 — the implementation.
- Issue 323 — the table-on-native-ask bug.
- Issue 325 — `translate_targets` and `invoke_wrote_native`.
- `langs/lang-spec.h` — the contract.
- `docs/007-architecture.md` — the language plugin boundary.
