# 315 — Reference-counted compiled-map artifacts

## Status
open

## Current behavior

`scripts/soramech-compile.sh` from issue 309 is destructive at the
top:

```bash
rm -rf "$COMPILED"
mkdir -p "$COMPILED" "$COMPILED/boxes" "$COMPILED/src" ...
```

Whatever lived in `<map>/compiled/` before the rebuild is gone. If
another process — a long-running runner, a different shell, a
sibling tool that opened the artifact yesterday — had pinned to
that exact compiled layout (a particular `pool-runner` binary, a
particular `spec.so` ABI, a particular set of pre-compiled C
boxes), the rebuild yanks the rug. The holder either crashes,
silently retains stale `dlopen` handles, or starts reading new
files mid-flight.

This is fine in single-user development. It stops being fine the
moment a SoraMech artifact is the kind of thing other programs
hold onto across time.

## Intended behavior

A compiled artifact carries its own reference count. Other
programs that depend on a particular build of a map are expected
to *acquire* a reference on startup and *release* it on shutdown.
When `soramech-compile` is asked to rebuild a map that has any
live references, it does NOT clobber the existing directory.
Instead it builds a sibling directory next door, leaving the
referenced one untouched until its holders are done with it.

The reference count is **expected to be unreliable**. A crashing
holder doesn't get the chance to release; an SSH session that
drops doesn't either. The design accepts that staleness is the
common case, not the exception — every reference entry carries
enough metadata to be independently validated and (eventually)
reaped. The reference count is a hint and a courtesy, not a
guarantee.

This is conceptually a copy-on-write filesystem for compiled
artifacts: writes (rebuilds) fork the universe when readers
exist; readers can keep reading the version they pinned to;
generations get garbage-collected when nothing observable still
points at them.

## Concept

