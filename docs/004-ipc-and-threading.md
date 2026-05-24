# SoraMech — IPC and Threading Model

## The problem

The current executor runs boxes sequentially. Each box invocation shells out to
a driver script, which starts a new process, runs the function, and exits. The
return value passes through stdout. This works but has two structural problems:

1. **Process-per-call cost.** Spawning a process takes roughly 1–10 ms depending
   on the runtime. For a map with ten boxes, that's dominated by spawn overhead,
   not by the actual work.

2. **stdout is the wrong channel.** A box function that prints debug output
   corrupts the wire value. Values are transient — they vanish once the pipe is
   drained. The channel has no addressing, no framing, and no backpressure.

The goal is a design where cross-language calls are fast, values are addressable,
and the execution model is ready to parallelize without redesign.

---

## The threading roadmap

Three stages, each building on the last:

**Stage 1 — Synchronous (current).** A single-threaded ready-queue walks the
graph in dependency order. `run_task(box, inputs)` in `src/004-executor.lua` is
the task boundary. It blocks until the driver responds.

**Stage 2 — Coroutine-based non-blocking.** The ready-queue becomes a coroutine
scheduler. Boxes yield when waiting on upstream results or cross-language calls.
LuaSocket non-blocking I/O lets the scheduler run other boxes while one is
waiting for a response. No true parallelism yet, but the graph's natural
concurrency is exploited without blocking the whole process.

**Stage 3 — SoraMech-owned thread pool.** A pool implementation
written for SoraMech, with the 3d-rts pool design as a reference.
The `task_fn` boundary swaps in the thread pool. Worker threads pull
tasks from the pool; OS-level scheduling replaces any single-threaded
ordering. Cross-language calls block a worker thread while other
threads continue. True parallelism across cores.

Effil-jit is not used. Coroutines are not used. The path goes
straight from synchronous (phase 2) to the C pool (phase 3) — no
intermediate coroutine stage.

---

## Stage 3 detail: pthreads task model

Each box execution in the thread pool is a **task struct** containing:

- **Function pointer** — the language-specific invocation wrapper (a C
  function, since pthreads requires a C entry point)
- **Shared memory pointer** — the region holding the input arguments and
  return value for this invocation
- **Dependency list** — pointers to the tasks this task is waiting on
- **Waiting list** — pointers to tasks that are waiting on this task
- **Counter** (iterator boxes only) — current iteration index, updated
  in-place when the iterator re-queues itself

Task structs are allocated once at run start, one per box (not one per
invocation). They live for the entire run.

### Blocking and unblocking — pointer-only transitions

When a task cannot run (upstream output not yet available), it is placed
in its upstream task's waiting list. This is a pointer move — no
allocation, no struct copy. The task struct stays exactly where it is.

When an upstream task completes and writes its return value to shared
memory, it walks its waiting list and re-adds each entry to the main task
queue. Again, a pointer move only. The unblocked task's struct is
unchanged; only its position in the queue changes.

This means "blocked", "queued", and "running" are states defined by which
list a task's pointer currently lives in — not by any field in the struct.

### Per-thread language runtimes

Each worker thread initializes its own language runtimes at startup:
- **Lua:** one `lua_State` per thread. Lua states are not thread-safe, but
  one per thread requires no locking. The task wrapper calls the box
  function via `lua_pcall`.
- **C:** the compiled box function is called directly through the function
  pointer. No interpreter, no overhead beyond the call itself.
- **Bash/other:** a persistent subprocess per thread (Unix domain socket
  server model — see Option 3 above). The task wrapper sends a request and
  blocks the thread waiting for the response.

### Iterator self-re-queue

An iterator task does not terminate after one invocation. When it
completes:
1. It increments its `counter` field in-place (or wraps to 0).
2. It checks its input queue for the next pending value.
3. If a value is waiting: it updates its input pointer and re-adds itself
   to the main task queue. No allocation — the same task struct re-enters
   the queue with updated state.
4. If no value is waiting: it adds itself to the upstream task's waiting
   list, to be re-added when the next input arrives.

