# 219 — Map compiler: generate standalone source code from a box graph

## Status
open

## Current behavior
The soramech-runner interprets the box graph at runtime. To execute a map, the
user must have SoraMech installed. The runner shells out to a language driver for
each box, captures stdout, and routes values to the next box. The runtime
dependency on SoraMech and the stdout-as-IPC design are both fragile.

## Intended behavior
The editor compiles a map into a standalone source file (or script) that runs
without SoraMech. The box graph is an authoring artifact — the output of the
editor is a real program.

### What the compiler produces

**Pure Lua map** → a single `.lua` file that requires each box's source file
and chains calls directly:
```lua
local greet = require("src/greet")
local write = require("src/write_result")
write.write_result(greet.hello())
```
Runnable with `luajit generated.lua`. No SoraMech, no drivers, no IPC.

**Mixed-language map** → a shell script that invokes each box's binary or
interpreter in sequence, passing values through named temp files rather than
stdout. Each step reads its input from a file and writes its output to a file.
The script is self-contained and inspectable.

**Comparator (branch) nodes** → if/elseif chains in the generated code.

### What stays in the box JSON
Canvas positions, labels, wire layout — everything the editor needs to display
and edit the graph. The JSON is the editor's internal representation. The
compiled output is the deliverable.

### Why stdout-as-IPC is the wrong model
The current runtime pipes each box's return value through stdout. Problems:
- A box that prints debug output corrupts the wire value
- Shell process overhead per box
- Can't pass binary or large values reliably
- Values are transient — can't inspect mid-run

The compiler eliminates IPC for same-language maps (direct function calls).
For mixed-language maps, file-based IPC is used instead of stdout:
each wire becomes a named temp file. The generated script writes/reads
explicitly. Files persist after the run and can be inspected.

### Language runner libraries (alternative to the current driver model)
For multi-language support without a runtime, each language needs a small
shim that:
- reads its input value from a file path passed as argv
- calls the box function
- writes the output value to a file path passed as argv

The compiler generates calls to these shims. The shims are minimal (~30 lines)
and are the only per-language artifact needed. No driver contract, no stdout,
no JSON array wrapping. A new language requires only a new shim, not a new
driver script with a parsing contract.

### The canvas/connection relationship
The box JSON currently stores connections in both endpoint files. The compiler
reads these to determine call order (topological sort). Once compiled, the
connections are embedded in the generated code — the JSON is not needed at
runtime. If someone reads the generated code, they can see exactly which
functions call which, without needing the JSON.

## Design constraints
- The editor has no conception of running maps. It creates and edits box graphs.
  The compiler is a separate CLI tool invoked outside the editor.
- The compiler writes its output into `maps/:name/output/run.lua` (pure Lua) or
  `maps/:name/output/run.sh` (mixed). The user invokes this directly.
- No "compile" button in the editor. No "run" button in the editor.

## Open questions
- Primary target language: Lua first, shell script fallback for mixed maps?
- For Lua↔C wires: emit LuaJIT FFI calls (C compiled to `.so`) or persistent
  subprocess? FFI is faster; subprocess is simpler to generate.
- For Lua↔Bash wires: persistent subprocess + anonymous pipe (one spawn per
  bash runtime at program start, reused across all calls).
- Are runner shims checked into the map's `src/` or shipped globally?
- Should the compiler use `shm_open` shared memory for cross-language wire
  values, or persistent subprocess + pipe? Shared memory is language-agnostic
  and faster; pipe is simpler and works without FFI.

## Suggested implementation sequence
1. Write the topological sort: given a box graph, produce a linear execution
   order respecting dependency edges. (Executor queue logic is the reference.)
2. Write a Lua code emitter: for a pure-Lua map, emit `require` calls and
   chained function calls in topo order, handling comparator branches as
   if/elseif in the generated file.
3. Write a CLI entry point: `soramech-compile.lua <map-dir>` that loads the
   graph, runs the emitter, and writes `output/run.lua`.
4. For mixed maps: choose IPC mechanism (FFI, shm, or persistent subprocess)
   and emit the appropriate glue in the generated script.

## Relevant files
- `src/004-executor.lua` — topological sort and execution model to adapt
- `src/003-loader.lua` — box graph loading, connection traversal
- `docs/001-architecture.md` — needs update once compiler exists
- `issues/completed/102-language-driver-interface.md` — driver model reference
