# 008-pool-runner.c — public surface

The `soramech-pool` binary. This is the program that runs a map.
It owns every other phase-3 component: it loads the graph, builds
the slot store, opens the language specs, starts the thread pool,
wires the dispatch context, submits the initial tasks, waits for
quiescence, and reports.

There is one external symbol, because this file is a `main` and
nothing links against it.

## Invocation

```bash
./soramech-pool <map-dir>
./soramech-pool --help
```

Exit codes: 0 on a clean run, 2 when no map directory was given,
nonzero on a load or runtime failure. Every failure prints a
`<file>:<line>: <message>` diagnostic to stderr — the loader's
error string is passed straight through rather than summarised.

## Environment

- `SORAMECH_WORKERS=N` — worker count. Defaults to one per CPU.
- `SORAMECH_LANGS_DIR=<path>` — where to find `<lang>/spec.so`.
  Defaults to the `langs/` directory beside the binary, resolved
  through `/proc/self/exe` so a copied compiled artifact finds
  its own specs rather than the source tree's.
- `SORAMECH_LOG_VALUES=1` — add `task_input` / `task_output`
  events carrying the byte payloads, truncated per record.
- `SORAMECH_LOG_SLOTS=1` — add the startup slot-layout
  enumeration and a per-push event for every wire push. The
  push event's skip reason is the load-bearing diagnostic when a
  box fires but its consumer never sees the value.
- `SORAMECH_BASH_PRESOURCE_FILES=<paths>` — files the Bash spec
  sources once per worker before any box runs.

## What `main` does, in order

1. **Load the graph** — `graph_load` on the map directory.
2. **Slot store and spec registry** — create the store, scan and
   `dlopen` every `langs/<name>/spec.so`.
3. **Pool** — start the workers.
4. **Attach runtime** — allocate the per-port slots, resolve each
   call box to its spec, enumerate the size classes the allocator
   pre-warms from.
5. **Dispatch context** — bind graph, slots, registry, pool, and
   the event queue together, with output capture on so the final
   summary can print each box's last value.
6. **Barrier** — every worker runs each spec's per-worker `init`
   (its own `lua_State`, its own `dlopen` cache, its own Bash
   socketpair) and only then enters the task loop. Nothing
   dispatches until every worker is ready.
7. **Seed** — push every typed-in literal to its port exactly
   once, then walk every box and spawn whichever are already
   ready. This is where the derived entry set enters the queue.
8. **Quiesce** — wait until the queue is empty, no task is in
   flight, and no slot holds a queued value.
9. **Report** — print the per-box outputs to stderr and the run
   totals; the JSONL transcript has already been written
   incrementally by the event queue's writer thread.

## The startup banner

```
soramech-pool: 'hello' — 2 box(es), entry seed, 14 worker(s)
```

The `entry` word comes from `meta.json`'s `entry_box_id`, which
selects nothing — execution uses the set derived by the loader's
entry detection. The field is commonly pointed at a `read` box,
which can never be an entry box at all. Issue 206 (entry box
designation) replaces this with a report of the derived set.

## External symbols

- `int main(int argc, char **argv)`

## Related

- Issue 301 — pool lifecycle this drives.
- Issue 305 — the graph loader called at step 1.
- Issue 311 — the JSONL transcript written across the run.
- `docs/007-architecture.md` — the module stack this sits on top
  of, and the ten-step path a value takes through it.
