# 308 — Bash language spec implementation

## Status
open

## Current behavior
Bash boxes today run via `drivers/bash.sh`, which `source`s the box
file and invokes the function in a fresh subshell per call. Like the
phase 2 Lua and C drivers, every invocation is a new process; spawn
cost dominates. There is no persistent Bash interpreter between
calls.

## Concept

The Bash language spec implements `lang_spec_t` (issue 303) for Bash.
Unlike the Lua and C specs, Bash cannot be embedded in the runner
process — there is no "libbash.so" to link against, and no in-process
API for executing Bash code. The Bash spec uses **Option 3 — Unix
domain socket servers** from `docs/004-ipc-and-threading.md`: each
worker thread starts and talks to its own persistent Bash subprocess
over a Unix domain socket. The subprocess stays alive for the whole
run, accepting one box invocation at a time.

This makes Bash the proof case that the spec interface accommodates
languages that cannot be embedded. The spec contract from the
dispatch layer's perspective is the same: byte buffers in, byte
buffers out, abort on nonzero return. Where those bytes go (a
`lua_State`, a function pointer, or a socket) is the spec's
business.

## Architecture

```
Worker thread N                  Bash subprocess N
─────────────────                 ─────────────────
spec.invoke(...)                  /tmp/soramech-<run-id>-bash-N.sock
   |                                       |
   send framed request    ──────────►      receive
   recv framed response   ◄──────────      execute box function
                                           send framed response
```

One Bash subprocess per worker thread, addressed by a per-worker
socket path. The subprocess runs `bash-server.sh` (shipped alongside
`spec.c`), which `source`s box files on demand, calls the named
function, and writes back the result.

The subprocess is single-threaded by Bash's nature, but that is
fine: one worker thread sends one request at a time. A 16-worker
pool means 16 Bash subprocesses, each handling its own thread's
requests serially. The pool's parallelism comes from running 16
worker threads (and 16 subprocesses) concurrently — not from any
one Bash subprocess.

## Per-worker state

`init` for the Bash spec:
1. Picks a socket path: `/tmp/soramech-<run-id>-bash-<worker_idx>.sock`.
2. Forks a child process.
3. Child execs `bash-server.sh <socket-path>`.
4. Parent waits briefly for the socket to exist, then connects via
   `connect(AF_UNIX, ...)`.
5. Returns a handle holding the connected socket fd.

```c
typedef struct {
    int   sock_fd;
    pid_t server_pid;
    char  socket_path[256];
} bash_handle_t;
```

`teardown`:
1. Sends a `{ "op": "shutdown" }` framed message.
2. `close(sock_fd)`.
3. `waitpid(server_pid)` to reap the subprocess.
4. `unlink(socket_path)`.

## Wire protocol

Length-prefix framing, identical in both directions:
- 4 bytes: big-endian message length, in bytes.
- N bytes: message body, JSON-encoded.

This handles bytes containing newlines, nulls, or any other character
without ambiguity.

### Request

```json
{
  "op": "invoke",
  "file": "/abs/path/to/box.sh",
  "fn": "process",
  "args": ["arg0_bytes", "arg1_bytes", ...]
}
```

### Response (success)

```json
{
  "ok": true,
  "out": "result_bytes"
}
```

### Response (error)

```json
{
  "ok": false,
  "err": "bash error message",
  "stderr": "any captured stderr"
}
```

A nonzero exit from the box function, a syntax error during `source`,
or a Bash signal all surface as `"ok": false`. The spec returns
nonzero from `invoke`, the dispatch layer aborts (issue 303).

## bash-server.sh

The subprocess script is small. Roughly:

```bash
#!/usr/bin/env bash
# bash-server.sh <socket-path>
SOCKET="$1"

# Set up a listening socket using socat (or python -c 'socket...' fallback).
# Read length-prefix framed messages, dispatch.
while read_request; do
    case "${OP}" in
        invoke)
            if ! declare -F "${FN}" > /dev/null; then
                source "${FILE}"
            fi
            OUTPUT="$( "${FN}" "${ARGS[@]}" 2>/tmp/bash-stderr )"
            RC=$?
            if [ "${RC}" -eq 0 ]; then
                send_response_ok "${OUTPUT}"
            else
                send_response_err "$(< /tmp/bash-stderr)"
            fi
            ;;
        shutdown)
            exit 0
            ;;
    esac
done
```

The actual implementation has more error checking and uses `socat`
or a small inline tool to handle framed socket I/O (Bash has no
native length-prefix socket reader). `socat` is broadly available;
for systems without it, a 30-line C tool can ship alongside.

The `source`-once-per-file caching is automatic from Bash's
perspective: once a file is sourced, its functions are defined in
the shell's environment. Subsequent calls reuse them.

## Box file convention

A Bash box file defines functions by name:

```bash
# maps/<name>/src/process.sh
process() {
    local input="$1"
    local count="$2"
    echo "${input}-${count}"
}
```