Input values are processed one-at-a-time in arrival order. Ten queued
inputs means the iterator task visits the queue ten times, advancing the
counter and firing a different output path each time.

### Blocking semantics for long-running operations

The pool's parallelism comes from running N worker threads concurrently
on N cores. Each worker is sitting inside a single dispatch task at any
given moment. If a box function does something that takes a long time —
`sleep`, blocking I/O, a synchronous network request, waiting on a long-
running subprocess — **the worker thread is occupied for the full
duration**. It cannot pick up other tasks while it waits.

Concretely:
- 16-worker pool, 1 box sleeping 10 seconds → the other 15 workers
  continue normally.
- 16-worker pool, 16 boxes all sleeping → all workers parked, no other
  task runs until a sleep finishes.
- The pool size is also the budget for "blocking activities". Heavy
  use of `sleep`-style boxes means raising `SORAMECH_WORKERS`.

Why the runtime does not have a scheduler: SoraMech is deliberately not
in the business of "park this task and wake it at time T." There is no
delayed-spawn primitive, no timer thread, no polling loop. Adding one
would mean a second concurrency model alongside the pool, with its own
synchronization story. Instead, we lean on the pool's existing
mechanism: a task either runs (occupying a worker) or is blocked on a
slot waiting for a value (parked, no worker held). Long sleeps are an
"occupying a worker" case.

For users who need many concurrent waits without burning workers, the
graph-level pattern is cooperative: split the operation into "kick off
the thing" (returns immediately) and "check whether the thing is done"
(returns immediately, returns a status), and wire them through an
iterator with a queued input. The iterator polls. No individual box
blocks for long. Each box completes quickly, freeing its worker.

This is a documented constraint, not a bug. Box authors should know
that an unbounded `sleep` inside a function is the equivalent of
holding a worker hostage. The cooperative pattern is the way around
it.

---

## The three IPC options

### Option 1 — LuaJIT FFI (Lua ↔ C, in-process)

The Foreign Function Interface lets Lua call C functions that live in a shared
library (`.so`) directly, in the same process, with no subprocess and no channel.
You provide a C declaration (`ffi.cdef[[ int add(int a, int b); ]]`), load the
compiled library (`ffi.load("./mybox.so")`), and call `lib.add(3, 4)` — the CPU
jumps into native code and returns. Overhead is nanoseconds.

Since the executor already does mtime-based dynamic compilation (see `drivers/c.sh`),
compiling to a `.so` instead of a binary is just a flag change (`-shared -fPIC`).
The box function signature changes from `main(argc, argv)` to a named function
with typed arguments — which is strictly cleaner.

**Limitation:** FFI only works for languages that compile to a C-compatible ABI.
Bash cannot. Python can (via `ctypes` on the other side), but it's awkward.
FFI is the right answer for Lua ↔ C only.

**Cross-platform:** LuaJIT FFI works on Linux, macOS, and Windows. `.so` becomes
`.dylib` on macOS and `.dll` on Windows — the `ffi.load()` call is portable if
you handle the extension.

---

### Option 2 — Shared memory (`shm_open` + `mmap`)

`shm_open("/soramech-wire-0", ...)` creates a named memory segment managed by
the kernel — not a file on disk, but a named object in RAM. `mmap()` maps it
into the calling process's address space. Any other process that opens the same
name gets access to the *same physical memory*. A write in one process is
immediately visible in another — no copy, no syscall per access after setup.

For SoraMech, each wire between two boxes would correspond to a named shared
memory slot. The compiler (issue 219) generates the slot names and the read/write
calls. Any language that can call POSIX can participate:

- Lua: via LuaJIT FFI calling `shm_open` + `mmap` from libc
- C: natively
- Bash: via `/dev/shm/<name>` — on Linux, the tmpfs backing shared memory is
  exposed as a filesystem path, so bash can `cat /dev/shm/soramech-wire-0`

**Performance:** After the initial `mmap()` setup, reads and writes are pointer
dereferences — no syscalls, no copies. For small values (strings, numbers) this
is essentially free.

