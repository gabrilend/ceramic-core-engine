# SoraMech — Roadmap

## Phase 1 — Foundation (complete)

Goal: a map you can edit in the browser and execute from the
terminal.

At the end of phase 1, the user can:
- Open the editor at `http://localhost:7700/`, see a canvas, create
  boxes, set their `ref` / `fn` / `inputs`, wire them together
- Save edits as box files on disk via the HTTP file CRUD server
- Run `luajit src/007-runner-main.lua <map-dir>` to execute the map
- Add a custom language driver and have the runner use it

Issues 101–110, all complete and moved to `issues/completed/`.

Demo: `issues/completed/demos/phase-1/`.

## Phase 2 — Editor & graph model (current)

Goal: an editor you can actually use to build maps, and the graph
model the phase 3 runtime will consume.

This phase is editor-focused with some graph-model cleanup:
- Map picker, fit-to-view, interaction hints, entry-box marking
- Source file browser with auto-populated input ports
- File browser library directories
- Single-output driver contract (issue 218)
- Comparator-based branching replaces the old `branch` box kind with
  named ports (issue 210)
- Comparator + iterator are the two routing primitives (issues 210,
  221)
- Variadic / queued inputs (issues 213, 217)
- Compile button (issue 222) — placeholder until phase 3's build
  system (issue 309) ships the backing endpoint
- Ollama / LLM library (issue 209)

Phase 2 runs on the synchronous Lua interpreter
(`src/007-runner-main.lua`). No threading. Drivers per phase 1.

See `issues/phase-2-progress.md` for the live status.

## Phase 3 — Thread pool runtime (planned, fully designed)

Goal: replace the Lua synchronous runner with a C thread pool runner
that executes box graphs in parallel, with persistent per-worker
language runtimes and proper inter-task value plumbing.

Designed across issues 301–311:

- 301 — pool lifecycle and per-worker init
- 302 — wire value slot store (per-task ring buffers, refcount,
  large-value heap)
- 303 — language runtime spec (`lang_spec_t` interface)
- 304 — task dispatch layer
- 305 — C graph loader (replaces `src/003-loader.lua`)
- 306 — Lua language spec (in-process via `lua_State`)
- 307 — C language spec (in-process via `dlopen`, with compile)
- 308 — Bash language spec (out-of-process via Unix domain socket)
- 309 — build system & Makefile orchestration
- 310 — priority queue (wired in, no-op behaviorally)
- 311 — integration tests & run output (`last-run.jsonl`)

When phase 3 lands, the synchronous Lua runner retires. The editor
and the map directory format do not change — the runtime mechanism
beneath the box is what changes. See `docs/004-ipc-and-threading.md`
for the design discussion.

## Phase 4 — SoraMind integration

Goal: turn the basic Ollama call box into a full inference surface
against the Alpine cluster.

The basic LLM call already ships in Phase 2 (issue 209): a Lua library
that takes a prompt string and returns a completion, usable directly
as a call box by copying it into a map's `src/` directory. Phase 4 is
the work on top of that:

- Streaming output from the inference endpoint back to the runner,
  surfacing tokens as they arrive instead of waiting for the full
  completion
- Prompt assembly sugar — a dedicated box kind that pulls named
  sections from data files into a prompt template, rather than the
  user concatenating strings by hand
- Multi-host routing — a single logical "LLM call" that picks among
  several backing endpoints (laptop Ollama, Alpine cluster, fallback
  hosts) based on availability or model
- Comparator-routed classifiers as a worked pattern, not new
  machinery — comparators already exist in Phase 2; the work here is
  documenting the rogue / wizard / else style and shipping it as a
  reusable example

Built on the phase 3 runtime — these LLM boxes are regular box
functions, just ones whose backing service is a network endpoint.

## Phase 5 — Map-to-map calls

Goal: a box that invokes another map as a subroutine.

- Map call box type: specifies another map directory, passes inputs,
  receives outputs
- Maps run concurrently with each other (sub-pool per map)
- Entry/exit contracts defined in `meta.json`

## Phase 6 — Remote runner

Goal: run a map on the Alpine cluster, edit it from a laptop.

- Pool runner on a remote machine
- File-CRUD server on the same machine exposes map files over HTTP
- Browser editor on the laptop connects to the remote server
- Results written to remote `last-run.jsonl`, accessible via server

Phases 4, 5, and 6 are scoped only briefly here — concrete planning
happens once phase 3 is done.
