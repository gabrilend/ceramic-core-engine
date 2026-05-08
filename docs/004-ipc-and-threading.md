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

**Stage 3 — 3d-rts thread pool.** The `task_fn` boundary swaps in the thread
pool. Worker threads pull tasks from the pool; the coroutine scheduler is
replaced by OS-level scheduling. Cross-language calls block a worker thread
while other threads continue. True parallelism across cores.

Effil-jit is not used. The path is synchronous → coroutine → 3d-rts pool.

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

**Integration with the 3d-rts thread pool (Stage 3):**

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

## Relevant files

- `src/004-executor.lua` — `run_task` boundary, ready-queue
- `src/007-runner-main.lua` — entry point for the interpreter path
- `issues/219-map-compiler.md` — compiler that generates the IPC calls
- `issues/104-runner-synchronous-executor.md` — task boundary design
- `issues/213-queued-inputs-and-task-model.md` — queue-per-port, thread pool notes
- `docs/002-roadmap.md` — phase plan for threading stages
