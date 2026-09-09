#!/usr/bin/env bash
# 116-test-outside-the-tree.sh — somebody else builds a program with this engine.
#
# What this proves: the engine can be taken away. A scratch directory
# that has no relationship to this repository gets a copy of cera.c,
# cera.h and the generator, plus a box source and a map written from
# scratch, and the three steps a consumer's build runs — generate,
# compile, link — produce a program that runs and prints the right
# answer.
#
# Why this is the capstone of phase nine. Every other claim in the phase
# is inference from reading the code: that the header is sufficient,
# that nothing reaches back into this tree, that the two linker settings
# are written down somewhere findable. Packaging that has not been
# compiled somewhere else is a guess.
#
# What each failure would mean:
#   - a missing declaration       -> the header is not sufficient
#   - a file not found            -> the engine reaches back into its tree
#   - a link error naming a shim  -> the export list is wrong or missing
#   - a wrong answer              -> the engine does not work when vendored
#
# Copies, never symlinks: a forgotten include path has to fail rather
# than silently resolve back here.
#
# The scratch tree goes in the executable RAM tier under /tmp rather
# than the artifact tier under /dev/shm, because this test compiles
# something and then runs it, and /dev/shm is mounted without execute
# permission. That is the whole reason the project keeps two tiers.
#
# Usage: 116-test-outside-the-tree.sh <project-dir>
set -u
DIR="${1:-/mnt/mtwo/programming/ai-playground/minimal-soramech}"
CC="${CC:-gcc}"
AWAY="/tmp/$(basename "${DIR}")/outside"

rm -rf "${AWAY}"
mkdir -p "${AWAY}/engine" "${AWAY}/generator" "${AWAY}/boxes" "${AWAY}/build"

fail() { echo "  $*"; exit 1; }

# --- what a consumer takes: two files, plus the generator ------------
cp "${DIR}/src/cera.c" "${DIR}/src/cera.h" "${AWAY}/engine/" || fail "no engine to copy"
cp "${DIR}/src/098-engine-surface.syms" "${AWAY}/engine/"    || fail "no export list"
cp "${DIR}"/scripts/*.c "${DIR}"/scripts/*.h "${AWAY}/generator/" 2>/dev/null
[[ -f "${AWAY}/generator/070-generate.c" ]] || fail "the generator did not come across"

# --- what a consumer writes: one box, one map ------------------------
cat > "${AWAY}/boxes/arithmetic.c" <<'BOX'
/* Two boxes. Ordinary C functions: arguments by value, one return each,
 * nothing remembered between calls.
 *
 * Both belong to this imaginary consumer. The map below names these and
 * nothing else, which matters: reaching for one of the engine
 * repository's own demo boxes would make this test pass for a reason
 * that has nothing to do with whether the engine can be taken away. */
int carry(int v)
{
    return v;
}

int add_up(int a, int b)
{
    return a + b;
}
BOX

cat > "${AWAY}/away.map" <<'MAP'
station feed carry p entry
  out 0 - total.0

station total add_up p result
  in 0 - feed.0
  in 1 = 30
MAP

# --- and the program that runs it ------------------------------------
cat > "${AWAY}/away.c" <<'PROG'
#include "cera.h"

#include <stdio.h>

int main(void)
{
    const cera_map_build_t *program = cera_map_build_find("away.map");
    if (!program) {
        fprintf(stderr, "this binary was not built with away.map\n");
        return 1;
    }

    cera_map_t *m = cera_map_create_empty();
    program->build(m, NULL, 0);
    cera_map_start(m, 2);

    const char *refused = cera_map_bring_up(m);
    if (refused) {
        fprintf(stderr, "refused: %s\n", refused);
        return 1;
    }

    const int value = 12;
    const char *no = cera_map_deliver_argument(m, 0, 0, &value, (int)sizeof value);
    if (no) {
        fprintf(stderr, "could not feed it: %s\n", no);
        return 1;
    }

    cera_pool_release(m->pool);
    cera_pool_join(m->pool);

    int answer = 0;
    if (!cera_map_output_take(m, 1, &answer, sizeof answer)) {
        fprintf(stderr, "nothing came back\n");
        return 1;
    }
    printf("%d\n", answer);
    cera_map_destroy(m);
    return 0;
}
PROG

# --- step one: build the generator, which needs nothing from the engine
#     but the one header -------------------------------------------------
"${CC}" -std=gnu11 -O2 -I"${AWAY}/generator" -I"${AWAY}/engine" \
    -o "${AWAY}/build/generate" "${AWAY}"/generator/*.c \
    || fail "the generator did not compile away from home"

# --- step two: generate, over the consumer's own box and map ----------
"${AWAY}/build/generate" "${AWAY}/build/emitted.c" \
    --root="${AWAY}" --map="${AWAY}/away.map" "${AWAY}/boxes/arithmetic.c" \
    || fail "the generator refused a box and a map it should accept"

[[ -s "${AWAY}/build/emitted.c" ]] || fail "the generator wrote nothing"

# --- step three: compile and link ------------------------------------
#
# The two linker settings are not optional and are the easiest thing for
# a consumer to omit: without the export list a program builds and then
# fails at run time when a box arrives, which looks like success.
"${CC}" -std=gnu11 -O2 -pthread -I"${AWAY}/engine" \
    -ffunction-sections -fdata-sections \
    -DCERA_CC='"'"${CC}"'"' \
    -DCERA_GENERATOR='"'"${AWAY}/build/generate"'"' \
    -DCERA_ROOT='"'"${AWAY}"'"' \
    -DCERA_INCLUDE='"'"${AWAY}/engine"'"' \
    -DCERA_RAM_SHARED='"/dev/shm"' -DCERA_RAM_EXEC='"/tmp"' \
    -o "${AWAY}/build/away" \
    "${AWAY}/away.c" "${AWAY}/engine/cera.c" "${AWAY}/build/emitted.c" \
    -Wl,--dynamic-list="${AWAY}/engine/098-engine-surface.syms" \
    -Wl,--gc-sections \
    || fail "a program using the engine did not link away from home"

# --- and it has to be right, not merely built ------------------------
got="$("${AWAY}/build/away" 2>"${AWAY}/build/stderr.txt")"
status=$?
[[ ${status} -eq 0 ]] || fail "the program exited ${status}: $(cat "${AWAY}/build/stderr.txt")"
[[ "${got}" == "42" ]] || fail "the program printed '${got}', not 42"

echo "  a program was built with this engine in a directory that cannot see this one"
echo "  and 12 fed into a box adding 30 came back as ${got}"

# --- the engine must not have reached home ---------------------------
if grep -rq -- "${DIR}" "${AWAY}/build/emitted.c"; then
    fail "the emitted file names this repository; a consumer's build would not relocate"
fi
echo "  and nothing it generated names the repository it came from"

rm -rf "${AWAY}"
