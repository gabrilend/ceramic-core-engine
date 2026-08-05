#!/usr/bin/env bash
#
# 059-demo-engine.sh — the build front shared by every phase demo.
#
# What this is: the one place that knows how to prepare the two RAM
# tiers, regenerate the box registry, and compile a demo program
# against the engine. It is sourced by the phase-* launchers, never
# run on its own.
#
# How it does it, in general terms: the engine's source files are
# discovered by pattern rather than listed, so a new engine file
# enrolls itself in every demo the moment it is written — the same
# rule the Makefile already follows for the tests.
#
# Why it exists: each demo used to carry its own list of engine
# sources, naming the files that existed when that phase was finished.
# That reads like layering but is not: the station layer calls into
# the statics table, the gatherer, and the observer, and those arrived
# in phases 4 and 7. So when the observer put a shutdown report inside
# map_destroy, every demo from phase 2 through 6 stopped linking at
# once, each failing on a symbol its author had never heard of. A list
# of filenames kept in step by hand with code it does not live beside
# will drift every time. Discovery cannot drift.
#
# Phase 1 is the deliberate exception and links the pool alone. The
# pool genuinely depends on nothing above it, and that independence is
# the phase's whole claim — linking more would hide it.

# {{{ sora_demo_paths()
# sora_demo_paths <project-root>
#
# Sets BUILD and SHARED for the caller and makes sure both RAM tiers
# exist: /tmp/<project> for things that get executed, /dev/shm/<project>
# for things that get read.
sora_demo_paths()
{
    local root="$1"
    local project
    project="$(basename "${root}")"

    BUILD="/tmp/${project}/build"
    SHARED="/dev/shm/${project}"
    mkdir -p "${BUILD}"
    mkdir -p "${SHARED}"

    # tmp/shared-memory is a symlink committed to the repository, but
    # what it points at lives in /dev/shm and vanishes on reboot. The
    # Makefile relinks it before it builds; a demo started from the
    # launcher never calls make, so it relinks it too. Without this,
    # every demo's closing "report mirrored at ..." names a path that
    # is not there — the report was written, and the reader is sent
    # somewhere empty.
    ln -sfn "${SHARED}" "/tmp/${project}/shared-memory"
}
# }}}

# {{{ sora_demo_registry()
# sora_demo_registry <project-root>
#
# Regenerates the box registry from whatever sits in src/boxes/,
# exactly as the build does. Run before compiling anything that
# reaches a box by name: a demo compiled against yesterday's registry
# would demonstrate yesterday's engine.
sora_demo_registry()
{
    local root="$1"

    mkdir -p "${root}/src/generated"
    luajit "${root}/scripts/028-generate.lua" \
        "${root}/src/generated/registry.c" "${root}"/src/boxes/*.c
}
# }}}

# {{{ sora_demo_compile()
# sora_demo_compile <project-root> <binary> <demo source> [extra flags...]
#
# Compiles one demo program against the whole engine. Extra flags are
# passed through for the phase that builds itself twice to price its
# own instrumentation.
#
# The engine is libs/ plus src/, plus the generated registry. The box
# sources under src/boxes/ are deliberately absent: the registry
# includes them whole, so compiling them again would define every box
# twice. The generated registry sits in its own directory for the same
# reason the wildcard stops at one level — it is derived, not written.
#
# The presenter comes along too. It is demo scaffolding rather than
# engine — it knows about paragraphs and columns and nothing about
# stations — but every demo speaks through it, so no demo should have
# to remember to ask for it.
sora_demo_compile()
{
    local root="$1"
    local binary="$2"
    local source="$3"
    shift 3

    local engine=(
        "${root}"/libs/*.c
        "${root}"/src/*.c
        "${root}/src/generated/registry.c"
    )

    gcc -std=gnu11 -Wall -Wextra -Werror -g -O2 -pthread "$@" \
        -I"${root}/libs" -I"${root}/src" \
        -o "${binary}" "${source}" \
        "${root}/issues/completed/demos/061-demo-scene.c" \
        "${engine[@]}"
}
# }}}