**Complication:** Shared memory is untyped raw bytes. You need a size header
(how many bytes is this value?), a type tag (string? number? JSON object?), and
a synchronization mechanism (how does the reader know the writer is done?). None
of these are hard — a small fixed header in the segment handles all three — but
the compiler must generate all of it correctly for every wire.

**Cross-platform:**
- Linux: full support. `/dev/shm/` filesystem exposure is Linux-specific.
- macOS: `shm_open()` works, but the objects are NOT exposed as files. No
  `/dev/shm/` path. Bash access via file path doesn't work.
- Windows: completely different API (`CreateFileMapping` / `MapViewOfFile`).
  No POSIX `shm_open` without WSL or Cygwin.

Shared memory is powerful but not the most portable option. The bash filesystem
shortcut that makes it cross-language is Linux-only.

---

### Option 3 — Unix domain socket servers (chosen direction)

A Unix domain socket is a socket addressed by a filesystem path instead of an
IP address and port. `/tmp/soramech-bash.sock` is a special file; any process
on the same machine connects to it with the standard socket API. The kernel
routes data directly between processes in memory — no network stack, no loopback,
no protocol headers. Performance is bounded by memory bandwidth.

Unlike anonymous pipes, a socket has a name that unrelated processes can find
without inheriting file descriptors. Multiple clients connect simultaneously;
each gets its own channel. The server handles them with `accept()` in a loop.

**The model for SoraMech:**

Each language runtime runs as a persistent socket server. One server instance
per language per worker thread:

```
/tmp/soramech-bash-0.sock   ← worker thread 0's bash runtime
/tmp/soramech-bash-1.sock   ← worker thread 1's bash runtime
/tmp/soramech-c-0.sock      ← worker thread 0's C runtime
```

The server starts when the program starts, stays alive for the whole run, and
accepts sequential requests — one box call at a time per instance. A Lua worker
connects to its designated socket, sends a framed request (function name +
arguments), and reads back a framed response (return value). The server is a
small script (~50 lines) that sources or compiles the box file, calls the
function, and writes the result.

**Wire protocol:** Length-prefix framing. The sender writes a 4-byte big-endian
integer (the message length in bytes), then that many bytes of content. The
receiver reads 4 bytes, then reads exactly that many bytes. This handles values
containing newlines or null bytes without ambiguity.

**Cost of idle servers:** An idle server process is blocked on `accept()` — zero
CPU, approximately 3–5 MB RSS for a bash process. For 10 languages × 4 threads
= 40 processes ≈ 150–200 MB. Acceptable.

**Integration with the coroutine scheduler (Stage 2):**

With LuaSocket non-blocking sockets, the coroutine that makes a cross-language
call does this:
1. Connect to the language server socket (non-blocking)
2. Send the request
3. `coroutine.yield()` — registers the socket fd with the scheduler
4. Scheduler polls all pending fds with `select()`
5. When the fd is readable, scheduler resumes the coroutine
6. Coroutine reads the response and continues

The cross-language call never blocks the main Lua thread. Other boxes run in
other coroutines while the bash/C server is processing.

**Integration with the SoraMech thread pool (Stage 3):**

Each worker thread owns its set of language server sockets. When `run_task` calls
a cross-language box, it sends to the socket and blocks on the read — the thread
blocks, not the whole program. Other workers continue processing other boxes on
other cores. The server processes run concurrently on whatever cores the OS
schedules them to. This gives real parallelism: a bash box and a C box assigned
to different workers can run simultaneously.

**Cross-platform:**
- Linux: full support.
- macOS: full support. Unix domain sockets are POSIX.
- Windows: supported since Windows 10 1803 (2018). `AF_UNIX` sockets work
  natively. Most languages abstract this through their socket libraries.

Unix domain sockets are the most portable cross-language IPC mechanism that also
supports concurrent connections. They are the chosen direction for the compiler
output and the coroutine/thread-pool executor.

---

## Summary comparison

