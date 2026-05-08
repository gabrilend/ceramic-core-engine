# 214 — tmp symlink recreation on reboot

## Status

completed

## Current Behavior
Each map directory has a `tmp/` symlink pointing to `/tmp/soramech-<name>/`.
On system reboot, `/tmp/` is cleared, so the symlink target no longer exists.
Subsequent runs fail when trying to read or write files through the broken symlink:

```
cat: /mnt/mtwo/programs/sora/soramech/maps/classify-demo/tmp/last-run.json: No such file or directory
```

The runner's existing `mkdir -p .../tmp/logs` call does not reliably recreate
the symlink target because the broken symlink is traversed, not the target itself.

## Intended Behavior
Before any tmp/ access, the runner should resolve the symlink target and ensure
the directory exists, creating it if /tmp/ was wiped since last run.

## Suggested Implementation Steps
1. In `soramech-runner.lua`, before the existing `mkdir -p .../tmp/logs`:
   - Use `readlink` (read-only) to resolve `map_dir .. "/tmp"` to its target path
   - Run a separate `mkdir -p` on the resolved target path
2. The existing `mkdir -p .../tmp/logs` then succeeds as before.

## Implementation notes

`src/007-runner-main.lua` now resolves the `tmp/` symlink target with `readlink` before any tmp/ access, then runs `mkdir -p` on the resolved path. This ensures the `/tmp/soramech-<name>/` directory is recreated after a reboot wipes tmpfs, allowing the subsequent `mkdir -p tmp/logs` to succeed as before.

## Relevant Files
- `soramech-runner.lua` — entry point; line 24 is where the fix is applied
- `scripts/create-map.sh` — originally creates the symlink and its target
