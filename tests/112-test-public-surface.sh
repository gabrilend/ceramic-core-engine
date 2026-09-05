#!/usr/bin/env bash
# 112-test-public-surface.sh — the header is the whole public surface.
#
# What this proves: every symbol the engine exports is declared in
# cera.h, and nothing else is exported. That is the property the whole
# of phase 9 exists to establish — private by default, public only by a
# deliberate act — and it is the kind of property that decays silently
# unless something checks it, because the way to break it is to write
# an ordinary function and forget one word.
#
# How it does it: compile the engine on its own, ask the object file
# which symbols it defines for other translation units to see, and
# check each one against the declarations in the header. A symbol that
# is exported without being declared is a joint that leaked; the fix is
# either the word `static` or a deliberate line in cera.h, and the test
# says which symbol so the author can decide which.
#
# Why an object file rather than a linked binary: a binary carries the
# C library's symbols and the generated code's, and neither is the
# engine's business. The object file holds exactly what this one file
# chose to publish.
#
# Usage: 112-test-public-surface.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
WORK="/dev/shm/$(basename "${DIR}")/surface"
mkdir -p "${WORK}"

CC="${CC:-gcc}"

# The build-time facts do not change which symbols exist, so they are
# given placeholder values rather than being threaded through from the
# build. What matters here is the symbol table, not a runnable object.
"${CC}" -std=gnu11 -O2 -pthread \
    -I"${DIR}/libs" -I"${DIR}/src" \
    -DSORA_CC='"cc"' -DSORA_GENERATOR='"generate"' \
    -DSORA_ROOT='"."' -DSORA_INCLUDE='"."' -DSORA_INCLUDE_LIBS='"."' \
    -DSORA_RAM_SHARED='"/dev/shm"' -DSORA_RAM_EXEC='"/tmp"' \
    -ffunction-sections -fdata-sections \
    -c "${DIR}/src/cera.c" -o "${WORK}/cera.o" || {
        echo "  the engine did not compile on its own"
        exit 1
    }

nm --defined-only --extern-only "${WORK}/cera.o" | awk '{print $3}' | sort -u > "${WORK}/exported.txt"

# Every identifier the header declares, whether a function or anything
# else that could carry a symbol.
grep -oE '\b[a-z_][a-z0-9_]*\s*\(' "${DIR}/src/cera.h" | tr -d ' (' | sort -u > "${WORK}/declared.txt"

comm -23 "${WORK}/exported.txt" "${WORK}/declared.txt" > "${WORK}/leaked.txt"

exported=$(wc -l < "${WORK}/exported.txt")
leaked=$(wc -l < "${WORK}/leaked.txt")

if [[ "${leaked}" -ne 0 ]]; then
    echo "  ${leaked} symbol(s) exported without being declared in cera.h:"
    sed 's/^/    /' "${WORK}/leaked.txt"
    echo
    echo "  Each one is either a joint that wants the word 'static', or a"
    echo "  call that wants a deliberate line in cera.h. Both are one-line"
    echo "  fixes and they mean opposite things, so the choice is the"
    echo "  author's rather than this test's."
    exit 1
fi

echo "  the engine exports ${exported} symbols and declares every one of them"

# The other direction is not an error but is worth saying: a header can
# legitimately declare something the engine does not define, because
# static inline functions live entirely in the header.
undefined=$(comm -13 "${WORK}/exported.txt" "${WORK}/declared.txt" | wc -l)
echo "  the header declares ${undefined} further names it defines inline or does not define"
