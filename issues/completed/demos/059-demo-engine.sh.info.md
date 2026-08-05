# 059-demo-engine.sh — the demos' shared build front

Sourced by every `phase-*` launcher. The one place that knows how to
prepare the RAM tiers, regenerate the box registry, and compile a demo
against the engine.

The engine's sources are discovered by pattern rather than listed, so a
new engine file enrolls itself in every demo the moment it is written —
the same rule the Makefile follows for the tests. Each demo used to
carry its own list of the files that existed when its phase finished,
which reads like layering but is not: the station layer calls into the
statics table, the gatherer, and the observer. When the observer put a
shutdown report inside map teardown, phases 2 through 6 all stopped
linking at once.

## Functions

| Function | Takes | Sets or does |
|---|---|---|
| `sora_demo_paths` | project root | Sets `BUILD` (`/tmp/<project>/build`) and `SHARED` (`/dev/shm/<project>`) for the caller, creates both, and relinks `tmp/shared-memory`. The Makefile relinks it too, but a demo run from the launcher never calls make, and the symlink's target vanishes on reboot. |
| `sora_demo_registry` | project root | Regenerates the box registry from whatever sits in `src/boxes/`, exactly as the build does. Run before compiling anything that reaches a box by name. |
| `sora_demo_compile` | project root, output binary, demo source, then any extra compiler flags | Compiles one demo against the whole engine plus the presenter. Extra flags are passed through, for the phase 7 demo which builds itself twice to price its own instrumentation. |

## What gets linked

`libs/*.c`, `src/*.c`, and the generated registry — plus
`061-demo-scene.c`, the presenter, which is demo scaffolding rather
than engine but is wanted by every demo.

`src/boxes/` is deliberately absent: the generated registry includes
those sources whole, so compiling them again would define every box
twice.

## The one exception

The phase 1 launcher does not use `sora_demo_compile`. It names its two
sources — the pool and the presenter — because the pool depends on
nothing above it, and that demo running with nothing else linked is the
phase's entire claim. If that line ever needs another engine source
file, something has reached into the pool that should not have.