The box JSON references it as `ref = "process.sh"`, `fn = "process"`.
Inputs become positional args (`$1`, `$2`, …). The function's stdout
is captured and returned as the output value.

This is the existing phase 2 convention — kept as-is.

## Input marshalling

Inputs are passed as positional arguments to the Bash function. Each
input is bytes; Bash treats them as strings (with the usual caveat
that null bytes are not survivable in Bash variables). Type-aware
marshalling like the Lua and C specs do is not applicable — Bash has
one type, "string." Numeric and boolean inputs arrive as their
text form (`"42"`, `"true"`); the function uses arithmetic expansion
or `[[ ]]` tests as needed.

## Output marshalling

The function's stdout is captured by `$(...)` and sent back as the
output bytes. A `nil` / empty equivalent is just an empty string.
The output type declared in the box JSON is advisory for downstream
consumers — the bytes themselves are whatever the function `echo`ed.

## Variable-size outputs

For variable-size outputs (issue 302's large-value heap), the
spec.c side allocates from the large-value heap on receiving the
response, copies the response bytes there, stores the handle in
`out_buf`. The Bash subprocess does not interact with the heap —
it just sends back its full output, and the spec.c places it.

## Idle cost

Each Bash subprocess at rest is blocked on `read` from its socket.
Zero CPU. Roughly 3–5 MB RSS per process. For a 16-worker pool that
is 50–80 MB total. Acceptable.

The subprocesses start at pool init and stay alive until pool
shutdown. There is no per-call subprocess churn.

## Build

```
langs/bash/
    spec.c            ← lang_spec_t implementation (socket client)
    bash-server.sh    ← persistent subprocess
    Makefile          ← builds spec.so
    spec.so           ← built artifact
```

Spec Makefile target:
```make
spec.so: spec.c
	$(CC) -shared -fPIC -O2 -Wall -o $@ $<
```

No external dependencies beyond libc. The spec uses the standard
POSIX socket API. `bash-server.sh` requires `bash` and `socat`
(or the bundled framing helper) at runtime.

## Error handling

- Subprocess fails to start (missing `bash` or `bash-server.sh`):
  `init` returns NULL. Pool runner aborts.
- Subprocess dies mid-run (segfault, killed): `read` returns 0 or
  short; `invoke` returns nonzero, dispatch layer aborts.
- Box function nonzero exit: `"ok": false` in response, spec
  returns nonzero, abort.
- Wire protocol corruption (bad length prefix, malformed JSON):
  `invoke` returns nonzero, abort.

No retry, no respawn. Per issue 303, errors crash the program.

## Open questions

- `socat` vs a bundled C framing helper: `socat` is broadly
  available but is not always installed by default (Alpine, minimal
  containers). A small bundled tool removes the external dependency.
  Decide based on what's convenient when implementing.
- Per-worker stderr capture: the example uses a shared
  `/tmp/bash-stderr` which would race between workers. Real
  implementation needs per-worker stderr (e.g., a pipe back to the
  subprocess that reads in a separate Bash background job, or just
  per-worker stderr files).
- Bundled framing helper: if we ship one, it lives in
  `langs/bash/bash-frame` and is built by the Makefile. Decision
  deferred.

## Suggested implementation sequence

1. Set up `langs/bash/` directory with a Makefile that builds an
   empty `spec.so` exporting a stub `soramech_lang_spec`.
2. Write `bash-server.sh` standalone — accepts framed requests on
   a socket, sources files, calls functions, writes framed
   responses. Test by hand with `socat` from the command line.
3. Implement `init` in `spec.c`: fork, exec `bash-server.sh`, wait
   for socket, connect, return handle.
4. Implement `teardown`: send shutdown, close socket, reap, unlink.
5. Implement `invoke`: build request JSON, send framed, read framed
   response, parse, copy output bytes, return.
6. Smoke test: a Bash box with one input, one output, runs end-to-end
   through the pool.
7. Add error-path handling: bad responses, dead subprocess, nonzero
   exit codes.
8. Test against `maps/driver-test` (Bash portions).
9. Add variable-size output support: route large responses through
   the large-value heap.
10. Decide socat-vs-bundled-framer based on implementation experience.

## Relevant files

- `drivers/bash.sh` — phase 2 Bash driver, retired by this spec
- `langs/lang-spec.h` — interface header (issue 303)
- `issues/303-language-runtime-spec.md` — spec contract this implements
- `issues/304-task-dispatch-layer.md` — dispatch layer that calls
  `invoke`
- `issues/302-wire-value-slot-store.md` — large-value heap for
  variable-size returns
- `docs/004-ipc-and-threading.md` — Option 3 (Unix domain sockets),
  the model this spec uses
- `issues/306-lua-language-spec.md` — sister spec, in-process Lua
- `issues/307-c-language-spec.md` — sister spec, in-process C with
  compile step
- `maps/driver-test` — multi-language test map including Bash boxes
