#!/usr/bin/env bash
# 131-test-a-second-opinion.sh — every compiler on this machine agrees.
#
# What this proves: the engine compiles without complaint under each C
# compiler available here, both with watching turned on and with it off.
#
# Why it exists. A variable was computed and then thrown away whenever
# watching was compiled out — the emit that would have read it having
# vanished with the flag. One compiler said nothing and the other called
# it out by name, so the fault reached somebody else's machine and
# stopped their build. It was never only a warning: the same lines were
# reading an atomic per station on every teardown to feed an event that
# was never sent, and an unwatched program is not supposed to pay
# anything at all.
#
# The lesson is narrow and worth keeping: **one compiler is one
# opinion.** Anything the build only ever sees through a single pair of
# eyes has a second pair available for free on most machines.
#
# Both halves of the flag are tried, because the interesting mistakes
# live in the half that is usually not compiled.
#
# Usage: 131-test-a-second-opinion.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"

found=()
for candidate in gcc clang cc tcc; do
    command -v "${candidate}" >/dev/null 2>&1 || continue
    # cc is usually one of the others wearing a different name; only
    # worth trying when it is neither.
    real="$(readlink -f "$(command -v "${candidate}")" 2>/dev/null || true)"
    duplicate=""
    for already in "${found[@]:-}"; do
        [[ "$(readlink -f "$(command -v "${already}")" 2>/dev/null || true)" == "${real}" ]] \
            && duplicate=yes
    done
    [[ -n "${duplicate}" ]] || found+=("${candidate}")
done

if [[ "${#found[@]}" -eq 0 ]]; then
    echo "  no compiler found, which cannot be true if this ran"
    exit 1
fi

flags=(-std=gnu11 -Wall -Wextra -Werror -O2 -pthread
       "-I${DIR}/src"
       -DCERA_COMPILER='"serac"'
       -DCERA_ROOT='"."'
       -DCERA_RAM_SHARED='"/dev/shm"' -DCERA_RAM_EXEC='"/tmp"')

failed=0
for compiler in "${found[@]}"; do
    for watching in off on; do
        extra=()
        [[ "${watching}" == "on" ]] && extra=(-DCERA_WATCH)
        if ! complaint="$("${compiler}" "${flags[@]}" "${extra[@]}" \
                          -c "${DIR}/src/cera.c" -o /dev/null 2>&1)"; then
            echo "  ${compiler}, watching ${watching}:"
            printf '%s\n' "${complaint}" | head -6 | sed 's/^/    /'
            failed=1
        fi
    done
done

if [[ "${failed}" -ne 0 ]]; then
    echo
    echo "  A compiler this build does not usually run has found something."
    echo "  One compiler is one opinion; this is the second."
    exit 1
fi

if [[ "${#found[@]}" -eq 1 ]]; then
    echo "  clean under ${found[0]}, watched and unwatched — but there is only"
    echo "  one compiler here, so this is one opinion rather than a second"
else
    echo "  clean under ${found[*]}, watched and unwatched"
fi