| Property              | FFI (.so)       | Shared memory       | Unix domain socket  |
|-----------------------|-----------------|---------------------|---------------------|
| Speed (per call)      | nanoseconds     | nanoseconds*        | microseconds        |
| Languages supported   | C-ABI only      | any (with caveats)  | any                 |
| Parallel connections  | N/A (in-process)| yes (with sync)     | yes (natural)       |
| Linux                 | yes             | yes                 | yes                 |
| macOS                 | yes             | partial†            | yes                 |
| Windows               | yes (.dll)      | no (different API)  | yes (Win10+)        |
| Complexity            | low             | medium              | low-medium          |

*After initial mmap setup. †shm_open works; /dev/shm filesystem path does not.

The compiler (issue 219) uses FFI for Lua ↔ C wires where the C source is
compiled to a `.so`, and Unix domain sockets for all other cross-language wires.
Same-language Lua wires are direct function calls — no IPC at all.

---

## Runtime self-construction (issue 319)

User box code can spawn new boxes and wire them into the live graph
mid-run. Two primitives, exposed as language-native function calls
that thin-wrap a single underlying C runtime API:

- **`create_box(spec)`** — takes a structured value matching the
  on-disk box JSON schema (a Lua table, a JSON string, etc.).
  Returns the new box's id. The runtime parses the spec, allocates
  a box record + one slot per input port (slot store growth — 319b),
  and appends to the graph's box index (graph 319d).

- **`connect(connection)`** — takes a structured value matching one
  entry in `connections[]` on disk (`{from_box, from_branch,
  to_box, to_input}`). Appends a wire to the producer's atomic
  connections array via copy-and-publish; the dispatcher walks the
  new array on the producer's next push.

**Plumbing.** Specs reach the active graph and slot store via a
thread-local that `dispatch_action` sets immediately before each
spec invoke and clears immediately after. The runner is linked
with `-rdynamic` so dlopen'd spec plugins can call back into the
runner's `runtime_*` symbols at load time.

**Three growable structures back the feature**:

- Slot store: two-level chunked-append index for slot pointers
  (319b). Slot records and chunks never move; only the top-level
  index can grow, via copy-and-publish + defer-free.
- Graph box index: pointer-array of individually-allocated box
  records, grown under a graph-level mutex via the same pattern.
- Per-box connection lists: atomic copy-and-publish under the
  graph mutex; old arrays stay alive on a stale-list until
  graph teardown.

In all three cases the hot read path is lock-free; mutation takes
a mutex on the rare path. The pattern is the same recipe applied
at three layers.

**Documented limits in slice 1 (issues 319d / 319e)**:

- The dispatch context's per-box arrays (spawn guards, output
  capture) are sized at init with a 4096-box headroom for runtime
  additions. Beyond the cap, spawns silently fail. Growable arrays
  are a follow-on.
- A worker can only dispatch boxes whose language spec was
  initialised on that worker. Specs are initialised at pool start
  based on the static graph's language set; a `create_box` call
  introducing a new language fails at dispatch time with "worker
  handle NULL." Workaround: include one static box of every
  language you intend to runtime-create.
- The connect primitive is race-safe when called from the spec
  of the currently-firing producer (the single-spawn CAS guard
  prevents concurrent reads of that box's connections array).
  Calling `connect` to add a wire FROM a different, potentially-
  firing box is not guaranteed race-safe.
- Lua and C bindings ship in 319e. Bash bindings need the bash
  server line protocol extended with new request types — deferred
  to a follow-on slice.
- The primitives are exposed as per-language function callbacks
  rather than as box kinds (`kind: "create_box"`, `kind: "connect"`).
  The box-kind shape would be more in line with the rest of
  SoraMech's data-flow philosophy; the per-language shape is
  documented as a slice-1 expedient pending a refactor.

---

## Relevant files

- `src/004-executor.lua` — `run_task` boundary, ready-queue
- `src/007-runner-main.lua` — entry point for the interpreter path
- `issues/219-map-compiler.md` — compiler that generates the IPC calls
- `issues/104-runner-synchronous-executor.md` — task boundary design
- `issues/213-queued-inputs-and-task-model.md` — queue-per-port, thread pool notes
- `docs/002-roadmap.md` — phase plan for threading stages
