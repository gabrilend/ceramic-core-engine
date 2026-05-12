# 008-pool-runner.c — public surface

Phase 3 runner binary. C entry point that will own the thread pool,
the slot store, the graph loader, and the dispatch layer.

Current state: scaffold. Prints a usage banner and exits 0 when
given a map directory argument.

## External symbols

- `int main(int argc, char **argv)` — entry point. Takes one argument
  (the map directory path), or `--help` / `-h`. Exits 2 if no
  argument is given, 0 otherwise. Will return a nonzero exit code
  on map-load or runtime errors once issues 305 / 301 / 304 land.

## Related

- Issue 301 — thread pool lifecycle that this entry point drives.
- Issue 305 — graph loader called from step 2.
- Issue 311 — JSONL run output written from step 11.
