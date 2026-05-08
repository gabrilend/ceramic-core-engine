# SoraMech — Roadmap

## Phase 1 — Foundation

Goal: a map you can edit in the browser and execute from the terminal.

At the end of phase 1, the user can:
- Open index.html, connect it to a running server, and see a canvas
- Create boxes, set their ref/fn/inputs/outputs, wire them together
- Edit branch boxes with named ports and predicates
- Save all edits as box files on disk
- Run `luajit soramech-runner.lua <map-dir>` and have it execute the map
- Write a custom language driver and have the runner use it

Issues:
  101 — project scaffold and map directory format
  102 — language driver interface
  103 — runner: graph loader and validator
  104 — runner: synchronous executor (task boundary)
  105 — HTTP server: file CRUD API
  106 — web editor: canvas and box rendering
  107 — web editor: wiring and connection UI
  108 — branch box and predicate routing
  109 — data files: persistent and ephemeral storage
  110 — phase 1 demo

Demo: a small lua FSM (3-4 boxes) runs via the CLI, exercises a branch
box, reads a data file, writes an output. The browser shows the same map
visually with correct wiring drawn.

## Phase 2 — Concurrent execution

Goal: boxes whose inputs are all satisfied fire concurrently.

- Coroutine-based non-blocking executor: the ready-queue becomes a
  coroutine scheduler; boxes yield while waiting on upstream results
- Cross-language calls use Unix domain socket servers (one per language
  runtime per worker), non-blocking on the Lua side via LuaSocket
- Multiple maps running concurrently share nothing (separate processes)
- Integration path toward the 3d-rts custom thread pool: the coroutine
  scheduler and task boundary are designed to swap in the thread pool
  without changing the box execution model

## Phase 3 — SoraMind integration

Goal: boxes that call SoraMind inference endpoints on the Alpine cluster.

- LLM call box type: sends a prompt, receives a completion string
- Prompt assembly: text values wired from upstream boxes, data file
  sections explicitly loaded into prompt context
- Branch box connected to an LLM classifier: "rogue, wizard, or else"
- Else port retry with pseudo-randomized temperature parameter
- Streaming output from SoraMind inference back to the runner

## Phase 4 — Map-to-map calls

Goal: a box that invokes another map as a subroutine.

- Map call box type: specifies another map directory, passes inputs,
  receives outputs
- Maps can run concurrently with each other
- Entry/exit contracts defined in meta.json

## Phase 5 — Remote runner

Goal: run a map on the Alpine cluster, edit it from a laptop.

- Runner process on a remote machine (Alpine node)
- Server on the same machine exposes map files over HTTP
- Browser editor on the laptop connects to the remote server
- Results written to remote data files, accessible via server