A compiled directory contains a `.refs` file (hidden, dot-prefixed
so it doesn't clutter directory listings). It's append-only by
design — concurrent acquires never need to take a lock; concurrent
releases write a separate marker without rewriting the file.

Each reference has three parts:

1. **A date.** When the reference was acquired. Lets a janitor
   prune references older than some threshold even if their back
   pointer can't be validated.
2. **A back-pointer.** The identity of the holder — enough
   information that an outside observer can ask "is this holder
   still alive, and does it still want this reference?" without
   trusting the count alone. Concrete candidates:
   - process PID + start time (PID alone is not enough; PIDs
     recycle)
   - a path to a marker file the holder maintains (presence of
     the marker = the holder is still live and aware of the
     ref)
   - some combination (PID + a marker path)
3. **An opaque ref id.** A short string the holder can use later
   to identify *its* reference when it wants to release, so it
   doesn't accidentally release someone else's.

A release writes an entry referencing the original acquire id.
Net live refs = acquires whose id isn't in the release set.

## Reference file format

A line-oriented log. Each line is one of two record types:

```
acquire <id> <iso8601-date> <pid> <pid-start-time-ticks> <marker-path>
release <id> <iso8601-date>
```

Append-only. Holders open with `O_APPEND` and write a single line
per operation. Atomic on POSIX as long as the line fits in
`PIPE_BUF` (4 KB on Linux) — well within budget for these short
records.

Reading: tail through the file, build a set of acquire ids,
subtract release ids; remaining ids are nominally live. Then
validate each survivor's back pointer.

A `.refs.lock` file is used only for compaction (rewriting the
log to drop matched acquire/release pairs) and for the
fork-to-sibling decision during compile. Steady-state reads /
writes don't lock.

## Acquire / release API

A small helper utility, probably a shell script for portability
since holders are written in many languages:

```
scripts/soramech-ref.sh acquire <compiled-dir> [--id <id>] [--marker <path>]
scripts/soramech-ref.sh release <compiled-dir> --id <id>
scripts/soramech-ref.sh list    <compiled-dir>          # JSON, including staleness verdicts
scripts/soramech-ref.sh reap    <compiled-dir>          # compact + drop dead refs
```

`acquire` prints the assigned id to stdout; the holder is
responsible for plumbing it to wherever it'll need to call
release. If no `--id` was supplied, the script generates one
(timestamp + random suffix).

`list` walks the log and prints a JSON array of `{id, date,
holder, valid}` entries. Validation reads the back pointer's
liveness signal (PID exists & start-time matches, or marker file
exists).

`reap` is destructive — it rewrites the log to remove entries
whose holders failed validation. Run periodically; never
automatically as part of compile (compile must not destroy state
its caller might care about).

## Compile-time fork logic

`soramech-compile.sh` consults `<map>/compiled/.refs` (and any
sibling generation directories' `.refs` files) before deciding
where to write:

1. If `<map>/compiled/` doesn't exist → build there, fresh.
2. If it exists but has zero live references → build there, after
   `rm -rf`. Same destructive behavior as today.
3. If it exists with one or more live references → pick a sibling
   directory name (see below) and build there. The original
   `compiled/` stays intact.

After a fork-build, the caller has a new directory; `manifest.json`
in the new directory records the fork relationship (`forked_from`
field pointing at the previous generation's path).

### Sibling directory naming

Two reasonable schemes; pick whichever the implementer prefers:

- **Generation index:** `compiled.0`, `compiled.1`, `compiled.2`...
  Always pick `compiled.N` where N is one more than the highest
  existing generation. Easy to grep for; index order = build
  order.
- **Timestamp suffix:** `compiled.2026-05-19T2347` (ISO 8601
  compact). Order-preserving lexicographically; the name itself
  tells the operator when it was built.

Whichever wins, the script should also leave a `compiled` symlink
pointing at the newest generation, so callers who want "the
latest" don't have to enumerate.

## Stale reference handling

A reference is stale if any of the following hold:

- The PID no longer exists on the system.
- The PID exists but its start time (`/proc/<pid>/stat` field 22)
  doesn't match the recorded value — different process, recycled
  PID.
- The marker file path was supplied at acquire and is now
  missing.

Stale refs are *tolerated*: compile-time fork still triggers if
ANY ref exists, stale or otherwise — the caller can run
`soramech-ref.sh reap` first if they want to consolidate. The
philosophy is: the artifact directory belongs to its readers
until proven otherwise, and the cost of an extra build dir is
trivial compared to the cost of a use-after-rebuild crash.

## Gitignore considerations

Every generation (`compiled/`, `compiled.0/`, `compiled.1/`,
`compiled.2026-05-19T2347/`, the `compiled` symlink) is a derived
artifact and should never be tracked. The existing
`.gitignore` line `tests/maps/*/compiled/` covers the canonical
name; add a sibling pattern to cover generations:

```
tests/maps/*/compiled/
tests/maps/*/compiled.*/
```

The `.refs` file itself lives *inside* the compiled directory,
so it inherits the same gitignore.

## Suggested implementation steps

1. Pick the back-pointer format. Recommend PID + start time +
   optional marker path. Document the choice with a comment at
   the top of the ref helper.
2. Write `scripts/soramech-ref.sh` with `acquire`, `release`,
   `list`, `reap`. Append-only writes for the first two, log
   rewrite under `.refs.lock` for the last.
3. Add a small `tests/315-refs-test.sh` that exercises:
   acquire → list (1 entry, valid) → release → list (0 valid) →
   acquire → kill the holder process → list (1 entry, stale) →
   reap → list (0).
4. Teach `soramech-compile.sh` to read `.refs` and pick the
   target directory. Implement the sibling-naming scheme.
   `compiled` becomes a symlink to the newest generation.
5. Add a `forked_from` field to `manifest.json` when forking.
6. Update `.gitignore` for the new generation pattern.
7. Integration check in `scripts/run-tests.sh`: compile the
   pipeline fixture twice — second time with a live `acquire` —
   assert a sibling generation was created and the original
   pinned generation still runs unchanged.

## Open questions

- Should the runner (`soramech-pool`) itself acquire a reference
  when it opens a compiled directory? On the one hand, yes —
  it's the canonical case the system protects against. On the
  other, soramech-pool runs are usually short-lived and the
  acquire/release overhead per run is unwelcome. Possible
  compromise: only acquire if pool runs longer than N seconds.
- How does a fork chain end? If `compiled.5` is built while
  `compiled.4` had live refs but `compiled.4`'s refs are now all
  stale (caller never released, then died), do we ever prune
  `compiled.4`? Probably yes, via `soramech-ref.sh reap-all
  <map>` which walks every generation and drops the ones whose
  refs all validate as dead.
- Is the `.refs` log itself something that should be versioned
  (`.refs.v1`)? If we ever change the line format, in-flight
  holders won't know how to read it. Versioning the filename
  avoids the problem.
- Does the editor (`maps/<x>/` user-facing) want the same
  mechanism? Probably yes eventually, but out of scope for this
  issue — start with `compiled/` artifacts only.

## Relevant files

- `scripts/soramech-compile.sh` — issue 309's compile pipeline,
  modified here to fork on live references
- `scripts/soramech-ref.sh` — new helper for acquire / release /
  list / reap
- `tests/315-refs-test.sh` — new
- `.gitignore` — extended pattern for generation directories
- `scripts/run-tests.sh` — new integration check
- `issues/309-build-system.md` — parent issue; references this
  one's fork behavior

## Notes (philosophy)

The unreliability is a feature, not a bug. A reliable reference
count would require a kernel-level mechanism (file locks held
for the lifetime of the holder), and that's a tower of
operational complexity for marginal value. By accepting that
holders sometimes lie and sometimes vanish, we get a system
that's robust to crashes, network drops, and SIGKILL — at the
cost of needing a "reap" pass to catch up with reality
occasionally. The back pointer is what makes that catch-up
possible: it's the bridge between what the log claims and what
the world actually shows.

A reference is a wish, not a contract. The compile pipeline
honors wishes by default and only stops honoring them when
nothing in the world supports the wish anymore. That's how
respectful software treats its peers.
